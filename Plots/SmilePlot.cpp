#include "SmilePlot.h"
#include "Glob/Logger.h"

#include <QVBoxLayout>
#include <QToolButton>
#include <QButtonGroup>
#include <QIcon>
#include <QRubberBand>
#include <QMouseEvent>
#include <QToolTip>
#include <QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QApplication>
#include <QToolTip>
#include <QDebug>
#include <limits>
#include <cmath> // For std::isnan, std::isinf


//----------------------------------------------------------------------------
// Constructor and Setup (No changes needed here)
//----------------------------------------------------------------------------
SmilePlot::SmilePlot(QWidget* parent)
    : QChartView(new QChart(), parent)
{
    m_CurrentMode = SmilePlot::PlotMode::pmPan;
    m_chart = chart();
    setupChart();
    Log.msg(QString("Widget created using Qt Charts."), Logger::Level::DEBUG);
}

void SmilePlot::setupChart()
{
    m_chart->setTitle("Implied Volatility Smile");
    m_chart->legend()->setVisible(true); 
    m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_theoSeries = new QLineSeries(m_chart); 
    m_askSeries = new QScatterSeries(m_chart); 
    m_bidSeries = new QScatterSeries(m_chart);
    m_theoSeries->setName("Theo IVs"); 
    m_theoSeries->setColor(Qt::darkGreen); 
    m_theoSeries->setPen(QPen(Qt::darkGreen, 2));
    m_askSeries->setName("Ask IV"); 
    m_askSeries->setColor(Qt::blue); 
    m_askSeries->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
    m_askSeries->setMarkerSize(7.0); 
    m_askSeries->setBorderColor(Qt::transparent);
    m_bidSeries->setName("Bid IV"); 
    m_bidSeries->setColor(Qt::red); 
    m_bidSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
    m_bidSeries->setMarkerSize(7.0); 
    m_bidSeries->setBorderColor(Qt::transparent);
    m_chart->addSeries(m_theoSeries); 
    m_chart->addSeries(m_askSeries); 
    m_chart->addSeries(m_bidSeries);
    m_chart->createDefaultAxes();
    m_axisX = qobject_cast<QValueAxis*>(m_chart->axes(Qt::Horizontal, m_theoSeries).first());
    m_axisY = qobject_cast<QValueAxis*>(m_chart->axes(Qt::Vertical, m_theoSeries).first());
    if (m_axisX && m_axisY) { // Axis configuration (same as before)
        m_axisX->setTitleText("Strike"); 
        m_axisY->setTitleText("Implied Volatility (IV)");
        m_axisY->setLabelFormat("%.4f"); 
        m_axisX->setLabelFormat("%.2f");
        m_axisY->setMin(0); 
        m_axisX->setTickCount(11); 
        m_axisY->setTickCount(6);
        m_axisX->setGridLineVisible(true); 
        m_axisY->setGridLineVisible(true); 
        m_axisX->setMinorGridLineVisible(true); 
        m_axisY->setMinorGridLineVisible(true);
        QPen gridPen(QColor(220, 220, 220), 1, Qt::DotLine); 
        QPen minorGridPen(QColor(240, 240, 240), 1, Qt::DotLine);
        m_axisX->setGridLinePen(gridPen); 
        m_axisY->setGridLinePen(gridPen); 
        m_axisX->setMinorGridLinePen(minorGridPen); 
        m_axisY->setMinorGridLinePen(minorGridPen);
    }

    setRubberBand(QChartView::RectangleRubberBand); 
    setDragMode(QChartView::ScrollHandDrag); 
    setRenderHint(QPainter::Antialiasing);
    setCursor(Qt::ArrowCursor);

    connect(m_askSeries, &QScatterSeries::clicked, this, &SmilePlot::handleAskClick);
    connect(m_bidSeries, &QScatterSeries::clicked, this, &SmilePlot::handleBidClick);

    connect(m_askSeries, &QScatterSeries::hovered, this, &SmilePlot::handleAskHover);
    connect(m_bidSeries, &QScatterSeries::hovered, this, &SmilePlot::handleBidHover);
}

//----------------------------------------------------------------------------
// Public Slots (No changes to updatePlot signature, change happens inside)
//----------------------------------------------------------------------------
void SmilePlot::updateData(const QVector<QPointF>& strikes,
    const QVector<QPointF>& theoPoints,
    const QVector<QPointF>& askPoints,
    const QVector<QPointF>& bidPoints,
    const QVector<SmilePointData>& pointDetails)
{
    Log.msg(FNAME + QString("Updating plot data. Points received: %1").arg(strikes.size()), Logger::Level::DEBUG);

    m_pointDetails = pointDetails;

    // Basic size check (optional but recommended)
    if (!theoPoints.isEmpty() && m_pointDetails.size() != theoPoints.size()) {
        Log.msg(FNAME + "Warning: Point details size mismatch with Theo series size. Tooltips might be incorrect.", 
            Logger::Level::WARNING);
    }

    // Update Series Data (same as before)
    m_theoSeries->replace(theoPoints);
    m_askSeries->replace(askPoints);
    m_bidSeries->replace(bidPoints);

    // Adjust Axes Ranges (same as before)
    if (!strikes.isEmpty()) {
        if (m_axisX && m_axisY) { /* ... manual bounds calculation and setRange ... */
            double minX = std::numeric_limits<double>::max(), maxX = std::numeric_limits<double>::lowest();
            double minY = 0, maxY = std::numeric_limits<double>::lowest(); bool dataFound = false;
            auto findBounds = [&](const QList<QPointF>& pts) { /* ... find bounds logic ... */
                for (const QPointF& p : pts) { minX = qMin(minX, p.x()); maxX = qMax(maxX, p.x()); maxY = qMax(maxY, p.y()); dataFound = true; }
                };
            findBounds(theoPoints); findBounds(askPoints); findBounds(bidPoints);
            if (dataFound) {
                double xRange = maxX - minX; double yRange = maxY - minY;
                double xPadding = (xRange < 1e-9) ? 1.0 : xRange * 0.05;
                double yPadding = (yRange < 1e-9) ? 0.1 : yRange * 0.1;
                m_axisX->setRange(minX - xPadding, maxX + xPadding); m_axisY->setRange(minY, maxY + yPadding);
            }
            else { m_axisX->setRange(0, 100); m_axisY->setRange(0, 1); }
        }
        else {
            /* Log Warning */
        }
    }
    else { clearPlot(); }
    Log.msg(FNAME + QString("Plot data updated and axes adjusted."), Logger::Level::DEBUG);
}

void SmilePlot::clearPlot()
{
    m_theoSeries->clear(); m_askSeries->clear(); m_bidSeries->clear();
    m_askTooltips.clear(); m_bidTooltips.clear(); // Clear QPoint maps
    if (m_axisX && m_axisY) { m_axisX->setRange(0, 100); m_axisY->setRange(0, 1); }
}

void SmilePlot::resetZoom()
{
    if (m_chart) {
        m_chart->zoomReset();
        // Note: This resets zoom/pan applied via mouse interactions.
        // It does NOT necessarily rescale axes to fit data if the underlying
        // data hasn't changed. If you want to force a rescale AND reset zoom,
        // you might need to call the axis adjustment logic from updatePlot again.
        // However, zoomReset() is usually what users expect from a "Reset Zoom" button.
    }
    else {
        Log.msg(FNAME + QString("Cannot reset zoom: Chart object is null."), Logger::Level::WARNING);
    }
}

//----------------------------------------------------------------------------
// Private Slots (Click Handlers - Use QPoint for lookup)
//----------------------------------------------------------------------------
void SmilePlot::handleAskClick(const QPointF& point) {
    Log.msg(FNAME + QString("Ask IV point clicked - Strike: %1, IV: %2").arg(point.x()).arg(point.y()), Logger::Level::INFO);
    QPoint keyPoint = point.toPoint();
    QString tooltip = m_askTooltips.value(keyPoint, QString("Strike: %1\nAsk IV: %2").arg(point.x()).arg(point.y(), 0, 'f', 4));
    QToolTip::showText(QCursor::pos(), tooltip, this);
    emit pointClicked(AskIV, point.x(), point.y(), mapFromGlobal(QCursor::pos()));
}

void SmilePlot::handleBidClick(const QPointF& point) {
    Log.msg(FNAME + QString("Bid IV point clicked - Strike: %1, IV: %2").arg(point.x()).arg(point.y()), Logger::Level::INFO);
    QPoint keyPoint = point.toPoint();
    QString tooltip = m_bidTooltips.value(keyPoint, QString("Strike: %1\nBid IV: %2").arg(point.x()).arg(point.y(), 0, 'f', 4));
    QToolTip::showText(QCursor::pos(), tooltip, this);
    emit pointClicked(BidIV, point.x(), point.y(), mapFromGlobal(QCursor::pos()));
}

void SmilePlot::handleAskHover(const QPointF& point, bool state) {
    if (state) { // Mouse is hovering OVER the point (or within tolerance)
        //int dataIndex = findDataIndexForPoint(point, m_theoSeries);
        //if (dataIndex >= 0) {
        //    const SmilePointData& pointData = mPointDetails[dataIndex];
        //    QString tooltipText = pointData.formatForTooltip();
        //    QPoint viewPos = m_chart->mapToPosition(point, m_theoSeries);
        //    QPoint globalPos = this->mapToGlobal(viewPos); // Use 'this' (the ChartView)
        //    QToolTip::showText(globalPos, tooltipText, this); // Use 'this'
        //}
        //else {
        //    QToolTip::hideText(); // Hide if index invalid
        //}
        QToolTip::showText(QCursor::pos(), "GG Ask", this);
    }
    else { // Mouse left the point
        QToolTip::hideText(); // Hide the tooltip
    }
}

void SmilePlot::handleBidHover(const QPointF& point, bool state) {
    if (state) { // Mouse is hovering OVER the point (or within tolerance)
        //int dataIndex = findDataIndexForPoint(point, m_theoSeries);
        //if (dataIndex >= 0) {
        //    const SmilePointData& pointData = mPointDetails[dataIndex];
        //    QString tooltipText = pointData.formatForTooltip();
        //    QPoint viewPos = m_chart->mapToPosition(point, m_theoSeries);
        //    QPoint globalPos = this->mapToGlobal(viewPos); // Use 'this' (the ChartView)
        //    QToolTip::showText(globalPos, tooltipText, this); // Use 'this'
        //}
        //else {
        //    QToolTip::hideText(); // Hide if index invalid
        //}
        QToolTip::showText(QCursor::pos(), "GG Bid", this);
    }
    else { // Mouse left the point
        QToolTip::hideText(); // Hide the tooltip
    }
}

//----------------------------------------------------------------------------
// Private Helpers (createPoints is same, mapTooltips adapts)
//----------------------------------------------------------------------------
QList<QPointF> SmilePlot::createPoints(const QVector<double>& xData, const QVector<double>& yData)
{
    // --- This helper remains the same, returns QList<QPointF> ---
    QList<QPointF> points;
    int size = qMin(xData.size(), yData.size()); points.reserve(size);
    for (int i = 0; i < size; ++i) {
        if (!std::isnan(xData[i]) && !std::isinf(xData[i]) && !std::isnan(yData[i]) && !std::isinf(yData[i])) {
            points.append(QPointF(xData[i], yData[i]));
        }
        else { /* Log Debug */ }
    }
    return points;
}

// --- mapTooltips now uses QMap<QPoint, QString> ---
void SmilePlot::mapTooltips(const QList<QPointF>& points, const QStringList& tooltips, QHash<QPoint, QString>& outTooltipMap) // <<< Map type changed
{
    outTooltipMap.clear();
    int size = qMin(points.size(), tooltips.size());
    if (points.size() != tooltips.size()) { /* Log Warning */ }

    for (int i = 0; i < size; ++i) {
        // --- Insert using the converted QPoint ---
        outTooltipMap.insert(points.at(i).toPoint(), tooltips.at(i)); // <<< Convert to QPoint key here
    }
}

QChartView* SmilePlot::chartView() {
    return reinterpret_cast<QChartView*>(this);
}


SmilePlot::PlotMode SmilePlot::currentMode() const
{
    return m_CurrentMode;
}

void SmilePlot::setCurrentMode(SmilePlot::PlotMode mode)
{
    m_CurrentMode = mode;
}

// --- Reimplemented Mouse Event Handlers ---

void SmilePlot::mousePressEvent(QMouseEvent* event) {
    if (m_CurrentMode == pmPan && event->button() == Qt::LeftButton) {
        m_panLastPos = event->pos(); // Record position relative to this widget
        m_isPanning = true;
        this->setCursor(Qt::CrossCursor);
        event->accept(); // Indicate we handled this
    }
    else if (m_CurrentMode == pmZoom) {
        // Let QChartView handle the start of rubber band selection
        QChartView::mousePressEvent(event);
    }
    else {
        // Pass event to base class if not panning/zooming
        QChartView::mousePressEvent(event);
    }
}

void SmilePlot::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) { // Check if we are in a panning drag
        // Calculate delta and scroll the chart's axes
        QPoint delta = event->pos() - m_panLastPos;
        m_chart->scroll(-delta.x(), delta.y());
        m_panLastPos = event->pos(); // Update position for next move
        event->accept(); // Indicate we handled this
    }
    else if (m_CurrentMode == pmZoom) {
        // Let QChartView handle drawing the rubber band
        QChartView::mouseMoveEvent(event);
    }
    else {
        QChartView::mouseMoveEvent(event);
    }
}

void SmilePlot::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isPanning && event->button() == Qt::LeftButton) {
        m_isPanning = false;
        this->setCursor(Qt::CrossCursor); // Reset cursor for pan mode
        event->accept(); // Indicate we handled this
    }
    else if (m_CurrentMode == pmZoom && event->button() == Qt::LeftButton) {
        // Zoom logic is handled by QChartView's rubber band selection
        // The base class implementation likely handles the zoomIn call
        // based on the rubberBandRect when the button is released.
        QChartView::mouseReleaseEvent(event); // IMPORTANT: Call base class for zoom!
        // Reset cursor after zoom attempt
        this->setCursor(Qt::CrossCursor);
    }
    else {
        QChartView::mouseReleaseEvent(event);
    }
}

void SmilePlot::wheelEvent(QWheelEvent* event) {
    // Get the angle delta to determine direction and magnitude
    // Positive delta usually means scrolling UP (zoom in), negative means DOWN (zoom out)
    const qreal delta = event->angleDelta().y(); // Use vertical delta

    if (qFuzzyIsNull(delta)) { // Check if delta is effectively zero
        event->accept(); // Accept the event but do nothing
        return;
    }

    // Define zoom factor (adjust sensitivity as needed)
    const qreal factor = 1.15; // Zoom factor > 1 for zooming in

    if (delta > 0) {
        // Zoom In
        Log.msg(FNAME + "Wheel Zoom In", Logger::Level::DEBUG);
        m_chart->zoom(factor); // Zoom the chart by the factor (centered)
    }
    else {
        // Zoom Out
        Log.msg(FNAME + "Wheel Zoom Out", Logger::Level::DEBUG);
        m_chart->zoom(1.0 / factor); // Zoom out by the inverse factor
    }

    event->accept(); // Indicate we handled the wheel event
}

// --- End Mouse Event Handlers ---
