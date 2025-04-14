#pragma once
#include <QWidget>
#include <QString>

#include "Data/SymbolDataManager.h"

class QLabel;
class QMenu;
class QAction;

// Represent Symbol item in Watchlist
class SymbolItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit SymbolItemWidget(const QString& symbol, const QString& model, QWidget* parent = nullptr);
    ~SymbolItemWidget() override = default;

    QString getSymbolName() const;
    QString getModelName() const;
    QString getUniqueKey() const;

public slots:
    void updateState(SymbolDataManager::SymbolState newState);

signals:
    // Signals emitted when a context menu action is triggered
    void removeRequested(const QString& symbol, const QString& model);
    void settingsRequested(const QString& symbol, const QString& model);
    void pauseRequested(const QString& symbol, const QString& model);
    void resumeRequested(const QString& symbol, const QString& model);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void handleContextMenuAction(); // Slot to determine which action was triggered

private:
    QString m_symbolName;
    QString m_modelName;

    SymbolDataManager::SymbolState m_currentState = SymbolDataManager::SymbolState::Active;

    // UI Elements
    QLabel* m_symbolLabel;
    QLabel* m_modelLabel;
    QLabel* m_statusIndicatorLabel;

    // Context Menu
    QMenu* m_contextMenu;
    QAction* m_removeAction;
    QAction* m_settingsAction;
    QAction* m_pauseAction;
    QAction* m_resumeAction;

    void setupUi();
    void createContextMenu();
    void updateContextMenuActions(); // Enable/disable pause/resume based on state
};
