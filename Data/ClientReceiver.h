#pragma once

#include <QAbstractTableModel>
#include <QMap>
#include <QVector>
#include <QVariant>
#include <QStringList>
#include <QJsonObject> 

class QJsonObject;

class ClientReceiver : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit ClientReceiver(QObject* parent = nullptr);
    ~ClientReceiver() override = default; // Use default destructor

    // --- QAbstractTableModel overrides ---
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
    // Slot to receive the incoming JSON message containing compressed data
    void processWebSocketMessage(const QJsonObject& message);

private:
    // Internal data storage: Column names mapped to vectors of cell data
    QMap<QString, QVector<QVariant>> m_tableData;
    // List to maintain the order of column headers
    QStringList m_columnHeaders;

    // Helper function to parse CSV and populate internal storage
    bool parseAndLoadData(const QByteArray& decompressedCsvData);
};