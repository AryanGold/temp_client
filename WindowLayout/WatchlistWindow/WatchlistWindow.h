#pragma once

#include "../BaseWindow.h"

#include <QMap> 
#include "Data/SymbolDataManager.h"

// Forward declarations
QT_BEGIN_NAMESPACE
namespace Ui { class WatchlistWindow; }
QT_END_NAMESPACE
class SymbolDataManager;
class WebSocketClient;
class SymbolItemWidget;
class QVBoxLayout;
class QScrollArea;
class QWidget;

class WatchlistWindow : public BaseWindow
{
    Q_OBJECT
public:
    WatchlistWindow(SymbolDataManager* dataManager, WebSocketClient* wsClient, WindowManager* windowManager, QWidget* parent = nullptr);
    ~WatchlistWindow();

private slots:
    // UI Actions
    void onAddSymbolClicked();

    // SymbolItemWidget Signals
    void handleRemoveRequested(const QString& symbol, const QString& model);
    void handleSettingsRequested(const QString& symbol, const QString& model);
    void handlePauseRequested(const QString& symbol, const QString& model);
    void handleResumeRequested(const QString& symbol, const QString& model);

    // SymbolDataManager Signals
    void handleSymbolAdded(const QString& symbol, const QString& model);
    void handleSymbolRemoved(const QString& symbol, const QString& model);
    void handleSymbolStateChanged(const QString& symbol, const QString& model, SymbolDataManager::SymbolState newState);
    // void handleSymbolSettingsChanged(const QString& symbol, const QString& model, const QVariantMap& settings); // If needed

    // WebSocketClient Confirmation/Error Signals
    void handleSymbolAddConfirmed(const QString& symbol, const QString& model);
    void handleSymbolAddFailed(const QString& symbol, const QString& model, const QString& error);
    void handleSymbolRemoveConfirmed(const QString& symbol, const QString& model);

private:
    Ui::WatchlistWindow* ui; // Using UI file is recommended
    SymbolDataManager* m_dataManager;
    WebSocketClient* m_wsClient;

    // UI Elements for the list
    QWidget* m_listContentWidget; // Widget inside scroll area
    QVBoxLayout* m_listLayout;    // Layout for m_listContentWidget
    QScrollArea* m_scrollArea;    // Makes the list scrollable

    // Keep track of widgets associated with symbols
    QMap<QString, SymbolItemWidget*> m_symbolWidgets; // Key: symbol_model

    QStringList m_availableModels; 

    void addSymbolToListWidget(const QString& symbol, const QString& model);
    void removeSymbolFromListWidget(const QString& symbol, const QString& model);
    QString generateKey(const QString& symbol, const QString& model) const;
    void loadExistingSymbols(); // Load symbols from manager on startup
};
