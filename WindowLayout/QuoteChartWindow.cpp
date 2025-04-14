
#include "QuoteChartWindow.h"
#include <QLabel>
#include <QWidget>
#include <QVBoxLayout>

QuoteChartWindow::QuoteChartWindow(WindowManager* windowManager, QWidget* parent)
    : BaseWindow("QuoteChart", windowManager, parent)
{
    auto label = new QLabel("This is a QuoteChart window.", this);
    label->setAlignment(Qt::AlignCenter);

    auto centralWidget = new QWidget(this);
    auto layout = new QVBoxLayout(centralWidget);
    layout->addWidget(label);

    setCentralWidget(centralWidget);
}
