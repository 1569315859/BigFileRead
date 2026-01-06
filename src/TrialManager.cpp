/**
 * @file TrialManager.cpp
 * @brief Trial Period Manager Implementation
 */

#include "TrialManager.h"

#include <QSettings>
#include <QDebug>
#include <QCoreApplication>

// Obfuscated storage key - looks like a system cache setting
const QString TrialManager::STORAGE_KEY = QStringLiteral("[System]/CacheConfig");

// ============================================================================
// Singleton Implementation
// ============================================================================

TrialManager& TrialManager::instance()
{
    static TrialManager instance;
    return instance;
}

TrialManager::TrialManager()
{
    // Try to read existing trial date on construction
    m_trialStartDate = readTrialDate();
    m_initialized = m_trialStartDate.isValid();
}

// ============================================================================
// Obfuscation Functions
// ============================================================================

QString TrialManager::obfuscate(const QString &data)
{
    // Step 1: XOR each character with the key
    QByteArray bytes = data.toUtf8();
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = bytes[i] ^ XOR_KEY;
    }
    
    // Step 2: Convert to Base64 for safe storage
    QString result = QString::fromLatin1(bytes.toBase64());
    
    // Step 3: Reverse the string for additional obfuscation
    std::reverse(result.begin(), result.end());
    
    return result;
}

QString TrialManager::deobfuscate(const QString &data)
{
    if (data.isEmpty()) {
        return QString();
    }
    
    // Step 1: Reverse the string
    QString reversed = data;
    std::reverse(reversed.begin(), reversed.end());
    
    // Step 2: Decode from Base64
    QByteArray bytes = QByteArray::fromBase64(reversed.toLatin1());
    
    // Step 3: XOR each character with the key to decrypt
    for (int i = 0; i < bytes.size(); ++i) {
        bytes[i] = bytes[i] ^ XOR_KEY;
    }
    
    return QString::fromUtf8(bytes);
}

// ============================================================================
// Storage Functions
// ============================================================================

QDateTime TrialManager::readTrialDate()
{
    QSettings settings;
    QString obfuscatedValue = settings.value(STORAGE_KEY).toString();
    
    if (obfuscatedValue.isEmpty()) {
        return QDateTime();  // Invalid - no trial data exists
    }
    
    QString plainValue = deobfuscate(obfuscatedValue);
    
    // The stored format is Unix timestamp as string
    bool ok = false;
    qint64 timestamp = plainValue.toLongLong(&ok);
    
    if (!ok || timestamp <= 0) {
        qWarning() << "[TrialManager] Corrupt trial data detected";
        return QDateTime();  // Corrupt data
    }
    
    return QDateTime::fromSecsSinceEpoch(timestamp);
}

void TrialManager::writeTrialDate(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) {
        return;
    }
    
    // Store as Unix timestamp string
    QString plainValue = QString::number(dateTime.toSecsSinceEpoch());
    QString obfuscatedValue = obfuscate(plainValue);
    
    QSettings settings;
    settings.setValue(STORAGE_KEY, obfuscatedValue);
    settings.sync();
    
    qDebug() << "[TrialManager] Trial date saved";
}

// ============================================================================
// Public Interface
// ============================================================================

void TrialManager::initTrial()
{
    if (m_initialized && m_trialStartDate.isValid()) {
        qDebug() << "[TrialManager] Trial already initialized";
        return;
    }
    
    // Record current time as trial start
    m_trialStartDate = QDateTime::currentDateTime();
    writeTrialDate(m_trialStartDate);
    m_initialized = true;
    
    qDebug() << "[TrialManager] Trial initialized, expires in" << TRIAL_DAYS << "days";
}

bool TrialManager::isTrialExpired()
{
    // If no trial data exists, initialize it now
    if (!m_initialized || !m_trialStartDate.isValid()) {
        initTrial();
    }
    
    int remaining = daysRemaining();
    return remaining <= 0;
}

int TrialManager::daysRemaining()
{
    if (!m_initialized || !m_trialStartDate.isValid()) {
        initTrial();
    }
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 daysPassed = m_trialStartDate.daysTo(now);
    
    int remaining = TRIAL_DAYS - static_cast<int>(daysPassed);
    
    // Additional tamper check: if system time moved backwards significantly
    if (daysPassed < -1) {
        qWarning() << "[TrialManager] System time tampering detected (time moved back)";
        return 0;  // Treat as expired
    }
    
    return remaining;
}

QDateTime TrialManager::getTrialStartDate()
{
    if (!m_initialized) {
        m_trialStartDate = readTrialDate();
        m_initialized = m_trialStartDate.isValid();
    }
    return m_trialStartDate;
}
