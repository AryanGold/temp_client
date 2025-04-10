#include "WebSocketClient.h"
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
}

WebSocketClient::~WebSocketClient() {
    disconnectFromServer();
}

void WebSocketClient::connectToServer(const QUrl& url) {
    if (m_isConnected || m_webSocket.state() == QAbstractSocket::ConnectingState) {
        qWarning() << "WebSocketClient: Already connected or connecting.";
        return;
    }
    m_url = url;
    qInfo() << "WebSocketClient: Connecting to" << url.toString();
    m_webSocket.open(m_url);
}

void WebSocketClient::disconnectFromServer() {
    if (m_webSocket.state() != QAbstractSocket::UnconnectedState) {
        m_webSocket.close(QWebSocketProtocol::CloseCodeNormal, "Client disconnecting");
    }
    m_isConnected = false; // Ensure state is updated even if already closed
}


void WebSocketClient::addSymbol(const QString& symbol, const QString& model, const QVariantMap& settings) {
    qInfo() << "WebSocketClient: Requesting add symbol:" << symbol << model;

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
    qInfo() << "WebSocketClient: Requesting remove symbol:" << symbol << model;

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
    qInfo() << "WebSocketClient: Requesting update settings for:" << symbol << model;
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
    qInfo() << "WebSocketClient: Requesting pause symbol:" << symbol << model;

}

void WebSocketClient::resumeSymbol(const QString& symbol, const QString& model) {
    qInfo() << "WebSocketClient: Requesting resume symbol:" << symbol << model;


}


// --- Private Slots (Implement later with actual QWebSocket logic) ---

void WebSocketClient::onConnected() {
    qInfo() << "WebSocketClient: Connected to server.";
    m_isConnected = true;
    emit connected();
}

void WebSocketClient::onDisconnected() {
    qInfo() << "WebSocketClient: Disconnected from server.";
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
        qInfo() << "WebSocketClient: Connection closed by remote host (expected during disconnect).";
    }
    else {
        QString errorString = m_webSocket.errorString();
        qWarning() << "WebSocketClient: Error occurred:" << error << "-" << errorString;
        emit errorOccurred(errorString);
    }
}


// --- Private Helpers ---

void WebSocketClient::sendJsonMessage(const QJsonObject& json) {
    if (!m_isConnected) {
        qWarning() << "WebSocketClient: Cannot send message, not connected.";
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
        qWarning() << "WebSocketClient: Received invalid JSON:" << message;
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
            emit tickerDataReceived(symbol, model, tickerFields);
        }
        else {
            qWarning() << "WebSocketClient: Received ticker data with missing symbol/model:" << message;
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
        qWarning() << "WebSocketClient: Received unhandled message type:" << type;
    }
}