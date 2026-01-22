/**
 * @file DirectoryWatcher.h
 * @brief 目录监控管理器 - 监控目录中的新文件和变化
 */

#ifndef DIRECTORYWATCHER_H
#define DIRECTORYWATCHER_H

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QMap>
#include <QObject>
#include <QTimer>

/**
 * @brief 文件信息结构体
 */
struct WatchedFileInfo {
  QString filePath;
  QString fileName;
  qint64 size;
  QDateTime lastModified;
  bool isNew;  // 是否是新发现的文件
};

/**
 * @brief 目录监控管理器
 * 
 * 功能：
 * - 监控指定目录中的文件变化
 * - 检测新增的日志文件
 * - 支持文件名过滤（通配符）
 * - 可配置监控间隔
 */
class DirectoryWatcher : public QObject {
  Q_OBJECT
  
  Q_PROPERTY(QString watchedDirectory READ watchedDirectory NOTIFY watchedDirectoryChanged)
  Q_PROPERTY(bool isWatching READ isWatching NOTIFY isWatchingChanged)
  Q_PROPERTY(QStringList fileFilter READ fileFilter WRITE setFileFilter NOTIFY fileFilterChanged)
  Q_PROPERTY(int pollInterval READ pollInterval WRITE setPollInterval NOTIFY pollIntervalChanged)
  Q_PROPERTY(QVariantList watchedFiles READ watchedFiles NOTIFY watchedFilesChanged)

public:
  static DirectoryWatcher &instance() {
    static DirectoryWatcher instance;
    return instance;
  }
  
  explicit DirectoryWatcher(QObject *parent = nullptr);
  ~DirectoryWatcher() override;

  // Properties
  QString watchedDirectory() const { return m_watchedDirectory; }
  bool isWatching() const { return m_isWatching; }
  QStringList fileFilter() const { return m_fileFilter; }
  int pollInterval() const { return m_pollInterval; }
  QVariantList watchedFiles() const;

  // Setters
  void setFileFilter(const QStringList &filter);
  void setPollInterval(int ms);

public slots:
  /**
   * @brief 开始监控目录
   * @param directory 目录路径
   * @return 成功返回 true
   */
  Q_INVOKABLE bool startWatching(const QString &directory);
  
  /**
   * @brief 停止监控
   */
  Q_INVOKABLE void stopWatching();
  
  /**
   * @brief 手动刷新文件列表
   */
  Q_INVOKABLE void refresh();
  
  /**
   * @brief 获取目录中最新的文件
   * @return 最新文件的完整路径
   */
  Q_INVOKABLE QString getLatestFile() const;

signals:
  void watchedDirectoryChanged();
  void isWatchingChanged();
  void fileFilterChanged();
  void pollIntervalChanged();
  void watchedFilesChanged();
  
  /**
   * @brief 发现新文件
   * @param filePath 新文件路径
   */
  void newFileDetected(const QString &filePath);
  
  /**
   * @brief 文件被修改
   * @param filePath 被修改的文件路径
   */
  void fileModified(const QString &filePath);
  
  /**
   * @brief 文件被删除
   * @param filePath 被删除的文件路径
   */
  void fileRemoved(const QString &filePath);

private slots:
  void onPollTimer();
  void onDirectoryChanged(const QString &path);

private:
  void scanDirectory();
  bool matchesFilter(const QString &fileName) const;
  
  QString m_watchedDirectory;
  bool m_isWatching = false;
  QStringList m_fileFilter;  // e.g., {"*.log", "*.txt"}
  int m_pollInterval = 2000; // Default 2 seconds
  
  QFileSystemWatcher m_fsWatcher;
  QTimer m_pollTimer;
  
  QMap<QString, WatchedFileInfo> m_files;  // 已知文件映射
};

#endif // DIRECTORYWATCHER_H
