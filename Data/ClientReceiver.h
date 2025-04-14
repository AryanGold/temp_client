#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QVariant>
#include <QStringList>
#include <QDate>
#include <QJsonObject>
#include <QMutex> // For thread safety

// Forward declaration
class QJsonObject;

// Structure to hold parsed data for a specific expiration date
struct SmileData {
    QVector<double> strikes;
    QVector<double> theoIvs;
    QVector<double> askIvs;
    QVector<double> bidIvs;
    QStringList tooltips; // Pre-generate tooltips here
    bool isValid = false;
};

class ClientReceiver : public QObject
{
    Q_OBJECT

public:
    explicit ClientReceiver(QObject* parent = nullptr);
    ~ClientReceiver() override = default;

    // Public method to get the smile data for plotting
    SmileData getSmileData(const QString& symbol, const QDate& expirationDate) const;

    // Public method to get currently available symbols
    QStringList getAvailableSymbols() const;

    // Public method to get available expiration dates for a symbol
    QList<QDate> getAvailableExpirationDates(const QString& symbol) const;


signals:
    // Emitted when new data has been processed and is ready
    void dataReady(const QStringList& availableSymbols, const QMap<QString, QList<QDate>>& availableDatesPerSymbol);

public slots:
    // Slot to receive the incoming JSON message containing compressed data
    void processWebSocketMessage(const QString& symbol, const QString& model, const QJsonObject& data);

private:
    // Internal data storage: Symbol -> ExpirationDate -> SmileData
    QMap<QString, QMap<QDate, SmileData>> m_dataStore;
    mutable QMutex m_dataMutex; // Protect access across threads

    // Helper function to parse CSV and populate internal storage
    bool parseAndLoadData(const QByteArray& decompressedCsvData, QMap<QString, QMap<QDate, SmileData>>& outData); // Pass temp map

    // Helper for zlib decompression (or include from utility)
    QByteArray decompressZlib(const QByteArray& compressedData);
};