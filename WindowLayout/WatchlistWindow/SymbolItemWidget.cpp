#include "SymbolItemWidget.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QIcon>
#include <QDebug>
#include <QPalette> 

SymbolItemWidget::SymbolItemWidget(const QString& symbol, const QString& model, QWidget* parent) :
    QWidget(parent),
    m_symbolName(symbol),
    m_modelName(model)
{
    setupUi();
    createContextMenu();
    updateState(SymbolDataManager::SymbolState::Active); // Initial state
    // Set minimum height to make it look better in a list
    setMinimumHeight(30);
}

QString SymbolItemWidget::getSymbolName() const {
    return m_symbolName;
}

QString SymbolItemWidget::getModelName() const {
    return m_modelName;
}

QString SymbolItemWidget::getUniqueKey() const {
    return m_symbolName + "_" + m_modelName;
}


void SymbolItemWidget::setupUi() {
    setAutoFillBackground(true); // Crucial for background/border styling via stylesheet on QWidget

    // Add a background color or border on hover
    setAttribute(Qt::WA_Hover);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 2, 5, 2); // Add some padding

    m_symbolLabel = new QLabel(m_symbolName, this);
    m_modelLabel = new QLabel(m_modelName, this);
    m_statusIndicatorLabel = new QLabel(this); // Initially empty
    // m_statusIndicatorLabel->setPixmap(QPixmap(ICON_PAUSED_STATUS).scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_statusIndicatorLabel->setFixedSize(18, 18); // Reserve space for icon
    m_statusIndicatorLabel->setVisible(false); // Hide initially


    // Make labels expand nicely
    m_symbolLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_modelLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Make sure labels don't steal the hover event needed by the parent SymbolItemWidget
    m_symbolLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_modelLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_statusIndicatorLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    layout->addWidget(m_symbolLabel);
    layout->addWidget(m_modelLabel);
    layout->addStretch(); // Push status indicator to the right
    layout->addWidget(m_statusIndicatorLabel);

    setLayout(layout);

    setStyleSheet("SymbolItemWidget { background-color: red; border: 5px solid blue; }");

    /*setStyleSheet(
        "SymbolItemWidget {"
        "   border: 1px solid #FF0000;" // Reserve space, but make it invisible
        "   padding: 2px;"                  // Add padding inside the border
        "}"
        "SymbolItemWidget:hover {"
        "   border: 1px solid #A0A0FF;"     // Visible border on hover (light blue)
        "   background-color: #F0F0F0;" // Optional: Slight background change on hover
        "}"
    );*/
}

void SymbolItemWidget::createContextMenu() {
    m_contextMenu = new QMenu(this);

    m_removeAction = new QAction(QIcon(":/icons/resources/icons/buttons/remove_item.png"), "Remove", this);
    m_settingsAction = new QAction(QIcon(":/icons/resources/icons/buttons/settings_item.png"), "Settings", this);
    m_pauseAction = new QAction(QIcon(":/icons/resources/icons/buttons/pause.png"), "Pause", this);
    m_resumeAction = new QAction(QIcon(":/icons/resources/icons/buttons/continue.png"), "Resume", this);

    connect(m_removeAction, &QAction::triggered, this, &SymbolItemWidget::handleContextMenuAction);
    connect(m_settingsAction, &QAction::triggered, this, &SymbolItemWidget::handleContextMenuAction);
    connect(m_pauseAction, &QAction::triggered, this, &SymbolItemWidget::handleContextMenuAction);
    connect(m_resumeAction, &QAction::triggered, this, &SymbolItemWidget::handleContextMenuAction);

    m_contextMenu->addAction(m_settingsAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_pauseAction);
    m_contextMenu->addAction(m_resumeAction);
    m_contextMenu->addSeparator();
    m_contextMenu->addAction(m_removeAction);
}

void SymbolItemWidget::contextMenuEvent(QContextMenuEvent* event) {
    updateContextMenuActions(); // Ensure actions are enabled/disabled correctly
    m_contextMenu->popup(event->globalPos());
    event->accept();
}

void SymbolItemWidget::updateContextMenuActions() {
    bool isActive = (m_currentState == SymbolDataManager::SymbolState::Active);
    m_pauseAction->setVisible(isActive);
    m_resumeAction->setVisible(!isActive);
}

void SymbolItemWidget::handleContextMenuAction() {
    QAction* action = qobject_cast<QAction*>(sender());
    if (!action) return;

    if (action == m_removeAction) {
        emit removeRequested(m_symbolName, m_modelName);
    }
    else if (action == m_settingsAction) {
        emit settingsRequested(m_symbolName, m_modelName);
    }
    else if (action == m_pauseAction) {
        emit pauseRequested(m_symbolName, m_modelName);
    }
    else if (action == m_resumeAction) {
        emit resumeRequested(m_symbolName, m_modelName);
    }
}

void SymbolItemWidget::updateState(SymbolDataManager::SymbolState newState) {
    m_currentState = newState;

    QPalette pal = palette(); // Get current palette
    bool isActive = (newState == SymbolDataManager::SymbolState::Active);

    if (isActive) {
        // Restore default text color (or active color)
        pal.setColor(QPalette::WindowText, QGuiApplication::palette().color(QPalette::Active, QPalette::WindowText));
        m_statusIndicatorLabel->setVisible(true);
        m_statusIndicatorLabel->setPixmap(QPixmap(":/icons/resources/icons/buttons/status_green.png")
            .scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_statusIndicatorLabel->setToolTip("Working");
        // setStyleSheet(""); // Clear any specific paused styling
    }
    else {
        // Set text color to gray (or disabled color)
        pal.setColor(QPalette::WindowText, QGuiApplication::palette().color(QPalette::Disabled, QPalette::WindowText));
        m_statusIndicatorLabel->setVisible(true);
        // Optional: Load a specific icon for paused state
        m_statusIndicatorLabel->setPixmap(QPixmap(":/icons/resources/icons/buttons/status_yellow.png")
            .scaled(16, 16, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        m_statusIndicatorLabel->setToolTip("Paused");
        // Or add visual cue like background
        // setStyleSheet("background-color: lightgray;");
    }

    // Apply palette changes to labels
    m_symbolLabel->setPalette(pal);
    m_modelLabel->setPalette(pal);
    // Ensure widget repaints if palette changed
    update();

    updateContextMenuActions(); // Update menu visibility too
}
