#include "Glob/Glob.h"
#include "Glob/Logger.h"
#include "WindowLayout/WindowManager.h"
#include "WindowLayout/ToolPanelWindow.h"
#include "WindowLayout/TakesPageWindow/TakesPageWindow.h"
#include "WindowLayout/QuoteChartWindow.h"
#include "WindowLayout/WatchlistWindow/WatchlistWindow.h"
#include "WindowLayout/LogWindow.h"

#include "Data/SymbolDataManager.h"
#include "Network/WebSocketClient.h"

#include <QApplication>
#include <QStyleFactory>
#include <QScreen>
#include <QMessageBox>

SymbolDataManager* dataManager = nullptr;
WebSocketClient* wsClient = nullptr;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Configure QSettings Defaults ---
    QCoreApplication::setOrganizationName(NAME_COMPANY);
    QCoreApplication::setApplicationName(NAME_PROGRAM_FULL);
    // Set the default format to INI for all QSettings instances
    QSettings::setDefaultFormat(QSettings::IniFormat);

    app.setStyle(QStyleFactory::create("Fusion"));

    WindowManager windowManager;

    LogWindow logWindow(&windowManager);
    Log.init(logWindow.logWidget);
    Log.setLevel(Logger::Level::DEBUG);
    Log.msg(APP_VERSION);

    QSettings pathFinder;
    Log.msg("Settings file location: " + pathFinder.fileName());

    // Create core components (ensure they exist before windows are created/restored)
    dataManager = new SymbolDataManager(&app); 
    wsClient = new WebSocketClient(&app);

    // Main windows
    ToolPanelWindow toolPanel(&windowManager);
    WatchlistWindow watchlist(dataManager, wsClient, &windowManager);

    windowManager.registerWindow(&toolPanel, "ToolPanel");
    windowManager.registerWindow(&watchlist, "Watchlist");
    windowManager.registerWindow(&logWindow, "LogWindow");

    windowManager.applyDefaultWindowStates();

    toolPanel.show(); // ensure that ToolPanel window always first in task bar
    watchlist.show();
    logWindow.show();

    windowManager.restoreWindowStates();

    bool focusConnectResult = QObject::connect(qApp, &QApplication::focusChanged,
        &windowManager, &WindowManager::handleFocusChanged);
    if (!focusConnectResult) {
        qCritical() << "FATAL: Failed to connect focusChanged signal!";
        Log.msg("FATAL: Failed to connect focusChanged signal!", Logger::Level::ERROR);
        // Consider adding a QMessageBox here too if the log might not work yet
        // QMessageBox::critical(nullptr, "Connect Error", "Failed to connect focusChanged signal!");
    }
    else {
        qInfo() << "Successfully connected focusChanged signal.";
        Log.msg("Successfully connected focusChanged signal.", Logger::Level::DEBUG);
    }

    bool quitConnectResult = QObject::connect(&app, &QApplication::aboutToQuit,
        &windowManager, &WindowManager::saveWindowStates);
    if (!quitConnectResult) {
        qCritical() << "FATAL: Failed to connect aboutToQuit signal for saveWindowStates!";
        Log.msg("FATAL: Failed to connect aboutToQuit signal for saveWindowStates!", Logger::Level::ERROR);
        // QMessageBox::critical(nullptr, "Connect Error", "Failed to connect aboutToQuit signal for saveWindowStates!");
    }
    else {
        qInfo() << "Successfully connected aboutToQuit signal for saveWindowStates.";
        Log.msg("Successfully connected aboutToQuit signal for saveWindowStates.", Logger::Level::DEBUG);
    }

    // Connect log closing separately
    //Logger* l = &Logger::getSingleton();
    //bool logQuitConnectResult = QObject::connect(&app, &QApplication::aboutToQuit, l, &Logger::closeLogger);
    //if (!logQuitConnectResult) {
    //    qWarning() << "Failed to connect aboutToQuit signal for Log::closeLogger!";
    //}
    //else {
    //    qInfo() << "Successfully connected aboutToQuit signal for Log::closeLogger.";
    //}

    return app.exec();
}

