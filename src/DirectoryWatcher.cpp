/**
 * @file DirectoryWatcher.cpp
 * @brief 目录监控管理器实现
 */

#include "DirectoryWatcher.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

DirectoryWatcher::DirectoryWatcher(QObject *parent)
    : QObject(parent),
      m_fileFilter({"*.log", "*.txt"})  // Default filter
{
  connect(&m_fsWatcher, &QFileSystemWatcher::directoryChanged,
          this, &DirectoryWatcher::onDirectoryChanged);
  
  connect(&m_pollTimer, &QTimer::timeout,
          this, &DirectoryWatcher::onPollTimer);
}

DirectoryWatcher::~DirectoryWatcher() {
  stopWatching();
}

void DirectoryWatcher::setFileFilter(const QStringList &filter) {
  if (m_fileFilter != filter) {
    m_fileFilter = filter;
    emit fileFilterChanged();
    
    // 如果正在监控，重新扫描
    if (m_isWatching) {
      scanDirectory();
    }
  }
}

void DirectoryWatcher::setPollInterval(int ms) {
  if (m_pollInterval != ms && ms >= 500) {
    m_pollInterval = ms;
    emit pollIntervalChanged();
    
    if (m_pollTimer.isActive()) {
      m_pollTimer.setInterval(ms);
    }
  }
}

QVariantList DirectoryWatcher::watchedFiles() const {
  QVariantList result;
  for (auto it = m_files.constBegin(); it != m_files.constEnd(); ++it) {
    const WatchedFileInfo &info = it.value();
    QVariantMap fileMap;
    fileMap["filePath"] = info.filePath;
    fileMap["fileName"] = info.fileName;
    fileMap["size"] = info.size;
    fileMap["lastModified"] = info.lastModified.toString(Qt::ISODate);
    fileMap["isNew"] = info.isNew;
    result.append(fileMap);
  }
  
  // 按修改时间排序，最新的在前
  std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
    return a.toMap()["lastModified"].toString() > b.toMap()["lastModified"].toString();
  });
  
  return result;
}

bool DirectoryWatcher::startWatching(const QString &directory) {
  QDir dir(directory);
  if (!dir.exists()) {
    qWarning() << "Directory does not exist:" << directory;
    return false;
  }
  
  // 停止之前的监控
  stopWatching();
  
  m_watchedDirectory = dir.absolutePath();
  emit watchedDirectoryChanged();
  
  // 添加目录到文件系统监控
  if (!m_fsWatcher.addPath(m_watchedDirectory)) {
    qWarning() << "Failed to add directory to watcher:" << m_watchedDirectory;
  }
  
  // 初始扫描
  scanDirectory();
  
  // 启动轮询定时器（作为补充）
  m_pollTimer.start(m_pollInterval);
  
  m_isWatching = true;
  emit isWatchingChanged();
  
  qDebug() << "Started watching directory:" << m_watchedDirectory 
           << "filter:" << m_fileFilter 
           << "interval:" << m_pollInterval << "ms";
  
  return true;
}

void DirectoryWatcher::stopWatching() {
  if (!m_isWatching) {
    return;
  }
  
  m_pollTimer.stop();
  
  if (!m_watchedDirectory.isEmpty()) {
    m_fsWatcher.removePath(m_watchedDirectory);
  }
  
  m_files.clear();
  m_watchedDirectory.clear();
  
  m_isWatching = false;
  emit isWatchingChanged();
  emit watchedDirectoryChanged();
  emit watchedFilesChanged();
  
  qDebug() << "Stopped watching directory";
}

void DirectoryWatcher::refresh() {
  if (m_isWatching) {
    scanDirectory();
  }
}

QString DirectoryWatcher::getLatestFile() const {
  if (m_files.isEmpty()) {
    return QString();
  }
  
  QString latestPath;
  QDateTime latestTime;
  
  for (auto it = m_files.constBegin(); it != m_files.constEnd(); ++it) {
    if (it.value().lastModified > latestTime) {
      latestTime = it.value().lastModified;
      latestPath = it.value().filePath;
    }
  }
  
  return latestPath;
}

void DirectoryWatcher::onPollTimer() {
  scanDirectory();
}

void DirectoryWatcher::onDirectoryChanged(const QString &path) {
  Q_UNUSED(path)
  scanDirectory();
}

void DirectoryWatcher::scanDirectory() {
  if (m_watchedDirectory.isEmpty()) {
    return;
  }
  
  QDir dir(m_watchedDirectory);
  if (!dir.exists()) {
    qWarning() << "Watched directory no longer exists:" << m_watchedDirectory;
    stopWatching();
    return;
  }
  
  // 获取匹配的文件
  QFileInfoList entries = dir.entryInfoList(m_fileFilter, QDir::Files | QDir::Readable);
  
  QSet<QString> currentFiles;
  bool filesChanged = false;
  
  for (const QFileInfo &fi : entries) {
    QString filePath = fi.absoluteFilePath();
    currentFiles.insert(filePath);
    
    if (m_files.contains(filePath)) {
      // 检查文件是否被修改
      WatchedFileInfo &existing = m_files[filePath];
      if (existing.lastModified != fi.lastModified() || existing.size != fi.size()) {
        existing.lastModified = fi.lastModified();
        existing.size = fi.size();
        existing.isNew = false;
        emit fileModified(filePath);
        filesChanged = true;
      } else if (existing.isNew) {
        // 不再是新文件
        existing.isNew = false;
        filesChanged = true;
      }
    } else {
      // 新文件
      WatchedFileInfo info;
      info.filePath = filePath;
      info.fileName = fi.fileName();
      info.size = fi.size();
      info.lastModified = fi.lastModified();
      info.isNew = true;
      
      m_files.insert(filePath, info);
      
      qDebug() << "New file detected:" << filePath;
      emit newFileDetected(filePath);
      filesChanged = true;
    }
  }
  
  // 检查删除的文件
  QStringList toRemove;
  for (auto it = m_files.constBegin(); it != m_files.constEnd(); ++it) {
    if (!currentFiles.contains(it.key())) {
      toRemove.append(it.key());
    }
  }
  
  for (const QString &path : toRemove) {
    m_files.remove(path);
    qDebug() << "File removed:" << path;
    emit fileRemoved(path);
    filesChanged = true;
  }
  
  if (filesChanged) {
    emit watchedFilesChanged();
  }
}

bool DirectoryWatcher::matchesFilter(const QString &fileName) const {
  if (m_fileFilter.isEmpty()) {
    return true;
  }
  
  for (const QString &pattern : m_fileFilter) {
    QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(pattern),
                              QRegularExpression::CaseInsensitiveOption);
    if (regex.match(fileName).hasMatch()) {
      return true;
    }
  }
  
  return false;
}
