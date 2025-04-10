#pragma once

#include <QObject>
#include <QWidget>
#include <QList>
#include <QSettings>
#include <QPointer>

class QMainWindow;

// Manage windows active state etc
// Responsible for store/restore latest active window.
class WindowManager : public QObject
{
    Q_OBJECT
public:
    explicit WindowManager(QObject* parent = nullptr);

    ////////////////////
    // Main windows fucntional
    void registerWindow(QMainWindow* window, const QString& name);
    void showAllWindows();
    ////////////////////

    ////////////////////
    // Dynamic windows fucntional
    enum class WindowType {
        QuoteChartWindow,
        TakesPageWindow
    };

    QMainWindow* createNewDynamicWindow(const QString& objectId, const QString& wType);
    void restoreWindowStates();
    void setWindowClosing(bool closing);

    void applyDefaultWindowStates();
    void applyDefaultGeometry(const QString& windowName, int wPix, int hPix, int wPerc, int hPerc, const QString& position);

public slots:
    void saveWindowStates();
    void handleDynamicWindowDestroyed(QObject* obj);
    ////////////////////

    void handleFocusChanged(QWidget* oldFocus, QWidget* newFocus);

private:
    QSettings settings;

    ////////////////////
    // Main windows fucntional
    QMap<QString, QPointer<QMainWindow>> m_mainWindows;

    bool isWindowManaged(const QMainWindow* window) const;
    ////////////////////

    ////////////////////
    // Dynamic windows fucntional
    QList<QMainWindow*> m_dynamicWindows;
    ////////////////////

    bool m_isWindowClosing = false; // Flag to indicate a close is in progres
};
