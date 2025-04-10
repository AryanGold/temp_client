#include "TickerDataTableModel.h"

#include <QDateTime>
#include <QFont>
#include <QColor>
#include <QDebug>
#include <algorithm> // for std::find

// Define initial headers - these might be adjusted dynamically
const QStringList INITIAL_HEADERS = { "Symbol", "Model", "Timestamp" };
// Define update interval for batching (milliseconds)
const int UPDATE_INTERVAL_MS = 100;


TickerDataTableModel::TickerDataTableModel(SymbolDataManager* dataManager, QObject* parent)
    : QAbstractTableModel(parent), m_dataManager(dataManager)
{
    m_headers = INITIAL_HEADERS;

    // Setup timer for batching updates
    m_updateTimer.setInterval(UPDATE_INTERVAL_MS);
    m_updateTimer.setSingleShot(true); // Fire only once per interval after activity
    connect(&m_updateTimer, &QTimer::timeout, this, &TickerDataTableModel::processPendingUpdates);
}

int TickerDataTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_tickerData.count();
}

int TickerDataTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_headers.count();
}

QVariant TickerDataTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_tickerData.count() || index.column() >= m_headers.count()) {
        return QVariant();
    }

    const TickerRowData& rowData = m_tickerData.at(index.row());
    const QString& header = m_headers.at(index.column());

    if (role == Qt::DisplayRole) {
        if (header == "Symbol") return rowData.symbol;
        if (header == "Model") return rowData.model;
        if (header == "Timestamp") {
            // Format timestamp nicely
            return QDateTime::fromMSecsSinceEpoch(rowData.lastUpdateTime).toString(Qt::ISODateWithMs);
        }
        // Return data from the dynamic fields map
        return rowData.fields.value(header, QVariant()); // Return empty QVariant if key not found
    }
    // Add other roles (e.g., Qt::TextAlignmentRole, Qt::ForegroundRole for highlighting changes)
    else if (role == Qt::TextAlignmentRole) {
        // Example: Right-align numerical data (check if value is numeric)
        QVariant value = rowData.fields.value(header);
        bool isNumeric;
        value.toDouble(&isNumeric);
        if (isNumeric || header == "Timestamp") { // Align timestamp right too
            return QVariant::fromValue(Qt::AlignRight | Qt::AlignVCenter);
        }
        return QVariant::fromValue(Qt::AlignLeft | Qt::AlignVCenter);
    }
    // Example: Highlight recent updates 
   /*
   else if (role == Qt::BackgroundRole) {
       qint64 now = QDateTime::currentMSecsSinceEpoch();
       if (now - rowData.lastUpdateTime < 500) { // Highlight if updated < 500ms ago
           return QColor(Qt::yellow).lighter(180);
       }
   }
   */

    return QVariant();
}

QVariant TickerDataTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role == Qt::DisplayRole && orientation == Qt::Horizontal) {
        if (section >= 0 && section < m_headers.count()) {
            return m_headers.at(section);
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

QString TickerDataTableModel::generateKey(const QString& symbol, const QString& model) const {
    return symbol + "_" + model;
}


void TickerDataTableModel::handleTickerDataReceived(const QString& symbol, const QString& model, const QVariantMap& data) {
    // Crucial: Check if the symbol is currently active in the data manager
    if (m_dataManager->getSymbolState(symbol, model) != SymbolDataManager::SymbolState::Active) {
        // qInfo() << "Ignoring data for paused/removed symbol:" << symbol << model;
        return; // Ignore data for paused or non-existent symbols
    }

    // Prepare data for batching
    QString key = generateKey(symbol, model);
    TickerRowData updateData;
    updateData.symbol = symbol;
    updateData.model = model;
    ///updateData.fields = QMap<QString, QVariant>::fromMap(data); // Convert QVariantMap to QMap
    updateData.lastUpdateTime = QDateTime::currentMSecsSinceEpoch(); // Use arrival time

    { // Lock scope for pending updates map
        QMutexLocker locker(&m_pendingUpdatesMutex);
        m_pendingUpdates[key] = updateData; // Overwrite previous pending update for the same key
    }

    // Start or restart the timer to process the batch soon
    if (!m_updateTimer.isActive()) {
        m_updateTimer.start();
    }
}

void TickerDataTableModel::processPendingUpdates() {
    QMap<QString, TickerRowData> updatesToProcess;
    { // Lock scope
        QMutexLocker locker(&m_pendingUpdatesMutex);
        if (m_pendingUpdates.isEmpty()) {
            return; // Nothing to process
        }
        updatesToProcess = std::move(m_pendingUpdates); // Move data out efficiently
        m_pendingUpdates.clear(); // Clear the original map
    }

    // Check for new headers from the batch
    if (!updatesToProcess.isEmpty()) {
        updateHeaders(updatesToProcess.first().fields); // Use first item's fields as sample
    }

    // Process updates (add or update rows)
    for (const auto& data : updatesToProcess) {
        addOrUpdateRow(data);
    }
}


void TickerDataTableModel::updateHeaders(const QVariantMap& dataSample) {
    bool headersChanged = false;
    QStringList currentKeys = dataSample.keys();

    // Check for new keys to add as headers
    for (const QString& key : std::as_const(currentKeys)) {
        if (!m_headers.contains(key)) {
            m_headers.append(key);
            headersChanged = true;
        }
    }

    // If headers changed, signal the view to reset
    if (headersChanged) {
        qInfo() << "Headers updated:" << m_headers;
        beginResetModel(); // More drastic than headerDataChanged, forces view update
        endResetModel();
    }
}

void TickerDataTableModel::addOrUpdateRow(const TickerRowData& newData) {
    QString key = generateKey(newData.symbol, newData.model);

    if (m_rowMap.contains(key)) {
        // Update existing row
        int rowIndex = m_rowMap.value(key);
        if (rowIndex >= 0 && rowIndex < m_tickerData.count()) {
            m_tickerData[rowIndex] = newData; // Replace data
            // Emit dataChanged for the entire row
            emit dataChanged(index(rowIndex, 0), index(rowIndex, columnCount() - 1), { Qt::DisplayRole, Qt::BackgroundRole }); // Include roles that might change
        }
        else {
            qWarning() << "Row map contains key but index is invalid:" << key << rowIndex;
            // Fallback to adding as new row?
            m_rowMap.remove(key); // Remove invalid entry
            goto AddNewRow; // Use goto for simplicity here, or restructure
        }
    }
    else {
    AddNewRow:
        // Add new row
        int newRowIndex = m_tickerData.count();
        beginInsertRows(QModelIndex(), newRowIndex, newRowIndex);
        m_tickerData.append(newData);
        m_rowMap.insert(key, newRowIndex); // Add to map
        endInsertRows();
    }
}

void TickerDataTableModel::removeRow(const QString& symbol, const QString& model) {
    QString key = generateKey(symbol, model);
    if (m_rowMap.contains(key)) {
        int rowIndex = m_rowMap.value(key);
        if (rowIndex >= 0 && rowIndex < m_tickerData.count()) {
            beginRemoveRows(QModelIndex(), rowIndex, rowIndex);
            m_tickerData.removeAt(rowIndex);
            m_rowMap.remove(key); // Remove from map

            // Update row indices in the map for rows that came after the removed one
            for (auto it = m_rowMap.begin(); it != m_rowMap.end(); ++it) {
                if (it.value() > rowIndex) {
                    it.value()--; // Decrement index
                }
            }
            endRemoveRows();
        }
        else {
            qWarning() << "Row map contains key for removal but index is invalid:" << key << rowIndex;
            m_rowMap.remove(key); // Clean up invalid entry
        }
    }
}

// --- Slots reacting to symbol list changes ---

void TickerDataTableModel::handleSymbolRemoved(const QString& symbol, const QString& model) {
    // If a symbol is removed from the watchlist, remove its row from the table
    removeRow(symbol, model);
}

void TickerDataTableModel::handleSymbolStateChanged(const QString& symbol, const QString& model, SymbolDataManager::SymbolState newState) {
    // If a symbol is paused, remove its row from the table
    if (newState == SymbolDataManager::SymbolState::Paused) {
        removeRow(symbol, model);
    }
    // If it's resumed, data will start flowing again via handleTickerDataReceived,
    // which will re-add the row if it's not present. No action needed here for resume.
}