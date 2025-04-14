#include "Glob/Glob.h"
#include "Glob/Logger.h"
#include "../Defines.h"
#include "WindowManager.h"
#include "WindowLayout/TakesPageWindow/TakesPageWindow.h"
#include "WindowLayout/QuoteChartWindow.h"

#include <QMainWindow>
#include <Qevent.h>
#include <QUuid>
#include <QScreen>
#include <QRect> 
#include <QGuiApplication> // For QGuiApplication::primaryScreen()
#include <QStyle> 
#include <QDebug> 
#include <set>

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

// Method to remove a window from the main tracking map
void WindowManager::unregisterMainWindow(const QString& name) {
    Log.msg(FNAME + "Attempting to unregister main window: " + name, Logger::Level::DEBUG);
    if (m_mainWindows.contains(name)) {
        // Remove the entry from the map.
        // The QPointer will become null automatically if the object is deleted later,
        // but removing the map entry prevents showAllWindows from finding it.
        int removedCount = m_mainWindows.remove(name);
        if (removedCount > 0) {
            Log.msg(FNAME + "Successfully unregistered main window: " + name, Logger::Level::DEBUG);
        }
        else {
            // Should not happen if contains() was true
            Log.msg(FNAME + "Failed to remove main window from map even though it was found: " + name, 
                Logger::Level::WARNING);
        }
    }
    else {
        Log.msg(FNAME + "Window not found in main window map for unregistration: " + name, Logger::Level::WARNING);
    }
}

// When user click on any window of this app then we show (unhide all other windows).
// For avoid click on each window for show whole app if it hidden.
void WindowManager::handleFocusChanged(QWidget* oldFocus, QWidget* newFocus) {
    Q_UNUSED(oldFocus); // We only care about the new focus

    // --- Check the flag ---
    bool wasClosing = m_isWindowClosing; // Store current flag state
    if (m_isWindowClosing) {
        Log.msg(FNAME + "Ignoring focus change during window close.", Logger::Level::DEBUG);
        // --- Reset the flag HERE ---
        m_isWindowClosing = false; // Reset after detecting it was set
        // --- End Reset ---
        return; // Don't proceed to show windows
    }
    // --- End Check ---

    if (!newFocus) {
        // Focus left the application, do nothing regarding showing windows
        return;
    }

    m_isWindowClosing = false; // double check

    // Find the top-level window containing the newly focused widget
    QWidget* topLevelWidget = newFocus->window();
    QMainWindow* activatedWindow = qobject_cast<QMainWindow*>(topLevelWidget);

    if (topLevelWidget) {
        QString topLevelClassName = topLevelWidget ? topLevelWidget->metaObject()->className() : "None";
        //QString msg = QString("Focus changed: %1").arg(topLevelClassName);
        //Log.msg(FNAME + msg, Logger::Level::DEBUG);
    }

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
            if (topLevelWidget) {
                Log.msg(FNAME + "Focus changed to unmanaged window: " + topLevelWidget->objectName(),
                    Logger::Level::DEBUG);
            }
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
        window = new QuoteChartWindow(this, Glob.dataReceiver, nullptr);
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

        connect(window, &QObject::destroyed, this, &WindowManager::handleDynamicWindowDestroyed);
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


void WindowManager::handleDynamicWindowDestroyed(QObject* obj) {
    QObject* senderObj = sender();
    if (!senderObj) {
        Log.msg(FNAME + "Signal received, but sender() returned null!", Logger::Level::ERROR);
        return;
    }

    // --- Get sender info ---
    QString senderClassName = senderObj->metaObject()->className();
    QString senderObjectName = senderObj->objectName();
    quintptr senderPtrAddr = reinterpret_cast<quintptr>(senderObj);


    // --- Attempt direct cast FIRST (might work sometimes) ---
    QMainWindow* mainWindow = qobject_cast<QMainWindow*>(senderObj);
    if (mainWindow) {
        bool removed = m_dynamicWindows.removeOne(mainWindow);
        if (removed) {
            Log.msg(FNAME + "Stopped tracking window (direct): " + mainWindow->objectName(), Logger::Level::DEBUG);
        }
        else {
            Log.msg(FNAME + "Attempted to stop tracking untracked window (direct sender): " + mainWindow->objectName(), 
                Logger::Level::WARNING);
        }
        return; // Done
    }

    // --- If direct cast failed, Log and try removal by matching pointer address ---
    Log.msg(FNAME + "Sender was not castable to QMainWindow. Attempting removal by pointer address matching.", 
        Logger::Level::WARNING);

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

    if (removedByAddress) {
        Log.msg(FNAME + "Stopped tracking window (by address): " + senderObjectName, Logger::Level::DEBUG); // Use sender's name for logging
    }
    else {
        // This indicates a bigger issue - the pointer wasn't in the list, or sender() is somehow wrong
        Log.msg(FNAME + "FAILED to find window in list matching sender address! Sender: " + senderObjectName, 
            Logger::Level::ERROR);
    }
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
    Log.msg("WindowManager: Restoring application state...", Logger::Level::DEBUG);
    QSettings settings;

    // --- 1. Recreate Dynamic Windows ---
    // (This part remains the same - reads Session/OpenDynamicWindows, calls createNewDynamicWindow)
    settings.beginGroup("Session/OpenDynamicWindows");
    QStringList idsAndTypes = settings.value("IdsAndTypes").toStringList();
    settings.endGroup();
    int dynamicRecreatedCount = 0;
    for (const QString& idAndType : idsAndTypes) {
        // ... (split id/type, call createNewDynamicWindow) ...
        // createNewDynamicWindow adds to m_dynamicWindows and shows
        QStringList parts = idAndType.split('|');
        if (parts.size() == 2) {
            QString id = parts[0];
            QString type = parts[1];
            QMainWindow* restoredDynamicWindow = this->createNewDynamicWindow(id, type);
            if (restoredDynamicWindow) {
                dynamicRecreatedCount++;
            }
            else {
                Log.msg(QString("Failed to recreate dynamic window: %1, Type: %2").arg(id).arg(type),
                    Logger::Level::WARNING);
            }
        }
        else {
            Log.msg(QString("Invalid format in OpenDynamicWindows list: %1").arg(idAndType),
                Logger::Level::WARNING);
        }
    }
    Log.msg(QString("Recreated %1 dynamic windows.").arg(dynamicRecreatedCount),
        Logger::Level::DEBUG);

    // Combine main and dynamic windows into a single list for processing
    QList<QMainWindow*> allManagedWindows;
    for (const QPointer<QMainWindow>& ptr : m_mainWindows.values()) {
        if (ptr) allManagedWindows.append(ptr);
    }
    allManagedWindows.append(m_dynamicWindows); // Add dynamic ones (already created)

    int stateRestoreCount = 0;
    int defaultAppliedCount = 0;

    settings.beginGroup("Session/WindowStates"); // Group where session state is saved

    for (QMainWindow* window : std::as_const(allManagedWindows)) {
        if (!window) continue; // Should not happen, but check pointer

        QString id = window->objectName();
        if (id.isEmpty()) {
            Log.msg(QString("Window lacks objectName. Cannot restore/apply default state."),
                Logger::Level::WARNING);
            continue;
        }

        bool restoredState = false;
        // Check if state exists for this specific window ID within the session group
        if (settings.childGroups().contains(id)) {
            settings.beginGroup(id);
            QByteArray geometry = settings.value("geometry").toByteArray();
            bool wasVisible = settings.value("isVisible", true).toBool();
            settings.endGroup(); // End specific window state group

            if (!geometry.isEmpty()) {
                if (window->restoreGeometry(geometry)) {
                    restoredState = true; // Mark that we successfully restored geometry
                }
                else {
                    // Fall through to apply default geometry if restore failed
                }
            }
            else {
                // Fall through to apply default geometry
            }

            // Apply visibility from session state ONLY if geometry was successfully restored
            if (restoredState) {
                if (wasVisible) {
                    if (id != "ToolPanel") window->show(); // Show unless it's ToolPanel
                }
                else {
                    window->hide();
                }
                stateRestoreCount++;
            }
        }
        else {
            // Fall through to apply default geometry
        }


        // --- Apply Default Geometry if State Was Not Found or Restore Failed ---
        if (!restoredState) {
            // Define defaults here or retrieve them from a config structure
            if (id == "ToolPanel") {
                // size(600px, 20px) position[top left]
                applyDefaultGeometry(id, 600, 20, 0, 0, "top left");
                window->show(); // Ensure ToolPanel is shown even if default applied
                defaultAppliedCount++;
            }
            else if (id == "LogWindow") {
                // size(100%, 10%) position[bottom left]
                applyDefaultGeometry(id, 0, 0, 100, 10, "bottom left");
                window->show(); // Show windows with defaults applied
                defaultAppliedCount++;
            }
            else if (id == "Watchlist") {
                // size(200px, 50%) position[top right]
                applyDefaultGeometry(id, 200, 0, 0, 50, "top right");
                window->show(); // Show windows with defaults applied
                defaultAppliedCount++;
            }
            else if (id == "TakesPage") { // Example default for a statically created TakesPage
                applyDefaultGeometry(id, 800, 600, 0, 0, "center");
                window->show();
                defaultAppliedCount++;
            }
            else if (id == "QuoteChart") { // Example default for a statically created QuoteChart
                applyDefaultGeometry(id, 800, 600, 0, 0, "center");
                window->show();
                defaultAppliedCount++;
            }
            else if (id.startsWith("TakesPage_") || id.startsWith("Plot chart_")) {
                // Optional: Apply a default for dynamic windows if their state is missing
                applyDefaultGeometry(id, 600, 400, 0, 0, "center");
                window->show(); // Dynamic windows are usually shown by createNewDynamicWindow anyway
                defaultAppliedCount++;
            }
            else {
                Log.msg("No default geometry defined for window: " + id,
                    Logger::Level::WARNING);
                window->show(); // Show anyway with Qt's default size/pos
            }
        }
    } // End loop through allManagedWindows

    settings.endGroup(); // End WindowStates group
}

/////////////////////////////////////////////////////////////////////////////

// Methods to set/clear the flag 
void WindowManager::setWindowClosing(bool closing) {
    m_isWindowClosing = closing;
}

/////////////////////////////////////////////////////////////////////////////
// Methods for degault windows size and positions

// Helper function to calculate and apply geometry
void WindowManager::applyDefaultGeometry(const QString & windowName, int wPix, int hPix, int wPerc, int hPerc, const QString & position) {

    QMainWindow* window = nullptr;
    if (m_mainWindows.contains(windowName)) {
        window = m_mainWindows.value(windowName);
    }
    else {
        // Check dynamic windows too, though defaults are less common for them
        for (QMainWindow* dynWin : std::as_const(m_dynamicWindows)) {
            if (dynWin && dynWin->objectName() == windowName) {
                window = dynWin;
                break;
            }
        }
    }

    if (!window) {
        QString msg = QString("Cannot apply default geometry: Window '%1' not found in managed lists..").arg(windowName);
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