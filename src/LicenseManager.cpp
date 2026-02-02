/**
 * @file LicenseManager.cpp
 * @brief RSA License Verification System - Implementation
 */

#include "LicenseManager.h"

#include <QSysInfo>
#include <QNetworkInterface>
#include <QCryptographicHash>
#include <QSettings>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QCoreApplication>

#ifdef ENABLE_LICENSE_SYSTEM
// OpenSSL (only included when license system is enabled via CMake)
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

// ============================================================================
// CRITICAL: Replace this with your actual RSA Public Key from the server
// ============================================================================
static const char* publicKeyPEM = R"(-----BEGIN PUBLIC KEY-----
REPLACE_ME_WITH_YOUR_ACTUAL_RSA_PUBLIC_KEY
This is a placeholder. Generate a real RSA key pair using:
  openssl genrsa -out private.pem 2048
  openssl rsa -in private.pem -pubout -out public.pem
Then paste the contents of public.pem here.
-----END PUBLIC KEY-----)";

// Example of a valid public key format (2048-bit RSA):
// static const char* publicKeyPEM = R"(-----BEGIN PUBLIC KEY-----
// MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA...base64 content...
// -----END PUBLIC KEY-----)";

#endif // ENABLE_LICENSE_SYSTEM

// ============================================================================
// Singleton Implementation
// ============================================================================

LicenseManager& LicenseManager::instance()
{
    static LicenseManager instance;
    return instance;
}

LicenseManager::LicenseManager()
{
#ifdef ENABLE_LICENSE_SYSTEM
    // Initialize OpenSSL (for older versions, newer versions auto-init)
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
#endif
#endif
}

LicenseManager::~LicenseManager()
{
#ifdef ENABLE_LICENSE_SYSTEM
    // Cleanup OpenSSL (for older versions)
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    EVP_cleanup();
    ERR_free_strings();
#endif
#endif
}

// ============================================================================
// Machine ID Generation
// ============================================================================

QString LicenseManager::getMachineId()
{
    QString rawData;

    // 1. Add machine unique ID (Qt's built-in hardware identifier)
    QByteArray machineId = QSysInfo::machineUniqueId();
    if (!machineId.isEmpty()) {
        rawData += QString::fromLatin1(machineId.toHex());
    }

    // 2. Add primary MAC address for additional uniqueness
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        // Skip loopback, virtual, and down interfaces
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))
            continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsRunning))
            continue;
        
        QString mac = iface.hardwareAddress();
        if (!mac.isEmpty() && mac != "00:00:00:00:00:00") {
            rawData += mac.remove(':');  // Remove colons from MAC
            break;  // Use only the first valid interface
        }
    }

    // 3. Fallback: Add hostname if no other identifiers available
    if (rawData.isEmpty()) {
        rawData = QSysInfo::machineHostName();
    }

    // 4. Generate MD5 hash of the combined data
    QByteArray hash = QCryptographicHash::hash(
        rawData.toUtf8(),
        QCryptographicHash::Md5
    );

    return QString::fromLatin1(hash.toHex()).toUpper();
}

// ============================================================================
// License Verification (RSA Signature Check)
// ============================================================================

bool LicenseManager::verifyLicense(const QString &licenseKey)
{
#ifdef ENABLE_LICENSE_SYSTEM
    m_lastError.clear();

    if (licenseKey.isEmpty()) {
        m_lastError = "License key is empty";
        return false;
    }

    // 1. Decode the Base64 license key to get signature bytes
    QByteArray signature = QByteArray::fromBase64(licenseKey.toLatin1());
    if (signature.isEmpty()) {
        m_lastError = "Invalid Base64 encoding in license key";
        return false;
    }

    // 2. Get the machine ID as the data that was signed
    QString machineIdStr = getMachineId();
    QByteArray data = machineIdStr.toUtf8();

    // 3. Load the RSA public key from PEM
    BIO *bio = BIO_new_mem_buf(publicKeyPEM, -1);
    if (!bio) {
        m_lastError = "Failed to create BIO buffer for public key";
        return false;
    }

    RSA *rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);

    if (!rsa) {
        unsigned long err = ERR_get_error();
        char errBuf[256];
        ERR_error_string_n(err, errBuf, sizeof(errBuf));
        m_lastError = QString("Failed to load public key: %1").arg(errBuf);
        return false;
    }

    // 4. Compute SHA-256 hash of the machine ID
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.constData()),
           static_cast<size_t>(data.size()),
           hash);

    // 5. Verify the RSA signature
    int result = RSA_verify(
        NID_sha256,
        hash, SHA256_DIGEST_LENGTH,
        reinterpret_cast<const unsigned char*>(signature.constData()),
        static_cast<unsigned int>(signature.size()),
        rsa
    );

    // 6. Cleanup
    RSA_free(rsa);

    if (result == 1) {
        qDebug() << "[LicenseManager] License verification successful";
        return true;
    } else {
        unsigned long err = ERR_get_error();
        char errBuf[256];
        ERR_error_string_n(err, errBuf, sizeof(errBuf));
        m_lastError = QString("Signature verification failed: %1").arg(errBuf);
        qDebug() << "[LicenseManager]" << m_lastError;
        return false;
    }
#else
    // License system not enabled - always return false
    Q_UNUSED(licenseKey)
    m_lastError = "License system is not enabled (compile with -DENABLE_LICENSE_SYSTEM=ON)";
    qWarning() << "[LicenseManager]" << m_lastError;
    return false;
#endif
}

// ============================================================================
// License Storage Check
// ============================================================================

bool LicenseManager::hasValidLicense()
{
    QSettings settings;
    QString storedKey = settings.value("license/key").toString();
    
    if (storedKey.isEmpty()) {
        return false;
    }

    return verifyLicense(storedKey);
}

bool LicenseManager::isRegistered()
{
    return hasValidLicense();
}

QString LicenseManager::lastError() const
{
    return m_lastError;
}

// ============================================================================
// Online Activation
// ============================================================================

bool LicenseManager::activateOnline(const QString &licenseKey)
{
    if (verifyLicense(licenseKey)) {
        saveLicense(licenseKey);
        m_validated = true;
        return true;
    }
    return false;
}

// ============================================================================
// Offline Activation
// ============================================================================

QString LicenseManager::generateActivationRequest()
{
    // Create activation request JSON
    QJsonObject request;
    request["machineId"] = getMachineId();
    request["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    request["appVersion"] = QCoreApplication::applicationVersion();
    request["platform"] = QSysInfo::prettyProductName();
    
    QJsonDocument doc(request);
    QByteArray json = doc.toJson(QJsonDocument::Compact);
    
    // Return Base64-encoded request
    return QString::fromLatin1(json.toBase64());
}

bool LicenseManager::activateOffline(const QString &activationResponse)
{
    m_lastError.clear();
    
    if (activationResponse.isEmpty()) {
        m_lastError = "Activation response is empty";
        return false;
    }
    
    // Decode Base64 response
    QByteArray responseData = QByteArray::fromBase64(activationResponse.toLatin1());
    if (responseData.isEmpty()) {
        m_lastError = "Invalid Base64 encoding in activation response";
        return false;
    }
    
    // Parse JSON response
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (!doc.isObject()) {
        m_lastError = "Invalid activation response format";
        return false;
    }
    
    QJsonObject response = doc.object();
    
    // Verify machine ID matches
    QString responseMachineId = response["machineId"].toString();
    if (responseMachineId != getMachineId()) {
        m_lastError = "Activation response is for a different machine";
        return false;
    }
    
    // Get license key from response
    QString licenseKey = response["licenseKey"].toString();
    if (licenseKey.isEmpty()) {
        m_lastError = "No license key in activation response";
        return false;
    }
    
    // Verify the license key
    if (!verifyLicense(licenseKey)) {
        return false; // m_lastError already set by verifyLicense
    }
    
    // Extract tier and expiry info
    m_licenseTier = response["tier"].toInt(0);
    m_licenseExpiry = response["expiry"].toString();
    
    // Save the license
    saveLicense(licenseKey);
    
    // Save tier and expiry
    QSettings settings;
    settings.setValue("license/tier", m_licenseTier);
    settings.setValue("license/expiry", m_licenseExpiry);
    
    m_validated = true;
    qDebug() << "[LicenseManager] Offline activation successful, tier:" << m_licenseTier;
    
    return true;
}

// ============================================================================
// License Storage
// ============================================================================

void LicenseManager::saveLicense(const QString &licenseKey)
{
    QSettings settings;
    settings.setValue("license/key", licenseKey);
    settings.setValue("license/activatedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    settings.sync();
    qDebug() << "[LicenseManager] License saved to settings";
}

QString LicenseManager::loadLicense() const
{
    QSettings settings;
    return settings.value("license/key").toString();
}

void LicenseManager::clearLicense()
{
    QSettings settings;
    settings.remove("license/key");
    settings.remove("license/tier");
    settings.remove("license/expiry");
    settings.remove("license/activatedAt");
    settings.sync();
    qDebug() << "[LicenseManager] License cleared from settings";
}

int LicenseManager::getLicenseTier() const
{
    if (m_licenseTier > 0) {
        return m_licenseTier;
    }
    QSettings settings;
    return settings.value("license/tier", 0).toInt();
}

QString LicenseManager::getLicenseExpiry() const
{
    if (!m_licenseExpiry.isEmpty()) {
        return m_licenseExpiry;
    }
    QSettings settings;
    return settings.value("license/expiry").toString();
}

bool LicenseManager::isLicenseExpired() const
{
    QString expiry = getLicenseExpiry();
    if (expiry.isEmpty()) {
        return false; // Perpetual license or no expiry set
    }
    
    QDateTime expiryDate = QDateTime::fromString(expiry, Qt::ISODate);
    if (!expiryDate.isValid()) {
        return false;
    }
    
    return QDateTime::currentDateTimeUtc() > expiryDate;
}
