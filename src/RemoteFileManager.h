/**
 * @file RemoteFileManager.h
 * @brief Remote File Access via libssh2/SFTP for BigFileViewer
 * 
 * Enterprise feature: Open and monitor log files from remote servers
 * Uses libssh2 for native SSH/SFTP support - no external dependencies needed
 */

#ifndef REMOTEFILEMANAGER_H
#define REMOTEFILEMANAGER_H

#include "BuildConfig.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QTimer>
#include <QThread>

#ifdef BFV_REMOTE_FEATURES
// Forward declarations for libssh2
struct _LIBSSH2_SESSION;
typedef struct _LIBSSH2_SESSION LIBSSH2_SESSION;
struct _LIBSSH2_SFTP;
typedef struct _LIBSSH2_SFTP LIBSSH2_SFTP;
#endif

/**
 * @class RemoteFileManager
 * @brief Manages remote file access via libssh2/SFTP
 * 
 * Features:
 * - SSH key and password authentication (built-in, no sshpass needed)
 * - Remote file browsing via SFTP
 * - File download
 * - Auto-refresh for live monitoring
 * - Connection profiles management
 */
class RemoteFileManager : public QObject
{
    Q_OBJECT
    
    // ===== QML Properties =====
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(bool libssh2Available READ isLibssh2Available CONSTANT)
    Q_PROPERTY(QString libraryVersion READ libraryVersion CONSTANT)
    Q_PROPERTY(QString currentHost READ currentHost NOTIFY connectionChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY pathChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(bool autoRefreshEnabled READ autoRefreshEnabled WRITE setAutoRefreshEnabled NOTIFY autoRefreshChanged)
    Q_PROPERTY(int autoRefreshInterval READ autoRefreshInterval WRITE setAutoRefreshInterval NOTIFY autoRefreshChanged)

public:
    /**
     * @brief Singleton instance accessor
     */
    static RemoteFileManager& instance();
    
    // ===== Connection State =====
    
    bool isConnected() const { return m_isConnected; }
    bool isLibssh2Available() const;
    QString libraryVersion() const;
    QString currentHost() const { return m_currentHost; }
    QString currentPath() const { return m_currentPath; }
    bool isLoading() const { return m_loading; }
    QString lastError() const { return m_lastError; }
    
    // ===== Auto-refresh =====
    
    bool autoRefreshEnabled() const { return m_autoRefreshEnabled; }
    void setAutoRefreshEnabled(bool enabled);
    
    int autoRefreshInterval() const { return m_autoRefreshInterval; }
    void setAutoRefreshInterval(int seconds);
    
    // ===== Q_INVOKABLE Methods for QML =====
    
    /**
     * @brief Connect to a remote server
     * @param host Hostname or IP address
     * @param port SSH port (default 22)
     * @param username SSH username
     * @param authMethod "key" or "password"
     * @param credential Private key path or password
     */
    Q_INVOKABLE void connectToServer(const QString &host,
                                      int port,
                                      const QString &username,
                                      const QString &authMethod,
                                      const QString &credential);
    
    /**
     * @brief Disconnect from current server
     */
    Q_INVOKABLE void disconnect();
    
    /**
     * @brief Test connection to server
     */
    Q_INVOKABLE void testConnection(const QString &host,
                                     int port,
                                     const QString &username,
                                     const QString &authMethod,
                                     const QString &credential);
    
    /**
     * @brief List files in a remote directory
     * @param path Remote path (default: current path or home)
     */
    Q_INVOKABLE void listDirectory(const QString &path = QString());
    
    /**
     * @brief Download a remote file and open it
     * @param remotePath Full path to remote file
     */
    Q_INVOKABLE void downloadAndOpen(const QString &remotePath);
    
    /**
     * @brief Refresh the currently opened remote file
     */
    Q_INVOKABLE void refreshCurrentFile();
    
    /**
     * @brief Navigate to parent directory
     */
    Q_INVOKABLE void navigateUp();
    
    /**
     * @brief Navigate to a specific directory
     */
    Q_INVOKABLE void navigateTo(const QString &path);
    
    // ===== Connection Profiles =====
    
    /**
     * @brief Save a connection profile
     */
    Q_INVOKABLE void saveProfile(const QString &name,
                                  const QString &host,
                                  int port,
                                  const QString &username,
                                  const QString &authMethod,
                                  const QString &keyPath);
    
    /**
     * @brief Delete a connection profile
     */
    Q_INVOKABLE void deleteProfile(const QString &name);
    
    /**
     * @brief Get all saved profiles
     * @return List of profile objects
     */
    Q_INVOKABLE QVariantList getProfiles() const;
    
    /**
     * @brief Get a specific profile
     */
    Q_INVOKABLE QVariantMap getProfile(const QString &name) const;

signals:
    void connectionChanged();
    void pathChanged();
    void loadingChanged();
    void errorOccurred(const QString &error);
    void autoRefreshChanged();
    
    /**
     * @brief Emitted when connection test completes
     */
    void connectionTested(bool success, const QString &message);
    
    /**
     * @brief Emitted when directory listing is ready
     * @param files List of file objects {name, isDir, size, modified}
     */
    void directoryListed(const QVariantList &files);
    
    /**
     * @brief Emitted when file is downloaded and ready
     * @param localPath Path to the downloaded file
     * @param remotePath Original remote path
     */
    void fileReady(const QString &localPath, const QString &remotePath);
    
    /**
     * @brief Emitted when file content has been refreshed
     */
    void fileRefreshed(const QString &localPath);
    
    /**
     * @brief Emitted when profiles list changes
     */
    void profilesChanged();

private:
    explicit RemoteFileManager(QObject *parent = nullptr);
    ~RemoteFileManager();
    
    // Disable copy
    RemoteFileManager(const RemoteFileManager&) = delete;
    RemoteFileManager& operator=(const RemoteFileManager&) = delete;
    
    /**
     * @brief Initialize libssh2 library
     */
    void setLoading(bool loading);
    
    /**
     * @brief Set error message
     */
    void setError(const QString &error);
    
    /**
     * @brief Load profiles from settings
     */
    void loadProfiles();
    
    /**
     * @brief Save profiles to settings
     */
    void saveProfiles();
    
#ifdef BFV_REMOTE_FEATURES
    /**
     * @brief Initialize libssh2 library
     */
    bool initLibssh2();
    
    /**
     * @brief Cleanup libssh2 resources
     */
    void cleanupSession();
    
    /**
     * @brief Create TCP socket connection
     */
    int createSocket(const QString &host, int port);
    
    /**
     * @brief Authenticate with password
     */
    bool authenticatePassword(const QString &username, const QString &password);
    
    /**
     * @brief Authenticate with public key
     */
    bool authenticatePublicKey(const QString &username, const QString &keyPath);
    
    /**
     * @brief Initialize SFTP subsystem
     */
    bool initSftp();
    
    // libssh2 handles
    LIBSSH2_SESSION *m_session;
    LIBSSH2_SFTP *m_sftp;
    int m_socket;
#endif
    
    // Connection state
    bool m_isConnected;
    QString m_currentHost;
    int m_currentPort;
    QString m_currentUsername;
    QString m_authMethod;
    QString m_credential;
    QString m_currentPath;
    
    // Current file tracking
    QString m_currentRemotePath;
    QString m_currentLocalPath;
    
    // Auto-refresh
    bool m_autoRefreshEnabled;
    int m_autoRefreshInterval;
    QTimer *m_refreshTimer;
    
    // State
    bool m_loading;
    QString m_lastError;
    bool m_libssh2Initialized;
    
    // Saved profiles
    QMap<QString, QVariantMap> m_profiles;
};

#endif // REMOTEFILEMANAGER_H
