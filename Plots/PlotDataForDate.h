#pragma once

#include <QVector>
#include <QPointF>
#include "SmilePlotData.h"

struct PlotDataForDate {
    QVector<QPointF> theoPoints;
    QVector<QPointF> midPoints;
    QVector<QPointF> bidPoints;
    QVector<QPointF> askPoints;
    QVector<SmilePlotData> pointDetails; // Matching details for points
};
