#pragma once

#include "../Defines.h"

#include <QString>

class SymbolDataManager;
class WebSocketClient;
class ClientReceiver;

using namespace Qt::StringLiterals;

#define APP_VERSION u"%1 v%2"_s.arg(NAME_PROGRAM_FULL, VERSION)

// Singleton pattern for global object.
#define Glob (GlobSingleton::getSingleton())

class GlobSingleton {
public:
    GlobSingleton(const GlobSingleton&) = delete;
    GlobSingleton& operator=(const GlobSingleton&) = delete;

    enum class Level { DEBUG = 0, INFO, WARNING, ERROR };

    static GlobSingleton& getSingleton() {
        static GlobSingleton instance; // Guaranteed to be destroyed and thread-safe
        return instance;
    }

private:
    // Private constructor to prevent direct instantiation
    GlobSingleton() = default;
    ~GlobSingleton() = default;

    ///////////
    // Payload
public:

    SymbolDataManager* dataManager = nullptr;
    WebSocketClient* wsClient = nullptr;
    ClientReceiver* dataReceiver = nullptr;
};

