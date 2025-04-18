
#include "ClientReceiver.h"
#include "Glob/Logger.h"
#include "libs/Compressor.h"

#include <QJsonValue>
#include <QByteArray>
#include <QDateTime>
#include <QStringList>
#include <QDebug>
#include <QMutexLocker>
#include <QTextStream>
#include <limits>
#include <vector> // For zlib buffer

ClientReceiver::ClientReceiver(QObject* parent) : QObject(parent) {
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

// Helper function to safely get a field by index, returning empty string if out of bounds
QString ClientReceiver::getFieldSafe(const QStringList& fields, int index) {
    return (index >= 0 && index < fields.size()) ? fields[index].trimmed() : QString();
}

// Helper function to parse CSV data into PlotDataForDate struct using column names
bool ClientReceiver::parseSmileCSV(const QString& csvData, QDate& outDate, PlotDataForDate& outPlotData) {
    outPlotData.theoPoints.clear();
    outPlotData.midPoints.clear();
    outPlotData.bidPoints.clear();
    outPlotData.askPoints.clear();
    outPlotData.pointDetails.clear();
    outDate = QDate(); // Reset date

    QStringList lines = csvData.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) { // Need header + data
        Log.msg(FNAME + "CSV data has too few lines (< 2).", Logger::Level::WARNING);
        return false;
    }

    // --- Parse Header Row ---
    QStringList headerFields = lines[0].split(',');
    QMap<QString, int> headerMap; // Map column name to index
    for (int i = 0; i < headerFields.size(); ++i) {
        // Trim whitespace and store (convert to lower for case-insensitivity if desired)
        headerMap[headerFields[i].trimmed()] = i;
        // Example case-insensitive: headerMap[headerFields[i].trimmed().toLower()] = i;
    }
    Log.msg(FNAME + "Parsed header with " + QString::number(headerMap.size()) + " columns.", Logger::Level::DEBUG);

    // --- Define Required Columns and Get Indices ---
    // Store required column names
    const QList<QString> requiredColumns = {
        "snap_shot_dates", "log_moneyness", "theo_ivs", "mid_iv", "bid_iv",
        "ask_iv", "strikes", "symbol", "bid_prices", "ask_prices"
        // Add any other columns you absolutely need
    };

    // Create a map to store the resolved indices for required columns
    QMap<QString, int> colIndices;
    for (const QString& colName : requiredColumns) {
        if (!headerMap.contains(colName)) {
            Log.msg(FNAME + "CSV header is missing required column: '" + colName + "'.", Logger::Level::ERROR);
            return false; // Cannot proceed without required columns
        }
        colIndices[colName] = headerMap.value(colName);
    }
    // --- End Header Parsing and Index Mapping ---


    bool dateParsed = false;
    int lineNum = 0;

    for (const QString& line : lines) {
        lineNum++;
        if (lineNum == 1) continue; // Skip header row

        QString cleanLine = line.trimmed();
        if (cleanLine.isEmpty()) continue;

        QStringList fields = cleanLine.split(',');
        // No strict check on field count needed now, just access by mapped index

        bool ok; // For checking numeric conversions

        // --- Parse Date (using mapped index) ---
        if (!dateParsed) {
            QString dateStr = getFieldSafe(fields, colIndices["snap_shot_dates"]);
            outDate = QDate::fromString(dateStr, Qt::ISODate);
            if (!outDate.isValid()) {
                Log.msg(FNAME + "Failed to parse snapshot date from first data row: " + dateStr, Logger::Level::ERROR);
                return false;
            }
            dateParsed = true;
            Log.msg(FNAME + "Parsed snapshot date: " + outDate.toString(Qt::ISODate), Logger::Level::DEBUG);
        }

        // --- Parse Numeric Values (using mapped indices) ---
        double logMny = getFieldSafe(fields, colIndices["log_moneyness"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid log_moneyness value.", Logger::Level::WARNING); continue; }

        double theoIv = getFieldSafe(fields, colIndices["theo_ivs"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid theo_ivs value.", Logger::Level::WARNING); continue; }

        double midIv = getFieldSafe(fields, colIndices["mid_iv"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid mid_iv value.", Logger::Level::WARNING); continue; }

        double bidIv = getFieldSafe(fields, colIndices["bid_iv"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid bid_iv value.", Logger::Level::WARNING); continue; }

        double askIv = getFieldSafe(fields, colIndices["ask_iv"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid ask_iv value.", Logger::Level::WARNING); continue; }

        double strike = getFieldSafe(fields, colIndices["strikes"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid strikes value.", Logger::Level::WARNING); continue; }

        double bidPrice = getFieldSafe(fields, colIndices["bid_prices"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid bid_prices value.", Logger::Level::WARNING); continue; }

        double askPrice = getFieldSafe(fields, colIndices["ask_prices"]).toDouble(&ok);
        if (!ok) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Invalid ask_prices value.", Logger::Level::WARNING); continue; }

        // Get option symbol string
        QString optionSymbol = getFieldSafe(fields, colIndices["symbol"]);
        if (optionSymbol.isEmpty()) { Log.msg(FNAME + "Skipping line " + QString::number(lineNum) + 
            ": Missing option symbol.", Logger::Level::WARNING); continue; }
        // --- End Parse Numeric Values ---


        // Add points to vectors
        outPlotData.theoPoints.append(QPointF(logMny, theoIv));
        outPlotData.midPoints.append(QPointF(logMny, midIv));
        outPlotData.bidPoints.append(QPointF(logMny, bidIv));
        outPlotData.askPoints.append(QPointF(logMny, askIv));

        // Create and add details struct
        SmilePointData details;
        details.symbol = optionSymbol;
        details.strike = strike;
        details.mid_iv = midIv;
        details.theo_iv = theoIv;
        details.bid_iv = bidIv;
        details.ask_iv = askIv;
        details.bid_price = bidPrice;
        details.ask_price = askPrice;
        // Add other details if parsed using their column names/indices
        outPlotData.pointDetails.append(details);

    } // End for loop

    if (outPlotData.theoPoints.isEmpty() && outPlotData.midPoints.isEmpty()) {
        Log.msg(FNAME + "No valid data points parsed from CSV.", Logger::Level::WARNING);
        return false;
    }
    if (!dateParsed) {
        Log.msg(FNAME + "No valid date found in CSV data.", Logger::Level::ERROR);
        return false;
    }

    return true; // Success
}


void ClientReceiver::processWebSocketMessage(const QString& symbol, const QString& model, const QJsonObject& data) {
    Log.msg(FNAME + QString("Processing WebSocket message..."), Logger::Level::DEBUG);

    if (data.isEmpty()) {
        Log.msg(FNAME + QString("Data is empty"), Logger::Level::WARNING);
        return;
    }

    QStringList keys = data.keys();

    if (!keys.contains("type")) {
        Log.msg(FNAME + QString("Received message not have 'type' row."), Logger::Level::WARNING);
        return;
    }

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
            decompressedBytes = Compressor::decompressZlib(compressedBytes);
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

    // --- 2. Convert Decompressed Bytes to QString (UTF-8 CSV) ---
    QString csvData = QString::fromUtf8(decompressedBytes);
    if (csvData.isEmpty() && !decompressedBytes.isEmpty()) {
        Log.msg(FNAME + "Failed to convert decompressed bytes to UTF-8 string.", Logger::Level::ERROR);
        return;
    }
    // --- End Conversion ---

    // --- 3. Parse the CSV Data ---
    QDate snapshotDate;
    PlotDataForDate plotData;

    // Parse the CSV data from the decompressed string
    if (parseSmileCSV(csvData, snapshotDate, plotData)) {
        // If parsing succeeded, emit the new signal
        Log.msg(FNAME + "CSV parsed successfully for date: " + snapshotDate.toString(Qt::ISODate)
            + ". Emitting plotDataUpdated.", Logger::Level::DEBUG);
        emit plotDataUpdated(symbol, snapshotDate, plotData);
    }
    else {
        // Parsing failed, error logged within parseSmileCSV
        Log.msg(FNAME + "Failed to parse CSV data after decompression for " + symbol, Logger::Level::ERROR);
    }
    // --- End Parsing ---

    /*
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
    */
}

/*
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
*/