/**
 * @file UpdateChecker.cpp
 * @brief Update Checker implementation
 */

#include "UpdateChecker.h"
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QSettings>
#include <QCryptographicHash>
#include <QProcess>
#include <QDesktopServices>

// Current application version - should match CMakeLists.txt
#define APP_VERSION "1.0.0"

UpdateChecker& UpdateChecker::instance()
{
    static UpdateChecker instance;
    return instance;
}

UpdateChecker::UpdateChecker(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentReply(nullptr)
    , m_autoCheckTimer(new QTimer(this))
    , m_checking(false)
    , m_updateAvailable(false)
    , m_autoCheckEnabled(true)
    , m_checkIntervalDays(7)
    , m_includePreReleases(false)
    , m_downloadProgress(0)
    , m_updateUrl("https://api.github.com/repos/YourOrg/BigFileViewer/releases")
{
    connect(m_autoCheckTimer, &QTimer::timeout, this, &UpdateChecker::onAutoCheckTimer);
    
    loadSettings();
    setupAutoCheck();
    
    // Check on startup if enabled and due
    if (m_autoCheckEnabled && shouldAutoCheck()) {
        QTimer::singleShot(5000, this, &UpdateChecker::checkForUpdatesInBackground);
    }
}

UpdateChecker::~UpdateChecker()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
}

QString UpdateChecker::currentVersion() const
{
    return QString(APP_VERSION);
}

void UpdateChecker::checkForUpdates()
{
    if (m_checking) return;
    
    m_checking = true;
    m_updateAvailable = false;
    emit checkingChanged();
    emit updateAvailableChanged();
    
    QUrl url(m_updateUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    request.setRawHeader("User-Agent", "BigFileViewer/" APP_VERSION);
    
    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateChecker::onCheckReplyFinished);
}

void UpdateChecker::checkForUpdatesInBackground()
{
    // Same as checkForUpdates but doesn't show UI
    checkForUpdates();
}

void UpdateChecker::onCheckReplyFinished()
{
    m_checking = false;
    emit checkingChanged();
    
    if (!m_currentReply) return;
    
    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString error = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit checkFailed(error);
        return;
    }
    
    QByteArray data = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    
    parseGitHubRelease(data);
    
    m_lastCheckTime = QDateTime::currentDateTime();
    saveSettings();
    
    emit checkCompleted(m_updateAvailable);
}

void UpdateChecker::parseGitHubRelease(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit checkFailed(tr("Failed to parse release information: %1").arg(error.errorString()));
        return;
    }
    
    QJsonArray releases = doc.array();
    if (releases.isEmpty()) {
        emit checkFailed(tr("No releases found"));
        return;
    }
    
    // Find the latest suitable release
    for (const QJsonValue &val : releases) {
        QJsonObject release = val.toObject();
        
        bool isPreRelease = release["prerelease"].toBool();
        if (isPreRelease && !m_includePreReleases) {
            continue;
        }
        
        QString tagName = release["tag_name"].toString();
        // Remove 'v' prefix if present
        QString version = tagName.startsWith('v') ? tagName.mid(1) : tagName;
        
        // Check if this version is skipped
        if (isVersionSkipped(version)) {
            continue;
        }
        
        // Compare versions
        QVersionNumber current = QVersionNumber::fromString(currentVersion());
        QVersionNumber remote = QVersionNumber::fromString(version);
        
        if (remote > current) {
            m_updateInfo.version = version;
            m_updateInfo.releaseNotes = release["body"].toString();
            m_updateInfo.changelogUrl = release["html_url"].toString();
            m_updateInfo.releaseDate = QDateTime::fromString(release["published_at"].toString(), Qt::ISODate);
            m_updateInfo.isPreRelease = isPreRelease;
            
            // Find the appropriate asset for this platform
            QJsonArray assets = release["assets"].toArray();
            for (const QJsonValue &assetVal : assets) {
                QJsonObject asset = assetVal.toObject();
                QString assetName = asset["name"].toString();
                
                // Windows: .exe or .msi
                #ifdef Q_OS_WIN
                if (assetName.endsWith(".exe") || assetName.endsWith(".msi") || assetName.endsWith(".zip")) {
                    m_updateInfo.downloadUrl = asset["browser_download_url"].toString();
                    m_updateInfo.fileSize = asset["size"].toInteger();
                    break;
                }
                #endif
                
                // macOS: .dmg or .pkg
                #ifdef Q_OS_DARWIN
                if (assetName.endsWith(".dmg") || assetName.endsWith(".pkg")) {
                    m_updateInfo.downloadUrl = asset["browser_download_url"].toString();
                    m_updateInfo.fileSize = asset["size"].toInteger();
                    break;
                }
                #endif
                
                // Linux: .AppImage, .deb, .rpm
                #ifdef Q_OS_LINUX
                if (assetName.endsWith(".AppImage") || assetName.endsWith(".deb") || assetName.endsWith(".rpm")) {
                    m_updateInfo.downloadUrl = asset["browser_download_url"].toString();
                    m_updateInfo.fileSize = asset["size"].toInteger();
                    break;
                }
                #endif
            }
            
            // Look for checksum file
            for (const QJsonValue &assetVal : assets) {
                QJsonObject asset = assetVal.toObject();
                QString assetName = asset["name"].toString();
                if (assetName.endsWith(".sha256") || assetName == "CHECKSUMS") {
                    // Would need to download and parse this file
                    break;
                }
            }
            
            m_updateAvailable = !m_updateInfo.downloadUrl.isEmpty();
            emit updateAvailableChanged();
            emit updateInfoChanged();
            return;
        }
    }
    
    m_updateAvailable = false;
    emit updateAvailableChanged();
}

void UpdateChecker::downloadUpdate()
{
    if (m_updateInfo.downloadUrl.isEmpty()) {
        emit downloadFailed(tr("No download URL available"));
        return;
    }
    
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
    }
    
    m_downloadProgress = 0;
    emit downloadProgressChanged();
    emit downloadStarted();
    
    QNetworkRequest request(QUrl(m_updateInfo.downloadUrl));
    request.setRawHeader("User-Agent", "BigFileViewer/" APP_VERSION);
    
    m_currentReply = m_networkManager->get(request);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &UpdateChecker::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateChecker::onDownloadFinished);
}

void UpdateChecker::cancelDownload()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    m_downloadProgress = 0;
    emit downloadProgressChanged();
}

void UpdateChecker::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        m_downloadProgress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        emit downloadProgressChanged();
    }
}

void UpdateChecker::onDownloadFinished()
{
    if (!m_currentReply) return;
    
    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString error = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit downloadFailed(error);
        return;
    }
    
    // Save to downloads folder
    QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QString fileName = getDownloadFileName();
    m_downloadedFilePath = QDir(downloadsPath).filePath(fileName);
    
    QFile file(m_downloadedFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        emit downloadFailed(tr("Failed to save file: %1").arg(file.errorString()));
        return;
    }
    
    file.write(m_currentReply->readAll());
    file.close();
    
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    
    // Verify checksum if available
    if (!m_updateInfo.checksum.isEmpty()) {
        if (!verifyChecksum(m_downloadedFilePath, m_updateInfo.checksum)) {
            QFile::remove(m_downloadedFilePath);
            emit downloadFailed(tr("Checksum verification failed"));
            return;
        }
    }
    
    m_downloadProgress = 100;
    emit downloadProgressChanged();
    emit downloadCompleted(m_downloadedFilePath);
    emit installationReady(m_downloadedFilePath);
}

void UpdateChecker::installUpdate()
{
    if (m_downloadedFilePath.isEmpty() || !QFile::exists(m_downloadedFilePath)) {
        emit downloadFailed(tr("Update file not found"));
        return;
    }
    
    #ifdef Q_OS_WIN
    // Run the installer
    if (m_downloadedFilePath.endsWith(".exe") || m_downloadedFilePath.endsWith(".msi")) {
        QProcess::startDetached(m_downloadedFilePath, QStringList());
        QCoreApplication::quit();
    } else if (m_downloadedFilePath.endsWith(".zip")) {
        // Open in file explorer
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_downloadedFilePath).absolutePath()));
    }
    #endif
    
    #ifdef Q_OS_DARWIN
    // Open DMG
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_downloadedFilePath));
    #endif
    
    #ifdef Q_OS_LINUX
    if (m_downloadedFilePath.endsWith(".AppImage")) {
        // Make executable and run
        QFile::setPermissions(m_downloadedFilePath, 
            QFile::permissions(m_downloadedFilePath) | QFile::ExeUser);
        QProcess::startDetached(m_downloadedFilePath, QStringList());
        QCoreApplication::quit();
    } else {
        // Open file manager
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_downloadedFilePath).absolutePath()));
    }
    #endif
}

QString UpdateChecker::getDownloadFileName() const
{
    QUrl url(m_updateInfo.downloadUrl);
    QString fileName = url.fileName();
    if (fileName.isEmpty()) {
        #ifdef Q_OS_WIN
        fileName = QString("BigFileViewer_%1_setup.exe").arg(m_updateInfo.version);
        #elif defined(Q_OS_DARWIN)
        fileName = QString("BigFileViewer_%1.dmg").arg(m_updateInfo.version);
        #else
        fileName = QString("BigFileViewer_%1.AppImage").arg(m_updateInfo.version);
        #endif
    }
    return fileName;
}

bool UpdateChecker::verifyChecksum(const QString &filePath, const QString &expectedHash)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return false;
    }
    
    QString actualHash = QString::fromLatin1(hash.result().toHex());
    return actualHash.compare(expectedHash, Qt::CaseInsensitive) == 0;
}

void UpdateChecker::setUpdateUrl(const QString &url)
{
    m_updateUrl = url;
    saveSettings();
}

void UpdateChecker::skipVersion(const QString &version)
{
    if (!m_skippedVersions.contains(version)) {
        m_skippedVersions.append(version);
        saveSettings();
    }
    
    // Clear current update if it matches
    if (m_updateInfo.version == version) {
        m_updateAvailable = false;
        emit updateAvailableChanged();
    }
}

bool UpdateChecker::isVersionSkipped(const QString &version) const
{
    return m_skippedVersions.contains(version);
}

void UpdateChecker::clearSkippedVersions()
{
    m_skippedVersions.clear();
    saveSettings();
}

void UpdateChecker::setAutoCheckEnabled(bool enabled)
{
    if (m_autoCheckEnabled != enabled) {
        m_autoCheckEnabled = enabled;
        saveSettings();
        setupAutoCheck();
        emit settingsChanged();
    }
}

void UpdateChecker::setCheckIntervalDays(int days)
{
    if (days < 1) days = 1;
    if (days > 365) days = 365;
    
    if (m_checkIntervalDays != days) {
        m_checkIntervalDays = days;
        saveSettings();
        setupAutoCheck();
        emit settingsChanged();
    }
}

void UpdateChecker::setIncludePreReleases(bool include)
{
    if (m_includePreReleases != include) {
        m_includePreReleases = include;
        saveSettings();
        emit settingsChanged();
    }
}

void UpdateChecker::setupAutoCheck()
{
    m_autoCheckTimer->stop();
    
    if (m_autoCheckEnabled) {
        // Check once per day while running
        m_autoCheckTimer->start(24 * 60 * 60 * 1000);  // 24 hours
    }
}

bool UpdateChecker::shouldAutoCheck()
{
    if (!m_lastCheckTime.isValid()) {
        return true;
    }
    
    int daysSinceLastCheck = m_lastCheckTime.daysTo(QDateTime::currentDateTime());
    return daysSinceLastCheck >= m_checkIntervalDays;
}

void UpdateChecker::onAutoCheckTimer()
{
    if (shouldAutoCheck()) {
        checkForUpdatesInBackground();
    }
}

void UpdateChecker::loadSettings()
{
    QSettings settings;
    settings.beginGroup("UpdateChecker");
    
    m_autoCheckEnabled = settings.value("autoCheck", true).toBool();
    m_checkIntervalDays = settings.value("checkInterval", 7).toInt();
    m_includePreReleases = settings.value("includePreReleases", false).toBool();
    m_skippedVersions = settings.value("skippedVersions").toStringList();
    m_lastCheckTime = settings.value("lastCheckTime").toDateTime();
    
    QString customUrl = settings.value("updateUrl").toString();
    if (!customUrl.isEmpty()) {
        m_updateUrl = customUrl;
    }
    
    settings.endGroup();
}

void UpdateChecker::saveSettings()
{
    QSettings settings;
    settings.beginGroup("UpdateChecker");
    
    settings.setValue("autoCheck", m_autoCheckEnabled);
    settings.setValue("checkInterval", m_checkIntervalDays);
    settings.setValue("includePreReleases", m_includePreReleases);
    settings.setValue("skippedVersions", m_skippedVersions);
    settings.setValue("lastCheckTime", m_lastCheckTime);
    
    if (m_updateUrl != "https://api.github.com/repos/YourOrg/BigFileViewer/releases") {
        settings.setValue("updateUrl", m_updateUrl);
    }
    
    settings.endGroup();
}
