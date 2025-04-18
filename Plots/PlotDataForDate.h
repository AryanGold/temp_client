#pragma once

#include <QVector>
#include <QPointF>
#include "SmilePointData.h"

struct PlotDataForDate {
    QVector<QPointF> theoPoints;
    QVector<QPointF> midPoints;
    QVector<QPointF> bidPoints;
    QVector<QPointF> askPoints;
    QVector<SmilePointData> pointDetails; // Matching details for points
};
