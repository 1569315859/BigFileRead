/**
 * @file SessionRecovery.h
 * @brief Session Recovery Manager Header
 * 
 * Provides crash recovery and session persistence:
 * - Auto-save session state periodically
 * - Recover from unexpected crashes
 * - Restore open files, scroll positions, bookmarks
 * - Track unsaved changes
 */

#ifndef SESSIONRECOVERY_H
#define SESSIONRECOVERY_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QDateTime>
#include <QJsonObject>

/**
 * @brief Session state structure
 */
struct SessionState {
    QString sessionId;
    QDateTime timestamp;
    QStringList openFiles;
    QString activeFile;
    QVariantMap scrollPositions;    // file -> line number
    QVariantMap bookmarks;           // file -> list of bookmarks
    QVariantMap searchHistory;
    QVariantMap filterHistory;
    QString currentFilter;
    bool hasUnsavedChanges = false;
    
    QJsonObject toJson() const;
    static SessionState fromJson(const QJsonObject &obj);
};

/**
 * @brief Session Recovery Manager singleton
 * 
 * Automatically saves session state and recovers from crashes:
 * - Periodic auto-save every 30 seconds
 * - Save on significant state changes
 * - Detect crash on startup and offer recovery
 * - Clean up old session files
 */
class SessionRecovery : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool hasRecoverableSession READ hasRecoverableSession NOTIFY recoverableSessionChanged)
    Q_PROPERTY(QString recoveryInfo READ recoveryInfo NOTIFY recoverableSessionChanged)
    Q_PROPERTY(bool autoSaveEnabled READ autoSaveEnabled WRITE setAutoSaveEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int autoSaveIntervalSecs READ autoSaveIntervalSecs WRITE setAutoSaveIntervalSecs NOTIFY settingsChanged)
    Q_PROPERTY(int maxBackupCount READ maxBackupCount WRITE setMaxBackupCount NOTIFY settingsChanged)
    
public:
    static SessionRecovery& instance();
    
    // Recovery state
    bool hasRecoverableSession() const;
    QString recoveryInfo() const;
    
    // Settings
    bool autoSaveEnabled() const { return m_autoSaveEnabled; }
    void setAutoSaveEnabled(bool enabled);
    int autoSaveIntervalSecs() const { return m_autoSaveIntervalSecs; }
    void setAutoSaveIntervalSecs(int secs);
    int maxBackupCount() const { return m_maxBackupCount; }
    void setMaxBackupCount(int count);
    
    // Session management
    Q_INVOKABLE void saveSession();
    Q_INVOKABLE void recoverSession();
    Q_INVOKABLE void discardRecovery();
    Q_INVOKABLE void clearAllSessions();
    
    // State tracking
    Q_INVOKABLE void trackOpenFile(const QString &filePath);
    Q_INVOKABLE void trackCloseFile(const QString &filePath);
    Q_INVOKABLE void trackActiveFile(const QString &filePath);
    Q_INVOKABLE void trackScrollPosition(const QString &filePath, int lineNumber);
    Q_INVOKABLE void trackBookmark(const QString &filePath, int lineNumber, const QString &comment);
    Q_INVOKABLE void trackRemoveBookmark(const QString &filePath, int lineNumber);
    Q_INVOKABLE void trackFilter(const QString &filter);
    Q_INVOKABLE void trackSearch(const QString &searchText);
    Q_INVOKABLE void trackUnsavedChanges(bool hasChanges);
    
    // Session info
    Q_INVOKABLE QVariantList getSessionBackups() const;
    Q_INVOKABLE QVariantMap getSessionInfo(const QString &sessionId) const;
    Q_INVOKABLE bool restoreSession(const QString &sessionId);
    Q_INVOKABLE bool deleteSession(const QString &sessionId);
    
signals:
    void recoverableSessionChanged();
    void settingsChanged();
    void sessionSaved();
    void sessionRecovered(const QVariantMap &sessionData);
    void recoveryFailed(const QString &error);
    
    // Recovery signals for UI
    void requestOpenFile(const QString &filePath);
    void requestScrollTo(const QString &filePath, int lineNumber);
    void requestRestoreBookmark(const QString &filePath, int lineNumber, const QString &comment);
    void requestApplyFilter(const QString &filter);
    
private:
    explicit SessionRecovery(QObject *parent = nullptr);
    ~SessionRecovery();
    Q_DISABLE_COPY(SessionRecovery)
    
    void loadSettings();
    void saveSettings();
    void checkForRecoverableSession();
    void cleanupOldSessions();
    void writeLockFile();
    void removeLockFile();
    bool isLockFileStale() const;
    
    QString getSessionDir() const;
    QString getCurrentSessionPath() const;
    QString getLockFilePath() const;
    
    SessionState m_currentState;
    SessionState m_recoverableState;
    bool m_hasRecoverableSession = false;
    
    QTimer *m_autoSaveTimer;
    bool m_autoSaveEnabled = true;
    int m_autoSaveIntervalSecs = 30;
    int m_maxBackupCount = 10;
    
private slots:
    void onAutoSave();
};

#endif // SESSIONRECOVERY_H
