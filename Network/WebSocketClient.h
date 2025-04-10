#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonDocument>
#include <QWebSocket>
#include <QUrl>

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

    void connectToServer(const QUrl& url);
    void disconnectFromServer();

signals:
    // Signals data *received* from the WebSocket server
    void tickerDataReceived(const QString& symbol, const QString& model, const QVariantMap& data);
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
    void onError(QAbstractSocket::SocketError error); // Use QAbstractSocket::SocketError

private:
    QWebSocket m_webSocket;
    QUrl m_url;
    bool m_isConnected = false;

    // Helper to send JSON messages
    void sendJsonMessage(const QJsonObject& json);
    // Helper to parse incoming messages
    void parseIncomingMessage(const QString& message);
};