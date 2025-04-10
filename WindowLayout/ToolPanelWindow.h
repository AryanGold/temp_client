#pragma once

#include "BaseWindow.h"

class WindowManager;

class ToolPanelWindow : public BaseWindow
{
    Q_OBJECT
public:
    ToolPanelWindow(WindowManager* manager, QWidget* parent = nullptr);

private slots:
    void buttonClicked();
    void showAllWindows();
    void openChartWindow();
    void openTakesWindow();
    void exitApp();

private:
    WindowManager* windowManager = nullptr;
};
