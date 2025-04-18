#include "QuoteChartWindow.h"
#include "Data/ClientReceiver.h"
#include "Glob/Logger.h"

#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDateTime>
#include <QVariant>
#include <QDebug>
#include <QMessageBox>
#include <QLabel> 
#include <QCloseEvent>
#include <QStatusBar>
#include <QToolButton>
#include <QButtonGroup>

QuoteChartWindow::QuoteChartWindow(WindowManager* windowManager, ClientReceiver* clientReceiver, QWidget* parent)
    : BaseWindow("QuoteChart", windowManager, parent),
    m_clientReceiver(clientReceiver)
{
    Log.msg(FNAME + QString("Creating main chart window..."), Logger::Level::DEBUG);
    
    if (!m_clientReceiver) {
        Log.msg(FNAME + QString("FATAL: ClientReceiver pointer is null! UI will not function correctly."), 
            Logger::Level::ERROR);
        // Q_ASSERT(m_clientReceiver != nullptr); // Use assertion in debug builds
    }

    setupUi();
    setupConnections();

    applyCurrentInteractionMode();
}

void QuoteChartWindow::setupUi() {
    Log.msg(FNAME + QString("Setting up UI elements."), Logger::Level::DEBUG);
    setWindowTitle("Volatility Smile Plot (Qt Charts)");
    resize(900, 700);
    
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);

    // Controls Layout
    m_controlsLayout = new QHBoxLayout();
    m_symbolCombo = new QComboBox(m_centralWidget);
    m_dateCombo = new QComboBox(m_centralWidget);
    m_recalibrateButton = new QPushButton("Recalibrate", m_centralWidget);

    m_resetZoomButton = new QPushButton("Reset Zoom", m_centralWidget); 
    m_resetZoomButton->setToolTip("Reset plot zoom and pan to default view");
    
    ////////////////////
    setupModeButtons(); // Create Pan/Zoom buttons
    ////////////////////
    
    m_controlsLayout->addWidget(new QLabel("Symbol:", m_centralWidget));
    m_controlsLayout->addWidget(m_symbolCombo);
    m_controlsLayout->addSpacing(20);
    m_controlsLayout->addWidget(new QLabel("Expiration:", m_centralWidget));
    m_controlsLayout->addWidget(m_dateCombo);
    m_controlsLayout->addSpacing(20);
    m_controlsLayout->addWidget(m_panButton);
    m_controlsLayout->addWidget(m_zoomButton);
    m_controlsLayout->addStretch(1);
    m_controlsLayout->addWidget(m_resetZoomButton);
    m_controlsLayout->addSpacing(20);
    m_controlsLayout->addWidget(m_recalibrateButton);
    
    // Plot Widget (Qt Charts based SmilePlot)
    m_smilePlot = new SmilePlot(m_centralWidget);
    
    m_mainLayout->addLayout(m_controlsLayout);
    m_mainLayout->addWidget(m_smilePlot, 1); // Plot takes stretch space

    setCentralWidget(m_centralWidget);

    // Initialize combo box states
    m_symbolCombo->setEnabled(false);
    m_dateCombo->setEnabled(false);
    m_symbolCombo->addItem("Loading symbols...");
    m_dateCombo->addItem("Select symbol first...");

    // Add a status bar for displaying click info (optional)
    statusBar()->showMessage("Ready");
}

void QuoteChartWindow::setupConnections() {
    // Connect UI controls
    connect(m_symbolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QuoteChartWindow::onSymbolChanged);
    connect(m_dateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QuoteChartWindow::onDateChanged);
    connect(m_recalibrateButton, &QPushButton::clicked, this, &QuoteChartWindow::onRecalibrateClicked);
    connect(m_resetZoomButton, &QPushButton::clicked, this, &QuoteChartWindow::onResetZoomClicked);

    // Connect receiver's signal to update UI controls
    if (m_clientReceiver) {
        connect(m_clientReceiver, &ClientReceiver::plotDataUpdated, this, &QuoteChartWindow::plotDataUpdated,
            Qt::QueuedConnection); // Use Queued if ClientReceiver might be in another thread
    }
    else {
        Log.msg(FNAME + QString("Cannot connect model signals: ClientReceiver is null."), Logger::Level::WARNING);
    }

    // Connect the plot's click signal to our handler slot
    connect(m_smilePlot, &SmilePlot::pointClicked, this, &QuoteChartWindow::onPlotPointClicked);
}

// Slot called when the complete data model is ready/updated
void QuoteChartWindow::plotDataUpdated(const QString& symbol, const QDate& date, const PlotDataForDate& data) {
    Log.msg(FNAME + "Received plot data update for " + symbol + " / " + date.toString(Qt::ISODate), Logger::Level::DEBUG);

    // --- Update internal data store ---
    m_allPlotData[symbol][date] = data; // Insert or update data for this symbol/date

    // --- Update list of known symbols ---
    if (!m_availableSymbols.contains(symbol)) {
        m_availableSymbols.append(symbol);
        m_availableSymbols.sort(); // Keep sorted
        // Repopulate symbol combo ONLY if the list actually changed
        populateSymbolCombo();
        Log.msg(FNAME + "Added new symbol: " + symbol, Logger::Level::DEBUG);
    }

    // --- Update date combo IF the updated data is for the CURRENTLY selected symbol ---
    if (symbol == m_currentSymbol) {
        // Check if the new date needs to be added to the list for the current symbol
        bool dateListChanged = false;
        if (!m_availableDatesForCurrentSymbol.contains(date)) {
            m_availableDatesForCurrentSymbol.append(date);
            std::sort(m_availableDatesForCurrentSymbol.begin(), m_availableDatesForCurrentSymbol.end());
            dateListChanged = true;
            Log.msg(FNAME + "Added new date for current symbol: " + date.toString(Qt::ISODate), Logger::Level::DEBUG);
        }
        // Repopulate date combo if list changed OR if it was previously empty
        if (dateListChanged || m_dateCombo->count() == 0) {
            populateDateCombo(); // This will also trigger plotting if needed
        }
        // --- Re-plot IF the updated data matches the currently selected symbol AND date ---
        else if (date == m_currentDate) {
            Log.msg(FNAME + "Data for currently selected symbol/date updated. Re-plotting.", Logger::Level::DEBUG);
            plotSelectedData(); // Replot with the new data
        }
    }
}

// Updates the items in the symbol combo box using m_availableSymbols
void QuoteChartWindow::populateSymbolCombo() {
    if (!m_symbolCombo) return;

    Log.msg(FNAME + "Populating symbol combo.", Logger::Level::DEBUG);
    QString currentSelection = m_symbolCombo->currentText(); // Preserve selection attempt

    m_symbolCombo->blockSignals(true);
    m_symbolCombo->clear();
    m_symbolCombo->addItems(m_availableSymbols); // Use member list

    int idx = m_symbolCombo->findText(currentSelection);
    QString newSelectionSymbol = "";
    if (idx != -1) {
        m_symbolCombo->setCurrentIndex(idx);
        newSelectionSymbol = m_symbolCombo->itemText(idx);
    }
    else if (m_symbolCombo->count() > 0) {
        m_symbolCombo->setCurrentIndex(0); // Select first symbol if previous not found
        newSelectionSymbol = m_symbolCombo->itemText(0);
    }

    m_symbolCombo->blockSignals(false);

    // Update m_currentSymbol only if the actual selection changed
    // This prevents unnecessary date combo repopulation if the symbol stayed the same
    if (m_currentSymbol != newSelectionSymbol) {
        m_currentSymbol = newSelectionSymbol;
        Log.msg(FNAME + "Symbol selection changed to: " + m_currentSymbol + " after populating combo.", Logger::Level::DEBUG);
        populateDateCombo(); // Populate dates for the newly selected symbol
    }
    else if (m_currentSymbol.isEmpty()) {
        // Handle case where combo becomes empty
        populateDateCombo(); // Will clear dates and plot
    }
}

// Updates the items in the date combo box based on the selected symbol
void QuoteChartWindow::populateDateCombo() {
    if (!m_dateCombo) return;

    m_dateCombo->blockSignals(true);
    m_dateCombo->clear();
    m_availableDatesForCurrentSymbol.clear();
    m_dateCombo->setEnabled(false);
    QDate newSelectionDate; // Store the date that will be selected

    if (!m_currentSymbol.isEmpty() && m_allPlotData.contains(m_currentSymbol)) {
        Log.msg(FNAME + "Populating date combo for symbol: " + m_currentSymbol, Logger::Level::DEBUG);
        m_availableDatesForCurrentSymbol = m_allPlotData.value(m_currentSymbol).keys();
        std::sort(m_availableDatesForCurrentSymbol.begin(), m_availableDatesForCurrentSymbol.end());

        QStringList dateStrings;
        for (const QDate& date : std::as_const(m_availableDatesForCurrentSymbol)) {
            dateStrings.append(date.toString(Qt::ISODate));
        }

        QString currentDateStringSelection = m_currentDate.isValid() ? m_currentDate.toString(Qt::ISODate) : "";

        m_dateCombo->addItems(dateStrings);
        m_dateCombo->setEnabled(m_dateCombo->count() > 0);

        int idx = m_dateCombo->findText(currentDateStringSelection);
        if (idx != -1) {
            m_dateCombo->setCurrentIndex(idx);
            newSelectionDate = m_currentDate; // Keep existing valid date
        }
        else if (m_dateCombo->count() > 0) {
            m_dateCombo->setCurrentIndex(m_dateCombo->count() - 1); // Select latest date
            newSelectionDate = QDate::fromString(m_dateCombo->itemText(m_dateCombo->count() - 1), Qt::ISODate);
        }
        // else: newSelectionDate remains invalid
    }
    else {
        Log.msg(FNAME + "Cannot populate dates - symbol invalid or no data: " + m_currentSymbol, Logger::Level::DEBUG);
        // newSelectionDate remains invalid
    }

    m_dateCombo->blockSignals(false);

    // Update m_currentDate only if the actual selection changed
    if (m_currentDate != newSelectionDate) {
        m_currentDate = newSelectionDate;
        Log.msg(FNAME + "Date selection changed to: " + (m_currentDate.isValid() ? m_currentDate.toString(Qt::ISODate) : "None") + " after populating combo.", Logger::Level::DEBUG);
        plotSelectedData(); // Plot the newly selected date (or clear if invalid)
    }
    else {
        // If date selection didn't change (e.g., was already latest and still is),
        // but data might have updated, ensure plot reflects current state.
        plotSelectedData();
    }
}

// Retrieves data for the current symbol/date and sends it to SmilePlot
void QuoteChartWindow::plotSelectedData() {
    if (!m_smilePlot) {
        Log.msg(FNAME + "SmilePlot widget is null, cannot plot.", Logger::Level::ERROR);
        return;
    }
    if (m_currentSymbol.isEmpty() || !m_currentDate.isValid()) {
        Log.msg(FNAME + "Cannot plot - Symbol or Date not selected/valid.", Logger::Level::DEBUG);
        m_smilePlot->updateData({}, {}, {}, {}, {}); // Clear the plot
        return;
    }

    Log.msg(FNAME + "Plotting data for: " + m_currentSymbol + " / " + m_currentDate.toString(Qt::ISODate), Logger::Level::DEBUG);

    // Safely access data
    const auto& datesMap = m_allPlotData.value(m_currentSymbol);
    const PlotDataForDate& dataToPlot = datesMap.value(m_currentDate);

    // Check if data is actually populated
    if (dataToPlot.theoPoints.isEmpty() && dataToPlot.midPoints.isEmpty()) {
        Log.msg(FNAME + "No actual plot data found in map for selected symbol/date.", Logger::Level::WARNING);
        m_smilePlot->updateData({}, {}, {}, {}, {}); // Clear the plot
        return;
    }

    // Pass data vectors to the SmilePlot widget
    m_smilePlot->updateData(dataToPlot.theoPoints,
        dataToPlot.midPoints,
        dataToPlot.bidPoints,
        dataToPlot.askPoints,
        dataToPlot.pointDetails);
}

// --- Slot Implementations for UI changes ---

void QuoteChartWindow::onSymbolChanged(int index) {
    if (!m_symbolCombo || index < 0) return;
    QString newSymbol = m_symbolCombo->itemText(index);
    // Only proceed if symbol actually changed to prevent potential loops
    if (newSymbol != m_currentSymbol) {
        m_currentSymbol = newSymbol;
        Log.msg(FNAME + "Symbol changed via UI to: " + m_currentSymbol, Logger::Level::DEBUG);
        populateDateCombo(); // Update dates and trigger plot for the new symbol
    }
}

void QuoteChartWindow::onDateChanged(int index) {
    if (!m_dateCombo || index < 0) return;
    QDate newDate = QDate::fromString(m_dateCombo->itemText(index), Qt::ISODate);
    // Only proceed if date actually changed
    if (newDate.isValid() && newDate != m_currentDate) {
        m_currentDate = newDate;
        Log.msg(FNAME + "Date changed via UI to: " + m_currentDate.toString(Qt::ISODate), Logger::Level::DEBUG);
        plotSelectedData(); // Re-plot with the newly selected date
    }
    else if (!newDate.isValid() && m_currentDate.isValid()) {
        // Handle case where selection becomes invalid (e.g., combo cleared)
        m_currentDate = QDate();
        plotSelectedData(); // Clear plot
    }
}


void QuoteChartWindow::onRecalibrateClicked() {
    Log.msg(FNAME + QString("Recalibrate button clicked (placeholder action)."), Logger::Level::INFO);
    QString currentSymbol = m_symbolCombo->currentText();
    QVariant dateData = m_dateCombo->itemData(m_dateCombo->currentIndex());
    QString dateStr = dateData.canConvert<QDate>() ? dateData.toDate().toString(Qt::ISODate) : "N/A";

    QMessageBox::information(this, "Recalibrate",
        QString("Recalibration requested for Symbol '%1', Date '%2'.\n(Functionality not implemented yet).")
        .arg(currentSymbol).arg(dateStr));

    // TODO: Send command to backend via WebSocketClient instance
    // e.g., m_webSocketClient->sendCommand({"action": "force_recalibrate", "symbol": currentSymbol, "date": dateStr });
}

// --- Slot for Handling Plot Clicks ---
void QuoteChartWindow::onPlotPointClicked(const SmilePointData& pointData) {
    statusBar()->showMessage(QString("Current ticker:  %1, strike[%2] ask[%3] bid[%4]")
        .arg(pointData.symbol).arg(pointData.strike)
        .arg(pointData.ask_price).arg(pointData.bid_price)
    );
}

void QuoteChartWindow::closeEvent(QCloseEvent* event) {
    Log.msg(FNAME + QString("Close event received."), Logger::Level::INFO);
    // Example: Disconnect signals if receiver is managed externally
    // if(m_clientReceiver) {
    //     m_clientReceiver->disconnect(this); // Disconnect all signals from receiver to this window
    // }
    QMainWindow::closeEvent(event);
}

void QuoteChartWindow::onResetZoomClicked()
{
    Log.msg(FNAME + QString("Reset Zoom button clicked."), Logger::Level::INFO);
    if (m_smilePlot) {
        m_smilePlot->resetZoom(); // Call the reset function on the plot widget
    }
    else {
        Log.msg(FNAME + QString("Cannot reset zoom: SmilePlot widget is null."), Logger::Level::WARNING);
    }
}

void QuoteChartWindow::setupModeButtons() {
    // Create buttons (parented to this SmilePlot widget)
    m_panButton = new QToolButton(m_centralWidget);
    m_panButton->setIcon(QIcon(":/icons/resources/icons/buttons/pan_icon.png"));
    m_panButton->setToolTip("Pan Mode (Click and drag chart)");
    m_panButton->setCheckable(true);
    m_panButton->setAutoRaise(true);
    m_panButton->setFixedSize(24, 24);
    m_panButton->setIconSize(QSize(20, 20));

    m_zoomButton = new QToolButton(m_centralWidget);
    m_zoomButton->setIcon(QIcon(":/icons/resources/icons/buttons/zoom.png"));
    m_zoomButton->setToolTip("Zoom Mode (Click and drag to select region)");
    m_zoomButton->setCheckable(true);
    m_zoomButton->setAutoRaise(true);
    m_zoomButton->setFixedSize(24, 24);
    m_zoomButton->setIconSize(QSize(20, 20));

    // Group buttons for exclusive selection
    m_modeButtons = new QButtonGroup(this);
    m_modeButtons->addButton(m_panButton, SmilePlot::PlotMode::pmPan);
    m_modeButtons->addButton(m_zoomButton, SmilePlot::PlotMode::pmZoom);
    m_modeButtons->setExclusive(true);

    // Connect signal for mode changes
    connect(m_modeButtons, &QButtonGroup::idClicked, this, &QuoteChartWindow::onModeButtonClicked);

    m_panButton->show();
    m_zoomButton->show();

    m_panButton->setChecked(true); // by default
}

void QuoteChartWindow::setMode(SmilePlot::PlotMode mode) {
    if (m_smilePlot->currentMode() == mode && m_modeButtons->checkedButton()) return;

    Log.msg(FNAME + "Setting plot mode to " + (mode == SmilePlot::PlotMode::pmPan ? "Pan" : "Zoom"), Logger::Level::DEBUG);
    m_smilePlot->setCurrentMode(mode);
    applyCurrentInteractionMode();

    // Update button checked state
    if (m_modeButtons) {
        QAbstractButton* buttonToCheck = m_modeButtons->button(mode);
        if (buttonToCheck && !buttonToCheck->isChecked()) {
            buttonToCheck->setChecked(true);
        }
    }
}

void QuoteChartWindow::applyCurrentInteractionMode() {
    if (m_smilePlot->currentMode() == SmilePlot::PlotMode::pmPan) {
        m_smilePlot->chartView()->setDragMode(QGraphicsView::NoDrag);
        m_smilePlot->chartView()->setRubberBand(QChartView::NoRubberBand); // Disable rubber band
        m_smilePlot->chartView()->setCursor(Qt::ArrowCursor);
    }
    else { // pmZoom
        m_smilePlot->chartView()->setDragMode(QGraphicsView::NoDrag); // Disable chart view's panning
        m_smilePlot->chartView()->setRubberBand(QChartView::RectangleRubberBand); // Enable rubber band for zooming
        m_smilePlot->chartView()->setCursor(Qt::CrossCursor); // Show cross cursor for zoom
    }
}

void QuoteChartWindow::onModeButtonClicked(int id) {
    setMode(static_cast<SmilePlot::PlotMode>(id));
}
