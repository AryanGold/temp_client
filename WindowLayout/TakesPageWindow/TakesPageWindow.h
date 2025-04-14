#pragma once

#include "../BaseWindow.h"

class QTableWidget;

class TakesPageWindow : public BaseWindow
{
    Q_OBJECT
public:
    TakesPageWindow(WindowManager* windowManager, QWidget* parent = nullptr);

private:
    QTableWidget* tableWidget;
};
