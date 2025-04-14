
#include "ClientReceiver.h"
#include "Glob/Logger.h"

#include <zlib.h> 
#include <QJsonValue>
#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QDebug>
#include <QMutexLocker>
#include <QTextStream>
#include <limits>
#include <vector> // For zlib buffer

// --- ZLIB Decompression Helper ---
#define ZLIB_CHUNK_SIZE 16384 // Process 16KB chunks

QByteArray ClientReceiver::decompressZlib(const QByteArray& compressedData) {
    if (compressedData.isEmpty()) {
        return QByteArray();
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        Log.msg(FNAME + QString("zlib inflateInit failed with error code: %1").arg(ret), Logger::Level::ERROR);
        return QByteArray(); // Return empty on init error
    }

    QByteArray decompressedResult;
    // Reserve space heuristically (e.g., assume 5x compression ratio)
    decompressedResult.reserve(compressedData.size() * 5);
    std::vector<Bytef> outBuffer(ZLIB_CHUNK_SIZE);

    strm.avail_in = compressedData.size();
    // Need non-const pointer for zlib C API
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.constData()));

    // Run inflate() on input until output buffer is not full or end of stream
    do {
        strm.avail_out = ZLIB_CHUNK_SIZE;
        strm.next_out = outBuffer.data(); // Use .data() for pointer

        ret = inflate(&strm, Z_NO_FLUSH);
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR; // Dictionary needed error -> treat as data error
            [[fallthrough]]; // Intentional fallthrough
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_STREAM_ERROR: // Include stream error
            Log.msg(FNAME + QString("zlib inflate failed with error code: %1, msg: %2")
                .arg(ret).arg(strm.msg ? strm.msg : "N/A"), Logger::Level::ERROR);
            inflateEnd(&strm);
            return QByteArray(); // Return empty on error
        }

        // Calculate how much data was actually decompressed in this chunk
        unsigned int have = ZLIB_CHUNK_SIZE - strm.avail_out;
        if (have > 0) {
            // Append the valid data from outBuffer to our result QByteArray
            decompressedResult.append(reinterpret_cast<const char*>(outBuffer.data()), have);
        }
    } while (strm.avail_out == 0); // Continue if the output buffer was filled completely

    // Clean up
    inflateEnd(&strm);

    // Check if stream ended properly (could potentially have ended mid-chunk)
    if (ret != Z_STREAM_END) {
        Log.msg(FNAME + QString("zlib inflate finished, but stream did not end properly (final ret code: %1). Data might be incomplete or corrupt.")
            .arg(ret), Logger::Level::WARNING);
    }

    Log.msg(FNAME + QString("Decompressed data size: %1 bytes.").arg(decompressedResult.size()), Logger::Level::DEBUG);
    return decompressedResult;
}
// --- End ZLIB Helper ---


ClientReceiver::ClientReceiver(QObject* parent) : QObject(parent) {
    Log.msg(FNAME + QString("Created."), Logger::Level::INFO);
}

SmileData ClientReceiver::getSmileData(const QString& symbol, const QDate& expirationDate) const {
    QMutexLocker locker(&m_dataMutex);
    // Use value() with default SmileData to avoid creating empty maps if symbol/date doesn't exist
    return m_dataStore.value(symbol).value(expirationDate, SmileData());
}

QStringList ClientReceiver::getAvailableSymbols() const {
    QMutexLocker locker(&m_dataMutex);
    return m_dataStore.keys();
}

QList<QDate> ClientReceiver::getAvailableExpirationDates(const QString& symbol) const {
    QMutexLocker locker(&m_dataMutex);
    QList<QDate> dates = m_dataStore.value(symbol).keys(); // Use value() to avoid creating entry if symbol not found
    std::sort(dates.begin(), dates.end());
    return dates;
}

void ClientReceiver::processWebSocketMessage(const QString& symbol, const QString& model, const QJsonObject& data) {
    Log.msg(FNAME + QString("Processing WebSocket message..."), Logger::Level::DEBUG);
    QString type = data.value("type").toString();
    if (type != "data_stream") {
        Log.msg(FNAME + QString("Received message with invalid type: '%1'.").arg(type), Logger::Level::WARNING);
        return;
    }

    QJsonObject metrics = data.value("metrics").toObject();
    QJsonValue compressedDataValue = data.value("data_compressed");

    Log.msg(FNAME + QString("Processing data stream for Symbol: %1 / Model: %2").arg(symbol).arg(model),
        Logger::Level::DEBUG);

    QByteArray decompressedBytes;
    if (!compressedDataValue.isNull() && compressedDataValue.isString()) {
        QString compressedDataB64 = compressedDataValue.toString();
        if (!compressedDataB64.isEmpty()) {
            QByteArray compressedBytes = QByteArray::fromBase64(compressedDataB64.toLatin1());
            decompressedBytes = decompressZlib(compressedBytes);
            if (decompressedBytes.isNull() && !compressedBytes.isEmpty()) {
                Log.msg(FNAME + QString("Failed to decompress data for symbol '%1'.").arg(symbol), Logger::Level::ERROR);
                return;
            }
        }
        else {
            Log.msg(FNAME + QString("Received empty compressed data string for symbol[%1], model[%2].")
                .arg(symbol).arg(model), Logger::Level::WARNING);
            // Decide if empty data means clearing existing data for this symbol?
            // For now, just return without updating.
            return;
        }
    }
    else {
        Log.msg(FNAME + QString("'data_compressed' field missing, null, or not a string for symbolsymbol[%1], model[%2].")
            .arg(symbol).arg(model), Logger::Level::WARNING);
        return;
    }

    if (decompressedBytes.isEmpty()) {
        Log.msg(FNAME + QString("Decompressed data is empty for symbol[%1], model[%2].")
            .arg(symbol).arg(model), Logger::Level::WARNING);
        // Decide if empty data means clearing existing data for this symbol?
        // For now, just return without updating.
        return;
    }

    // Use a temporary map for parsing results
    QMap<QString, QMap<QDate, SmileData>> newlyParsedData;
    bool parseOk = parseAndLoadData(decompressedBytes, newlyParsedData);

    if (parseOk) {
        Log.msg(FNAME + QString("Successfully parsed decompressed data."), Logger::Level::DEBUG);
        QMutexLocker locker(&m_dataMutex);

        // Merge newly parsed data into the main data store
        // This implementation REPLACES data for existing symbol/date combinations found in the new data
        // and adds any new symbol/date combinations. It does NOT remove old dates not present in the new data.
        for (auto symbol_it = newlyParsedData.constBegin(); symbol_it != newlyParsedData.constEnd(); ++symbol_it) {
            const QString& currentSymbol = symbol_it.key();
            // Get or create the symbol entry in the main store
            QMap<QDate, SmileData>& targetDateMap = m_dataStore[currentSymbol];
            const QMap<QDate, SmileData>& sourceDateMap = symbol_it.value();
            for (auto date_it = sourceDateMap.constBegin(); date_it != sourceDateMap.constEnd(); ++date_it) {
                targetDateMap[date_it.key()] = date_it.value(); // Replace or insert
                Log.msg(FNAME + QString("Updated internal store for Symbol: %1, Date: %2")
                    .arg(currentSymbol).arg(date_it.key().toString(Qt::ISODate)), Logger::Level::DEBUG);
            }
        }

        // Prepare data for the signal
        QStringList availableSymbols = m_dataStore.keys();
        QMap<QString, QList<QDate>> availableDatesPerSymbol;
        for (const QString& sym : availableSymbols) {
            QList<QDate> dates = m_dataStore[sym].keys();
            std::sort(dates.begin(), dates.end());
            availableDatesPerSymbol[sym] = dates;
        }
        locker.unlock(); // Unlock before emitting signal
        emit dataReady(availableSymbols, availableDatesPerSymbol);

    }
    else {
        Log.msg(FNAME + QString("Failed to parse or load decompressed data symbol[%1], model[%2].")
            .arg(symbol).arg(model), Logger::Level::ERROR);
    }
}

bool ClientReceiver::parseAndLoadData(const QByteArray& decompressedCsvData, QMap<QString, QMap<QDate, SmileData>>& outData) {
    outData.clear();
    QTextStream stream(decompressedCsvData);
    QString headerLine = stream.readLine().trimmed(); // Trim header line
    if (headerLine.isNull() || headerLine.isEmpty()) {
        Log.msg(FNAME + QString("CSV data is empty or contains no header."), Logger::Level::WARNING);
        return false;
    }

    QStringList headers = headerLine.split(',');
    // Trim headers
    for (QString& header : headers) {
        header = header.trimmed();
    }

    // --- Find required column indices ---
    int idx_ticker = headers.indexOf("ticker");
    int idx_exp_date = headers.indexOf("expiration_dates");
    int idx_strike = headers.indexOf("strikes");
    int idx_theo_iv = headers.indexOf("theo_ivs");
    int idx_ask_iv = headers.indexOf("ask_iv");
    int idx_bid_iv = headers.indexOf("bid_iv");
    // Optional indices for tooltips
    int idx_option_type = headers.indexOf("option_types");
    int idx_mid_iv = headers.indexOf("mid_iv");
    int idx_log_moneyness = headers.indexOf("log_moneyness");

    // --- Basic Validation ---
    if (idx_ticker == -1 || idx_exp_date == -1 || idx_strike == -1 || idx_theo_iv == -1 || idx_ask_iv == -1 || idx_bid_iv == -1) {
        Log.msg(FNAME + QString("CSV missing one or more required columns (ticker, expiration_dates, strikes, theo_ivs, ask_iv, bid_iv). Header: '%1'").arg(headerLine), Logger::Level::ERROR);
        return false;
    }
    Log.msg(FNAME + QString("CSV Header parsed successfully. Required columns found."), Logger::Level::DEBUG);

    // --- Parse Data Rows ---
    int lineNum = 1;
    int rowsParsed = 0;
    int rowsSkipped = 0;
    while (!stream.atEnd()) {
        lineNum++;
        QString line = stream.readLine();
        if (line.trimmed().isEmpty()) continue; // Skip empty lines

        QStringList values = line.split(',');
        if (values.size() != headers.size()) {
            Log.msg(FNAME + QString("Skipping CSV line %1: Mismatched column count (%2 vs header %3)")
                .arg(lineNum).arg(values.size()).arg(headers.size()), Logger::Level::WARNING);
            rowsSkipped++;
            continue;
        }

        // --- Extract and Validate Data ---
        QString symbol = values.at(idx_ticker).trimmed();
        // Date parsing: Try both formats just in case backend format changes slightly
        QDate expirationDate = QDate::fromString(values.at(idx_exp_date).trimmed(), "yyyy-MM-dd");

        bool strikeOk, theoOk, askOk, bidOk, midOk, moneyOk;
        double strike = values.at(idx_strike).trimmed().toDouble(&strikeOk);
        double theoIv = values.at(idx_theo_iv).trimmed().toDouble(&theoOk);
        double askIv = values.at(idx_ask_iv).trimmed().toDouble(&askOk);
        double bidIv = values.at(idx_bid_iv).trimmed().toDouble(&bidOk);
        // Optional data for tooltips
        QString optionType = (idx_option_type != -1) ? values.at(idx_option_type).trimmed() : QString("N/A");
        double midIv = (idx_mid_iv != -1) ? values.at(idx_mid_iv).trimmed().toDouble(&midOk) : std::numeric_limits<double>::quiet_NaN();
        double logMoneyness = (idx_log_moneyness != -1) ? values.at(idx_log_moneyness).trimmed().toDouble(&moneyOk) : std::numeric_limits<double>::quiet_NaN();

        // Check for essential data validity
        if (symbol.isEmpty() || !expirationDate.isValid() || !strikeOk) {
            Log.msg(FNAME + QString("Skipping CSV line %1: Invalid symbol ('%2'), expiration date ('%3'), or strike ('%4').")
                .arg(lineNum).arg(symbol).arg(values.at(idx_exp_date).trimmed()).arg(values.at(idx_strike).trimmed()),
                Logger::Level::WARNING);
            rowsSkipped++;
            continue;
        }

        // Skip row if essential IVs are not valid numbers
        if (!theoOk || !askOk || !bidOk) {
            Log.msg(FNAME + QString("Skipping CSV line %1 (Symbol %2, Date %3, Strike %4): Invalid Theo/Ask/Bid IV value found.")
                .arg(lineNum).arg(symbol).arg(expirationDate.toString(Qt::ISODate)).arg(strike),
                Logger::Level::WARNING);
            rowsSkipped++;
            continue;
        }

        // --- Store in temporary map ---
        SmileData& smile = outData[symbol][expirationDate];
        smile.isValid = true; // Mark as valid

        smile.strikes.append(strike);
        smile.theoIvs.append(theoIv);
        smile.askIvs.append(askIv);
        smile.bidIvs.append(bidIv);

        // Generate tooltip string
        QString tooltip = QString("Strike: %1\nType: %2\nTheo IV: %3\nAsk IV: %4\nBid IV: %5\nMid IV: %6\nLogMny: %7")
            .arg(strike)
            .arg(optionType)
            .arg(theoIv, 0, 'f', 4)
            .arg(askIv, 0, 'f', 4)
            .arg(bidIv, 0, 'f', 4)
            .arg(midOk ? QString::number(midIv, 'f', 4) : QString("N/A"))
            .arg(moneyOk ? QString::number(logMoneyness, 'f', 4) : QString("N/A"));
        smile.tooltips.append(tooltip);
        rowsParsed++;
    }

    Log.msg(FNAME + QString("CSV Parsing finished. Lines processed: %1, Rows Parsed: %2, Rows Skipped: %3.")
        .arg(lineNum - 1).arg(rowsParsed).arg(rowsSkipped), Logger::Level::INFO);

    return true; // Indicate successful parsing attempt
}

/*
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
*/