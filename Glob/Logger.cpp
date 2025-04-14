#include "Logger.h"

#include <QPlainTextEdit>
#include <QTextCursor>
#include <QScrollBar>
#include <QTextDocument>
#include <QTextBlock>
#include <QMutexLocker>
#include <QDir>
#include <QCoreApplication>
#include <QThread>

///////////////////////////////////////////////////////////////////
// Payload

void Logger::init(QPlainTextEdit* loggerWidget) {
    QMutexLocker locker(&m_logMutex); // Lock for initialization

    if (isInited) {
        return;
    }

    loggetTextEdit = loggerWidget;

    // 1. Determine log directory
    QString logPath = QCoreApplication::applicationDirPath() + "/logs";
    QDir logDir(logPath);
    if (!logDir.exists()) {
        qInfo() << "Creating log directory:" << logPath;
        if (!logDir.mkpath(".")) { // Create directory relative to logPath
            qCritical() << "Failed to create log directory:" << logPath;
            return;
        }
    }

    // 2. Generate filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    QString fileName = QString("%1/%2.txt").arg(logPath, timestamp);

    // 3. Create and open the file
    m_logFile = new QFile(fileName);
    if (!m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qCritical() << "Failed to open log file:" << fileName << "Error:" << m_logFile->errorString();
        delete m_logFile;
        m_logFile = nullptr;
        return;
    }

    // 4. Create the text stream
    m_logStream = new QTextStream(m_logFile);
    m_logStream->setEncoding(QStringConverter::Utf8); // Ensure UTF-8 encoding

    isInited = true;
    qInfo() << "Logger initialized. Log file:" << fileName;

    locker.unlock();
}

void Logger::closeLogger() {
    QMutexLocker locker(&m_logMutex); // Ensure exclusive access for cleanup

    if (!isInited) {
        return;
    }

    if (m_logStream) {
        m_logStream->flush(); // Final flush
        delete m_logStream;
        m_logStream = nullptr;
    }

    if (m_logFile) {
        if (m_logFile->isOpen()) {
            m_logFile->close();
        }
        delete m_logFile;
        m_logFile = nullptr;
    }
    isInited = false;
}

// Print mesage in logger widget
void Logger::msg(const QString& msg, const Level level) {
    if (!isInited) {
        return;
    }

    if (level < m_level) {
        return;
    }

    QMutexLocker locker(&m_logMutex); // Lock for writing

    // Double check initialization after locking
    if (!isInited) return;

    QString levelStr;
    QString styleStart = "";
    QString styleEnd = "";

    switch (level)
    {
    case Level::DEBUG:
        styleStart = "<p style=\"color:gray;\">";
        levelStr = "DEBUG";
        break;
    case Level::INFO:
        styleStart = "<p style=\"color:black;\">";
        levelStr = "INFO";
        break;
    case Level::WARNING:
        styleStart = "<p style=\"color:#d47f00;\">";
        levelStr = "WARNING";
        break;
    case Level::ERROR:
        styleStart = "<p style=\"color:red;\">";
        levelStr = "ERROR";
        break;
    default:
        break;
    }

    //////////////////
    // Get pointers to the current thread and the application's main GUI thread
    QThread* currentThread = QThread::currentThread();
    QThread* mainGuiThread = QCoreApplication::instance() ? QCoreApplication::instance()->thread() : nullptr;

    if (mainGuiThread && currentThread == mainGuiThread) {
        // --- Current thread IS the main GUI thread ---
        // Call the update function directly
        addHtmlInverted(styleStart + msg + styleEnd);
    }
    else if (mainGuiThread) {
        // Use invokeMethod to ensure thread-safety if msg can be called from other threads
        QMetaObject::invokeMethod(loggetTextEdit, [=]() {
            // Inverted for show newest messages on logs top
            addHtmlInverted(styleStart + msg + styleEnd);
            },
            Qt::QueuedConnection // Queue to UI thread
        );
    }
    //////////////////

    logToFile(msg, levelStr);
}

void Logger::addHtmlInverted(const QString& htmlLine) {
    QTextDocument* doc = loggetTextEdit->document();
    if (!doc) {
        qWarning("addHtmlInverted: Could not get document from QPlainTextEdit!");
        return;
    }

    // --- 1. Insert the new content at the beginning ---
    QTextCursor cursor(doc); 
    cursor.movePosition(QTextCursor::Start); 

    // Insert the HTML for the new line
    cursor.insertHtml(htmlLine);

    // Insert a block separator (like pressing Enter) right after the inserted HTML.
    // This ensures the new HTML is its own paragraph/block and pushes
    // the previous content down onto the next line(s).
    cursor.insertBlock();

    // --- 2. Limit the number of blocks ---
    const int maxBlocks = 1000;
    // Keep removing the *last* block until the count is within the limit
    // Note: blockCount() can be slightly expensive if called repeatedly in a tight loop
    // for very large documents, but is generally fine here.
    while (doc->blockCount() > maxBlocks) {
        QTextCursor removeCursor(doc->lastBlock()); // Cursor targeting the last block
        removeCursor.select(QTextCursor::BlockUnderCursor); // Select the entire block
        removeCursor.removeSelectedText(); // Remove the selected block content
        // For QPlainTextEdit, removeSelectedText on a BlockUnderCursor
        // usually removes the block itself cleanly.
    }
    // As an alternative, consider using QPlainTextEdit::setMaximumBlockCount(maxBlocks)
    // It's simpler but might have different performance characteristics. Test which works best.
    // plainTextEdit->setMaximumBlockCount(maxBlocks); // Call this once during setup if preferred

    // --- 3. Scroll to the top ---
    // Ensure the scrollbar is positioned at the top to show the newly added line.
    QScrollBar* scrollBar = loggetTextEdit->verticalScrollBar();
    if (scrollBar) {
        scrollBar->setValue(scrollBar->minimum());
    }
}

void Logger::setLevel(const Level level) {
    QMutexLocker locker(&m_logMutex);
    m_level = level;
}

Logger::Level Logger::currentLevel() {
    QMutexLocker locker(&m_logMutex);
    return m_level;
}

void Logger::logToFile(const QString& message, const QString& levelStr) {
    //QMutexLocker locker(&m_logMutex); // Lock for the duration of the write

    if (!isInited) {
        return;
    }

    // Get current timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");

    // Write formatted message and flush immediately
    // Qt::endl flushes automatically
    (*m_logStream) << "[" << timestamp << "] " << levelStr << " " << message << Qt::endl; 
}

// Helper function to convert string name to Level enum
Logger::Level Logger::levelFromString(const QString& levelStr, Logger::Level defaultLevel) {
    QString upperLevel = levelStr.toUpper().trimmed();
    if (upperLevel == "DEBUG") return Level::DEBUG;
    if (upperLevel == "INFO") return Level::INFO;
    if (upperLevel == "WARNING" || upperLevel == "WARN") return Level::WARNING;
    if (upperLevel == "ERROR") return Level::ERROR;
    qWarning() << FNAME << "Unknown log level string:" << levelStr << ". Using default.";
    return defaultLevel;
}

// Helper to get string representation for level
QString Logger::levelToString(Logger::Level level) {
    switch (level) {
    case Level::DEBUG:    return "DEBUG";
    case Level::INFO:     return "INFO";
    case Level::WARNING:  return "WARNING";
    case Level::ERROR:    return "ERROR";
    default:              return "UNKN";
    }
}
