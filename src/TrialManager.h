/**
 * @file TrialManager.h
 * @brief Trial Period Manager with Obfuscated Storage
 * @details Manages trial start date securely to prevent simple tampering
 */

#ifndef TRIALMANAGER_H
#define TRIALMANAGER_H

#include <QString>
#include <QDateTime>

/**
 * @class TrialManager
 * @brief Singleton class for managing trial period
 * 
 * Features:
 * - Obfuscated storage of trial start date
 * - 30-day trial period
 * - Tamper-resistant time tracking
 */
class TrialManager
{
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the TrialManager instance
     */
    static TrialManager& instance();

    // Disable copy and move
    TrialManager(const TrialManager&) = delete;
    TrialManager& operator=(const TrialManager&) = delete;
    TrialManager(TrialManager&&) = delete;
    TrialManager& operator=(TrialManager&&) = delete;

    /**
     * @brief Initialize trial period (record first run date)
     * @details Called automatically if no trial data exists
     */
    void initTrial();

    /**
     * @brief Check if trial period has expired
     * @return true if trial has expired (> 30 days), false otherwise
     */
    bool isTrialExpired();

    /**
     * @brief Get number of days remaining in trial
     * @return Days remaining (0 if expired, negative if past expiration)
     */
    int daysRemaining();

    /**
     * @brief Get the trial start date
     * @return QDateTime of when trial started, or invalid if not initialized
     */
    QDateTime getTrialStartDate();

    /**
     * @brief Trial period in days
     */
    static constexpr int TRIAL_DAYS = 30;

private:
    TrialManager();
    ~TrialManager() = default;

    /**
     * @brief Encrypt data using XOR obfuscation
     * @param data Plain text data
     * @return Obfuscated string
     */
    QString obfuscate(const QString &data);

    /**
     * @brief Decrypt data using XOR deobfuscation
     * @param data Obfuscated string
     * @return Plain text data
     */
    QString deobfuscate(const QString &data);

    /**
     * @brief Read trial start timestamp from obfuscated storage
     * @return QDateTime of trial start, or invalid DateTime if not found/corrupt
     */
    QDateTime readTrialDate();

    /**
     * @brief Write trial start timestamp to obfuscated storage
     * @param dateTime The trial start date to store
     */
    void writeTrialDate(const QDateTime &dateTime);

    // Obfuscation key (simple XOR)
    static constexpr char XOR_KEY = 0x5A;
    
    // Obfuscated registry key name (not "FirstRunDate" to avoid detection)
    static const QString STORAGE_KEY;
    
    // Cached trial start date
    QDateTime m_trialStartDate;
    bool m_initialized = false;
};

#endif // TRIALMANAGER_H
