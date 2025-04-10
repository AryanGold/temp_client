#include "BaseWindow.h"
#include "WindowManager.h"
#include "../Defines.h"
#include "Glob/Logger.h"

#include <QSettings>
#include <QCloseEvent>

BaseWindow::BaseWindow(const QString& windowName, WindowManager* windowManager, QWidget* parent)
    : QMainWindow(parent), windowName(windowName), m_windowManager(windowManager)
{
    setObjectName(windowName);

    setWindowTitle(windowName);
    //setWindowFlags(Qt::Window);
    loadSettings();
}

BaseWindow::~BaseWindow()
{
}

void BaseWindow::loadSettings()
{
    // Only load geometry saved specifically for this window instance
    QSettings settings;
    QString settingsGroup = "WindowState/" + objectName(); // Use a unique group prefix

    qDebug() << "BaseWindow::loadSettings for" << objectName() << "from group" << settingsGroup;

    // Check if the specific group for this window exists
    QStringList pathSegments = settingsGroup.split('/');
    bool groupExists = true;
    QString currentPath;
    for (const QString& segment : pathSegments) {
        if (currentPath.isEmpty()) {
            if (!settings.childGroups().contains(segment)) {
                groupExists = false;
                break;
            }
            currentPath = segment;
        }
        else {
            settings.beginGroup(currentPath);
            if (!settings.childGroups().contains(segment)) {
                groupExists = false;
                settings.endGroup();
                break;
            }
            settings.endGroup(); // End temporary group
            currentPath += "/" + segment;
        }
        if (!groupExists) break; // Exit loop early if path segment not found
    }


    if (groupExists) {
        settings.beginGroup(settingsGroup);
        QByteArray geometry = settings.value("geometry").toByteArray();
        if (!geometry.isEmpty()) {
            // --- Defer restoreGeometry if called from constructor? ---
            // Sometimes restoring after show() is better. For simplicity now, try here.
            // If issues persist, consider moving this restore call to *after*
            // the window is shown in main.cpp or WindowManager::restoreWindowStates
            if (!restoreGeometry(geometry)) {
                qWarning() << "Failed to restore geometry for" << objectName() << "during loadSettings.";
            }
            else {
                qDebug() << "Restored geometry for" << objectName() << "during loadSettings.";
            }
        }
        else {
            qWarning() << "No geometry found in settings for" << objectName();
            // resize(800, 600); // Optional default size
        }
        settings.endGroup();
    }
    else {
        qWarning() << "Settings group" << settingsGroup << "not found for" << objectName();
        // resize(800, 600); // Optional default size
    }
}

void BaseWindow::saveSettings()
{
    // Save geometry specifically for this window instance when it's closed/moved
    QSettings settings;
    QString settingsGroup = "WindowState/" + objectName(); // Use the same unique group prefix

    qDebug() << "BaseWindow::saveSettings for" << objectName() << "to group" << settingsGroup;

    settings.beginGroup(settingsGroup);
    settings.setValue("geometry", saveGeometry());
    // Don't save isVisible here, let WindowManager::saveWindowStates handle that
    settings.endGroup();
}

void BaseWindow::closeEvent(QCloseEvent* event)
{
    // Call WindowManager to set the closing flag
    m_windowManager->setWindowClosing(true);

    // Accept the event to allow closing
    QMainWindow::closeEvent(event);
}
