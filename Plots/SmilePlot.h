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
#include <QPointer>

#include "SmilePointData.h"

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
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

signals:
    void pointClicked(const SmilePointData& pointData);

public slots:
    void updateData(const QVector<QPointF>& strikes,
        const QVector<QPointF>& theoIvs,
        const QVector<QPointF>& askIvs,
        const QVector<QPointF>& bidIvs,
        const QVector<SmilePointData>& pointDetails);
    void clearPlot();
    void resetZoom();

private slots:
    //void handleAskClick(const QPointF& point);
    //void handleBidClick(const QPointF& point);

    void handleAskHover(const QPointF& point, bool state);
    void handleBidHover(const QPointF& point, bool state);

private:
    QChart* m_chart = nullptr;
    QLineSeries* m_theoSeries = nullptr;
    QScatterSeries* m_askSeries = nullptr;
    QScatterSeries* m_bidSeries = nullptr;
    QValueAxis* m_axisX = nullptr;
    QValueAxis* m_axisY = nullptr;

    void setupChart();

    PlotMode m_CurrentMode;
    // Panning State
    bool m_isPanning = false;
    QPoint m_panLastPos;

    // Stores details for the currently plotted points
    QVector<SmilePointData> m_pointDetails;

    // --- Hover/Click State ---
    QPointer<QAbstractSeries> m_hoveredSeries = nullptr; // Store pointer to hovered series
    int mHoveredDataIndex = -1; // Store index of the point within mPointDetails
    // --- End Hover/Click State ---

    int findDataIndexForPoint(const QPointF& seriesPoint, QAbstractSeries* series) const;
    // Helper to show tooltip (can be called by hover handlers)
    void showPointTooltip(int dataIndex, const QPoint& globalPos);
};