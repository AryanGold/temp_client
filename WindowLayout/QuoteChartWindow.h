#pragma once

#include "BaseWindow.h"
#include "Plots/SmilePlot.h"

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
class SmilePlot; // Include Qt Charts based SmilePlot class header
class QLabel;     // Include QLabel

class QuoteChartWindow : public BaseWindow
{
    Q_OBJECT

public:
    explicit QuoteChartWindow(WindowManager* windowManager, ClientReceiver* clientReceiver, QWidget* parent = nullptr);
    ~QuoteChartWindow() override = default;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDataModelReady(const QStringList& availableSymbols, const QMap<QString, QList<QDate>>& availableDatesPerSymbol);
    void onSymbolChanged(int index);
    void onDateChanged(int index);
    void onRecalibrateClicked();
    void requestPlotUpdate();
    // Slot to handle clicks emitted from SmilePlot
    void onPlotPointClicked(SmilePlot::ScatterType type, double strike, double iv, QPointF screenPos);
    void onResetZoomClicked();


private:
    QWidget* m_centralWidget = nullptr;
    QVBoxLayout* m_mainLayout = nullptr;
    QHBoxLayout* m_controlsLayout = nullptr;
    QComboBox* m_symbolCombo = nullptr;
    QComboBox* m_dateCombo = nullptr;
    QPushButton* m_recalibrateButton = nullptr;
    SmilePlot* m_smilePlot = nullptr; // Instance of the Qt Charts based SmilePlot
    QPushButton* m_resetZoomButton = nullptr;

    ClientReceiver* m_clientReceiver = nullptr;
    QMap<QString, QList<QDate>> m_availableDates; // Cache available dates

    void setupUi();
    void setupConnections();
    void populateSymbolComboBox(const QStringList& symbols);
    void populateDateComboBox(const QList<QDate>& dates);
};
