#include "WebSocketClient.h"
#include "Glob/Logger.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include <QAbstractSocket> // SocketError enum

WebSocketClient::WebSocketClient(QObject* parent) : QObject(parent) {
    connect(&m_webSocket, &QWebSocket::connected, this, &WebSocketClient::onConnected);
    connect(&m_webSocket, &QWebSocket::disconnected, this, &WebSocketClient::onDisconnected);
    connect(&m_webSocket, &QWebSocket::textMessageReceived, this, &WebSocketClient::onTextMessageReceived);
    // Handle error signal correctly
    connect(&m_webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred),
        this, &WebSocketClient::onError);

    // Register QVariantMap if passing it through signals/slots
    qRegisterMetaType<QVariantMap>("QVariantMap");
    qRegisterMetaType<QJsonObject>("QJsonObject");
}

WebSocketClient::~WebSocketClient() {
    disconnectFromServer();
}

void WebSocketClient::connectToServer(const QUrl& url) {
    if (m_isConnected || m_webSocket.state() == QAbstractSocket::ConnectingState) {
        Log.msg(FNAME + "WebSocketClient: Already connected or connecting.", Logger::Level::WARNING);
        return;
    }
    m_url = url;

    Log.msg(FNAME + QString("WebSocketClient: Connecting to %1").arg(url.toString()), Logger::Level::INFO);
    m_webSocket.open(m_url);
}

void WebSocketClient::disconnectFromServer() {
    if (m_webSocket.state() != QAbstractSocket::UnconnectedState) {
        m_webSocket.close(QWebSocketProtocol::CloseCodeNormal, "Client disconnecting");
    }
    m_isConnected = false; // Ensure state is updated even if already closed
}


void WebSocketClient::addSymbol(const QString& symbol, const QString& model, const QVariantMap& settings) {
    Log.msg(FNAME + QString("WebSocketClient: Requesting add symbol[%1/%2]").arg(symbol, model), Logger::Level::DEBUG);

    QJsonObject dataObject;
    dataObject["symbol_name"] = symbol;
    dataObject["model_name"] = model;
    dataObject["model_settings"] = QJsonObject::fromVariantMap(settings); // Assuming settings are simple key-value

    QJsonObject requestObject;
    requestObject["type"] = "symbol";
    requestObject["action"] = "add";
    requestObject["data"] = dataObject;

    sendJsonMessage(requestObject);
}

void WebSocketClient::removeSymbol(const QString& symbol, const QString& model) {
    Log.msg(FNAME + QString("WebSocketClient: Requesting remove symbol[%1/%2]").arg(symbol, model), Logger::Level::DEBUG);

    QJsonObject dataObject;
    dataObject["symbol_name"] = symbol;
    dataObject["model_name"] = model;

    QJsonObject requestObject;
    requestObject["type"] = "symbol";
    requestObject["action"] = "remove";
    requestObject["data"] = dataObject;

    sendJsonMessage(requestObject);
}

void WebSocketClient::updateSymbolSettings(const QString& symbol, const QString& model, const QVariantMap& settings) {
    Log.msg(FNAME + QString("WebSocketClient: Requesting update symbol[%1/%2]").arg(symbol, model), Logger::Level::DEBUG);
    QJsonObject dataObject;
    dataObject["symbol_name"] = symbol;
    dataObject["model_name"] = model;
    dataObject["model_settings"] = QJsonObject::fromVariantMap(settings);

    QJsonObject requestObject;
    requestObject["type"] = "symbol";
    requestObject["action"] = "update";
    requestObject["data"] = dataObject;

    sendJsonMessage(requestObject);
}

void WebSocketClient::pauseSymbol(const QString& symbol, const QString& model) {
    Log.msg(FNAME + QString("WebSocketClient: Requesting pause symbol[%1/%2]").arg(symbol, model), Logger::Level::DEBUG);
}

void WebSocketClient::resumeSymbol(const QString& symbol, const QString& model) {
    Log.msg(FNAME + QString("WebSocketClient: Requesting resume symbol[%1/%2]").arg(symbol, model), Logger::Level::DEBUG);
}


// --- Private Slots (Implement later with actual QWebSocket logic) ---

void WebSocketClient::onConnected() {
    Log.msg(FNAME + QString("WebSocketClient: Connected to server."), Logger::Level::DEBUG);
    m_isConnected = true;
    emit connected();
}

void WebSocketClient::onDisconnected() {
    Log.msg(FNAME + QString("WebSocketClient: Disconnected to server."), Logger::Level::DEBUG);
    m_isConnected = false;
    emit disconnected();

    // Todo: attempt to reconnect here or signal the main application
}

void WebSocketClient::onTextMessageReceived(const QString& message) {
    // qInfo() << "WebSocketClient: Message received:" << message; // Can be very verbose
    parseIncomingMessage(message);
}

void WebSocketClient::onError(QAbstractSocket::SocketError error) {
    // Avoid logging "RemoteHostClosedError" as a critical error if it happens during normal disconnect
    if (error == QAbstractSocket::RemoteHostClosedError && !m_isConnected) {
        Log.msg(FNAME + QString("WebSocketClient: Connection closed by remote host (expected during disconnect)."), 
            Logger::Level::INFO);
    }
    else {
        QString errorString = m_webSocket.errorString();
        Log.msg(FNAME + QString("WebSocketClient: Error occurred '%1': %2").arg(error).arg(errorString),
            Logger::Level::ERROR);
        emit errorOccurred(errorString);
    }
}


// --- Private Helpers ---

void WebSocketClient::sendJsonMessage(const QJsonObject& json) {
    if (!m_isConnected) {
        Log.msg(FNAME + QString("WebSocketClient: Cannot send message, not connected."),Logger::Level::WARNING);
        return;
    }
    QJsonDocument doc(json);
    QString messageStr = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    // qInfo() << "WebSocketClient: Sending:" << messageStr; // For debugging
    m_webSocket.sendTextMessage(messageStr);
}

void WebSocketClient::parseIncomingMessage(const QString& message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (doc.isNull() || !doc.isObject()) {
        Log.msg(FNAME + QString("WebSocketClient: Received invalid JSON: %1").arg(message), Logger::Level::WARNING);
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj.value("type").toString();

    if (type == "ticker_data") { // Example: receiving ticker data
        QJsonObject data = obj.value("data").toObject();
        QString symbol = data.value("symbol_name").toString();
        QString model = data.value("model_name").toString();
        // Assuming the rest of 'data' contains the fields for the table
        QVariantMap tickerFields = data.toVariantMap();
        // Remove keys we already extracted if they are not needed in the map itself
        tickerFields.remove("symbol_name");
        tickerFields.remove("model_name");

        if (!symbol.isEmpty() && !model.isEmpty()) {
            emit tickerDataReceived(symbol, model, obj);
        }
        else {
            Log.msg(FNAME + QString("WebSocketClient: Received ticker data with missing symbol/model: %1").arg(message), 
                Logger::Level::WARNING);
        }
    }
    else if (type == "symbol_response") { 
        QString action = obj.value("action").toString();
        bool success = obj.value("success").toBool(false); 
        QJsonObject data = obj.value("data").toObject();
        QString symbol = data.value("symbol_name").toString();
        QString model = data.value("model_name").toString();
        QString errorMsg = obj.value("error").toString();

        if (action == "add") {
            if (success) emit symbolAddConfirmed(symbol, model);
            else emit symbolAddFailed(symbol, model, errorMsg.isEmpty() ? "Unknown add error" : errorMsg);
        }
        else if (action == "remove") {
            if (success) emit symbolRemoveConfirmed(symbol, model);
            // else emit symbolRemoveFailed(symbol, model, errorMsg); // Add if needed
        }
        else if (action == "update") {
            if (success) emit symbolUpdateConfirmed(symbol, model);
            // else emit symbolUpdateFailed(symbol, model, errorMsg); // Add if needed
        }
    }
    else {
        Log.msg(FNAME + QString("WebSocketClient: Received unhandled message type: %1").arg(type),
            Logger::Level::WARNING);
    }
}