#include "SymbolDataManager.h"
#include "SymbolData.h"
#include <QReadLocker>
#include <QWriteLocker>
#include <QDebug>
#include <QMessageBox>

SymbolDataManager::SymbolDataManager(QObject* parent) : QObject(parent) {
}

QString SymbolDataManager::generateKey(const QString& symbol, const QString& model) const {
    return symbol + "_" + model;
}

bool SymbolDataManager::addSymbol(const QString& symbol, const QString& model) {
    QWriteLocker locker(&m_lock);
    QString key = generateKey(symbol, model);
    if (m_symbols.contains(key)) {
        qWarning() << "SymbolDataManager: Symbol/Model already exists:" << key;
        return false;
    }
    SymbolData data(symbol, model); // State defaults to Active in SymbolData constructor
    m_symbols.insert(key, data);
    locker.unlock();

    qInfo() << "SymbolDataManager: Added" << key;
    emit symbolAdded(symbol, model);
    return true;
}

bool SymbolDataManager::removeSymbol(const QString& symbol, const QString& model) {
    QWriteLocker locker(&m_lock);
    QString key = generateKey(symbol, model);
    if (!m_symbols.contains(key)) {
        qWarning() << "SymbolDataManager: Symbol/Model not found for removal:" << key;
        return false;
    }
    m_symbols.remove(key);
    locker.unlock();

    qInfo() << "SymbolDataManager: Removed" << key;
    emit symbolRemoved(symbol, model);
    return true;
}

bool SymbolDataManager::setSymbolState(const QString& symbol, const QString& model, SymbolState newState) {
    QWriteLocker locker(&m_lock);
    QString key = generateKey(symbol, model);
    auto it = m_symbols.find(key);
    if (it == m_symbols.end()) {
        qWarning() << "SymbolDataManager: Symbol/Model not found for state change:" << key;
        return false;
    }

    if (it.value().state == newState) {
        return true;
    }

    it.value().state = newState;
    locker.unlock();

    qInfo() << "SymbolDataManager: State changed for" << key << "to" << (newState == SymbolState::Active ? "Active" : "Paused");
    emit symbolStateChanged(symbol, model, newState);
    return true;
}

bool SymbolDataManager::updateSymbolSettings(const QString& symbol, const QString& model, const QVariantMap& settings) {
    QWriteLocker locker(&m_lock);
    QString key = generateKey(symbol, model);
    auto it = m_symbols.find(key);
    if (it == m_symbols.end()) {
        qWarning() << "SymbolDataManager: Symbol/Model not found for settings update:" << key;
        return false;
    }
    it.value().settings = settings;
    locker.unlock();

    qInfo() << "SymbolDataManager: Settings updated for" << key;
    emit symbolSettingsChanged(symbol, model, settings);
    return true;
}

// Use the nested enum type
SymbolDataManager::SymbolState SymbolDataManager::getSymbolState(const QString& symbol, const QString& model) const {
    QReadLocker locker(&m_lock);
    QString key = generateKey(symbol, model);
    auto it = m_symbols.constFind(key);
    if (it != m_symbols.constEnd()) {
        return it.value().state;
    }
    qWarning() << "SymbolDataManager: Symbol/Model not found for get state:" << key;
    return SymbolState::Paused; // Default fallback
}

QVariantMap SymbolDataManager::getSymbolSettings(const QString& symbol, const QString& model) const {
    QReadLocker locker(&m_lock);
    QString key = generateKey(symbol, model);
    auto it = m_symbols.constFind(key);
    if (it != m_symbols.constEnd()) {
        return it.value().settings;
    }
    qWarning() << "SymbolDataManager: Symbol/Model not found for get settings:" << key;
    return QVariantMap(); 
}

QList<SymbolData> SymbolDataManager::getAllSymbols() const {
    QReadLocker locker(&m_lock);
    return m_symbols.values();
}

bool SymbolDataManager::contains(const QString& symbol, const QString& model) const
{
    QReadLocker locker(&m_lock);
    return m_symbols.contains(generateKey(symbol, model));
}