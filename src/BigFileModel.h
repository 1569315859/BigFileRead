/**
 * @file BigFileModel.h
 * @brief 大文件模型类 - 使用内存映射实现高性能大文件读取
 * @description 继承 QAbstractListModel，通过 QFile::map()
 * 将文件映射到虚拟内存， 配合行偏移索引实现按需读取，支持打开 10GB+ 文本文件。
 * @note 目标架构：仅支持 64 位系统，假设虚拟内存空间足够映射整个文件
 * @version 1.5 - 增加异步索引、分块更新、长行截断
 */

#ifndef BIGFILEMODEL_H
#define BIGFILEMODEL_H

#include <QAbstractListModel>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QStringConverter>
#include <QTimer>
#include <atomic>
#include <vector>


class BigFileModel : public QAbstractListModel {
  Q_OBJECT

public:
  /// UI 刷新间隔（毫秒）- 每秒 10 次更新
  static constexpr int UI_UPDATE_INTERVAL_MS = 200;
  /// Worker 每批次 flush 的行数
  static constexpr int WORKER_CHUNK_SIZE = 1000;
  /// 长行截断阈值
  static constexpr int MAX_DISPLAY_LENGTH = 2000;
  /// 搜索结果最大数量限制（1百万结果 ≈ 4MB RAM）
  static constexpr int MAX_SEARCH_RESULTS = 1000000;
  
  /// 书签数据角色 (用于 data() 返回书签状态)
  static constexpr int BookmarkRole = Qt::UserRole + 5;

  explicit BigFileModel(QObject *parent = nullptr);
  ~BigFileModel() override;

  /**
   * @brief 异步加载文件并建立行索引
   * @param filePath 文件路径
   * @return 成功启动返回 true
   */
  bool loadFile(const QString &filePath);

  /**
   * @brief 关闭当前文件并释放资源
   */
  void closeFile();

  /**
   * @brief 取消正在进行的索引操作
   */
  void cancelIndexing();

  /**
   * @brief 是否正在索引中
   */
  bool isIndexing() const { return m_isIndexing.load(); }

  /**
   * @brief 获取当前文件路径
   */
  QString filePath() const { return m_filePath; }

  /**
   * @brief 获取文件大小（字节）
   */
  qint64 fileSize() const { return m_fileSize; }

  /**
   * @brief 获取当前已索引的行数（受过滤影响）
   */
  int lineCount() const;

  /**
   * @brief 获取总行数（不受过滤影响）
   */
  int totalLineCount() const { return static_cast<int>(m_lineOffsets.size()); }

  /**
   * @brief 异步搜索文本
   * @param text 搜索文本（区分大小写）
   * @param useRegex 是否使用正则表达式
   * @note 使用 QtConcurrent::run 在后台线程执行，不阻塞主线程
   */
  void search(const QString &text, bool useRegex = false);

  /**
   * @brief 取消正在进行的搜索
   */
  void cancelSearch();

  /**
   * @brief 是否正在搜索中
   */
  bool isSearching() const { return m_isSearching.load(); }

  /**
   * @brief 获取搜索结果（行索引列表）
   */
  const std::vector<int> &searchResults() const { return m_searchResults; }

  /**
   * @brief 设置文本编码
   * @param encoding 编码类型 (Utf8, System/Local 等)
   */
  void setEncoding(QStringConverter::Encoding encoding);

  // ============ 日志过滤功能 (Virtual Mapping Vector) ============

  /**
   * @brief 异步应用过滤器
   * @param keyword 过滤关键词（为空则清除过滤）
   * @param useRegex 是否使用正则表达式
   * @note 使用虚拟行映射，不复制数据，内存开销极低
   */
  void applyFilter(const QString &keyword, bool useRegex = false);

  /**
   * @brief 清除过滤器，显示所有行
   */
  void clearFilter();

  /**
   * @brief 取消正在进行的过滤操作
   */
  void cancelFilter();

  /**
   * @brief 是否正在过滤中
   */
  bool isFiltering() const { return m_isFiltering.load(); }

  /**
   * @brief 是否处于过滤模式（显示过滤结果）
   */
  bool isFilterMode() const { return m_filterMode.load(); }

  /**
   * @brief 获取当前过滤关键词
   */
  QString filterKeyword() const { return m_filterKeyword; }

  /**
   * @brief 将视图行号转换为原始行号
   * @param viewRow 视图中的行号
   * @return 原始文件中的行号（如果不在过滤模式，返回 viewRow 本身）
   */
  int toRealRow(int viewRow) const;

  // ============ 书签功能 ============

  /**
   * @brief 切换指定视图行的书签状态
   * @param viewRow 视图中的行号
   * @note 内部会转换为真实行号并存储，过滤时书签不会丢失
   */
  void toggleBookmark(int viewRow);

  /**
   * @brief 检查指定视图行是否已添加书签
   * @param viewRow 视图中的行号
   * @return 如果该行已添加书签返回 true
   */
  bool isBookmarked(int viewRow) const;

  /**
   * @brief 获取当前视图行之后的下一个书签行
   * @param currentViewRow 当前视图行号
   * @return 下一个书签的视图行号，如果没有则返回 -1
   */
  int getNextBookmark(int currentViewRow) const;

  /**
   * @brief 清除所有书签
   */
  void clearAllBookmarks();

  // QAbstractListModel 接口实现
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  // 禁用 fetchMore 机制（我们使用 Timer-Pull 模式主动推送数据）
  bool canFetchMore(const QModelIndex &parent) const override;
  void fetchMore(const QModelIndex &parent) override;

signals:
  /**
   * @brief 索引构建进度信号
   * @param percent 进度百分比 (0-100)
   */
  void indexingProgress(int percent);

  /**
   * @brief 文件加载完成信号
   * @param success 是否成功
   * @param message 状态消息
   */
  void fileLoaded(bool success, const QString &message);

  /**
   * @brief 索引被取消信号
   */
  void indexingCancelled();

  // 内部信号，用于跨线程通信
  void indexingFinished(bool success, const QString &message);

  /**
   * @brief 搜索完成信号
   * @param results 匹配的行索引列表
   */
  void searchFinished(const std::vector<int> &results);

  /**
   * @brief 搜索进度信号
   * @param percent 进度百分比 (0-100)
   */
  void searchProgress(int percent);

  /**
   * @brief 日志追加信号（文件尾部有新内容）
   */
  void logAppended();

  // ============ 过滤信号 ============

  /**
   * @brief 过滤进度信号
   * @param percent 进度百分比 (0-100)
   */
  void filterProgress(int percent);

  /**
   * @brief 过滤完成信号
   * @param matchCount 匹配的行数
   */
  void filterFinished(int matchCount);

private slots:
  /**
   * @brief Timer 超时处理 - 将 pending 数据合并到主模型
   */
  void onUpdateTimerTimeout();

  /**
   * @brief 处理索引完成
   */
  void onIndexingFinished(bool success, const QString &message);

  /**
   * @brief 处理文件变化（用于实时日志监控）
   */
  void onFileChanged(const QString &path);

private:
  /**
   * @brief 异步构建行偏移索引（在后台线程执行）
   */
  void buildIndexAsync();

  /**
   * @brief 获取指定行的文本内容
   * @param row 行号（从 0 开始）
   * @return 该行的 QString 内容（可能被截断）
   */
  QString getLine(int row) const;

  /**
   * @brief 获取指定行的原始长度
   */
  qint64 getLineLength(int row) const;

  /**
   * @brief 异步执行搜索（在后台线程执行）
   * @param searchText 搜索文本
   * @param useRegex 是否使用正则表达式
   */
  void executeSearchAsync(const QString &searchText, bool useRegex);

  /**
   * @brief 异步执行过滤（在后台线程执行）
   * @param keyword 过滤关键词
   * @param useRegex 是否使用正则表达式
   */
  void executeFilterAsync(const QString &keyword, bool useRegex);

private:
  QFile m_file;              ///< 文件对象
  QString m_filePath;        ///< 文件路径
  qint64 m_fileSize = 0;     ///< 文件大小
  uchar *m_mapPtr = nullptr; ///< 内存映射指针

  std::vector<qint64> m_lineOffsets; ///< 主数据：每行起始偏移量（仅主线程访问）
  std::vector<qint64> m_pendingOffsets; ///< 待合并数据：后台线程写入
  mutable QMutex m_pendingMutex;        ///< 仅保护 m_pendingOffsets

  std::atomic<bool> m_isIndexing{false};      ///< 是否正在索引
  std::atomic<bool> m_cancelRequested{false}; ///< 是否请求取消
  std::atomic<int> m_lastReportedPercent{0};  ///< 上次报告的进度百分比

  QTimer *m_refreshTimer = nullptr;     ///< Pull Timer（每 50ms 触发）
  QFutureWatcher<void> m_futureWatcher; ///< 异步任务监视器

  // 搜索相关
  std::vector<int> m_searchResults;                 ///< 搜索结果（行索引列表）
  std::atomic<bool> m_isSearching{false};           ///< 是否正在搜索
  std::atomic<bool> m_searchCancelRequested{false}; ///< 是否请求取消搜索
  QFutureWatcher<void> m_searchWatcher;             ///< 搜索任务监视器

  // 文本编码
  mutable QStringDecoder m_decoder{QStringConverter::Utf8};  ///< 字符解码器（默认 UTF-8）

  // 文件监控（实时日志）
  QFileSystemWatcher *m_watcher = nullptr;  ///< 文件变化监视器

  // ============ 日志过滤 (Virtual Mapping Vector) ============
  std::vector<int> m_filteredRows;                ///< 虚拟映射向量：存储匹配行的原始索引
  QString m_filterKeyword;                        ///< 当前过滤关键词
  std::atomic<bool> m_isFiltering{false};         ///< 是否正在执行过滤
  std::atomic<bool> m_filterCancelRequested{false}; ///< 是否请求取消过滤
  std::atomic<bool> m_filterMode{false};          ///< 是否处于过滤模式
  QFutureWatcher<void> m_filterWatcher;           ///< 过滤任务监视器

  // ============ 书签功能 ============
  QSet<qint64> m_bookmarks;  ///< 书签集合（存储原始行索引，过滤时保持有效）
};

#endif // BIGFILEMODEL_H
