#include "Glob/Glob.h"
#include "Glob/Logger.h"
#include "Glob/Config.h"
#include "WindowLayout/WindowManager.h"
#include "WindowLayout/ToolPanelWindow.h"
#include "WindowLayout/TakesPageWindow/TakesPageWindow.h"
#include "WindowLayout/QuoteChartWindow.h"
#include "WindowLayout/WatchlistWindow/WatchlistWindow.h"
#include "WindowLayout/LogWindow.h"

#include "Data/ClientReceiver.h"
#include "Data/SymbolDataManager.h"
#include "Network/WebSocketClient.h"

#include <QApplication>
#include <QStyleFactory>
#include <QScreen>
#include <QSessionManager>
#include <QGuiApplication>
#include <QObject>
#include <QMessageBox>
#include <QStandardPaths>

QByteArray loadCsvDataFromFile(const QString& filename);
QJsonObject generateTestDataFromCsv(const QByteArray& csvDataBytes);

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    Config::initializeUserSettingsDefaults();

    app.setStyle(QStyleFactory::create("Fusion"));

    WindowManager windowManager;

    LogWindow logWindow(&windowManager);
    Log.init(logWindow.logWidget);
    Log.msg(APP_VERSION);
    Logger::Level logLevel = Config::getLogLevel(); // Get level from config
    Log.msg("Using Log Level: " + Logger::levelToString(logLevel), Logger::Level::INFO);
    Log.setLevel(logLevel);


    QSettings pathFinder;
    Log.msg("Window settings file location: " + pathFinder.fileName());

    ///////////
    // Data pipeline
    // Create core components (ensure they exist before windows are created/restored)
    Glob.dataManager = new SymbolDataManager(&app);
    Glob.dataReceiver = new ClientReceiver();
    Glob.wsClient = new WebSocketClient(&app);

    Log.msg("Initiating WebSocket connection process...", Logger::Level::INFO);
    QUrl webSocketUrl = Config::getWebSocketUrl();
    Glob.wsClient->connectToServer(webSocketUrl); // Start connecting (will retry automatically)

    QObject::connect(Glob.wsClient, &WebSocketClient::tickerDataReceived,
                     Glob.dataReceiver, &ClientReceiver::processWebSocketMessage);

    //Connect WS signals to UI for status updates-- -
    QObject::connect(Glob.wsClient, &WebSocketClient::connected, [&]() {
            Log.msg("Main: WebSocket Connected!", Logger::Level::INFO);
            // update a status bar, enable UI elements
            // toolPanel.updateWsStatus(true);
        }
    );
    QObject::connect(Glob.wsClient, &WebSocketClient::disconnected, [&]() {
            Log.msg("Main: WebSocket Disconnected! Reconnecting...", Logger::Level::WARNING);
            // update status bar, disable UI elements
            // toolPanel.updateWsStatus(false);
        }
    );
    ///////////

    // Main windows
    ToolPanelWindow toolPanel(&windowManager);
    WatchlistWindow watchlist(Glob.dataManager, Glob.wsClient, &windowManager);

    windowManager.registerWindow(&toolPanel, "ToolPanel");
    windowManager.registerWindow(&watchlist, "Watchlist");
    windowManager.registerWindow(&logWindow, "LogWindow");

    toolPanel.show(); // ensure that ToolPanel window always first in task bar
    watchlist.show();
    logWindow.show();

    windowManager.restoreWindowStates();

    QObject::connect(qApp, &QApplication::focusChanged,
        &windowManager, &WindowManager::handleFocusChanged);

    /////////////////////////
    // --- Simulate receiving data (Optional - Keep for testing) ---
    QString appPath = QCoreApplication::applicationDirPath();
    QString csvFilePath = appPath + "/c2_update_volsmile_canvas__smile_to_be_plotted.csv";
    Log.msg(QString("[main] Attempting to load test data from: %1").arg(csvFilePath), Logger::Level::DEBUG);
    QByteArray csvBytes = loadCsvDataFromFile(csvFilePath);
    if (!csvBytes.isEmpty()) {
        QJsonObject testMessage = generateTestDataFromCsv(csvBytes);
        QString type = testMessage.value("type").toString();
        Log.msg(QString("B1: %1").arg(type), Logger::Level::DEBUG);
        if (!testMessage.isEmpty()) {
            QTimer::singleShot(150, Qt::CoarseTimer, [&, testMessage]() {
                Log.msg("[main] Sending simulated WebSocket message to receiver...", Logger::Level::INFO);
                QString type = testMessage.value("type").toString();
                Log.msg(QString("B2: %1").arg(type), Logger::Level::DEBUG);
                Glob.dataReceiver->processWebSocketMessage("AAPL", "SSVI", testMessage);
                });
        }
        else { /* Log Warning */ }
    }
    else { /* Log Warning */ }
    /////////////////////////

    return app.exec();
}

//////////////////////////////////////////
// For Local test

// --- ZLIB/Compress Helper ---
#include <zlib.h>
#include <vector>
#include <QFile>
#include <QDebug>
#define ZLIB_CHUNK_SIZE 16384

// --- ZLIB/Compress Helper Implementation ---
QByteArray compressZlib(const QByteArray& inputData, int level) {
    if (inputData.isEmpty()) return QByteArray();
    if (level < -1 || level > 9) level = Z_DEFAULT_COMPRESSION;
    z_stream strm; strm.zalloc = Z_NULL; strm.zfree = Z_NULL; strm.opaque = Z_NULL;
    int ret = deflateInit(&strm, level);
    if (ret != Z_OK) { Log.msg("[compressZlib] deflateInit failed.", Logger::Level::ERROR); return QByteArray(); }
    QByteArray compressedResult; compressedResult.reserve(inputData.size() / 2);
    std::vector<Bytef> outBuffer(ZLIB_CHUNK_SIZE);
    strm.avail_in = inputData.size();
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(inputData.constData()));
    do {
        strm.avail_out = ZLIB_CHUNK_SIZE; strm.next_out = outBuffer.data();
        ret = deflate(&strm, (strm.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH);
        if (ret == Z_STREAM_ERROR) { Log.msg("[compressZlib] deflate failed.", Logger::Level::ERROR); deflateEnd(&strm); return QByteArray(); }
        unsigned int have = ZLIB_CHUNK_SIZE - strm.avail_out;
        if (have > 0) compressedResult.append(reinterpret_cast<const char*>(outBuffer.data()), have);
    } while (strm.avail_out == 0);
    if (ret != Z_STREAM_END) { Log.msg("[compressZlib] deflate did not end stream properly.", Logger::Level::WARNING); }
    deflateEnd(&strm);
    return compressedResult;
}

// --- Test Data Generation Implementation ---
QByteArray loadCsvDataFromFile(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        Log.msg(QString("[main] Failed to open test CSV file: %1").arg(filename), Logger::Level::ERROR);
        return QByteArray();
    }
    Log.msg(QString("[main] Loading test data from: %1").arg(filename), Logger::Level::INFO);
    QByteArray data = file.readAll();
    file.close();
    return data;
}

QJsonObject generateTestDataFromCsv(const QByteArray& csvDataBytes) {
    if (csvDataBytes.isEmpty()) { return QJsonObject(); }
    QByteArray compressedBytes = compressZlib(csvDataBytes, 6);
    if (compressedBytes.isEmpty() && !csvDataBytes.isEmpty()) { Log.msg(QString("[main] Test data compression failed!"), Logger::Level::ERROR); return QJsonObject(); }
    QString compressedDataB64 = QString::fromLatin1(compressedBytes.toBase64());
    QJsonObject message;
    message["type"] = "data_stream";
    message["symbol"] = "AAPL"; // Assuming from CSV
    message["model"] = "SSVI"; // Assuming from CSV
    message["metrics"] = QJsonObject{ {"load_time", QDateTime::currentMSecsSinceEpoch()} };
    message["data_compressed"] = compressedDataB64;
    return message;
}

//////////////////////////////////////////

