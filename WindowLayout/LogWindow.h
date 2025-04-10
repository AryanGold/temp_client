#pragma once

#include "BaseWindow.h"

class QPlainTextEdit;

class LogWindow : public BaseWindow
{
    Q_OBJECT
public:
    LogWindow(WindowManager* windowManager, QWidget* parent = nullptr);

    QPlainTextEdit* logWidget;
};
