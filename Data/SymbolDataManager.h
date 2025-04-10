#pragma once

#include <QObject>
#include <QMap>
#include <QReadWriteLock>
#include <QMutex>
#include <QVariantMap> 
#include <QString>    

class SymbolDataManager : public QObject {
    Q_OBJECT

public:
    enum class SymbolState {
        Active,
        Paused
    };
    Q_ENUM(SymbolState) // Register the enum with the meta-object system

    explicit SymbolDataManager(QObject* parent = nullptr);
    ~SymbolDataManager() override = default;

    // Concurent-safe methods to access and modify symbol data
    bool addSymbol(const QString& symbol, const QString& model);
    bool removeSymbol(const QString& symbol, const QString& model);
    bool setSymbolState(const QString& symbol, const QString& model, SymbolState newState); 
    bool updateSymbolSettings(const QString& symbol, const QString& model, const QVariantMap& settings);

    SymbolState getSymbolState(const QString& symbol, const QString& model) const; 
    QVariantMap getSymbolSettings(const QString& symbol, const QString& model) const;
    QList<struct SymbolData> getAllSymbols() const; 
    bool contains(const QString& symbol, const QString& model) const;


signals:
    // Signals emitted *after* the internal state has been successfully updated
    void symbolAdded(const QString& symbol, const QString& model);
    void symbolRemoved(const QString& symbol, const QString& model);
    void symbolStateChanged(const QString& symbol, const QString& model, SymbolState newState);
    void symbolSettingsChanged(const QString& symbol, const QString& model, const QVariantMap& settings);

private:
    // Using QReadWriteLock for better concurrency (many readers, one writer)
    mutable QReadWriteLock m_lock;

    // We need the full definition of SymbolData here for the map value
    // Include SymbolData.h *after* the enum definition or ensure SymbolData.h includes this header.
    QMap<QString, struct SymbolData> m_symbols; // Key: symbol_model

    QString generateKey(const QString& symbol, const QString& model) const;
};

// Include SymbolData.h here AFTER SymbolDataManager declaration if SymbolData needs SymbolState
#include "SymbolData.h"

