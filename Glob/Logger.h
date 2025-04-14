#pragma once

#include <QString>
#include <QMutex>
#include <atomic>

class Logger;
class QPlainTextEdit;
class QFile;
class QTextStream;

#define Log (Logger::getSingleton())

// Generate name of function which call Loger...
#define FNAME ( (const QString("["))+__FUNCTION__+(const QString("()] ")) )

//////
// Get method name as string: <class>::<method>()

#if defined(_MSC_VER)
#  define Q_FUNC_INFO __FUNCSIG__
#else
#  define Q_FUNC_INFO __PRETTY_FUNCTION__
#endif

inline const QString methodName(const std::string& prettyFunction)
{
    size_t colons = prettyFunction.find_last_of("::");
    size_t begin = prettyFunction.substr(0, colons).rfind(" ") + 1;
    size_t end = prettyFunction.rfind("(") - begin;

    return QString::fromStdString(prettyFunction.substr(begin, end) + "() ");
}

#define METHOD_NAME methodName(Q_FUNC_INFO)
//////

// Singleton pattern for global object.

class Logger {
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    enum class Level { DEBUG = 0, INFO, WARNING, ERROR };

    static Logger& getSingleton() {
        static Logger instance; // Guaranteed to be destroyed and thread-safe
        return instance;
    }

private:
    // Private constructor to prevent direct instantiation
    Logger() = default;
    ~Logger() = default;

    ///////////
    // Payload
private:
    QPlainTextEdit* loggetTextEdit = nullptr;
    std::atomic<bool> isInited = false;
    Level m_level = Level::INFO;

    void addHtmlInverted(const QString& htmlLine);
    void logToFile(const QString& message, const QString& levelStr);

    QMutex m_logMutex; 
    QFile* m_logFile = nullptr;
    QTextStream* m_logStream = nullptr;

public:
    void init(QPlainTextEdit* loggerWidget);
    void msg(const QString& msg, const Level level = Level::INFO);
    void setLevel(const Level level);
    Level currentLevel();

    static Level levelFromString(const QString& levelStr, Logger::Level defaultLevel = Logger::Level::INFO);
    static QString levelToString(Logger::Level level);

    void closeLogger();
};

