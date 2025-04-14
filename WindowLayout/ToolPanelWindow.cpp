#include "Glob/Glob.h"
#include "Glob/Logger.h"
#include "ToolPanelWindow.h"
#include "WindowManager.h"

#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QMessageBox>
#include <QSpacerItem>
#include <QFrame>
#include <QApplication>
#include <QIcon>

ToolPanelWindow::ToolPanelWindow(WindowManager* manager, QWidget* parent)
    : BaseWindow(NAME_PROGRAM_FULL, windowManager, parent)
{
    windowManager = manager;

    // Central Widget setup
    auto centralWidget = new QWidget(this);
    auto layout = new QHBoxLayout(centralWidget);

    // Button properties
    const int buttonSize = 30;

    layout->addStretch();

    auto showAllButton = new QPushButton(this);
    showAllButton->setIcon(QIcon(":/icons/resources/icons/buttons/show_all.png"));
    showAllButton->setIconSize(QSize(buttonSize - 10, buttonSize - 10));
    showAllButton->setFixedSize(buttonSize, buttonSize);
    layout->addWidget(showAllButton);
    connect(showAllButton, &QPushButton::clicked, this, &ToolPanelWindow::showAllWindows);

    auto openChartButton = new QPushButton(this);
    openChartButton->setIcon(QIcon(":/icons/resources/icons/buttons/new_chart.png"));
    openChartButton->setIconSize(QSize(buttonSize - 10, buttonSize - 10));
    openChartButton->setFixedSize(buttonSize, buttonSize);
    layout->addWidget(openChartButton);
    connect(openChartButton, &QPushButton::clicked, this, &ToolPanelWindow::openChartWindow);

    auto openTakesButton = new QPushButton(this);
    openTakesButton->setIcon(QIcon(":/icons/resources/icons/buttons/add_table.png"));
    openTakesButton->setIconSize(QSize(buttonSize - 10, buttonSize - 10));
    openTakesButton->setFixedSize(buttonSize, buttonSize);
    layout->addWidget(openTakesButton);
    connect(openTakesButton, &QPushButton::clicked, this, &ToolPanelWindow::openTakesWindow);

    // Add vertical separator
    {
        auto separator = new QFrame(this);
        separator->setFrameShape(QFrame::VLine);
        separator->setFrameShadow(QFrame::Sunken);
        separator->setLineWidth(1);
        layout->addWidget(separator);
    }

    // Reload Button
    auto reloadButton = new QPushButton(this);
    reloadButton->setIcon(QIcon(":/icons/resources/icons/buttons/reload_big.png"));
    reloadButton->setIconSize(QSize(buttonSize - 10, buttonSize - 10));
    reloadButton->setFixedSize(buttonSize, buttonSize);
    layout->addWidget(reloadButton);
    connect(reloadButton, &QPushButton::clicked, this, &ToolPanelWindow::buttonClicked);

    // Settings Button
    auto settingsbutton = new QPushButton(this);
    settingsbutton->setIcon(QIcon(":/icons/resources/icons/buttons/settings.png"));
    settingsbutton->setIconSize(QSize(buttonSize - 10, buttonSize - 10));
    settingsbutton->setFixedSize(buttonSize, buttonSize);
    layout->addWidget(settingsbutton);
    connect(settingsbutton, &QPushButton::clicked, this, &ToolPanelWindow::buttonClicked);

    // Add vertical separator
    {
        auto separator = new QFrame(this);
        separator->setFrameShape(QFrame::VLine);
        separator->setFrameShadow(QFrame::Sunken);
        separator->setLineWidth(1);
        layout->addWidget(separator);
    }

    layout->addStretch();

    // Exit Button
    auto exitButton = new QPushButton(this);
    exitButton->setFixedSize(buttonSize, buttonSize);
    exitButton->setIcon(QIcon(":/icons/resources/icons/buttons/exit.png"));
    exitButton->setIconSize(QSize(buttonSize - 10, buttonSize - 10));
    layout->addWidget(exitButton);

    // Connect Exit button to close the application
    connect(exitButton, &QPushButton::clicked, this, &ToolPanelWindow::exitApp);

    setCentralWidget(centralWidget);
}

void ToolPanelWindow::buttonClicked() // SLOT
{
    QMessageBox::information(this, "Info", "Button clicked!");
}

void ToolPanelWindow::showAllWindows() // SLOT
{
    windowManager->showAllWindows();
}

void ToolPanelWindow::openChartWindow() // SLOT
{
    windowManager->createNewDynamicWindow("", "QuoteChartWindow");
}

void ToolPanelWindow::openTakesWindow() // SLOT
{
    windowManager->createNewDynamicWindow("", "TakesPageWindow");
}

void ToolPanelWindow::exitApp() // SLOT
{
    windowManager->saveWindowStates();

    Log.msg("App Exit");
    Log.closeLogger();

    qApp->quit();
}
