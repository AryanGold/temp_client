#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QWebSocket>
#include <QUrl>
#include <QTimer>

class WebSocketClient : public QObject {
    Q_OBJECT

public:
    explicit WebSocketClient(QObject* parent = nullptr);
    ~WebSocketClient() override;

    // Methods to initiate actions (will send JSON requests)
    void addSymbol(const QString& symbol, const QString& model, const QVariantMap& settings = {});
    void removeSymbol(const QString& symbol, const QString& model);
    void updateSymbolSettings(const QString& symbol, const QString& model, const QVariantMap& settings);
    void pauseSymbol(const QString& symbol, const QString& model); // May need specific API call or just stop processing locally
    void resumeSymbol(const QString& symbol, const QString& model);// May need specific API call or just start processing locally

    // --- Connection Control ---
    // Sets the target URL and starts connection attempts immediately
    void connectToServer(const QUrl& url);
    // Stops connection attempts and closes the socket if connected
    void disconnectFromServer();
    // Call this if you want to manually trigger connection attempts after setting URL separately
    void startConnectionAttempts();

    bool isConnected() const; // Public method to check status

signals:
    // Signals data *received* from the WebSocket server
    void tickerDataReceived(const QString& symbol, const QString& model, const QJsonObject& data);
    void symbolAddConfirmed(const QString& symbol, const QString& model); // Example confirmation
    void symbolRemoveConfirmed(const QString& symbol, const QString& model); // Example confirmation
    void symbolUpdateConfirmed(const QString& symbol, const QString& model); // Example confirmation
    void symbolAddFailed(const QString& symbol, const QString& model, const QString& error); // Example error
    // Add more signals for other confirmations/errors as needed

    // Connection status signals
    void connected();
    void disconnected();
    void errorOccurred(const QString& errorString); // General WebSocket errors


private slots:
    // Slots for internal QWebSocket signals (to be implemented later)
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onError(QAbstractSocket::SocketError error);

    void attemptConnection();

private:
    QWebSocket m_webSocket;
    QUrl m_url;
    bool m_isConnected = false;
    bool m_explicitDisconnect = false; // Flag to prevent reconnect after explicit disconnect call
    QTimer m_reconnectTimer; // Timer for connection retries

    // Helper to send JSON messages
    void sendJsonMessage(const QJsonObject& json);
    // Helper to parse incoming messages
    void parseIncomingMessage(const QString& message);
    // Helper to schedule next connection attempt
    void scheduleReconnect();

    // Define reconnect interval
    static const int RECONNECT_INTERVAL_MS = 3000; // 3 seconds
};