
#pragma once

#include "BaseWindow.h"

class QuoteChartWindow : public BaseWindow
{
    Q_OBJECT
public:
    QuoteChartWindow(WindowManager* windowManager, QWidget* parent = nullptr);
};
