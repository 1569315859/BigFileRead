/**
 * @file LicenseManager.h
 * @brief RSA License Verification System - Client Side
 * @details Singleton class for verifying RSA-signed license keys using OpenSSL
 */

#ifndef LICENSEMANAGER_H
#define LICENSEMANAGER_H

#include <QString>
#include <QByteArray>

// OpenSSL headers (only included when license system is enabled)
#ifdef ENABLE_LICENSE_SYSTEM
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#endif

/**
 * @class LicenseManager
 * @brief Singleton class for client-side RSA license verification
 * 
 * This class handles:
 * - Generation of unique machine identifiers
 * - Verification of RSA-signed license keys
 * 
 * Usage:
 * @code
 * QString machineId = LicenseManager::instance().getMachineId();
 * bool valid = LicenseManager::instance().verifyLicense(licenseKey);
 * @endcode
 */
class LicenseManager
{
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the LicenseManager instance
     */
    static LicenseManager& instance();

    // Disable copy and move
    LicenseManager(const LicenseManager&) = delete;
    LicenseManager& operator=(const LicenseManager&) = delete;
    LicenseManager(LicenseManager&&) = delete;
    LicenseManager& operator=(LicenseManager&&) = delete;

    /**
     * @brief Generate a unique machine identifier
     * @details Combines machineUniqueId + MAC address, returns MD5 hex digest
     * @return MD5 hash of the machine's unique identifiers (32 hex characters)
     */
    QString getMachineId();

    /**
     * @brief Verify a license key signature
     * @param licenseKey Base64-encoded RSA signature from the server
     * @return true if the license is valid for this machine, false otherwise
     * 
     * @details The verification process:
     * 1. Decode the Base64 license key to get the signature bytes
     * 2. Get the machine ID and hash it with SHA-256
     * 3. Use RSA_verify to check if the signature matches the hash
     */
    bool verifyLicense(const QString &licenseKey);

    /**
     * @brief Activate license online
     * @param licenseKey The license key to activate
     * @return true if activation successful
     */
    bool activateOnline(const QString &licenseKey);

    /**
     * @brief Generate activation request for offline activation
     * @details Creates a JSON file containing machine ID and request timestamp
     * @return Base64-encoded activation request string, or empty on failure
     */
    QString generateActivationRequest();

    /**
     * @brief Process offline activation response
     * @param activationResponse Base64-encoded activation response from server
     * @return true if offline activation successful
     */
    bool activateOffline(const QString &activationResponse);

    /**
     * @brief Save license to persistent storage
     * @param licenseKey The validated license key to save
     */
    void saveLicense(const QString &licenseKey);

    /**
     * @brief Load license from persistent storage
     * @return The stored license key, or empty string if none
     */
    QString loadLicense() const;

    /**
     * @brief Clear stored license
     */
    void clearLicense();

    /**
     * @brief Get license tier from stored license
     * @return License tier (0=Free, 1=Pro, 2=Enterprise)
     */
    int getLicenseTier() const;

    /**
     * @brief Get license expiry date
     * @return Expiry date string in ISO format, or empty if perpetual/no license
     */
    QString getLicenseExpiry() const;

    /**
     * @brief Check if license is expired
     * @return true if license has expired
     */
    bool isLicenseExpired() const;

    /**
     * @brief Check if a valid license is stored in settings
     * @return true if a valid license exists
     */
    bool hasValidLicense();

    /**
     * @brief Check if the application is registered (alias for hasValidLicense)
     * @return true if registered, false if in trial mode
     */
    bool isRegistered();

    /**
     * @brief Get the last error message
     * @return Description of the last error, or empty string if no error
     */
    QString lastError() const;

private:
    /**
     * @brief Private constructor for singleton pattern
     */
    LicenseManager();

    /**
     * @brief Private destructor
     */
    ~LicenseManager();

    /**
     * @brief Store the last error message
     */
    QString m_lastError;

    /**
     * @brief Cached license tier
     */
    int m_licenseTier = 0;

    /**
     * @brief Cached license expiry date
     */
    QString m_licenseExpiry;

    /**
     * @brief Whether license has been validated this session
     */
    bool m_validated = false;
};

#endif // LICENSEMANAGER_H
