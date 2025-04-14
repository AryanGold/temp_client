#pragma once

#include "Glob/Logger.h"

#include <QString>
#include <QUrl>
#include <QHash>
#include <QVariant> 

// --- Configuration Namespace ---
namespace Config {

    // --- Section Names ---
    const QString SECTION_NETWORK = "Network";
    const QString SECTION_LOGGING = "Logging";
    // Add other sections like "UI", "Trading", etc. as needed

    // --- Network Settings ---
    // Define keys and their default values
    // Key = INI Key Name, Value = Default Value (as QString)
    const QHash<QString, QString> NetworkDefaults = {
        {"WebSocketUrl", "ws://127.0.0.1:8765"},
        {"ConnectionTimeout", "5000"} // Example: Timeout in ms
        // Add other network keys and defaults here
    };

    // --- Logging Settings ---
    const QHash<QString, QString> LoggingDefaults = {
        {"Level", "INFO"} // Default level is INFO
        // Add other logging keys like "LogRotation", "MaxSize" etc. later
    };

    // --- Public Functions ---

    /**
     * @brief Initializes application-wide settings defaults (OrgName, AppName, User INI format).
     * Call this ONCE early in main().
     */
    void initializeUserSettingsDefaults();

    /**
     * @brief Reads a specific setting from the application config file (DataAlpha.ini next to exe).
     *
     * @param section The INI section name (e.g., "Network").
     * @param key The INI key name (e.g., "WebSocketUrl").
     * @param defaultValue A QVariant holding the default value to return if the key is not found.
     * @return QVariant The value read from the config file or the defaultValue.
     */
    QVariant getAppSetting(const QString& section, const QString& key, const QVariant& defaultValue = QVariant());

    /**
     * @brief Reads the WebSocket URL from the application config file, with validation and fallback.
     * @return QUrl The configured or default WebSocket URL.
     */
    QUrl getWebSocketUrl();

    Logger::Level getLogLevel();

    // Add other specific getter functions as needed, e.g.:
    // int getConnectionTimeout();

} // namespace Config
