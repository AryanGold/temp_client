#include "LogWindow.h"

#include <QPlainTextEdit>

LogWindow::LogWindow(WindowManager* windowManager, QWidget* parent)
    : BaseWindow("Logs", windowManager, parent)
{
    logWidget = new QPlainTextEdit(this);
    logWidget->setReadOnly(true);

    setCentralWidget(logWidget);
}
