#include "TakesPageWindow.h"
#include <QTableWidget>
#include <QHeaderView>

TakesPageWindow::TakesPageWindow(WindowManager* windowManager, QWidget* parent)
    : BaseWindow("TakesPage", windowManager, parent)
{
    tableWidget = new QTableWidget(5, 7, this); // Minimal example: 5 rows, 3 columns
    tableWidget->setHorizontalHeaderLabels({ "tick", "expo_date", "strikes", "tt", "cp", "BS", "bid" });
    tableWidget->horizontalHeader()->setStretchLastSection(true);

    setCentralWidget(tableWidget);
}
