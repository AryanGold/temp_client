#include "ClientReceiver.h"
#include "ArchiveHelper.h"
#include "Glob/Logger.h"

#include <QJsonValue>
#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QDebug>
#include <QColor>


ClientReceiver::ClientReceiver(QObject* parent)
    : QAbstractTableModel(parent)
{
}

// --- QAbstractTableModel Implementation ---

int ClientReceiver::rowCount(const QModelIndex& parent) const
{
    // The parent parameter is for tree models, ignore it for table models.
    if (parent.isValid())
        return 0;

    // Row count is the size of any column vector (they should all be the same size)
    // Return 0 if there are no columns/rows yet.
    if (m_columnHeaders.isEmpty()) {
        return 0;
    }
    // Assuming all vectors have the same size after population
    return m_tableData.value(m_columnHeaders.first()).size();
}

int ClientReceiver::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    // Column count is the number of headers we stored
    return m_columnHeaders.size();
}

QVariant ClientReceiver::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant(); // Invalid index

    // Check row and column bounds
    if (index.row() >= rowCount() || index.row() < 0 ||
        index.column() >= columnCount() || index.column() < 0) {
        return QVariant();
    }

    // Get the column name based on the index column
    const QString& columnName = m_columnHeaders.at(index.column());

    // --- Handle different data roles ---
    if (role == Qt::DisplayRole) {
        // Return the data from the specific cell
        return m_tableData.value(columnName).at(index.row());
    }
    else if (role == Qt::ToolTipRole) {
        // Example: Show data type as tooltip
        return QString("Value: %1\nType: %2")
            .arg(m_tableData.value(columnName).at(index.row()).toString()) // Show string representation
            .arg(m_tableData.value(columnName).at(index.row()).typeName());
    }
    // Example: Background color based on column (can be expanded)
    // else if (role == Qt::BackgroundRole) {
    //     if (index.column() % 2 == 0) {
    //         return QColor(Qt::lightGray);
    //     }
    // }

    // Return empty QVariant for unhandled roles
    return QVariant();
}

QVariant ClientReceiver::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant(); // Only handle display role for headers

    if (orientation == Qt::Horizontal) {
        // Return the column header name
        if (section >= 0 && section < m_columnHeaders.size()) {
            return m_columnHeaders.at(section);
        }
    }
    else if (orientation == Qt::Vertical) {
        // Return the row number (starting from 1 for display)
        return section + 1;
    }

    return QVariant(); // Invalid section or orientation
}

// --- Data Processing Slot ---

void ClientReceiver::processWebSocketMessage(const QJsonObject& message)
{
    QString type = message.value("type").toString();
    if (type != "data_stream") { 
        return; 
    }
    QString symbol = message.value("symbol").toString();
    QString model = message.value("model").toString();
    QJsonObject metrics = message.value("metrics").toObject();
    QJsonValue compressedDataValue = message.value("data_compressed");

    qDebug() << "Processing data stream for" << symbol << "/" << model;

    QByteArray decompressedBytes; // Will hold the result

    if (!compressedDataValue.isNull() && compressedDataValue.isString()) {
        QString compressedDataB64 = compressedDataValue.toString();
        if (!compressedDataB64.isEmpty()) {
            QByteArray compressedBytes = QByteArray::fromBase64(compressedDataB64.toLatin1());

            decompressedBytes = decompressZlib(compressedBytes); 

            if (decompressedBytes.isNull() && !compressedBytes.isEmpty()) { // Check for null from our helper
                qWarning() << "Failed to decompress data for" << symbol << model << "(decompressZlib returned null/error)";
                decompressedBytes.clear(); // Ensure it's empty
            }
        }
        else {
            qDebug() << "Received empty compressed data string for" << symbol << model;
        }
    }
    else {
        qWarning() << "'data_compressed' field missing, null, or not a string for" << symbol << model;
    }

    // --- Reset Model and Load New Data ---
    beginResetModel();
    m_tableData.clear();
    m_columnHeaders.clear();

    bool parseOk = false;
    if (!decompressedBytes.isEmpty()) {
        parseOk = parseAndLoadData(decompressedBytes);
    }
    else {
        parseOk = true; // Handle case of no data
    }
    endResetModel();

    if (parseOk) {
        qDebug() << "Model updated successfully for" << symbol << "/" << model << "Rows:" << rowCount() << "Cols:" << columnCount();
    }
    else {
        qWarning() << "Failed to parse or load data into model for" << symbol << "/" << model;
    }
}

// --- Private Helper for Parsing ---

bool ClientReceiver::parseAndLoadData(const QByteArray& decompressedCsvData)
{
    QString csvData = QString::fromUtf8(decompressedCsvData);
    QStringList lines = csvData.split('\n', Qt::SkipEmptyParts);

    if (lines.isEmpty()) {
        qDebug() << "CSV data is empty after decompression.";
        return true; // Successfully processed empty data
    }

    // --- Parse Header ---
    QString headerLine = lines.first();
    QStringList rawHeaders = headerLine.split(','); // Use correct separator
    if (rawHeaders.isEmpty()) {
        qWarning() << "CSV header is empty or invalid.";
        return false;
    }

    // Prepare internal headers and create column vectors
    m_columnHeaders.reserve(rawHeaders.size());
    for (const QString& header : rawHeaders) {
        QString trimmedHeader = header.trimmed();
        if (!trimmedHeader.isEmpty()) {
            m_columnHeaders.append(trimmedHeader);
            m_tableData[trimmedHeader] = QVector<QVariant>();
        }
        else {
            qWarning() << "Empty column header detected, skipping.";
        }
    }

    if (m_columnHeaders.isEmpty()) {
        qWarning() << "No valid column headers found.";
        return false;
    }
    int numInternalColumns = m_columnHeaders.size();

    // --- Parse Data Rows ---
    int expectedRowCount = lines.size() - 1;
    if (expectedRowCount <= 0) {
        qDebug() << "CSV contains only a header row.";
        return true; // Successfully processed header-only data
    }

    // Pre-allocate rows for potential performance improvement
    for (const QString& header : m_columnHeaders) {
        m_tableData[header].reserve(expectedRowCount);
    }

    for (int i = 1; i < lines.size(); ++i) { // Start from second line (data)
        QStringList values = lines[i].split(',');
        if (values.size() != rawHeaders.size()) { // Check against original header count
            qWarning() << "Skipping CSV line" << i + 1 << ": Mismatched column count (" << values.size() << "vs header" << rawHeaders.size() << ")";
            // How to handle? Option 1: Skip row (simple). Option 2: Try to pad/truncate (complex).
            // Option 3: Invalidate all data for this row? For now, we skip appending, which
            // might lead to inconsistent column lengths if not handled carefully later.
            // A better approach might be to fail the whole parse or pre-validate row lengths.
            continue; // Skipping this row
        }

        // Populate the vectors based on the *internal* header order
        for (int j = 0; j < rawHeaders.size(); ++j) {
            QString rawHeader = rawHeaders[j].trimmed();
            // Only process if it's a header we decided to keep
            if (m_tableData.contains(rawHeader)) {
                QString valueStr = values[j].trimmed();
                QVariant cellValue;

                // --- Basic Type Sniffing/Conversion ---
                // This should ideally be more robust, maybe based on column name patterns
                // or predefined schema if available.
                bool okNum;
                double d = valueStr.toDouble(&okNum);
                if (okNum) {
                    bool okInt;
                    int intVal = valueStr.toInt(&okInt);
                    if (okInt && qFuzzyCompare(d, static_cast<double>(intVal))) {
                        cellValue = intVal;
                    }
                    else {
                        cellValue = d;
                    }
                }
                else {
                    QDateTime dt = QDateTime::fromString(valueStr, "yyyy-MM-ddTHH:mm:ss.zzzZ"); // ISO Format
                    if (dt.isValid()) {
                        cellValue = dt;
                    }
                    else if (valueStr.compare("true", Qt::CaseInsensitive) == 0) {
                        cellValue = true;
                    }
                    else if (valueStr.compare("false", Qt::CaseInsensitive) == 0) {
                        cellValue = false;
                    }
                    else {
                        // Store as string if no other type matches
                        cellValue = valueStr;
                    }
                }
                m_tableData[rawHeader].append(cellValue);
            }
        }
    }

    // --- Final Validation (Optional but Recommended) ---
    int finalRowCount = -1;
    bool consistent = true;
    for (auto it = m_tableData.constBegin(); it != m_tableData.constEnd(); ++it) {
        if (finalRowCount == -1) {
            finalRowCount = it.value().size();
        }
        else if (it.value().size() != finalRowCount) {
            qWarning() << "Inconsistent row count detected after parsing! Column" << it.key() << "has" << it.value().size() << "rows, expected" << finalRowCount;
            consistent = false;
            // Decide how to handle - maybe clear everything and return false?
        }
    }

    return consistent; // Return true only if all columns have the same number of rows
}
