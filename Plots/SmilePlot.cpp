#include "SmilePlot.h"
#include "Glob/Logger.h"

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
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
    m_chart = chart();
    setupChart();
    Log.msg(QString("Widget created using Qt Charts."), Logger::Level::DEBUG);
}

void SmilePlot::setupChart()
{
    // --- This setup logic remains the same ---
    m_chart->setTitle("Implied Volatility Smile");
    m_chart->legend()->setVisible(true); m_chart->legend()->setAlignment(Qt::AlignBottom);
    m_theoSeries = new QLineSeries(m_chart); m_askSeries = new QScatterSeries(m_chart); m_bidSeries = new QScatterSeries(m_chart);
    m_theoSeries->setName("Theo IVs"); m_theoSeries->setColor(Qt::darkGreen); m_theoSeries->setPen(QPen(Qt::darkGreen, 2));
    m_askSeries->setName("Ask IV"); m_askSeries->setColor(Qt::blue); m_askSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle); m_askSeries->setMarkerSize(7.0); m_askSeries->setBorderColor(Qt::transparent);
    m_bidSeries->setName("Bid IV"); m_bidSeries->setColor(Qt::red); m_bidSeries->setMarkerShape(QScatterSeries::MarkerShapeRectangle); m_bidSeries->setMarkerSize(7.0); m_bidSeries->setBorderColor(Qt::transparent);
    m_chart->addSeries(m_theoSeries); m_chart->addSeries(m_askSeries); m_chart->addSeries(m_bidSeries);
    m_chart->createDefaultAxes();
    m_axisX = qobject_cast<QValueAxis*>(m_chart->axes(Qt::Horizontal, m_theoSeries).first());
    m_axisY = qobject_cast<QValueAxis*>(m_chart->axes(Qt::Vertical, m_theoSeries).first());
    if (m_axisX && m_axisY) { // Axis configuration (same as before)
        m_axisX->setTitleText("Strike"); m_axisY->setTitleText("Implied Volatility (IV)");
        m_axisY->setLabelFormat("%.4f"); m_axisX->setLabelFormat("%.2f");
        m_axisY->setMin(0); m_axisX->setTickCount(11); m_axisY->setTickCount(6);
        m_axisX->setGridLineVisible(true); m_axisY->setGridLineVisible(true); m_axisX->setMinorGridLineVisible(true); m_axisY->setMinorGridLineVisible(true);
        QPen gridPen(QColor(220, 220, 220), 1, Qt::DotLine); QPen minorGridPen(QColor(240, 240, 240), 1, Qt::DotLine);
        m_axisX->setGridLinePen(gridPen); m_axisY->setGridLinePen(gridPen); m_axisX->setMinorGridLinePen(minorGridPen); m_axisY->setMinorGridLinePen(minorGridPen);
    }
    else { /* Log Error */ }
    setRubberBand(QChartView::RectangleRubberBand); setDragMode(QChartView::ScrollHandDrag); setRenderHint(QPainter::Antialiasing);
    connect(m_askSeries, &QScatterSeries::clicked, this, &SmilePlot::handleAskClick);
    connect(m_bidSeries, &QScatterSeries::clicked, this, &SmilePlot::handleBidClick);
}

//----------------------------------------------------------------------------
// Public Slots (No changes to updatePlot signature, change happens inside)
//----------------------------------------------------------------------------
void SmilePlot::updatePlot(const QVector<double>& strikes,
    const QVector<double>& theoIvs,
    const QVector<double>& askIvs,
    const QVector<double>& bidIvs,
    const QStringList& tooltips)
{
    Log.msg(FNAME + QString("Updating plot data. Points received: %1").arg(strikes.size()), Logger::Level::DEBUG);

    // Data Validation (same as before)
    if (strikes.size() != theoIvs.size() || strikes.size() != askIvs.size() ||
        strikes.size() != bidIvs.size() || strikes.size() != tooltips.size())
    { /* Log Error */ clearPlot(); return;
    }

    // Create QPointF lists for plotting (same as before)
    QList<QPointF> theoPoints = createPoints(strikes, theoIvs);
    QList<QPointF> askPoints = createPoints(strikes, askIvs);
    QList<QPointF> bidPoints = createPoints(strikes, bidIvs);

    // --- Map tooltips using QPoint keys ---
    mapTooltips(askPoints, tooltips, m_askTooltips); // Pass the QMap<QPoint, QString>
    mapTooltips(bidPoints, tooltips, m_bidTooltips); // Pass the QMap<QPoint, QString>

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
