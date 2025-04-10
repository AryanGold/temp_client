#include "WatchlistWindow.h"
#include "ui_WatchlistWindow.h"
#include "Data/SymbolDataManager.h"
#include "Network/WebSocketClient.h"
#include "AddSymbolDialog.h"
#include "SettingsDialog.h"
#include "SymbolItemWidget.h"

#include <QPushButton>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QWidget>
#include <QMessageBox>
#include <QDebug>
#include <QSpacerItem>


WatchlistWindow::WatchlistWindow(SymbolDataManager* dataManager, WebSocketClient* wsClient, WindowManager* windowManager, QWidget* parent)
    : BaseWindow("Watchlist", windowManager, parent),
    ui(new Ui::WatchlistWindow),
    m_dataManager(dataManager),
    m_wsClient(wsClient)
{
    ui->setupUi(this);
    setWindowTitle("Watchlist");

    m_scrollArea = ui->scrollArea;
    m_listContentWidget = ui->scrollAreaWidgetContents; // The QWidget inside QScrollArea
    m_listLayout = qobject_cast<QVBoxLayout*>(m_listContentWidget->layout()); // Get the layout

    // Ensure the layout exists and add a spacer to push items to the top
    if (!m_listLayout) {
        m_listLayout = new QVBoxLayout(m_listContentWidget);
        m_listLayout->setContentsMargins(0, 0, 0, 0);
        m_listLayout->setSpacing(1); // Small spacing between items
        m_listContentWidget->setLayout(m_listLayout);
    }
    m_listLayout->addStretch(1); // Add spacer at the end

    // Example model list - replace with dynamic list if needed
    m_availableModels << "SSVI";

    // Connect UI signals
    connect(ui->addSymbolButton, &QPushButton::clicked, this, &WatchlistWindow::onAddSymbolClicked);

    // Connect signals from Data Manager
    connect(m_dataManager, &SymbolDataManager::symbolAdded, this, &WatchlistWindow::handleSymbolAdded);
    connect(m_dataManager, &SymbolDataManager::symbolRemoved, this, &WatchlistWindow::handleSymbolRemoved);
    connect(m_dataManager, &SymbolDataManager::symbolStateChanged, this, &WatchlistWindow::handleSymbolStateChanged);

    // Connect signals from WebSocket Client (for feedback)
    connect(m_wsClient, &WebSocketClient::symbolAddConfirmed, this, &WatchlistWindow::handleSymbolAddConfirmed);
    connect(m_wsClient, &WebSocketClient::symbolAddFailed, this, &WatchlistWindow::handleSymbolAddFailed);
    connect(m_wsClient, &WebSocketClient::symbolRemoveConfirmed, this, &WatchlistWindow::handleSymbolRemoveConfirmed);
    // Connect others as needed (update, pause/resume confirmations etc.)


    // Load any symbols already present in the manager at startup
    loadExistingSymbols();
}

WatchlistWindow::~WatchlistWindow() {
    delete ui;
}

QString WatchlistWindow::generateKey(const QString& symbol, const QString& model) const {
    return symbol + "_" + model;
}

void WatchlistWindow::loadExistingSymbols() {
    QList<SymbolData> symbols = m_dataManager->getAllSymbols();
    for (const auto& data : symbols) {
        addSymbolToListWidget(data.symbolName, data.modelName);
        // Also update initial state if not default Active
        if (data.state != SymbolDataManager::SymbolState::Active) {
            handleSymbolStateChanged(data.symbolName, data.modelName, data.state);
        }
    }
}


void WatchlistWindow::onAddSymbolClicked() {
    AddSymbolDialog dialog(m_availableModels, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString symbol = dialog.getSelectedSymbol();
        QString model = dialog.getSelectedModel();

        if (symbol.isEmpty() || model.isEmpty()) {
            QMessageBox::warning(this, "Input Error", "Symbol and Model cannot be empty.");
            return;
        }

        QString key = generateKey(symbol, model);
        if (m_symbolWidgets.contains(key)) {
            QMessageBox::information(this, "Already Exists", QString("The symbol/model pair '%1 / %2' is already in the watchlist.").arg(symbol, model));
            return;
        }

        addSymbolToListWidget(symbol, model); // Do this in handleSymbolAdded instead

        qInfo() << "Requesting ADD from WS Client:" << symbol << model;
        m_wsClient->addSymbol(symbol, model);

        // 3. Update the central data manager (maybe after confirmation?)
        // For now, let's assume the WS request *will* eventually lead to a confirmation
        // signal which then triggers the data manager update.
        // If using optimistic UI, you might add to DataManager here and handle rollback on failure.

        // If backend confirmation is required before adding to DataManager:
        // The flow would be:
        // User Add -> WSClient.addSymbol -> Backend -> WSClient receives confirmation -> WSClient emits symbolAddConfirmed
        // -> handleSymbolAddConfirmed slot -> DataManager.addSymbol -> DataManager emits symbolAdded -> handleSymbolAdded slot -> UI updated
        // Temporary:
        handleSymbolAddConfirmed(symbol, model);
    }
}

// --- Slots for SymbolItemWidget Signals ---

void WatchlistWindow::handleRemoveRequested(const QString& symbol, const QString& model) {
    qInfo() << "Requesting REMOVE from WS Client:" << symbol << model;
    // 1. Send request to backend
    m_wsClient->removeSymbol(symbol, model);

    // 2. Update Data Manager (or wait for confirmation)
    // Assuming confirmation flow: WSClient emits symbolRemoveConfirmed -> handleSymbolRemoveConfirmed -> DataManager.removeSymbol -> DataManager emits symbolRemoved -> handleSymbolRemoved -> UI update
}

void WatchlistWindow::handleSettingsRequested(const QString& symbol, const QString& model) {
    qInfo() << "Settings requested for:" << symbol << model;
    QVariantMap currentSettings = m_dataManager->getSymbolSettings(symbol, model);
    SettingsDialog dialog(symbol, model, currentSettings, this);
    if (dialog.exec() == QDialog::Accepted) {
        QVariantMap newSettings = dialog.getNewSettings();
        if (newSettings != currentSettings) {
            qInfo() << "Requesting UPDATE SETTINGS from WS Client:" << symbol << model;
            // 1. Send request to backend
            m_wsClient->updateSymbolSettings(symbol, model, newSettings);
            // 2. Update Data Manager (or wait for confirmation)
            // m_dataManager->updateSymbolSettings(symbol, model, newSettings); // If doing optimistically or via confirmation signal
        }
    }
}

void WatchlistWindow::handlePauseRequested(const QString& symbol, const QString& model) {
    qInfo() << "Requesting PAUSE from WS Client:" << symbol << model;
    // 1. Send request to backend (if required by API)
    m_wsClient->pauseSymbol(symbol, model);
    // 2. Update Data Manager immediately (pausing is often client-side state)
    m_dataManager->setSymbolState(symbol, model, SymbolDataManager::SymbolState::Paused);
    // State change will emit symbolStateChanged -> handleSymbolStateChanged -> updates UI
}

void WatchlistWindow::handleResumeRequested(const QString& symbol, const QString& model) {
    qInfo() << "Requesting RESUME from WS Client:" << symbol << model;
    // 1. Send request to backend (if required by API)
    m_wsClient->resumeSymbol(symbol, model);
    // 2. Update Data Manager immediately
    m_dataManager->setSymbolState(symbol, model, SymbolDataManager::SymbolState::Active);
    // State change will emit symbolStateChanged -> handleSymbolStateChanged -> updates UI
}


// --- Slots for SymbolDataManager Signals ---

void WatchlistWindow::handleSymbolAdded(const QString& symbol, const QString& model) {
    qInfo() << "WatchlistWindow: Handling symbol added:" << symbol << model;
    addSymbolToListWidget(symbol, model);
}

void WatchlistWindow::handleSymbolRemoved(const QString& symbol, const QString& model) {
    qInfo() << "WatchlistWindow: Handling symbol removed:" << symbol << model;
    removeSymbolFromListWidget(symbol, model);
}

void WatchlistWindow::handleSymbolStateChanged(const QString& symbol, const QString& model, SymbolDataManager::SymbolState newState) {
    qInfo() << "WatchlistWindow: Handling state change:" << symbol << model << (newState == SymbolDataManager::SymbolState::Active ? "Active" : "Paused");
    QString key = generateKey(symbol, model);
    if (m_symbolWidgets.contains(key)) {
        m_symbolWidgets[key]->updateState(newState);
    }
    else {
        qWarning() << "WatchlistWindow: Received state change for unknown widget:" << key;
    }
}

// --- Slots for WebSocketClient Confirmation/Error Signals ---

void WatchlistWindow::handleSymbolAddConfirmed(const QString& symbol, const QString& model) {
    qInfo() << "WatchlistWindow: Add confirmed by server for:" << symbol << model;
    // Now that server confirmed, update the Data Manager
    m_dataManager->addSymbol(symbol, model);
    // The DataManager::symbolAdded signal will then trigger handleSymbolAdded to update the UI.
}

void WatchlistWindow::handleSymbolAddFailed(const QString& symbol, const QString& model, const QString& error) {
    qWarning() << "WatchlistWindow: Add failed for" << symbol << model << ":" << error;
    QMessageBox::critical(this, "Add Symbol Failed", QString("Could not add symbol %1 / %2.\nReason: %3").arg(symbol, model, error));
    // If we were doing optimistic UI update, we would need to remove the widget here.
}

void WatchlistWindow::handleSymbolRemoveConfirmed(const QString& symbol, const QString& model) {
    qInfo() << "WatchlistWindow: Remove confirmed by server for:" << symbol << model;
    // Now that server confirmed, update the Data Manager
    m_dataManager->removeSymbol(symbol, model);
    // The DataManager::symbolRemoved signal will then trigger handleSymbolRemoved to update the UI.
}


// --- Private Helper Methods ---

void WatchlistWindow::addSymbolToListWidget(const QString& symbol, const QString& model) {
    QString key = generateKey(symbol, model);
    if (m_symbolWidgets.contains(key)) {
        qWarning() << "WatchlistWindow: Widget already exists for" << key;
        return; // Already exists
    }

    SymbolItemWidget* itemWidget = new SymbolItemWidget(symbol, model, m_listContentWidget); // Parent is important for layout

    // Connect signals from the item widget to the window's slots
    connect(itemWidget, &SymbolItemWidget::removeRequested, this, &WatchlistWindow::handleRemoveRequested);
    connect(itemWidget, &SymbolItemWidget::settingsRequested, this, &WatchlistWindow::handleSettingsRequested);
    connect(itemWidget, &SymbolItemWidget::pauseRequested, this, &WatchlistWindow::handlePauseRequested);
    connect(itemWidget, &SymbolItemWidget::resumeRequested, this, &WatchlistWindow::handleResumeRequested);

    // Insert widget *before* the stretch item
    int insertIndex = m_listLayout->count() - 1; // Index of the stretch item
    if (insertIndex < 0) insertIndex = 0;        // Handle empty layout case
    m_listLayout->insertWidget(insertIndex, itemWidget);

    m_symbolWidgets.insert(key, itemWidget);
    qInfo() << "WatchlistWindow: Added widget for" << key;
}

void WatchlistWindow::removeSymbolFromListWidget(const QString& symbol, const QString& model) {
    QString key = generateKey(symbol, model);
    if (m_symbolWidgets.contains(key)) {
        SymbolItemWidget* widget = m_symbolWidgets.take(key); // Remove from map
        m_listLayout->removeWidget(widget); // Remove from layout
        widget->deleteLater(); // Schedule for deletion
        qInfo() << "WatchlistWindow: Removed widget for" << key;
    }
    else {
        qWarning() << "WatchlistWindow: Cannot remove widget, not found:" << key;
    }
}
