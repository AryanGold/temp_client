#include "Glob/Logger.h"
#include "../Defines.h"
#include "WindowManager.h"
#include "WindowLayout/TakesPageWindow/TakesPageWindow.h"
#include "WindowLayout/QuoteChartWindow.h"

#include <QMainWindow>
#include <Qevent.h>
#include <QUuid>
#include <QVariant>
#include <QScreen>
#include <QRect> 
#include <QGuiApplication> // For QGuiApplication::primaryScreen()
#include <QStyle> 
#include <QDebug> 
#include <set> 

const QString FIRST_RUN_KEY = "Application/FirstRunCompleted"; // Key to check first run

WindowManager::WindowManager(QObject* parent)
    : QObject(parent), settings(NAME_COMPANY, NAME_PROGRAM_FULL)
{
}

/////////////////////////////////////////////////////////////////////////////
// Main windows functional

// Registers a pre-existing main window with the WindowManager.
void WindowManager::registerWindow(QMainWindow* window, const QString& name) {
    if (!window) {
        Log.msg(FNAME + "Attempted to register a null window pointer.", Logger::Level::WARNING);
        return;
    }

    if (name.isEmpty()) {
        Log.msg(FNAME + "Attempted to register a window without a name.", Logger::Level::WARNING);
        return;
    }

    // Assign the object name if it's not already set or different
    // This is crucial for QSettings keys
    if (window->objectName().isEmpty() || window->objectName() != name) {
        window->setObjectName(name);
    }

    // Check if already registered to avoid duplicates
    if (m_mainWindows.contains(name)) {
        if (m_mainWindows[name] == window) {
            Log.msg(FNAME + "Window '" + name + "' is already registered.", Logger::Level::DEBUG);
        }
        else {
            // Name collision! A different window instance is already registered with this name.
            Log.msg(FNAME + "Cannot register window. Another window with the name '" + name + "' already exists!", 
                Logger::Level::ERROR);
        }
        return; // Don't register again
    }

    // Store a QPointer in the main windows map
    m_mainWindows[name] = QPointer<QMainWindow>(window);

    // Do NOT connect the destroyed signal here for main windows unless you have
    // specific logic to handle a main window being unexpectedly destroyed.
    // Main windows are usually expected to live for the duration of the application.
    // Dynamic windows handle their destruction via handleDynamicWindowDestroyed.
}

// When user click on any window of this app then we show (unhide all other windows).
// For avoid click on each window for show whole app if it hidden.
void WindowManager::handleFocusChanged(QWidget* oldFocus, QWidget* newFocus) {
    Q_UNUSED(oldFocus); // We only care about the new focus

    if (m_isWindowClosing) { 
        return; // Don't proceed if a close operation is happening
    }

    if (!newFocus) {
        // Focus left the application, do nothing regarding showing windows
        return;
    }

    // Find the top-level window containing the newly focused widget
    QWidget* topLevelWidget = newFocus->window();
    QMainWindow* activatedWindow = qobject_cast<QMainWindow*>(topLevelWidget);

    // --- Temporary Logging ---
    QString topLevelClassName = topLevelWidget ? topLevelWidget->metaObject()->className() : "None";
    QString topLevelObjName = topLevelWidget ? topLevelWidget->objectName() : "N/A";
    qDebug() << "Focus changed. newFocus:" << newFocus->metaObject()->className() << "(" << newFocus->objectName() << ")"
        << " | topLevel:" << topLevelClassName << "(" << topLevelObjName << ")";
    QString msg = QString("Focus changed. newFocus: %1 (%2) | topLevel: %3(%4)")
        .arg(newFocus->metaObject()->className())
        .arg(newFocus->objectName())
        .arg(topLevelClassName)
        .arg(topLevelObjName);
    Log.msg(FNAME + msg, Logger::Level::DEBUG);
    // --- End Temporary Logging ---

    if (activatedWindow) {
        // Check if the activated window IS the ToolPanelWindow ---
        if (activatedWindow->objectName() == "ToolPanel") {
            showAllWindows(); // Trigger show all ONLY for ToolPanelWindow
        }
        // another managed window activated
        else if (isWindowManaged(activatedWindow)) {
            // DO NOTHING here regarding showing other windows
        }
        // unmanaged window activated e.g., a dialog, or external?
        else {
            Log.msg(FNAME + "Focus changed to unmanaged window: " + topLevelWidget->objectName(), Logger::Level::DEBUG);
        }
    }
}

void WindowManager::showAllWindows() {
    if (m_isWindowClosing) { 
        // Skipping showAllWindows during window close.
        return;
    }

    int shownCount = 0;

    // Use a set to avoid processing the same window twice if it's somehow
    // in both lists (shouldn't happen with proper registration)
    std::set<QMainWindow*> windowsToShow;

    // Collect windows from main list
    for (auto it = m_mainWindows.constBegin(); it != m_mainWindows.constEnd(); ++it) {
        QMainWindow* window = it.value();
        if (window) { // Check QPointer validity
            windowsToShow.insert(window);
        }
    }

    // Collect windows from dynamic list
    for (QMainWindow* window : std::as_const(m_dynamicWindows)) {
        if (window) { // Check pointer validity
            windowsToShow.insert(window);
        }
    }

    // Iterate through the unique set and show hidden ones
    for (QMainWindow* window : windowsToShow) {
        if (window) {// && !window->isActiveWindow()) { // && !window->isVisible()
            window->show();
            // Optional: Bring it more forcefully to the user's attention
            window->raise(); // Adjust stacking order within the application
            // window->activateWindow(); // Request focus from the OS
            shownCount++;
        }
    }
    if (shownCount > 0) {
        Log.msg(FNAME + "Shown " + QString::number(shownCount) + " hidden windows.", Logger::Level::DEBUG);
    }
}


// NEW: Helper implementation
bool WindowManager::isWindowManaged(const QMainWindow* window) const {
    if (!window) return false;

    // Check main windows map (using QPointer comparison is tricky, compare actual pointers)
    for (auto it = m_mainWindows.constBegin(); it != m_mainWindows.constEnd(); ++it) {
        if (it.value() == window) { // Compare the raw pointers
            return true;
        }
    }

    // Check dynamic windows list
    if (m_dynamicWindows.contains(const_cast<QMainWindow*>(window))) { // contains takes non-const
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// Dynamic windows fucntional

// Creates a new TakesPageWindow instance, assigns ID, connects signals, shows it.
QMainWindow* WindowManager::createNewDynamicWindow(const QString& objectId, const QString& wType) {
    QString msg = QString("Create dynamic window: objectId[%1], wType[%2]").arg(objectId).arg(wType);
    Log.msg(FNAME + msg, Logger::Level::DEBUG);

    QMainWindow* window = nullptr;
    QString uniqueId = objectId; // Assign a unique object name for saving/restoring geometry
    QString title;

    // Also update in: restoreWindowStates()
    if (wType == "QuoteChartWindow") {
        window = new QuoteChartWindow(this, nullptr);
        title = "Plot chart";
    }
    else if (wType == "TakesPageWindow") {
        window = new TakesPageWindow(this, nullptr);
        title = "Takes";
    }
    else {
        Log.msg(FNAME + "Undefined Window type:" + wType, Logger::Level::ERROR);
        return nullptr;
    }

    if (uniqueId.isEmpty()) {
        uniqueId = QString("%1_%2").arg(title, QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    window->setObjectName(uniqueId);
    window->setWindowTitle(title);

    if (window) {
        window->setAttribute(Qt::WA_DeleteOnClose); // Ensure attribute is set

        ///connect(window, &QObject::destroyed, this, &WindowManager::handleDynamicWindowDestroyed);
        m_dynamicWindows.append(window);

        // Load settings might have happened in constructor.
        // Now show it. Geometry restore might be better after show.
        // If BaseWindow::loadSettings struggles, move the restoreGeometry call
        // from there to *here* or into WindowManager::restoreWindowStates.
        window->show();

        Log.msg(FNAME + "Created and tracking window:" + window->objectName(), Logger::Level::DEBUG);
    }
    else {
        Log.msg(FNAME + "Failed to create window instance for type:" + wType, Logger::Level::ERROR);
    }
 
    return window; // Return pointer if needed elsewhere
}

// Slot/Function called when a tracked window is closed and destroyed
/*void WindowManager::handleDynamicWindowDestroyed(QObject* obj) {
    Log.msg(FNAME + "G1", Logger::Level::DEBUG);
    QMainWindow* window = qobject_cast<QMainWindow*>(obj);
    if (window) {
        Log.msg(FNAME + "G2: " + QString::number(m_dynamicWindows.count()), Logger::Level::DEBUG);
        bool removed = m_dynamicWindows.removeOne(window);
        Log.msg(FNAME + "G3: " + QString::number(m_dynamicWindows.count()), Logger::Level::DEBUG);
        if (removed) {
            Log.msg(FNAME + "Stopped tracking window:" + window->objectName(), Logger::Level::DEBUG);
        }
        else {
            Log.msg(FNAME + "Attempted to stop tracking untracked window:" + window->objectName(), Logger::Level::WARNING);
        }
        // No need to call saveWindowStates here, rely on aboutToQuit
    }
    else {
        Log.msg(FNAME + "Unable to destroy dynamic window", Logger::Level::ERROR);
    }
}*/

// WindowManager.cpp

void WindowManager::handleDynamicWindowDestroyed(QObject* obj /* Optional */) {
    Log.msg(FNAME + "G1 - Slot Triggered [LOGIC DISABLED]", Logger::Level::DEBUG);
    /*
    Log.msg(FNAME + "G1 - Slot Triggered", Logger::Level::DEBUG);
    QObject* senderObj = sender();
    if (!senderObj) {
        Log.msg(FNAME + "Signal received, but sender() returned null!", Logger::Level::ERROR);
        return;
    }

    // --- Get sender info ---
    QString senderClassName = senderObj->metaObject()->className();
    QString senderObjectName = senderObj->objectName();
    quintptr senderPtrAddr = reinterpret_cast<quintptr>(senderObj);
    Log.msg(FNAME + "Sender Info - Type: " + senderClassName + ", Name: " + senderObjectName + ", Addr: " + QString("0x%1").arg(senderPtrAddr, 0, 16), Logger::Level::DEBUG);


    // --- Attempt direct cast FIRST (might work sometimes) ---
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(senderObj);
    if (mainWindow) {
        Log.msg(FNAME + "Sender IS QMainWindow. Removing directly.", Logger::Level::DEBUG);
        bool removed = m_dynamicWindows.removeOne(mainWindow);
        Log.msg(FNAME + "After direct removal attempt. Dynamic count: " + QString::number(m_dynamicWindows.count()), Logger::Level::DEBUG);
        if (removed) {
            Log.msg(FNAME + "Stopped tracking window (direct): " + mainWindow->objectName(), Logger::Level::DEBUG);
        }
        else {
            Log.msg(FNAME + "Attempted to stop tracking untracked window (direct sender): " + mainWindow->objectName(), Logger::Level::WARNING);
        }
        return; // Done
    }

    // --- If direct cast failed, Log and try removal by matching pointer address ---
    Log.msg(FNAME + "Sender was not castable to QMainWindow. Attempting removal by pointer address matching.", Logger::Level::WARNING);

    bool removedByAddress = false;
    for (int i = m_dynamicWindows.size() - 1; i >= 0; --i) {
        QMainWindow* windowInList = m_dynamicWindows.at(i);
        // Compare the raw pointer addresses
        if (windowInList && reinterpret_cast<quintptr>(windowInList) == senderPtrAddr) {
            Log.msg(FNAME + "Found window in list by matching pointer address: " + windowInList->objectName() + ". Removing.", Logger::Level::DEBUG);
            m_dynamicWindows.removeAt(i);
            removedByAddress = true;
            break; // Found and removed
        }
    }

    Log.msg(FNAME + "After removal by address attempt. Dynamic count: " + QString::number(m_dynamicWindows.count()), Logger::Level::DEBUG);
    if (removedByAddress) {
        Log.msg(FNAME + "Stopped tracking window (by address): " + senderObjectName, Logger::Level::DEBUG); // Use sender's name for logging
    }
    else {
        // This indicates a bigger issue - the pointer wasn't in the list, or sender() is somehow wrong
        Log.msg(FNAME + "FAILED to find window in list matching sender address! Sender: " + senderObjectName, Logger::Level::ERROR);
    }
    */
}

// Function to save the state of all tracked dynamic windows
void WindowManager::saveWindowStates() {
    Log.msg(FNAME + "Saving application state...", Logger::Level::DEBUG);
    QSettings settings;

    // --- 1. Save List of Open Dynamic Windows ---
    settings.beginGroup("Session/OpenDynamicWindows"); // Group for session management
    settings.remove(""); // Clear previous list
    QStringList openDynamicWindowIds;
    int dynamicCount = 0;
    for (const QMainWindow* window : std::as_const(m_dynamicWindows)) {
        if (window) { // Check pointer validity
            QString id = window->objectName();
            QString type = window->metaObject()->className(); // Get the class name
            if (!id.isEmpty() && !type.isEmpty()) {
                // Store ID and Type together, e.g., "TakesPageWindow_uuid1|TakesPageWindow"
                openDynamicWindowIds.append(id + "|" + type);
                dynamicCount++;
            }
            else {
                QString msg("Dynamic window lacks objectName or valid className. Cannot save.");
                Log.msg(FNAME + msg, Logger::Level::WARNING);
            }
        }
    }
    settings.setValue("IdsAndTypes", openDynamicWindowIds);
    settings.endGroup();
    Log.msg(FNAME + QString("Saved list of %1 open dynamic windows.").arg(dynamicCount), Logger::Level::DEBUG);

    // --- 2. Save State (Geometry/Visibility) for ALL Managed Windows ---
    // This might overwrite geometry saved by BaseWindow::saveSettings if a window
    // was moved *after* being closed but *before* the app quit. This is usually acceptable.
    settings.beginGroup("Session/WindowStates"); // Use a different group for session state
    settings.remove("");

    // Combine main and dynamic windows for state saving
    QList<QMainWindow*> allWindows = m_dynamicWindows;
    for (const QPointer<QMainWindow>& ptr : m_mainWindows.values()) {
        if (ptr) allWindows.append(ptr);
    }

    int stateSaveCount = 0;
    for (const QMainWindow* window : std::as_const(allWindows)) {
        if (window) { // Check pointer validity
            QString id = window->objectName();
            if (!id.isEmpty()) {
                settings.beginGroup(id);
                settings.setValue("geometry", window->saveGeometry());
                settings.setValue("isVisible", window->isVisible()); // Save visibility at quit time
                settings.endGroup();
                stateSaveCount++;
            }
        }
    }
    settings.endGroup();

    Log.msg(FNAME + QString("Saved session state for %1 windows.").arg(stateSaveCount), Logger::Level::DEBUG);
}


void WindowManager::restoreWindowStates() {
    Log.msg(FNAME + "Restoring application state...", Logger::Level::DEBUG);
    QSettings settings;

    // Check if defaults should be applied INSTEAD of restorin
    // We already check this in applyDefaultWindowStates, but double-check is ok
    bool firstRun = !settings.contains(FIRST_RUN_KEY) || !settings.value(FIRST_RUN_KEY).toBool();
    if (firstRun) {
        Log.msg(FNAME + "Restore skipped on first run, defaults size and position were applied.", Logger::Level::INFO);
        return; // Defaults already applied, don't try to restore non-existent settings
    }

    // --- 1. Recreate Dynamic Windows ---
    settings.beginGroup("Session/OpenDynamicWindows");
    QStringList idsAndTypes = settings.value("IdsAndTypes").toStringList();
    settings.endGroup(); // End dynamic list group

    int dynamicRecreatedCount = 0;
    for (const QString& idAndType : idsAndTypes) {
        QStringList parts = idAndType.split('|');
        if (parts.size() == 2) {
            QString id = parts[0];
            QString type = parts[1];
            // createNewDynamicWindow handles adding to m_dynamicWindows, connecting signals, showing
            QMainWindow* restoredDynamicWindow = this->createNewDynamicWindow(id, type);
            if (restoredDynamicWindow) {
                dynamicRecreatedCount++;
            }
            else {
                QString msg = QString("Failed to recreate dynamic window: %1, Type: %2").arg(id).arg(type);
                Log.msg(FNAME + msg, Logger::Level::WARNING);
            }
        }
        else {
            QString msg = QString("Invalid format in OpenDynamicWindows list:: %1").arg(idAndType);
            Log.msg(FNAME + msg, Logger::Level::WARNING);
        }
    }
    Log.msg(FNAME + QString("Recreated %1 dynamic windows.").arg(dynamicRecreatedCount), Logger::Level::DEBUG);


    // --- 2. Restore State (Geometry/Visibility) for ALL Managed Windows ---
    // This runs *after* dynamic windows are recreated and main windows already exist.
    settings.beginGroup("Session/WindowStates");
    QStringList windowIds = settings.childGroups(); // Get list of windows with saved state
    int stateRestoreCount = 0;

    // Combine main and dynamic windows again to find the instances
    QMap<QString, QMainWindow*> allWindowsMap; // Map objectName to instance pointer
    for (QMainWindow* window : std::as_const(m_dynamicWindows)) {
        if (window) allWindowsMap.insert(window->objectName(), window);
    }
    for (auto it = m_mainWindows.constBegin(); it != m_mainWindows.constEnd(); ++it) {
        if (it.value()) allWindowsMap.insert(it.key(), it.value());
    }


    for (const QString& id : windowIds) {
        if (allWindowsMap.contains(id)) {
            QMainWindow* window = allWindowsMap.value(id);
            if (window) { // Should always be valid if found in map
                settings.beginGroup(id);
                QByteArray geometry = settings.value("geometry").toByteArray();
                bool wasVisible = settings.value("isVisible", true).toBool(); // Default true
                settings.endGroup(); // End specific window state group

                if (!geometry.isEmpty()) {
                    // Restore geometry AFTER the window exists and potentially after dynamic ones are shown
                    if (!window->restoreGeometry(geometry)) {
                        Log.msg(FNAME + QString("Could not restore session geometry for window: %1").arg(id), 
                            Logger::Level::WARNING);
                        // Maybe apply BaseWindow's loadSettings again as fallback?
                        // BaseWindow* bw = qobject_cast<BaseWindow*>(window);
                        // if(bw) bw->loadSettings(); // Careful about redundant loads
                    }
                    else {
                        //qInfo() << "Restored session geometry for window:" << id;
                    }
                }

                // Apply visibility state from session
                if (wasVisible) {
                    // Don't show ToolPanel here, main.cpp does it
                    if (id != "ToolPanel") {
                        window->show();
                    }
                }
                else {
                    window->hide();
                }
                stateRestoreCount++;
            }
        }
        else {
            Log.msg(FNAME + QString("Session state found for unknown/missing window ID: %1").arg(id),
                Logger::Level::WARNING);
        }
    }
    settings.endGroup(); // End WindowStates group
    Log.msg(FNAME + QString("Restored session state for %1 windows.").arg(stateRestoreCount),
        Logger::Level::DEBUG);
}


/////////////////////////////////////////////////////////////////////////////

// Methods to set/clear the flag 
void WindowManager::setWindowClosing(bool closing) {
    m_isWindowClosing = closing;
}

/////////////////////////////////////////////////////////////////////////////
// Methods for degault windows size and positions

//Method to apply default states if it's the first run
void WindowManager::applyDefaultWindowStates() {
    QSettings settings;
    if (settings.contains(FIRST_RUN_KEY) && settings.value(FIRST_RUN_KEY).toBool()) {
        return; // Not the first run, or key exists and is true
    }

    // --- Apply defaults using the helper ---
    // ToolPanelWindow: size(600px, 20px) position[top left]
    applyDefaultGeometry("ToolPanel", 600, 20, 0, 0, "top left");

    // LogWindow: size(100%, 10%) position[bottom left]
    applyDefaultGeometry("LogWindow", 0, 0, 100, 10, "bottom left");

    // WatchlistWindow: size(200px, 50%) position[top right]
    applyDefaultGeometry("Watchlist", 200, 0, 0, 50, "top right");

    // --- Mark first run as completed ---
    settings.setValue(FIRST_RUN_KEY, true);
    settings.sync(); // Ensure it's written immediately
}


// NEW: Helper function to calculate and apply geometry
void WindowManager::applyDefaultGeometry(const QString & windowName, int wPix, int hPix, int wPerc, int hPerc, const QString & position) {
    if (!m_mainWindows.contains(windowName)) {
        QString msg = QString("Cannot apply default geometry: Window '%1' not registered.").arg(windowName);
        Log.msg(FNAME + msg,Logger::Level::WARNING);
        return;
    }

    QMainWindow* window = m_mainWindows.value(windowName);
    if (!window) {
        QString msg = QString("Cannot apply default geometry: Window pointer for '%1' is null.").arg(windowName);
        Log.msg(FNAME + msg, Logger::Level::WARNING);
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        QString msg = QString("Cannot apply default geometry: No primary screen found.");
        Log.msg(FNAME + msg, Logger::Level::WARNING);
        return; // Should not happen in typical GUI apps
    }
    QRect availableGeometry = screen->availableGeometry(); // Geometry excluding taskbar etc.
    // QRect screenGeometry = screen->geometry(); // Full screen geometry

    int targetWidth = wPix;
    int targetHeight = hPix;

    // Calculate size from percentages if specified
    if (wPerc > 0) {
        targetWidth = availableGeometry.width() * wPerc / 100;
    }
    if (hPerc > 0) {
        targetHeight = availableGeometry.height() * hPerc / 100;
    }

    // Clamp size to screen dimensions (optional but recommended)
    targetWidth = qBound(100, targetWidth, availableGeometry.width());   // Min width 100
    targetHeight = qBound(20, targetHeight, availableGeometry.height()); // Min height 20

    int targetX = availableGeometry.x(); // Default to top-left x
    int targetY = availableGeometry.y(); // Default to top-left y

    // --- Calculate Title Bar / Frame Offset (Approximate) ---
    // This gives an estimate of the top frame/title bar extent.
    // It might not be pixel-perfect across all styles/platforms but is usually good enough.
    int topFrameOffset = 0;
    if (window->style()) {
        // PM_TitleBarHeight is a decent proxy, though frame border adds more.
        // Add a small extra buffer (e.g., 5 pixels) for the top border itself.
        topFrameOffset = window->style()->pixelMetric(QStyle::PM_TitleBarHeight, nullptr, window) + 5;
        // Ensure offset is reasonable
        topFrameOffset = qMax(0, topFrameOffset);
    }
    // Use a minimum offset if style metrics fail or seem too small
    if (topFrameOffset < 15) topFrameOffset = 25; // Fallback minimum offset
    // --- End Offset Calculation ---

    // Calculate position based on string descriptor
    QString posLower = position.toLower().trimmed();

    // --- Adjust Y position based on offset ---
    // If positioning relative to the top, add the offset.
    // If positioning relative to the bottom, the offset is already accounted for
    // because we position the bottom edge relative to the available geometry's bottom.
    bool positionRelativeToTop = !posLower.contains("bottom") && !posLower.contains("center"); // Simple check
    if (positionRelativeToTop) {
        targetY += topFrameOffset;
    }
    // --- End Y position adjustment ---

    if (posLower.contains("right")) {
        targetX = availableGeometry.right() - targetWidth + 1; // +1 because right() is exclusive
    }
    if (posLower.contains("bottom")) {
        targetY = availableGeometry.bottom() - targetHeight + 1; // +1 because bottom() is exclusive
    }
    if (posLower.contains("center")) {
        if (!posLower.contains("left") && !posLower.contains("right")) { // Horizontal center
            targetX = availableGeometry.center().x() - targetWidth / 2;
        }
        if (!posLower.contains("top") && !posLower.contains("bottom")) { // Vertical center
            targetY = availableGeometry.center().y() - targetHeight / 2;
        }
    }
    // "top" and "left" are defaults unless overridden by "bottom", "right", or "center"

    // Ensure position is within available bounds
    targetX = qMax(availableGeometry.x(), targetX);
    targetY = qMax(availableGeometry.y(), targetY);
    // Prevent window from going completely off-screen right/bottom if size is large
    if (targetX + targetWidth > availableGeometry.right() + 1) {
        targetX = availableGeometry.right() + 1 - targetWidth;
    }
    if (targetY + targetHeight > availableGeometry.bottom() + 1) {
        targetY = availableGeometry.bottom() + 1 - targetHeight;
    }

    // Add a check to prevent the adjusted top from going below the original available top
    targetY = qMax(availableGeometry.y(), targetY);

    QString msg = QString("Applying default geometry to %1: Pos[%2, %3], Size[%4 x %5]")
        .arg(windowName).arg(targetX).arg(targetY).arg(targetWidth).arg(targetHeight);
    Log.msg(FNAME + msg, Logger::Level::DEBUG);

    // --- Apply the calculated geometry ---
    // It's generally safer to set geometry AFTER the window is shown,
    // but let's try setting it directly first.
    window->setGeometry(targetX, targetY, targetWidth, targetHeight);

    // Alternative if setGeometry before show is unreliable:
    // window->resize(targetWidth, targetHeight);
    // window->move(targetX, targetY);
}

/////////////////////////////////////////////////////////////////////////////