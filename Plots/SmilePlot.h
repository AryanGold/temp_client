#pragma once

#include <QtCharts/QChartView>
#include <QtCharts/qchartglobal.h>

//QT_CHARTS_BEGIN_NAMESPACE
class QChartView;
class QChart;
class QLineSeries;
class QScatterSeries;
class QValueAxis;
//QT_CHARTS_END_NAMESPACE

#include <QVector>
#include <QStringList>
#include <QHash>
#include <QPoint>
#include <QPointF> // Needed for QMap key
#include <QMouseEvent>
#include <QWheelEvent>

class SmilePlot : public QChartView
{
    Q_OBJECT

public:
    explicit SmilePlot(QWidget* parent = nullptr);
    ~SmilePlot() override = default;

    enum ScatterType { AskIV, BidIV };
    Q_ENUM(ScatterType)

        // Enum for interaction modes
    enum PlotMode {
        pmPan, // Drag to move
        pmZoom // Drag to zoom region
    };
    Q_ENUM(PlotMode)

    QChartView* chartView();
    PlotMode currentMode() const;
    void setCurrentMode(PlotMode);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

signals:
    // Signal still uses QPointF for precise data coordinates
    void pointClicked(SmilePlot::ScatterType type, double strike, double iv, QPointF screenPos);

public slots:
    void updatePlot(const QVector<double>& strikes,
        const QVector<double>& theoIvs,
        const QVector<double>& askIvs,
        const QVector<double>& bidIvs,
        const QStringList& tooltips);
    void clearPlot();
    void resetZoom();

private slots:
    void handleAskClick(const QPointF& point); // Slot receives precise QPointF
    void handleBidClick(const QPointF& point);

private:
    QChart* m_chart = nullptr;
    QLineSeries* m_theoSeries = nullptr;
    QScatterSeries* m_askSeries = nullptr;
    QScatterSeries* m_bidSeries = nullptr;
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;

    // --- Store tooltips mapped to INTEGER point coordinates ---
    QHash<QPoint, QString> m_askTooltips; // <<< Key is now QPoint
    QHash<QPoint, QString> m_bidTooltips; // <<< Key is now QPoint

    void setupChart();
    // Helpers still work with QPointF internally for plotting
    QList<QPointF> createPoints(const QVector<double>& xData, const QVector<double>& yData);
    // Map tooltip helper needs adapting
    void mapTooltips(const QList<QPointF>& points, const QStringList& tooltips, QHash<QPoint, QString>& outTooltipMap); 

    PlotMode m_CurrentMode;
    // Panning State
    bool m_isPanning = false;
    QPoint m_panLastPos;
};