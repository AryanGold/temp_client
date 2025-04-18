#pragma once

#include "BaseWindow.h"
#include "Plots/SmilePlot.h"
#include "Plots/PlotDataForDate.h"

#include <QMainWindow>
#include <QMap>
#include <QList>
#include <QDate>
#include <QPointF>
#include <QStringList>

// Forward declarations
class QComboBox;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QWidget;
class ClientReceiver;
class QLabel;     // Include QLabel
class QButtonGroup;

class QuoteChartWindow : public BaseWindow
{
    Q_OBJECT

public:
    explicit QuoteChartWindow(WindowManager* windowManager, ClientReceiver* clientReceiver, QWidget* parent = nullptr);
    ~QuoteChartWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDataModelReady(const QStringList& availableSymbols,
        const QMap<QString, QMap<QDate, PlotDataForDate>>& allData);
    void onSymbolChanged(int index);
    void onDateChanged(int index);
    void onRecalibrateClicked();
    void requestPlotUpdate();
    // Slot to handle clicks emitted from SmilePlot
    void onPlotPointClicked(SmilePlot::ScatterType type, double strike, double iv, QPointF screenPos);
    void onResetZoomClicked();
    void onModeButtonClicked(int id);

private:
    QWidget* m_centralWidget = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QHBoxLayout* m_controlsLayout = nullptr;
    QComboBox* m_symbolCombo = nullptr;
    QComboBox* m_dateCombo = nullptr;
    QPushButton* m_recalibrateButton = nullptr;
    SmilePlot* m_smilePlot = nullptr; // Instance of the Qt Charts based SmilePlot

    QPushButton* m_resetZoomButton = nullptr;

    // Interaction Mode Handling
    QToolButton* m_panButton = nullptr;
    QToolButton* m_zoomButton = nullptr;
    QButtonGroup* m_modeButtons = nullptr;

    ClientReceiver* m_clientReceiver = nullptr;

    // --- Data Storage ---
    // Stores ALL plot data received, keyed by symbol string, then by QDate
    QMap<QString, QMap<QDate, PlotDataForDate>> m_allPlotData;
    // Stores available dates for the *currently selected* symbol (used to populate date combo)
    QList<QDate> m_availableDatesForCurrentSymbol;
    // Stores the currently selected symbol and date from the UI
    QString m_currentSymbol;
    QDate m_currentDate;

    void setupUi();
    void setupConnections();
    void populateSymbolCombo(const QStringList& symbols);
    void populateDateCombo();
    void plotSelectedData();  // Filters data and calls SmilePlot::updateData

    void setupModeButtons(); // Create Pan/Zoom buttons
    void applyCurrentInteractionMode();
    void setMode(SmilePlot::PlotMode mode);
};
