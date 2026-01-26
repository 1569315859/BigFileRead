/**
 * @file SessionRecovery.cpp
 * @brief Session Recovery Manager Implementation
 */

#include "SessionRecovery.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>

// ============== SessionState Implementation ==============

QJsonObject SessionState::toJson() const
{
    QJsonObject obj;
    obj["sessionId"] = sessionId;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["openFiles"] = QJsonArray::fromStringList(openFiles);
    obj["activeFile"] = activeFile;
    obj["scrollPositions"] = QJsonObject::fromVariantMap(scrollPositions);
    obj["bookmarks"] = QJsonObject::fromVariantMap(bookmarks);
    obj["searchHistory"] = QJsonObject::fromVariantMap(searchHistory);
    obj["filterHistory"] = QJsonObject::fromVariantMap(filterHistory);
    obj["currentFilter"] = currentFilter;
    obj["hasUnsavedChanges"] = hasUnsavedChanges;
    return obj;
}

SessionState SessionState::fromJson(const QJsonObject &obj)
{
    SessionState state;
    state.sessionId = obj["sessionId"].toString();
    state.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    
    QJsonArray filesArray = obj["openFiles"].toArray();
    for (const QJsonValue &v : filesArray) {
        state.openFiles << v.toString();
    }
    
    state.activeFile = obj["activeFile"].toString();
    state.scrollPositions = obj["scrollPositions"].toObject().toVariantMap();
    state.bookmarks = obj["bookmarks"].toObject().toVariantMap();
    state.searchHistory = obj["searchHistory"].toObject().toVariantMap();
    state.filterHistory = obj["filterHistory"].toObject().toVariantMap();
    state.currentFilter = obj["currentFilter"].toString();
    state.hasUnsavedChanges = obj["hasUnsavedChanges"].toBool();
    
    return state;
}

// ============== SessionRecovery Implementation ==============

SessionRecovery& SessionRecovery::instance()
{
    static SessionRecovery instance;
    return instance;
}

SessionRecovery::SessionRecovery(QObject *parent)
    : QObject(parent)
    , m_autoSaveTimer(new QTimer(this))
{
    // Generate session ID
    m_currentState.sessionId = QUuid::createUuid().toString(QUuid::Id128);
    m_currentState.timestamp = QDateTime::currentDateTime();
    
    loadSettings();
    
    // Check for recoverable session before writing lock file
    checkForRecoverableSession();
    
    // Write lock file to indicate active session
    writeLockFile();
    
    // Setup auto-save timer
    connect(m_autoSaveTimer, &QTimer::timeout, this, &SessionRecovery::onAutoSave);
    if (m_autoSaveEnabled) {
        m_autoSaveTimer->start(m_autoSaveIntervalSecs * 1000);
    }
    
    // Clean up old sessions
    cleanupOldSessions();
}

SessionRecovery::~SessionRecovery()
{
    // Save final state
    saveSession();
    
    // Remove lock file on clean exit
    removeLockFile();
}

void SessionRecovery::loadSettings()
{
    QSettings settings;
    settings.beginGroup("SessionRecovery");
    
    m_autoSaveEnabled = settings.value("autoSaveEnabled", true).toBool();
    m_autoSaveIntervalSecs = settings.value("autoSaveIntervalSecs", 30).toInt();
    m_maxBackupCount = settings.value("maxBackupCount", 10).toInt();
    
    settings.endGroup();
}

void SessionRecovery::saveSettings()
{
    QSettings settings;
    settings.beginGroup("SessionRecovery");
    
    settings.setValue("autoSaveEnabled", m_autoSaveEnabled);
    settings.setValue("autoSaveIntervalSecs", m_autoSaveIntervalSecs);
    settings.setValue("maxBackupCount", m_maxBackupCount);
    
    settings.endGroup();
}

QString SessionRecovery::getSessionDir() const
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString sessionDir = appDir + "/sessions";
    QDir().mkpath(sessionDir);
    return sessionDir;
}

QString SessionRecovery::getCurrentSessionPath() const
{
    return getSessionDir() + "/current_session.json";
}

QString SessionRecovery::getLockFilePath() const
{
    return getSessionDir() + "/session.lock";
}

void SessionRecovery::writeLockFile()
{
    QFile lockFile(getLockFilePath());
    if (lockFile.open(QIODevice::WriteOnly)) {
        QJsonObject lock;
        lock["pid"] = static_cast<qint64>(QCoreApplication::applicationPid());
        lock["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        lock["sessionId"] = m_currentState.sessionId;
        
        lockFile.write(QJsonDocument(lock).toJson());
        lockFile.close();
    }
}

void SessionRecovery::removeLockFile()
{
    QFile::remove(getLockFilePath());
}

bool SessionRecovery::isLockFileStale() const
{
    QFile lockFile(getLockFilePath());
    if (!lockFile.exists()) {
        return true;
    }
    
    if (!lockFile.open(QIODevice::ReadOnly)) {
        return true;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(lockFile.readAll());
    lockFile.close();
    
    if (!doc.isObject()) {
        return true;
    }
    
    QJsonObject lock = doc.object();
    qint64 pid = lock["pid"].toInteger();
    
    // Check if process with that PID still exists
    // On Windows, we can try to open the process
#ifdef Q_OS_WIN
    // Simple check: if file is older than 5 minutes and process doesn't respond, consider stale
    QDateTime lockTime = QDateTime::fromString(lock["timestamp"].toString(), Qt::ISODate);
    if (lockTime.secsTo(QDateTime::currentDateTime()) > 300) {
        // Lock file is old, likely stale
        return true;
    }
#endif
    
    // If current process, not stale
    if (pid == QCoreApplication::applicationPid()) {
        return false;
    }
    
    // Assume stale if different PID and file exists
    // (In production, should check if process is actually running)
    return true;
}

void SessionRecovery::checkForRecoverableSession()
{
    QString sessionPath = getCurrentSessionPath();
    QString lockPath = getLockFilePath();
    
    // If lock file exists and is stale, we have a crash recovery opportunity
    if (QFileInfo::exists(lockPath) && isLockFileStale()) {
        if (QFileInfo::exists(sessionPath)) {
            QFile file(sessionPath);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                file.close();
                
                if (doc.isObject()) {
                    m_recoverableState = SessionState::fromJson(doc.object());
                    
                    // Only offer recovery if there were actually open files
                    if (!m_recoverableState.openFiles.isEmpty()) {
                        m_hasRecoverableSession = true;
                        emit recoverableSessionChanged();
                    }
                }
            }
        }
    }
}

void SessionRecovery::cleanupOldSessions()
{
    QDir sessionDir(getSessionDir());
    QStringList backupFiles = sessionDir.entryList(QStringList() << "session_backup_*.json", QDir::Files, QDir::Time);
    
    // Remove old backups beyond max count
    while (backupFiles.size() > m_maxBackupCount) {
        QString oldestBackup = backupFiles.takeLast();
        QFile::remove(sessionDir.filePath(oldestBackup));
    }
}

bool SessionRecovery::hasRecoverableSession() const
{
    return m_hasRecoverableSession;
}

QString SessionRecovery::recoveryInfo() const
{
    if (!m_hasRecoverableSession) {
        return QString();
    }
    
    QString info = tr("Session from %1\n%2 file(s) were open")
                       .arg(m_recoverableState.timestamp.toString("yyyy-MM-dd hh:mm"))
                       .arg(m_recoverableState.openFiles.size());
    
    if (m_recoverableState.hasUnsavedChanges) {
        info += tr("\n(May have unsaved changes)");
    }
    
    return info;
}

void SessionRecovery::setAutoSaveEnabled(bool enabled)
{
    if (m_autoSaveEnabled != enabled) {
        m_autoSaveEnabled = enabled;
        
        if (enabled) {
            m_autoSaveTimer->start(m_autoSaveIntervalSecs * 1000);
        } else {
            m_autoSaveTimer->stop();
        }
        
        saveSettings();
        emit settingsChanged();
    }
}

void SessionRecovery::setAutoSaveIntervalSecs(int secs)
{
    if (secs < 10) secs = 10;  // Minimum 10 seconds
    if (secs > 600) secs = 600; // Maximum 10 minutes
    
    if (m_autoSaveIntervalSecs != secs) {
        m_autoSaveIntervalSecs = secs;
        
        if (m_autoSaveEnabled) {
            m_autoSaveTimer->setInterval(secs * 1000);
        }
        
        saveSettings();
        emit settingsChanged();
    }
}

void SessionRecovery::setMaxBackupCount(int count)
{
    if (count < 1) count = 1;
    if (count > 100) count = 100;
    
    if (m_maxBackupCount != count) {
        m_maxBackupCount = count;
        saveSettings();
        cleanupOldSessions();
        emit settingsChanged();
    }
}

void SessionRecovery::saveSession()
{
    m_currentState.timestamp = QDateTime::currentDateTime();
    
    QString sessionPath = getCurrentSessionPath();
    QFile file(sessionPath);
    
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(m_currentState.toJson());
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        
        emit sessionSaved();
    }
}

void SessionRecovery::recoverSession()
{
    if (!m_hasRecoverableSession) {
        return;
    }
    
    // Restore open files
    for (const QString &filePath : m_recoverableState.openFiles) {
        if (QFileInfo::exists(filePath)) {
            emit requestOpenFile(filePath);
        }
    }
    
    // Restore scroll positions
    for (auto it = m_recoverableState.scrollPositions.begin(); 
         it != m_recoverableState.scrollPositions.end(); ++it) {
        emit requestScrollTo(it.key(), it.value().toInt());
    }
    
    // Restore bookmarks
    for (auto it = m_recoverableState.bookmarks.begin();
         it != m_recoverableState.bookmarks.end(); ++it) {
        QVariantList bookmarkList = it.value().toList();
        for (const QVariant &bm : bookmarkList) {
            QVariantMap bmMap = bm.toMap();
            emit requestRestoreBookmark(it.key(), bmMap["line"].toInt(), bmMap["comment"].toString());
        }
    }
    
    // Restore filter
    if (!m_recoverableState.currentFilter.isEmpty()) {
        emit requestApplyFilter(m_recoverableState.currentFilter);
    }
    
    // Copy recoverable state to current state
    m_currentState.openFiles = m_recoverableState.openFiles;
    m_currentState.scrollPositions = m_recoverableState.scrollPositions;
    m_currentState.bookmarks = m_recoverableState.bookmarks;
    m_currentState.searchHistory = m_recoverableState.searchHistory;
    m_currentState.filterHistory = m_recoverableState.filterHistory;
    m_currentState.currentFilter = m_recoverableState.currentFilter;
    
    // Clear recoverable state
    m_hasRecoverableSession = false;
    m_recoverableState = SessionState();
    
    emit recoverableSessionChanged();
    emit sessionRecovered(m_currentState.toJson().toVariantMap());
}

void SessionRecovery::discardRecovery()
{
    m_hasRecoverableSession = false;
    m_recoverableState = SessionState();
    emit recoverableSessionChanged();
}

void SessionRecovery::clearAllSessions()
{
    QDir sessionDir(getSessionDir());
    QStringList files = sessionDir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString &file : files) {
        if (file != "current_session.json") {
            QFile::remove(sessionDir.filePath(file));
        }
    }
}

void SessionRecovery::trackOpenFile(const QString &filePath)
{
    if (!m_currentState.openFiles.contains(filePath)) {
        m_currentState.openFiles << filePath;
    }
}

void SessionRecovery::trackCloseFile(const QString &filePath)
{
    m_currentState.openFiles.removeAll(filePath);
    m_currentState.scrollPositions.remove(filePath);
    m_currentState.bookmarks.remove(filePath);
}

void SessionRecovery::trackActiveFile(const QString &filePath)
{
    m_currentState.activeFile = filePath;
}

void SessionRecovery::trackScrollPosition(const QString &filePath, int lineNumber)
{
    m_currentState.scrollPositions[filePath] = lineNumber;
}

void SessionRecovery::trackBookmark(const QString &filePath, int lineNumber, const QString &comment)
{
    QVariantList bookmarks = m_currentState.bookmarks[filePath].toList();
    
    // Check if bookmark already exists
    for (int i = 0; i < bookmarks.size(); ++i) {
        QVariantMap bm = bookmarks[i].toMap();
        if (bm["line"].toInt() == lineNumber) {
            bm["comment"] = comment;
            bookmarks[i] = bm;
            m_currentState.bookmarks[filePath] = bookmarks;
            return;
        }
    }
    
    // Add new bookmark
    QVariantMap newBookmark;
    newBookmark["line"] = lineNumber;
    newBookmark["comment"] = comment;
    bookmarks << newBookmark;
    m_currentState.bookmarks[filePath] = bookmarks;
}

void SessionRecovery::trackRemoveBookmark(const QString &filePath, int lineNumber)
{
    QVariantList bookmarks = m_currentState.bookmarks[filePath].toList();
    
    for (int i = 0; i < bookmarks.size(); ++i) {
        QVariantMap bm = bookmarks[i].toMap();
        if (bm["line"].toInt() == lineNumber) {
            bookmarks.removeAt(i);
            break;
        }
    }
    
    if (bookmarks.isEmpty()) {
        m_currentState.bookmarks.remove(filePath);
    } else {
        m_currentState.bookmarks[filePath] = bookmarks;
    }
}

void SessionRecovery::trackFilter(const QString &filter)
{
    m_currentState.currentFilter = filter;
    
    // Add to filter history
    QVariantList history = m_currentState.filterHistory["recent"].toList();
    history.removeAll(filter);
    history.prepend(filter);
    while (history.size() > 20) {
        history.removeLast();
    }
    m_currentState.filterHistory["recent"] = history;
}

void SessionRecovery::trackSearch(const QString &searchText)
{
    QVariantList history = m_currentState.searchHistory["recent"].toList();
    history.removeAll(searchText);
    history.prepend(searchText);
    while (history.size() > 20) {
        history.removeLast();
    }
    m_currentState.searchHistory["recent"] = history;
}

void SessionRecovery::trackUnsavedChanges(bool hasChanges)
{
    m_currentState.hasUnsavedChanges = hasChanges;
}

QVariantList SessionRecovery::getSessionBackups() const
{
    QVariantList backups;
    QDir sessionDir(getSessionDir());
    QStringList backupFiles = sessionDir.entryList(QStringList() << "session_backup_*.json", QDir::Files, QDir::Time);
    
    for (const QString &file : backupFiles) {
        QFile f(sessionDir.filePath(file));
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            
            if (doc.isObject()) {
                SessionState state = SessionState::fromJson(doc.object());
                QVariantMap backup;
                backup["sessionId"] = state.sessionId;
                backup["timestamp"] = state.timestamp;
                backup["fileCount"] = state.openFiles.size();
                backup["fileName"] = file;
                backups << backup;
            }
        }
    }
    
    return backups;
}

QVariantMap SessionRecovery::getSessionInfo(const QString &sessionId) const
{
    QDir sessionDir(getSessionDir());
    QStringList files = sessionDir.entryList(QStringList() << "*.json", QDir::Files);
    
    for (const QString &file : files) {
        QFile f(sessionDir.filePath(file));
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            
            if (doc.isObject()) {
                SessionState state = SessionState::fromJson(doc.object());
                if (state.sessionId == sessionId) {
                    return state.toJson().toVariantMap();
                }
            }
        }
    }
    
    return QVariantMap();
}

bool SessionRecovery::restoreSession(const QString &sessionId)
{
    QVariantMap sessionInfo = getSessionInfo(sessionId);
    if (sessionInfo.isEmpty()) {
        emit recoveryFailed(tr("Session not found"));
        return false;
    }
    
    m_recoverableState = SessionState::fromJson(QJsonObject::fromVariantMap(sessionInfo));
    m_hasRecoverableSession = true;
    emit recoverableSessionChanged();
    
    recoverSession();
    return true;
}

bool SessionRecovery::deleteSession(const QString &sessionId)
{
    QDir sessionDir(getSessionDir());
    QStringList files = sessionDir.entryList(QStringList() << "session_backup_*.json", QDir::Files);
    
    for (const QString &file : files) {
        QFile f(sessionDir.filePath(file));
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            f.close();
            
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj["sessionId"].toString() == sessionId) {
                    return QFile::remove(sessionDir.filePath(file));
                }
            }
        }
    }
    
    return false;
}

void SessionRecovery::onAutoSave()
{
    saveSession();
    
    // Also create a backup
    QString backupPath = getSessionDir() + QString("/session_backup_%1.json")
                             .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    
    QFile::copy(getCurrentSessionPath(), backupPath);
    
    cleanupOldSessions();
}
