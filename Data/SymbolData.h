#pragma once

#include <QString>
#include <QVariantMap>

// Include the header where the enum is now defined
#include "SymbolDataManager.h" // Provides SymbolDataManager::SymbolState

// Struct to hold data for a single symbol/model pair
struct SymbolData {
    QString symbolName;
    QString modelName;
    SymbolDataManager::SymbolState state = SymbolDataManager::SymbolState::Active; 
    QVariantMap settings; 

    // Need a unique key for maps/lookups
    QString uniqueKey() const {
        return symbolName + "_" + modelName;
    }

    // Default constructor for containers
    SymbolData() = default;

    SymbolData(QString sym, QString mod)
        : symbolName(std::move(sym)), modelName(std::move(mod)) {
    }
};