#include "Config.h"
#include "Glob/Logger.h" // Include Logger
#include "../Defines.h"

#include <QCoreApplication>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QVariant>
#include <QUrl>
#include <QMessageBox> // Optional for error display

namespace Config {

    // Helper function to get the application config file path
    QString getAppConfigPath() {
        return QCoreApplication::applicationDirPath() + "/DataAlpha.ini";
    }

    void initializeUserSettingsDefaults() {
        // Set defaults for USER state settings (AppData)
        QCoreApplication::setOrganizationName(NAME_COMPANY);
        QCoreApplication::setApplicationName(NAME_PROGRAM_FULL);
        // Set the default format to INI for all QSettings instances
        QSettings::setDefaultFormat(QSettings::IniFormat);
    }

    QVariant getAppSetting(const QString& section, const QString& key, const QVariant& defaultValue) {
        QString configPath = getAppConfigPath();
        QSettings appSettings(configPath, QSettings::IniFormat);

        QFileInfo configFileInfo(configPath);
        if (!configFileInfo.exists() || !configFileInfo.isReadable()) {
            // Log only once per missing file check maybe? Or use a flag.
            // For simplicity, log each time for now.
            QString msg = "App config file not found or not readable: " + configPath + 
                ". Returning default for " + section + "/" + key;
            QMessageBox::warning(nullptr, "Error", msg);
            return defaultValue;
        }

        QString settingsKey = section + "/" + key;
        return appSettings.value(settingsKey, defaultValue);
    }

    QUrl getWebSocketUrl() {
        QString key = "WebSocketUrl"; // Key name from the QHash
        QString defaultValue = NetworkDefaults.value(key, "ws://127.0.0.1:8765"); // Fallback default if key missing in QHash itself

        QVariant valueFromSettings = getAppSetting(SECTION_NETWORK, key, defaultValue);
        QString urlString = valueFromSettings.toString();

        QUrl url(urlString, QUrl::StrictMode);

        if (!url.isValid() || url.isEmpty()) {
            Log.msg(FNAME + "Invalid WebSocket URL read ('" + urlString + "'). Falling back to default: " + defaultValue, 
                Logger::Level::WARNING);
            // Optional: Show message box
            // QMessageBox::warning(nullptr, "Configuration Error", ...);
            url = QUrl(defaultValue, QUrl::StrictMode); // Use default

            // Optional: Write the default back if the setting was invalid/missing
            // QSettings appSettings(getAppConfigPath(), QSettings::IniFormat);
            // appSettings.setValue(SECTION_NETWORK + "/" + key, defaultValue);
            // appSettings.sync();
        }
        return url;
    }

    // Implement other specific getters here, e.g.:
    // int getConnectionTimeout() {
    //     QString key = "ConnectionTimeout";
    //     int defaultValue = NetworkDefaults.value(key, "5000").toInt(); // Get default from QHash
    //     QVariant valueFromSettings = getAppSetting(SECTION_NETWORK, key, defaultValue);
    //     bool ok;
    //     int timeout = valueFromSettings.toInt(&ok);
    //     if (!ok) {
    //         Log.msg(FNAME + "Invalid ConnectionTimeout value: " + valueFromSettings.toString() + ". Using default: " + QString::number(defaultValue), Logger::Level::WARNING);
    //         return defaultValue;
    //     }
    //     return timeout;
    // }

    Logger::Level getLogLevel() {
        QString key = "Level"; // Key name from QHash
        QString defaultLevelStr = LoggingDefaults.value(key, "INFO"); // Default from QHash

        QVariant valueFromSettings = getAppSetting(SECTION_LOGGING, key, defaultLevelStr);
        QString levelStr = valueFromSettings.toString();

        Logger::Level level = Logger::levelFromString(levelStr, Logger::Level::INFO); // Convert string to enum
        return level;
    }

} // namespace Config