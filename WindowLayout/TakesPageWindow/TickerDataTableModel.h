#pragma once

#include "Data/SymbolDataManager.h"

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include <QVariantMap>
#include <QStringList>
#include <QTimer> 
#include <QMutex> 

// Represents one row in the table
struct TickerRowData {
    QString symbol;
    QString model;
    QMap<QString, QVariant> fields; // FieldName -> Value
    qint64 lastUpdateTime = 0; // Timestamp for sorting or highlighting
};


class TickerDataTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit TickerDataTableModel(SymbolDataManager* dataManager, QObject* parent = nullptr);
    ~TickerDataTableModel() override = default;

    // QAbstractTableModel overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
    // Slot to receive data from WebSocket/DataManager
    void handleTickerDataReceived(const QString& symbol, const QString& model, const QVariantMap& data);
    // Slots to react to symbol list changes
    void handleSymbolRemoved(const QString& symbol, const QString& model);
    void handleSymbolStateChanged(const QString& symbol, const QString& model, SymbolDataManager::SymbolState newState);

private slots:
    void processPendingUpdates(); // Process batched updates

private:
    SymbolDataManager* m_dataManager; // To check if symbol is active

    QStringList m_headers; // Dynamic list of column headers (e.g., Symbol, Model, Price, Size, Time, ...)
    QList<TickerRowData> m_tickerData; // Holds all rows currently displayed
    QMap<QString, int> m_rowMap; // Map key (symbol_model) to row index for fast updates

    // Batching updates to avoid excessive UI refreshes
    QTimer m_updateTimer;
    QMap<QString, TickerRowData> m_pendingUpdates; // Key: symbol_model
    QMutex m_pendingUpdatesMutex; // Protect pending updates if accessed from different threads


    void updateHeaders(const QVariantMap& dataSample);
    void addOrUpdateRow(const TickerRowData& newData);
    void removeRow(const QString& symbol, const QString& model);
    QString generateKey(const QString& symbol, const QString& model) const;
};
