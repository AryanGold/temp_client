#pragma once

#include <QMainWindow>
#include <QSettings>
#include <QString>

class QCloseEvent;
class WindowManager;

class BaseWindow : public QMainWindow
{
    Q_OBJECT
public:
    BaseWindow(const QString& windowName, WindowManager* windowManager, QWidget* parent = nullptr);
    virtual ~BaseWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void loadSettings();
    void saveSettings();

    QString windowName;
    WindowManager* m_windowManager = nullptr;
};
