/**
 * @file UpdateChecker.h
 * @brief Update Checker - Check for application updates from GitHub releases
 */

#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVersionNumber>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QTimer>

/**
 * @brief Information about an available update
 */
struct UpdateInfo {
    QString version;
    QString releaseNotes;
    QString downloadUrl;
    QString changelogUrl;
    QDateTime releaseDate;
    qint64 fileSize;
    QString checksum;  // SHA256
    bool isMandatory;
    bool isPreRelease;
};

/**
 * @brief Singleton UpdateChecker for checking and downloading updates
 */
class UpdateChecker : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool checking READ isChecking NOTIFY checkingChanged)
    Q_PROPERTY(bool updateAvailable READ isUpdateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateInfoChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY updateInfoChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY updateInfoChanged)
    Q_PROPERTY(bool autoCheckEnabled READ autoCheckEnabled WRITE setAutoCheckEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int checkIntervalDays READ checkIntervalDays WRITE setCheckIntervalDays NOTIFY settingsChanged)
    Q_PROPERTY(bool includePreReleases READ includePreReleases WRITE setIncludePreReleases NOTIFY settingsChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadProgressChanged)

public:
    static UpdateChecker& instance();
    
    // Check for updates
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void checkForUpdatesInBackground();
    
    // Download update
    Q_INVOKABLE void downloadUpdate();
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void installUpdate();
    
    // Settings
    Q_INVOKABLE void setUpdateUrl(const QString &url);
    Q_INVOKABLE void skipVersion(const QString &version);
    Q_INVOKABLE bool isVersionSkipped(const QString &version) const;
    Q_INVOKABLE void clearSkippedVersions();
    
    // Getters
    bool isChecking() const { return m_checking; }
    bool isUpdateAvailable() const { return m_updateAvailable; }
    QString currentVersion() const;
    QString latestVersion() const { return m_updateInfo.version; }
    QString releaseNotes() const { return m_updateInfo.releaseNotes; }
    QString downloadUrl() const { return m_updateInfo.downloadUrl; }
    bool autoCheckEnabled() const { return m_autoCheckEnabled; }
    int checkIntervalDays() const { return m_checkIntervalDays; }
    bool includePreReleases() const { return m_includePreReleases; }
    int downloadProgress() const { return m_downloadProgress; }
    
    // Setters
    void setAutoCheckEnabled(bool enabled);
    void setCheckIntervalDays(int days);
    void setIncludePreReleases(bool include);

signals:
    void checkingChanged();
    void updateAvailableChanged();
    void updateInfoChanged();
    void settingsChanged();
    void downloadProgressChanged();
    void checkCompleted(bool updateAvailable);
    void checkFailed(const QString &error);
    void downloadStarted();
    void downloadCompleted(const QString &filePath);
    void downloadFailed(const QString &error);
    void installationReady(const QString &filePath);

private slots:
    void onCheckReplyFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();
    void onAutoCheckTimer();

private:
    explicit UpdateChecker(QObject *parent = nullptr);
    ~UpdateChecker();
    
    UpdateChecker(const UpdateChecker&) = delete;
    UpdateChecker& operator=(const UpdateChecker&) = delete;
    
    void parseGitHubRelease(const QByteArray &data);
    void loadSettings();
    void saveSettings();
    void setupAutoCheck();
    bool shouldAutoCheck();
    QString getDownloadFileName() const;
    bool verifyChecksum(const QString &filePath, const QString &expectedHash);
    
    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentReply;
    QTimer *m_autoCheckTimer;
    
    UpdateInfo m_updateInfo;
    bool m_checking;
    bool m_updateAvailable;
    bool m_autoCheckEnabled;
    int m_checkIntervalDays;
    bool m_includePreReleases;
    int m_downloadProgress;
    
    QString m_updateUrl;
    QString m_downloadedFilePath;
    QStringList m_skippedVersions;
    QDateTime m_lastCheckTime;
};

#endif // UPDATECHECKER_H
