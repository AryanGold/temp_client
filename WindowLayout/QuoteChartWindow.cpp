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

QuoteChartWindow::QuoteChartWindow(WindowManager* windowManager, ClientReceiver* clientReceiver, QWidget* parent)
    : BaseWindow("QuoteChart", windowManager, parent),
    m_clientReceiver(clientReceiver)
{
    Log.msg(FNAME + QString("Creating main chart window..."), Logger::Level::INFO);
    if (!m_clientReceiver) {
        Log.msg(FNAME + QString("FATAL: ClientReceiver pointer is null! UI will not function correctly."), Logger::Level::ERROR);
        // Q_ASSERT(m_clientReceiver != nullptr); // Use assertion in debug builds
    }

    setupUi();
    setupConnections();

    // Trigger initial population using current data from receiver (if any)
    if (m_clientReceiver) {
        QStringList initialSymbols = m_clientReceiver->getAvailableSymbols();
        QMap<QString, QList<QDate>> initialDates;
        for (const QString& sym : initialSymbols) {
            initialDates[sym] = m_clientReceiver->getAvailableExpirationDates(sym);
        }
        // Manually call the slot to populate UI based on initial receiver state
        onDataModelReady(initialSymbols, initialDates);
    }
    else {
        // Handle case where receiver is null on startup
        m_symbolCombo->clear();
        m_dateCombo->clear();
        m_symbolCombo->addItem("Error: No Data Receiver");
        m_dateCombo->addItem("Error: No Data Receiver");
        m_symbolCombo->setEnabled(false);
        m_dateCombo->setEnabled(false);
    }

    Log.msg(FNAME + QString("Chart window created successfully."), Logger::Level::INFO);
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

    m_controlsLayout->addWidget(new QLabel("Symbol:", m_centralWidget));
    m_controlsLayout->addWidget(m_symbolCombo);
    m_controlsLayout->addSpacing(20);
    m_controlsLayout->addWidget(new QLabel("Expiration:", m_centralWidget));
    m_controlsLayout->addWidget(m_dateCombo);
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
    Log.msg(FNAME + QString("Setting up signal/slot connections."), Logger::Level::DEBUG);

    // Connect UI controls
    connect(m_symbolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QuoteChartWindow::onSymbolChanged);
    connect(m_dateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &QuoteChartWindow::onDateChanged);
    connect(m_recalibrateButton, &QPushButton::clicked, this, &QuoteChartWindow::onRecalibrateClicked);
    connect(m_resetZoomButton, &QPushButton::clicked, this, &QuoteChartWindow::onResetZoomClicked);

    // Connect receiver's signal to update UI controls
    if (m_clientReceiver) {
        connect(m_clientReceiver, &ClientReceiver::dataReady, this, &QuoteChartWindow::onDataModelReady);
    }
    else {
        Log.msg(FNAME + QString("Cannot connect model signals: ClientReceiver is null."), Logger::Level::WARNING);
    }

    // Connect the plot's click signal to our handler slot
    connect(m_smilePlot, &SmilePlot::pointClicked, this, &QuoteChartWindow::onPlotPointClicked);
}

void QuoteChartWindow::populateSymbolComboBox(const QStringList& symbols) {
    Log.msg(FNAME + QString("Populating symbol combo box with %1 items.").arg(symbols.size()), Logger::Level::DEBUG);
    QString currentSymbol = m_symbolCombo->currentText(); // Remember current selection

    m_symbolCombo->blockSignals(true);
    m_symbolCombo->clear();
    if (symbols.isEmpty()) {
        m_symbolCombo->addItem("No symbols available");
        m_symbolCombo->setEnabled(false);
    }
    else {
        m_symbolCombo->addItems(symbols);
        // Try to restore previous selection
        int prevIndex = m_symbolCombo->findText(currentSymbol);
        if (prevIndex != -1) {
            m_symbolCombo->setCurrentIndex(prevIndex);
        }
        m_symbolCombo->setEnabled(true);
    }
    m_symbolCombo->blockSignals(false);

    // Manually trigger update for current/restored symbol if valid
    if (m_symbolCombo->currentIndex() >= 0) {
        onSymbolChanged(m_symbolCombo->currentIndex());
    }
    else {
        populateDateComboBox({}); // Clear dates if no symbol selected
    }
}

void QuoteChartWindow::populateDateComboBox(const QList<QDate>& dates) {
    Log.msg(FNAME + QString("Populating date combo box with %1 dates.").arg(dates.size()), Logger::Level::DEBUG);
    QVariant currentDateData = m_dateCombo->currentData(); // Remember current date

    m_dateCombo->blockSignals(true);
    m_dateCombo->clear();
    if (dates.isEmpty()) {
        m_dateCombo->addItem("No dates available");
        m_dateCombo->setEnabled(false);
    }
    else {
        int newIndex = -1;
        for (int i = 0; i < dates.size(); ++i) {
            const QDate& date = dates.at(i);
            m_dateCombo->addItem(date.toString(Qt::ISODate), QVariant::fromValue(date));
            // Check if this is the previously selected date
            if (currentDateData.isValid() && currentDateData.canConvert<QDate>() && date == currentDateData.toDate()) {
                newIndex = i;
            }
        }
        // Restore previous selection if found, otherwise select first date
        m_dateCombo->setCurrentIndex((newIndex != -1) ? newIndex : 0);
        m_dateCombo->setEnabled(true);
    }
    m_dateCombo->blockSignals(false);

    // Manually trigger plot update if a valid date is now selected
    if (m_dateCombo->currentIndex() >= 0) {
        requestPlotUpdate();
    }
    else {
        m_smilePlot->clearPlot(); // Clear plot if no date selected
    }
}

void QuoteChartWindow::requestPlotUpdate() {
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
        m_smilePlot->updatePlot(data.strikes, data.theoIvs, data.askIvs, data.bidIvs, data.tooltips);
    }
    else {
        Log.msg(FNAME + QString("No valid data found for selection in receiver. Clearing plot."), Logger::Level::WARNING);
        m_smilePlot->clearPlot();
    }
}

void QuoteChartWindow::onDataModelReady(const QStringList& availableSymbols, const QMap<QString, QList<QDate>>& availableDatesPerSymbol) {
    Log.msg(FNAME + QString("Received dataReady signal. Symbols: %1").arg(availableSymbols.join(", ")), Logger::Level::INFO);
    m_availableDates = availableDatesPerSymbol; // Update cache
    populateSymbolComboBox(availableSymbols);   // Repopulate symbols (will trigger date update)
}

void QuoteChartWindow::onSymbolChanged(int index) {
    if (index < 0) return;
    QString selectedSymbol = m_symbolCombo->itemText(index);
    Log.msg(FNAME + QString("Symbol changed to: '%1'. Updating date combo.").arg(selectedSymbol), Logger::Level::DEBUG);
    // Get available dates for the newly selected symbol from cache
    QList<QDate> dates = m_availableDates.value(selectedSymbol);
    populateDateComboBox(dates); // Repopulate dates (will trigger plot update)
}

void QuoteChartWindow::onDateChanged(int index) {
    if (index < 0) return;
    Log.msg(FNAME + QString("Date changed. Triggering plot update."), Logger::Level::DEBUG);
    requestPlotUpdate(); // Directly update plot for the new date
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
void QuoteChartWindow::onPlotPointClicked(SmilePlot::ScatterType type, double strike, double iv, QPointF screenPos) {
    QString typeStr = (type == SmilePlot::AskIV) ? "Ask" : "Bid";
    Log.msg(FNAME + QString("Received plot click signal. Type: %1, Strike: %2, IV: %3")
        .arg(typeStr).arg(strike).arg(iv), Logger::Level::INFO);

    // Display info in the status bar
    statusBar()->showMessage(QString("Clicked %1 Point - Strike: %L1, IV: %L2")
        .arg(typeStr)
        .arg(strike, 0, 'f', 2) // Format strike
        .arg(iv, 0, 'f', 4), // Format IV
        5000); // Show message for 5 seconds
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
