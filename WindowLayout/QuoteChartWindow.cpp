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
        connect(m_clientReceiver, &ClientReceiver::plotDataUpdated, this, &QuoteChartWindow::onDataModelReady);
    }
    else {
        Log.msg(FNAME + QString("Cannot connect model signals: ClientReceiver is null."), Logger::Level::WARNING);
    }

    // Connect the plot's click signal to our handler slot
    connect(m_smilePlot, &SmilePlot::pointClicked, this, &QuoteChartWindow::onPlotPointClicked);
}

// Updates the items in the symbol combo box
void QuoteChartWindow::populateSymbolCombo(const QStringList& symbols) {
    if (!m_symbolCombo) return;

    Log.msg(FNAME + "Populating symbol combo.", Logger::Level::DEBUG);
    // Store the currently selected text to try and preserve it
    QString currentSelection = m_symbolCombo->currentText();

    m_symbolCombo->blockSignals(true); // Prevent signals during modification
    m_symbolCombo->clear();
    m_symbolCombo->addItems(symbols);

    // Try to restore the previous selection
    int idx = m_symbolCombo->findText(currentSelection);
    if (idx != -1) {
        m_symbolCombo->setCurrentIndex(idx);
        // Update m_currentSymbol just in case itemText differs slightly
        m_currentSymbol = m_symbolCombo->itemText(idx);
    }
    else if (m_symbolCombo->count() > 0) {
        // If previous selection not found, select the first item
        m_symbolCombo->setCurrentIndex(0);
        m_currentSymbol = m_symbolCombo->itemText(0); // Update current symbol
    }
    else {
        // No symbols available
        m_currentSymbol = "";
    }
    m_symbolCombo->blockSignals(false); // Re-enable signals

    // If the selection logic resulted in no current symbol, handle it
    if (m_currentSymbol.isEmpty()) {
        populateDateCombo(); // This will clear the date combo and plot
    }
    // If the selection logic *did* change the index (e.g. from -1 to 0, or restored index),
    // the currentIndexChanged signal *should* fire now that signals are unblocked,
    // triggering onSymbolChanged -> populateDateCombo -> plotSelectedData.
    // If the index *didn't* change, we rely on the check in onDataModelReady to replot.
}

// Updates the items in the date combo box based on the selected symbol
void QuoteChartWindow::populateDateCombo() {
    if (!m_dateCombo) return;

    // Clear previous dates and disable combo initially
    m_dateCombo->blockSignals(true);
    m_dateCombo->clear();
    m_availableDatesForCurrentSymbol.clear();
    m_dateCombo->setEnabled(false);

    // Check if a valid symbol is selected and data exists for it
    if (m_currentSymbol.isEmpty() || !m_allPlotData.contains(m_currentSymbol)) {
        Log.msg(FNAME + "Cannot populate dates - no symbol selected or no data for symbol: " + m_currentSymbol, Logger::Level::DEBUG);
        m_dateCombo->blockSignals(false);
        plotSelectedData(); // Clear the plot
        return;
    }

    Log.msg(FNAME + "Populating date combo for symbol: " + m_currentSymbol, Logger::Level::DEBUG);

    // Get available dates and sort them
    m_availableDatesForCurrentSymbol = m_allPlotData.value(m_currentSymbol).keys();
    std::sort(m_availableDatesForCurrentSymbol.begin(), m_availableDatesForCurrentSymbol.end()); // Ascending sort

    // Convert dates to strings for the combo box
    QStringList dateStrings;
    for (const QDate& date : std::as_const(m_availableDatesForCurrentSymbol)) {
        dateStrings.append(date.toString(Qt::ISODate)); // Use YYYY-MM-DD format
    }

    // Store current selection string (if any) to try and preserve it
    QString currentDateStringSelection = m_currentDate.isValid() ? m_currentDate.toString(Qt::ISODate) : "";

    // Populate the combo box
    m_dateCombo->addItems(dateStrings);
    m_dateCombo->setEnabled(m_dateCombo->count() > 0); // Enable only if dates were added

    // Try to restore previous selection or select the latest date
    int idx = m_dateCombo->findText(currentDateStringSelection);
    if (idx != -1) {
        m_dateCombo->setCurrentIndex(idx);
        // m_currentDate should already be correct
    }
    else if (m_dateCombo->count() > 0) {
        // Default to the last (latest) date in the sorted list
        m_dateCombo->setCurrentIndex(m_dateCombo->count() - 1);
        // Update m_currentDate based on the new selection
        m_currentDate = QDate::fromString(m_dateCombo->itemText(m_dateCombo->count() - 1), Qt::ISODate);
    }
    else {
        // No dates available for this symbol
        m_currentDate = QDate(); // Set to invalid date
    }
    m_dateCombo->blockSignals(false); // Re-enable signals

    // Plot data for the automatically selected date (or clear plot if no date)
    plotSelectedData();
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

    // Safely access the data using .value() which returns a default-constructed map if key not found
    const auto& datesMap = m_allPlotData.value(m_currentSymbol);
    // Safely access the specific date data using .value() which returns a default-constructed PlotDataForDate
    const PlotDataForDate& dataToPlot = datesMap.value(m_currentDate);

    // Check if the retrieved data is actually populated (value() returns default if key not found)
    // A simple check could be if one of the vectors is empty, assuming valid data always has points.
    if (dataToPlot.theoPoints.isEmpty() && dataToPlot.midPoints.isEmpty()) { // Adjust check as needed
        Log.msg(FNAME + "No actual plot data found in map for selected symbol/date.", Logger::Level::WARNING);
        m_smilePlot->updateData({}, {}, {}, {}, {}); // Clear the plot
        return;
    }

    // Pass the retrieved data vectors to the SmilePlot widget
    m_smilePlot->updateData(dataToPlot.theoPoints,
        dataToPlot.midPoints,
        dataToPlot.bidPoints,
        dataToPlot.askPoints,
        dataToPlot.pointDetails);
}

/*void QuoteChartWindow::requestPlotUpdate() {
    Log.msg(FNAME + QString("Plot update requested."), Logger::Level::DEBUG);
    if (!m_clientReceiver) {
        Log.msg(FNAME + QString("Cannot update plot: ClientReceiver is null."), Logger::Level::WARNING);
        m_smilePlot->clearPlot();
        return;
    }

    QString currentSymbol = m_symbolCombo->currentText();
    int dateIndex = m_dateCombo->currentIndex();
    QVariant dateData = m_dateCombo->itemData(dateIndex);

    // Check if valid selections are made
    if (currentSymbol.isEmpty() || currentSymbol.startsWith("No symbols") || currentSymbol.startsWith("Loading") ||
        dateIndex < 0 || !dateData.isValid() || !dateData.canConvert<QDate>())
    {
        Log.msg(FNAME + QString("Invalid selection for plot update (Symbol: '%1', Date Index: %2). Clearing plot.")
            .arg(currentSymbol).arg(dateIndex), Logger::Level::DEBUG);
        m_smilePlot->clearPlot();
        return;
    }

    QDate selectedDate = dateData.toDate();

    Log.msg(FNAME + QString("Fetching data for Symbol: %1, Date: %2")
        .arg(currentSymbol).arg(selectedDate.toString(Qt::ISODate)), Logger::Level::DEBUG);

    SmileData data = m_clientReceiver->getSmileData(currentSymbol, selectedDate);

    if (data.isValid) {
        Log.msg(FNAME + QString("Data retrieved (%1 points). Updating plot widget.")
            .arg(data.strikes.size()), Logger::Level::DEBUG);
        m_smilePlot->updateData(data.strikes, data.theoIvs, data.askIvs, data.bidIvs, data.tooltips);
    }
    else {
        Log.msg(FNAME + QString("No valid data found for selection in receiver. Clearing plot."), Logger::Level::WARNING);
        m_smilePlot->clearPlot();
    }
}*/

// Slot called when the complete data model is ready/updated
void QuoteChartWindow::onDataModelReady(const QStringList& availableSymbols,
    const QMap<QString, QMap<QDate, PlotDataForDate>>& allData)
{
    Log.msg(FNAME + "Received new data model. Symbols count: " + QString::number(availableSymbols.size()), Logger::Level::INFO);

    // Store the complete dataset
    m_allPlotData = allData;

    // Update the list of symbols in the combo box
    populateSymbolCombo(availableSymbols);

    // If the currently selected symbol remained the same after repopulating the combo,
    // but the underlying data might have changed, we need to trigger a replot manually.
    if (m_symbolCombo && m_symbolCombo->currentText() == m_currentSymbol && !m_currentSymbol.isEmpty()) {
        Log.msg(FNAME + "Symbol selection unchanged (" + m_currentSymbol + "), triggering plot update.", Logger::Level::DEBUG);
        // Need to ensure date combo is also up-to-date before plotting
        populateDateCombo(); // This will call plotSelectedData if a valid date exists/is selected
    }
    else if (availableSymbols.isEmpty()) {
        // Handle case where no symbols are available after update
        m_dateCombo->clear();
        m_dateCombo->setEnabled(false);
        if (m_smilePlot) m_smilePlot->updateData({}, {}, {}, {}, {}); // Clear plot
    }
    // Note: If populateSymbolCombo *changes* the current symbol, the onSymbolChanged slot
    // will automatically trigger populateDateCombo and plotSelectedData.
}

// --- Slot Implementations for UI changes ---

void QuoteChartWindow::onSymbolChanged(int index) {
    // This slot is triggered when the user selects a different symbol
    if (!m_symbolCombo || index < 0) return; // Basic safety check
    m_currentSymbol = m_symbolCombo->itemText(index); // Update the stored symbol
    Log.msg(FNAME + "Symbol changed via UI to: " + m_currentSymbol, Logger::Level::DEBUG);
    // Need to update the available dates and potentially the plot
    populateDateCombo(); // This will find dates for the new symbol and trigger plotting
}

void QuoteChartWindow::onDateChanged(int index) {
    // This slot is triggered when the user selects a different date
    if (!m_dateCombo || index < 0) return; // Basic safety check
    // Update the stored date
    m_currentDate = QDate::fromString(m_dateCombo->itemText(index), Qt::ISODate);
    Log.msg(FNAME + "Date changed via UI to: " + m_currentDate.toString(Qt::ISODate), Logger::Level::DEBUG);
    if (m_currentDate.isValid()) {
        plotSelectedData(); // Re-plot with the newly selected date
    }
    else {
        Log.msg(FNAME + "Invalid date selected via UI.", Logger::Level::WARNING);
        if (m_smilePlot) m_smilePlot->updateData({}, {}, {}, {}, {}); // Clear plot
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

//// --- Slot for Handling Plot Clicks ---
//void QuoteChartWindow::onPlotPointClicked(SmilePlot::ScatterType type, double strike, double iv, QPointF screenPos) {
//    QString typeStr = (type == SmilePlot::AskIV) ? "Ask" : "Bid";
//    Log.msg(FNAME + QString("Received plot click signal. Type: %1, Strike: %2, IV: %3")
//        .arg(typeStr).arg(strike).arg(iv), Logger::Level::INFO);
//
//    // Display info in the status bar
//    statusBar()->showMessage(QString("Clicked %1 Point - Strike: %L1, IV: %L2")
//        .arg(typeStr)
//        .arg(strike, 0, 'f', 2) // Format strike
//        .arg(iv, 0, 'f', 4), // Format IV
//        5000); // Show message for 5 seconds
//}

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
