/**
 * @file RemoteFileManager.cpp
 * @brief Remote File Access implementation via libssh2/SFTP
 * 
 * Uses libssh2 for native SSH/SFTP support - no external dependencies needed
 */

#include "RemoteFileManager.h"
#include "BuildConfig.h"
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QCoreApplication>

#ifdef BFV_REMOTE_FEATURES
// Platform-specific socket includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

// libssh2 includes
#include <libssh2.h>
#include <libssh2_sftp.h>
#endif // BFV_REMOTE_FEATURES

// ============================================================================
// Singleton
// ============================================================================

RemoteFileManager& RemoteFileManager::instance()
{
    static RemoteFileManager instance;
    return instance;
}

RemoteFileManager::RemoteFileManager(QObject *parent)
    : QObject(parent)
#ifdef BFV_REMOTE_FEATURES
    , m_session(nullptr)
    , m_sftp(nullptr)
    , m_socket(-1)
#endif
    , m_isConnected(false)
    , m_currentPort(22)
    , m_autoRefreshEnabled(false)
    , m_autoRefreshInterval(5)
    , m_refreshTimer(new QTimer(this))
    , m_loading(false)
    , m_libssh2Initialized(false)
{
#ifdef BFV_REMOTE_FEATURES
    // Initialize libssh2 and platform sockets
    initLibssh2();
#endif
    
    // Setup auto-refresh timer
    connect(m_refreshTimer, &QTimer::timeout, this, &RemoteFileManager::refreshCurrentFile);
    
    loadProfiles();
}

RemoteFileManager::~RemoteFileManager()
{
#ifdef BFV_REMOTE_FEATURES
    cleanupSession();
    
    if (m_libssh2Initialized) {
        libssh2_exit();
    }
    
#ifdef _WIN32
    WSACleanup();
#endif
#endif // BFV_REMOTE_FEATURES
}

// ============================================================================
// libssh2 Initialization
// ============================================================================

#ifdef BFV_REMOTE_FEATURES
bool RemoteFileManager::initLibssh2()
{
#ifdef _WIN32
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
        qWarning() << "RemoteFileManager: Failed to initialize Winsock";
        return false;
    }
#endif
    
    int rc = libssh2_init(0);
    if (rc != 0) {
        qWarning() << "RemoteFileManager: Failed to initialize libssh2:" << rc;
        return false;
    }
    
    m_libssh2Initialized = true;
    qDebug() << "RemoteFileManager: libssh2 initialized -" << libssh2_version(0);
    return true;
}

void RemoteFileManager::cleanupSession()
{
    if (m_sftp) {
        libssh2_sftp_shutdown(m_sftp);
        m_sftp = nullptr;
    }
    
    if (m_session) {
        libssh2_session_disconnect(m_session, "Bye");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    
    if (m_socket != INVALID_SOCKET) {
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
}
#endif // BFV_REMOTE_FEATURES

bool RemoteFileManager::isLibssh2Available() const
{
#ifdef BFV_REMOTE_FEATURES
    return m_libssh2Initialized;
#else
    return false;
#endif
}

QString RemoteFileManager::libraryVersion() const
{
#ifdef BFV_REMOTE_FEATURES
    return QString("libssh2 %1").arg(libssh2_version(0));
#else
    return tr("Remote features not enabled");
#endif
}

// ============================================================================
// Socket Connection
// ============================================================================

#ifdef BFV_REMOTE_FEATURES
int RemoteFileManager::createSocket(const QString &host, int port)
{
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;  // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    
    QString portStr = QString::number(port);
    int status = getaddrinfo(host.toUtf8().constData(), portStr.toUtf8().constData(), &hints, &res);
    if (status != 0) {
        setError(tr("Failed to resolve host: %1").arg(host));
        return INVALID_SOCKET;
    }
    
    int sock = INVALID_SOCKET;
    for (p = res; p != nullptr; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock == INVALID_SOCKET) continue;
        
        // Set socket timeout for connect
#ifdef _WIN32
        DWORD timeout = 10000;  // 10 seconds
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
        
        if (::connect(sock, p->ai_addr, (int)p->ai_addrlen) == 0) {
            break;  // Success
        }
        
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
    
    freeaddrinfo(res);
    
    if (sock == INVALID_SOCKET) {
        setError(tr("Failed to connect to %1:%2").arg(host).arg(port));
    }
    
    return sock;
}

// ============================================================================
// Authentication
// ============================================================================

bool RemoteFileManager::authenticatePassword(const QString &username, const QString &password)
{
    if (!m_session) return false;
    
    int rc = libssh2_userauth_password(m_session, 
                                        username.toUtf8().constData(),
                                        password.toUtf8().constData());
    
    if (rc != 0) {
        char *errmsg;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        setError(tr("Password authentication failed: %1").arg(errmsg));
        return false;
    }
    
    return true;
}

bool RemoteFileManager::authenticatePublicKey(const QString &username, const QString &keyPath)
{
    if (!m_session) return false;
    
    // Expand ~ to home directory
    QString expandedPath = keyPath;
    if (expandedPath.startsWith("~")) {
        expandedPath = QDir::homePath() + expandedPath.mid(1);
    }
    
    // Check if key file exists
    if (!QFile::exists(expandedPath)) {
        setError(tr("Private key file not found: %1").arg(expandedPath));
        return false;
    }
    
    // Try to find public key file
    QString pubKeyPath = expandedPath + ".pub";
    const char *pubKey = nullptr;
    if (QFile::exists(pubKeyPath)) {
        pubKey = pubKeyPath.toUtf8().constData();
    }
    
    // Try key authentication (with passphrase prompt if needed)
    int rc = libssh2_userauth_publickey_fromfile(
        m_session,
        username.toUtf8().constData(),
        pubKey,
        expandedPath.toUtf8().constData(),
        nullptr  // No passphrase - could add later
    );
    
    if (rc != 0) {
        char *errmsg;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        setError(tr("Public key authentication failed: %1").arg(errmsg));
        return false;
    }
    
    return true;
}

// ============================================================================
// SFTP Initialization
// ============================================================================

bool RemoteFileManager::initSftp()
{
    if (!m_session) return false;
    
    m_sftp = libssh2_sftp_init(m_session);
    if (!m_sftp) {
        char *errmsg;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        setError(tr("Failed to initialize SFTP: %1").arg(errmsg));
        return false;
    }
    
    return true;
}
#endif // BFV_REMOTE_FEATURES

// ============================================================================
// Connection Management
// ============================================================================

void RemoteFileManager::connectToServer(const QString &host,
                                         int port,
                                         const QString &username,
                                         const QString &authMethod,
                                         const QString &credential)
{
#ifdef BFV_REMOTE_FEATURES
    if (m_loading) return;
    
    setLoading(true);
    
    // Store connection info
    m_currentHost = host;
    m_currentPort = port;
    m_currentUsername = username;
    m_authMethod = authMethod;
    m_credential = credential;
    
    // Cleanup any existing session
    cleanupSession();
    
    // Create socket connection
    m_socket = createSocket(host, port);
    if (m_socket == INVALID_SOCKET) {
        setLoading(false);
        emit connectionTested(false, m_lastError);
        return;
    }
    
    // Create SSH session
    m_session = libssh2_session_init();
    if (!m_session) {
        setError(tr("Failed to create SSH session"));
        cleanupSession();
        setLoading(false);
        emit connectionTested(false, m_lastError);
        return;
    }
    
    // Set non-blocking mode
    libssh2_session_set_blocking(m_session, 1);
    
    // Handshake
    int rc = libssh2_session_handshake(m_session, m_socket);
    if (rc != 0) {
        char *errmsg;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        setError(tr("SSH handshake failed: %1").arg(errmsg));
        cleanupSession();
        setLoading(false);
        emit connectionTested(false, m_lastError);
        return;
    }
    
    // Authenticate
    bool authSuccess = false;
    if (authMethod == "password") {
        authSuccess = authenticatePassword(username, credential);
    } else {
        authSuccess = authenticatePublicKey(username, credential);
    }
    
    if (!authSuccess) {
        cleanupSession();
        setLoading(false);
        emit connectionTested(false, m_lastError);
        return;
    }
    
    // Initialize SFTP
    if (!initSftp()) {
        cleanupSession();
        setLoading(false);
        emit connectionTested(false, m_lastError);
        return;
    }
    
    m_isConnected = true;
    m_currentPath = "/home/" + username;  // Default path
    
    setLoading(false);
    emit connectionChanged();
    emit connectionTested(true, tr("Connected as: %1").arg(username));
    
    qDebug() << "RemoteFileManager: Connected to" << host << "as" << username;
    
    // List initial directory
    listDirectory(m_currentPath);
#else
    Q_UNUSED(host); Q_UNUSED(port); Q_UNUSED(username); Q_UNUSED(authMethod); Q_UNUSED(credential);
    setError(tr("Remote features not enabled in this build"));
    emit connectionTested(false, m_lastError);
#endif
}

void RemoteFileManager::disconnect()
{
#ifdef BFV_REMOTE_FEATURES
    cleanupSession();
#endif
    
    m_isConnected = false;
    m_currentHost.clear();
    m_currentPath.clear();
    m_currentRemotePath.clear();
    m_currentLocalPath.clear();
    
    m_refreshTimer->stop();
    
    emit connectionChanged();
    qDebug() << "RemoteFileManager: Disconnected";
}

void RemoteFileManager::testConnection(const QString &host,
                                        int port,
                                        const QString &username,
                                        const QString &authMethod,
                                        const QString &credential)
{
    // For libssh2, test connection is same as connect
    connectToServer(host, port, username, authMethod, credential);
}

// ============================================================================
// Directory Operations
// ============================================================================

void RemoteFileManager::listDirectory(const QString &path)
{
#ifdef BFV_REMOTE_FEATURES
    if (!m_isConnected || !m_sftp) {
        setError(tr("Not connected to server"));
        return;
    }
    
    QString targetPath = path.isEmpty() ? m_currentPath : path;
    if (targetPath.isEmpty()) targetPath = "/home/" + m_currentUsername;
    
    // Expand ~ to home directory
    if (targetPath.startsWith("~")) {
        targetPath = "/home/" + m_currentUsername + targetPath.mid(1);
    }
    
    setLoading(true);
    
    // Open directory
    LIBSSH2_SFTP_HANDLE *dirHandle = libssh2_sftp_opendir(m_sftp, targetPath.toUtf8().constData());
    if (!dirHandle) {
        char *errmsg;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        setError(tr("Failed to open directory: %1").arg(errmsg));
        setLoading(false);
        return;
    }
    
    QVariantList files;
    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    
    while (true) {
        int rc = libssh2_sftp_readdir(dirHandle, buffer, sizeof(buffer), &attrs);
        if (rc <= 0) break;
        
        QString name = QString::fromUtf8(buffer);
        
        // Skip . and ..
        if (name == "." || name == "..") continue;
        
        QVariantMap file;
        file["name"] = name;
        file["isDir"] = LIBSSH2_SFTP_S_ISDIR(attrs.permissions);
        file["isLink"] = LIBSSH2_SFTP_S_ISLNK(attrs.permissions);
        file["size"] = static_cast<qint64>(attrs.filesize);
        
        // Convert mtime to string
        QDateTime modTime = QDateTime::fromSecsSinceEpoch(attrs.mtime);
        file["modified"] = modTime.toString("MMM dd hh:mm");
        
        // Permission string
        QString perms;
        perms += LIBSSH2_SFTP_S_ISDIR(attrs.permissions) ? 'd' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IRUSR) ? 'r' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IWUSR) ? 'w' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IXUSR) ? 'x' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IRGRP) ? 'r' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IWGRP) ? 'w' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IXGRP) ? 'x' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IROTH) ? 'r' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IWOTH) ? 'w' : '-';
        perms += (attrs.permissions & LIBSSH2_SFTP_S_IXOTH) ? 'x' : '-';
        file["permissions"] = perms;
        
        files.append(file);
    }
    
    libssh2_sftp_closedir(dirHandle);
    
    // Sort: directories first, then by name
    std::sort(files.begin(), files.end(), [](const QVariant &a, const QVariant &b) {
        QVariantMap ma = a.toMap();
        QVariantMap mb = b.toMap();
        
        bool aDir = ma["isDir"].toBool();
        bool bDir = mb["isDir"].toBool();
        
        if (aDir != bDir) return aDir;  // Directories first
        
        return ma["name"].toString().toLower() < mb["name"].toString().toLower();
    });
    
    m_currentPath = targetPath;
    
    setLoading(false);
    emit pathChanged();
    emit directoryListed(files);
#else
    Q_UNUSED(path);
    setError(tr("Remote features not enabled"));
#endif
}

void RemoteFileManager::navigateUp()
{
    if (m_currentPath == "/" || m_currentPath.isEmpty()) return;
    
    QString parent = m_currentPath;
    int lastSlash = parent.lastIndexOf('/');
    if (lastSlash > 0) {
        parent = parent.left(lastSlash);
    } else {
        parent = "/";
    }
    
    listDirectory(parent);
}

void RemoteFileManager::navigateTo(const QString &path)
{
    QString newPath = path;
    
    // Handle relative paths
    if (!path.startsWith('/') && !path.startsWith('~')) {
        if (m_currentPath.endsWith('/')) {
            newPath = m_currentPath + path;
        } else {
            newPath = m_currentPath + "/" + path;
        }
    }
    
    listDirectory(newPath);
}

// ============================================================================
// File Operations
// ============================================================================

void RemoteFileManager::downloadAndOpen(const QString &remotePath)
{
#ifdef BFV_REMOTE_FEATURES
    if (!m_isConnected || !m_sftp) {
        setError(tr("Not connected to server"));
        return;
    }
    
    setLoading(true);
    
    // Create temp file with appropriate extension
    QString fileName = QFileInfo(remotePath).fileName();
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString localPath = tempDir + "/BigFileViewer_" + fileName;
    
    // Open remote file
    LIBSSH2_SFTP_HANDLE *fileHandle = libssh2_sftp_open(
        m_sftp, 
        remotePath.toUtf8().constData(),
        LIBSSH2_FXF_READ,
        0
    );
    
    if (!fileHandle) {
        char *errmsg;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        setError(tr("Failed to open remote file: %1").arg(errmsg));
        setLoading(false);
        return;
    }
    
    // Open local file
    QFile localFile(localPath);
    if (!localFile.open(QIODevice::WriteOnly)) {
        libssh2_sftp_close(fileHandle);
        setError(tr("Failed to create local file: %1").arg(localFile.errorString()));
        setLoading(false);
        return;
    }
    
    // Download in chunks
    char buffer[32768];
    ssize_t bytesRead;
    
    while ((bytesRead = libssh2_sftp_read(fileHandle, buffer, sizeof(buffer))) > 0) {
        localFile.write(buffer, bytesRead);
        QCoreApplication::processEvents();  // Keep UI responsive
    }
    
    localFile.close();
    libssh2_sftp_close(fileHandle);
    
    if (bytesRead < 0) {
        char *errmsg;
        libssh2_session_last_error(m_session, &errmsg, nullptr, 0);
        setError(tr("Error reading remote file: %1").arg(errmsg));
        setLoading(false);
        return;
    }
    
    m_currentRemotePath = remotePath;
    m_currentLocalPath = localPath;
    
    setLoading(false);
    emit fileReady(localPath, remotePath);
    
    qDebug() << "RemoteFileManager: Downloaded" << remotePath << "to" << localPath;
    
    // Start auto-refresh if enabled
    if (m_autoRefreshEnabled) {
        m_refreshTimer->start(m_autoRefreshInterval * 1000);
    }
#else
    Q_UNUSED(remotePath);
    setError(tr("Remote features not enabled"));
#endif
}

void RemoteFileManager::refreshCurrentFile()
{
    if (m_currentRemotePath.isEmpty() || m_currentLocalPath.isEmpty()) {
        return;
    }
    
#ifdef BFV_REMOTE_FEATURES
    if (!m_isConnected || !m_sftp) {
        m_refreshTimer->stop();
        return;
    }
    
    // Re-download the file
    LIBSSH2_SFTP_HANDLE *fileHandle = libssh2_sftp_open(
        m_sftp,
        m_currentRemotePath.toUtf8().constData(),
        LIBSSH2_FXF_READ,
        0
    );
    
    if (!fileHandle) {
        qWarning() << "RemoteFileManager: Failed to refresh file";
        return;
    }
    
    QFile localFile(m_currentLocalPath);
    if (!localFile.open(QIODevice::WriteOnly)) {
        libssh2_sftp_close(fileHandle);
        return;
    }
    
    char buffer[32768];
    ssize_t bytesRead;
    
    while ((bytesRead = libssh2_sftp_read(fileHandle, buffer, sizeof(buffer))) > 0) {
        localFile.write(buffer, bytesRead);
    }
    
    localFile.close();
    libssh2_sftp_close(fileHandle);
    
    emit fileRefreshed(m_currentLocalPath);
    qDebug() << "RemoteFileManager: Refreshed" << m_currentRemotePath;
#endif
}

// ============================================================================
// Auto-refresh
// ============================================================================

void RemoteFileManager::setAutoRefreshEnabled(bool enabled)
{
    if (m_autoRefreshEnabled != enabled) {
        m_autoRefreshEnabled = enabled;
        
        if (enabled && !m_currentRemotePath.isEmpty()) {
            m_refreshTimer->start(m_autoRefreshInterval * 1000);
        } else {
            m_refreshTimer->stop();
        }
        
        emit autoRefreshChanged();
    }
}

void RemoteFileManager::setAutoRefreshInterval(int seconds)
{
    if (seconds < 1) seconds = 1;
    if (seconds > 300) seconds = 300;
    
    if (m_autoRefreshInterval != seconds) {
        m_autoRefreshInterval = seconds;
        
        if (m_refreshTimer->isActive()) {
            m_refreshTimer->setInterval(seconds * 1000);
        }
        
        emit autoRefreshChanged();
    }
}

// ============================================================================
// Profile Management
// ============================================================================

void RemoteFileManager::saveProfile(const QString &name,
                                     const QString &host,
                                     int port,
                                     const QString &username,
                                     const QString &authMethod,
                                     const QString &keyPath)
{
    QVariantMap profile;
    profile["host"] = host;
    profile["port"] = port;
    profile["username"] = username;
    profile["authMethod"] = authMethod;
    profile["keyPath"] = keyPath;
    
    m_profiles[name] = profile;
    saveProfiles();
    emit profilesChanged();
    
    qDebug() << "RemoteFileManager: Saved profile" << name;
}

void RemoteFileManager::deleteProfile(const QString &name)
{
    if (m_profiles.remove(name)) {
        saveProfiles();
        emit profilesChanged();
        qDebug() << "RemoteFileManager: Deleted profile" << name;
    }
}

QVariantList RemoteFileManager::getProfiles() const
{
    QVariantList list;
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it) {
        QVariantMap profile = it.value();
        profile["name"] = it.key();
        list.append(profile);
    }
    return list;
}

QVariantMap RemoteFileManager::getProfile(const QString &name) const
{
    QVariantMap profile = m_profiles.value(name);
    if (!profile.isEmpty()) {
        profile["name"] = name;
    }
    return profile;
}

void RemoteFileManager::loadProfiles()
{
    m_profiles.clear();
    
    QSettings settings;
    settings.beginGroup("RemoteProfiles");
    QStringList names = settings.childGroups();
    
    for (const QString &name : names) {
        settings.beginGroup(name);
        QVariantMap profile;
        profile["host"] = settings.value("host").toString();
        profile["port"] = settings.value("port", 22).toInt();
        profile["username"] = settings.value("username").toString();
        profile["authMethod"] = settings.value("authMethod", "key").toString();
        profile["keyPath"] = settings.value("keyPath").toString();
        m_profiles[name] = profile;
        settings.endGroup();
    }
    
    settings.endGroup();
    qDebug() << "RemoteFileManager: Loaded" << m_profiles.size() << "profiles";
}

void RemoteFileManager::saveProfiles()
{
    QSettings settings;
    settings.beginGroup("RemoteProfiles");
    
    // Clear existing
    settings.remove("");
    
    for (auto it = m_profiles.begin(); it != m_profiles.end(); ++it) {
        settings.beginGroup(it.key());
        QVariantMap profile = it.value();
        settings.setValue("host", profile["host"]);
        settings.setValue("port", profile["port"]);
        settings.setValue("username", profile["username"]);
        settings.setValue("authMethod", profile["authMethod"]);
        settings.setValue("keyPath", profile["keyPath"]);
        settings.endGroup();
    }
    
    settings.endGroup();
    settings.sync();
}

// ============================================================================
// State Helpers
// ============================================================================

void RemoteFileManager::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}

void RemoteFileManager::setError(const QString &error)
{
    m_lastError = error;
    emit errorOccurred(error);
    qWarning() << "RemoteFileManager:" << error;
}
