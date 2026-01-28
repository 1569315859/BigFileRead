/**
 * @file BigFileModel.h
 * @brief 大文件模型类 - 使用内存映射实现高性能大文件读取
 * @description 继承 QAbstractTableModel，通过 QFile::map()
 * 将文件映射到虚拟内存， 配合行偏移索引实现按需读取，支持打开 10GB+ 文本文件。
 * @note 目标架构：仅支持 64 位系统，假设虚拟内存空间足够映射整个文件
 * @version 2.0 - Upgraded to TableModel with lazy log parsing support
 */

#ifndef BIGFILEMODEL_H
#define BIGFILEMODEL_H

#include <QAbstractTableModel>
#include <QCache>
#include <QDateTime>
#include <QFile>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QMap>
#include <QMutex>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringConverter>
#include <QTimer>
#include <atomic>
#include <vector>

/**
 * @brief 书签信息结构体 - 支持评论和时间戳
 */
struct BookmarkInfo {
  Q_GADGET
  Q_PROPERTY(qint64 lineIndex MEMBER lineIndex)
  Q_PROPERTY(QString comment MEMBER comment)
  Q_PROPERTY(QDateTime createdAt MEMBER createdAt)
public:
  qint64 lineIndex = -1; ///< 真实行索引（0-based）
  QString comment;       ///< 书签评论/备注
  QDateTime createdAt;   ///< 创建时间

  BookmarkInfo() = default;
  BookmarkInfo(qint64 line, const QString &note = QString())
      : lineIndex(line), comment(note),
        createdAt(QDateTime::currentDateTime()) {}
};

/**
 * @brief 日志分组信息结构体 - 用于日志条目分组显示
 */
struct GroupInfo {
  Q_GADGET
  Q_PROPERTY(int id MEMBER id)
  Q_PROPERTY(QString field MEMBER field)
  Q_PROPERTY(QString value MEMBER value)
  Q_PROPERTY(int startRow MEMBER startRow)
  Q_PROPERTY(int endRow MEMBER endRow)
  Q_PROPERTY(int count MEMBER count)
  Q_PROPERTY(bool isExpanded MEMBER isExpanded)
public:
  int id = -1;            ///< 分组ID
  QString field;          ///< 分组字段名
  QString value;          ///< 分组值
  int startRow = 0;       ///< 起始行号
  int endRow = 0;         ///< 结束行号
  int count = 0;          ///< 该分组内条目数
  bool isExpanded = true; ///< 是否展开

  GroupInfo() = default;
  GroupInfo(int gid, const QString &f, const QString &v, int start, int end,
            int cnt)
      : id(gid), field(f), value(v), startRow(start), endRow(end), count(cnt),
        isExpanded(true) {}
};

class BigFileModel : public QAbstractTableModel {
  Q_OBJECT

  // ============ Q_PROPERTY 用于 QML 绑定 ============
  Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
  Q_PROPERTY(qint64 fileSize READ fileSize NOTIFY fileSizeChanged)
  Q_PROPERTY(int lineCount READ lineCount NOTIFY lineCountChanged)
  Q_PROPERTY(
      int totalLineCount READ totalLineCount NOTIFY totalLineCountChanged)
  Q_PROPERTY(bool isFilterMode READ isFilterMode NOTIFY filterModeChanged)
  Q_PROPERTY(bool isIndexing READ isIndexing NOTIFY indexingStateChanged)
  Q_PROPERTY(int searchResultCount READ searchResultCount NOTIFY
                 searchResultCountChanged)
  Q_PROPERTY(
      QVariantList bookmarkLines READ bookmarkLines NOTIFY bookmarksChanged)
  Q_PROPERTY(QVariantList searchResultLines READ searchResultLines NOTIFY
                 searchResultCountChanged)
  Q_PROPERTY(
      QVariantList errorLines READ errorLines NOTIFY logLevelLinesChanged)
  Q_PROPERTY(
      QVariantList warningLines READ warningLines NOTIFY logLevelLinesChanged)
  Q_PROPERTY(QVariantList infoLines READ infoLines NOTIFY logLevelLinesChanged)
  Q_PROPERTY(bool deltaTimeEnabled READ deltaTimeEnabled WRITE
                 setDeltaTimeEnabled NOTIFY deltaTimeEnabledChanged)

  // ============ 分组功能属性 ============
  Q_PROPERTY(bool isGroupMode READ isGroupMode NOTIFY groupModeChanged)
  Q_PROPERTY(QVariantList groups READ getGroups NOTIFY groupsChanged)
  Q_PROPERTY(QString groupField READ groupField NOTIFY groupsChanged)

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
  /// 原始行号角色 (返回未过滤的真实行号)
  static constexpr int RealRowRole = Qt::UserRole + 6;
  /// 原始行文本角色 (返回完整的未解析行文本)
  static constexpr int RawLineRole = Qt::UserRole + 7;
  /// 日志级别角色 (返回检测到的日志级别)
  static constexpr int LogLevelRole = Qt::UserRole + 8;
  /// Delta 时间角色 (返回与上一行的时间差，毫秒)
  static constexpr int DeltaTimeRole = Qt::UserRole + 9;
  /// 时间戳角色 (返回解析的时间戳)
  static constexpr int TimestampRole = Qt::UserRole + 10;

  explicit BigFileModel(QObject *parent = nullptr);
  ~BigFileModel() override;

  /**
   * @brief 异步加载文件并建立行索引
   * @param filePath 文件路径
   * @return 成功启动返回 true
   */
  Q_INVOKABLE bool loadFile(const QString &filePath);

  /**
   * @brief 关闭当前文件并释放资源
   */
  Q_INVOKABLE void closeFile();

  /**
   * @brief 取消正在进行的索引操作
   */
  Q_INVOKABLE void cancelIndexing();

  /**
   * @brief 是否正在索引中
   */
  Q_INVOKABLE bool isIndexing() const { return m_isIndexing.load(); }

  /**
   * @brief 获取当前文件路径
   */
  Q_INVOKABLE QString filePath() const { return m_filePath; }

  /**
   * @brief 获取文件大小（字节）
   */
  Q_INVOKABLE qint64 fileSize() const { return m_fileSize; }

  /**
   * @brief 获取当前已索引的行数（受过滤影响）
   */
  Q_INVOKABLE int lineCount() const;

  /**
   * @brief 获取总行数（不受过滤影响）
   */
  Q_INVOKABLE int totalLineCount() const {
    return static_cast<int>(m_lineOffsets.size());
  }

  /**
   * @brief 异步搜索文本
   * @param text 搜索文本（区分大小写）
   * @param useRegex 是否使用正则表达式
   * @note 使用 QtConcurrent::run 在后台线程执行，不阻塞主线程
   */
  Q_INVOKABLE void search(const QString &text, bool useRegex = false);

  /**
   * @brief 取消正在进行的搜索
   */
  Q_INVOKABLE void cancelSearch();

  /**
   * @brief 是否正在搜索中
   */
  Q_INVOKABLE bool isSearching() const { return m_isSearching.load(); }

  /**
   * @brief 获取搜索结果（行索引列表）
   */
  const std::vector<int> &searchResults() const { return m_searchResults; }

  /**
   * @brief 获取下一个搜索结果的行号
   * @param currentViewRow 当前视图行号
   * @return 下一个匹配行的视图行号，如果没有则返回 -1
   */
  Q_INVOKABLE int nextSearchResult(int currentViewRow) const;

  /**
   * @brief 获取上一个搜索结果的行号
   * @param currentViewRow 当前视图行号
   * @return 上一个匹配行的视图行号，如果没有则返回 -1
   */
  Q_INVOKABLE int prevSearchResult(int currentViewRow) const;

  /**
   * @brief 获取搜索结果数量
   */
  Q_INVOKABLE int searchResultCount() const {
    return static_cast<int>(m_searchResults.size());
  }

  /**
   * @brief 设置文本编码
   * @param encoding 编码类型 (Utf8, System/Local 等)
   */
  void setEncoding(QStringConverter::Encoding encoding);

  // ============ 表格模式控制 ============

  /**
   * @brief 启用表格解析模式
   * @param enabled true = 使用 LogParser 解析列, false = 单列原始文本
   */
  Q_INVOKABLE void setTableModeEnabled(bool enabled);

  /**
   * @brief 是否处于表格解析模式
   */
  Q_INVOKABLE bool isTableModeEnabled() const { return m_tableModeEnabled; }

  /**
   * @brief 刷新表格结构（列变化后调用）
   */
  void refreshTableStructure();

  // ============ 日志过滤功能 (Virtual Mapping Vector) ============

  /**
   * @brief 异步应用过滤器
   * @param keyword 过滤关键词（为空则清除过滤）
   * @param useRegex 是否使用正则表达式
   * @note 使用虚拟行映射，不复制数据，内存开销极低
   */
  Q_INVOKABLE void applyFilter(const QString &keyword, bool useRegex = false);

  /**
   * @brief 应用高级过滤器（支持日志级别和多关键词）
   * @param levels 日志级别列表（空列表表示不过滤）
   * @param keywords 关键词列表
   * @param andLogic true = 所有关键词都必须匹配, false = 任意关键词匹配即可
   * @param useRegex 是否使用正则表达式
   */
  Q_INVOKABLE void applyAdvancedFilter(const QStringList &levels,
                                       const QStringList &keywords,
                                       bool andLogic = true,
                                       bool useRegex = false);

  /**
   * @brief 清除过滤器，显示所有行
   */
  Q_INVOKABLE void clearFilter();

  /**
   * @brief 取消正在进行的过滤操作
   */
  Q_INVOKABLE void cancelFilter();

  /**
   * @brief 是否正在过滤中
   */
  Q_INVOKABLE bool isFiltering() const { return m_isFiltering.load(); }

  /**
   * @brief 是否处于过滤模式（显示过滤结果）
   */
  Q_INVOKABLE bool isFilterMode() const { return m_filterMode.load(); }

  /**
   * @brief 获取当前过滤关键词
   */
  Q_INVOKABLE QString filterKeyword() const { return m_filterKeyword; }

  /**
   * @brief 将视图行号转换为原始行号
   * @param viewRow 视图中的行号
   * @return 原始文件中的行号（如果不在过滤模式，返回 viewRow 本身）
   */
  Q_INVOKABLE int toRealRow(int viewRow) const;

  // ============ 书签功能 ============

  /**
   * @brief 切换指定视图行的书签状态
   * @param viewRow 视图中的行号
   * @note 内部会转换为真实行号并存储，过滤时书签不会丢失
   */
  Q_INVOKABLE void toggleBookmark(int viewRow);

  /**
   * @brief 检查指定视图行是否已添加书签
   * @param viewRow 视图中的行号
   * @return 如果该行已添加书签返回 true
   */
  Q_INVOKABLE bool isBookmarked(int viewRow) const;

  /**
   * @brief 获取当前视图行之后的下一个书签行
   * @param currentViewRow 当前视图行号
   * @return 下一个书签的视图行号，如果没有则返回 -1
   */
  Q_INVOKABLE int getNextBookmark(int currentViewRow) const;

  /**
   * @brief 获取当前视图行之前的上一个书签行
   * @param currentViewRow 当前视图行号
   * @return 上一个书签的视图行号，如果没有则返回 -1
   */
  Q_INVOKABLE int getPrevBookmark(int currentViewRow) const;

  /**
   * @brief 清除所有书签
   */
  Q_INVOKABLE void clearAllBookmarks();

  /**
   * @brief 设置书签评论
   * @param viewRow 视图中的行号
   * @param comment 评论内容
   */
  Q_INVOKABLE void setBookmarkComment(int viewRow, const QString &comment);

  /**
   * @brief 获取书签评论
   * @param viewRow 视图中的行号
   * @return 评论内容，如果无书签返回空字符串
   */
  Q_INVOKABLE QString getBookmarkComment(int viewRow) const;

  /**
   * @brief 获取所有书签信息列表（用于书签面板）
   * @return 包含行号、评论、时间的 QVariantList
   */
  Q_INVOKABLE QVariantList getAllBookmarks() const;

  /**
   * @brief 保存书签到文件
   * @param path 书签文件路径（.json 格式）
   * @return 成功返回 true
   */
  Q_INVOKABLE bool saveBookmarksToFile(const QString &path);

  /**
   * @brief 从文件加载书签
   * @param path 书签文件路径
   * @return 成功返回 true
   */
  Q_INVOKABLE bool loadBookmarksFromFile(const QString &path);

  /**
   * @brief 自动保存书签到默认位置（与文件同目录）
   * @return 成功返回 true
   */
  Q_INVOKABLE bool autoSaveBookmarks();

  /**
   * @brief 自动加载默认位置的书签
   * @return 成功返回 true
   */
  Q_INVOKABLE bool autoLoadBookmarks();

  // ============ 导出功能 ============

  /**
   * @brief 导出当前视图数据到 CSV 文件
   * @param path 目标文件路径
   * @param startRow 起始行（视图行号，0-based）
   * @param endRow 结束行（视图行号，-1 表示全部）
   * @param includeBookmarksOnly 是否仅导出书签行
   * @param sanitizationLevel 脱敏级别: -1=禁用, 0=Minimal, 1=Standard, 2=Strict
   * @return 成功返回 true
   */
  Q_INVOKABLE bool exportToCSV(const QString &path, int startRow = 0,
                               int endRow = -1,
                               bool includeBookmarksOnly = false,
                               int sanitizationLevel = -1);

  /**
   * @brief 导出当前视图数据到 HTML 文件
   * @param path 目标文件路径
   * @param startRow 起始行（视图行号，0-based）
   * @param endRow 结束行（视图行号，-1 表示全部）
   * @param includeBookmarksOnly 是否仅导出书签行
   * @param sanitizationLevel 脱敏级别: -1=禁用, 0=Minimal, 1=Standard, 2=Strict
   * @return 成功返回 true
   */
  Q_INVOKABLE bool exportToHTML(const QString &path, int startRow = 0,
                                int endRow = -1,
                                bool includeBookmarksOnly = false,
                                int sanitizationLevel = -1);

  /**
   * @brief 导出数据并进行脱敏处理
   * @param path 目标文件路径
   * @param format 导出格式: "csv", "html", "txt"
   * @param sanitizeOptions 脱敏选项 (键: 规则名称, 值: 是否启用)
   * @param startRow 起始行（视图行号，0-based）
   * @param endRow 结束行（视图行号，-1 表示全部）
   * @return 成功返回 true
   */
  Q_INVOKABLE bool exportWithSanitization(const QString &path,
                                          const QString &format,
                                          const QVariantMap &sanitizeOptions,
                                          int startRow = 0, int endRow = -1);

  /**
   * @brief 从剪贴板文本创建临时文件并加载
   * @return 成功返回 true
   */
  Q_INVOKABLE bool loadFromClipboard();

  /**
   * @brief 获取所有书签行号列表（用于 NavigationBar）
   * @return 书签行号的 QVariantList
   */
  QVariantList bookmarkLines() const;

  /**
   * @brief 获取所有搜索结果行号列表（用于 NavigationBar）
   * @return 搜索结果行号的 QVariantList
   */
  QVariantList searchResultLines() const;

  /**
   * @brief 获取包含错误关键字的行号列表
   * @return 错误行号的 QVariantList
   */
  Q_INVOKABLE QVariantList errorLines() const;

  /**
   * @brief 获取包含警告关键字的行号列表
   * @return 警告行号的 QVariantList
   */
  Q_INVOKABLE QVariantList warningLines() const;

  /**
   * @brief 获取包含信息关键字的行号列表
   * @return 信息行号的 QVariantList
   */
  Q_INVOKABLE QVariantList infoLines() const;

  /**
   * @brief 获取日志级别统计信息
   * @return 包含各级别计数的 QVariantMap
   */
  Q_INVOKABLE QVariantMap getLogStatistics() const;

  /**
   * @brief 获取按时间段分组的日志统计信息（用于仪表盘图表）
   * @param interval 时间间隔: "minute", "hour", "day"
   * @param maxBuckets 最大时间桶数量（限制返回数据量，默认100）
   * @return QVariantList，每个元素包含 {timestamp, error, warn, info, debug,
   * trace, total}
   */
  Q_INVOKABLE QVariantList getTimeBasedStatistics(const QString &interval,
                                                  int maxBuckets = 100) const;

  /**
   * @brief 获取日志时间范围
   * @return QVariantMap 包含 {startTime, endTime} 的 QDateTime
   */
  Q_INVOKABLE QVariantMap getLogTimeRange() const;

  /**
   * @brief 搜索包含指定关键字的行（用于标记）
   * @param keywords 关键字列表
   * @param maxResults 最大结果数量（限制性能开销）
   * @return 匹配行号列表
   */
  Q_INVOKABLE QVariantList findLinesWithKeywords(const QStringList &keywords,
                                                 int maxResults = 5000) const;

  // ============ 日志分组功能 ============

  /**
   * @brief 是否处于分组模式
   */
  Q_INVOKABLE bool isGroupMode() const { return m_isGroupMode; }

  /**
   * @brief 获取当前分组字段
   */
  Q_INVOKABLE QString groupField() const { return m_groupField; }

  /**
   * @brief 按指定字段对日志进行分组
   * @param field 分组字段: "timestamp" (按时间段), "level" (按日志级别),
   *              "thread" (按线程ID), "custom:<pattern>" (自定义正则)
   * @param interval 时间分组间隔（仅 timestamp 时有效）: "minute", "hour",
   * "day"
   */
  Q_INVOKABLE void groupBy(const QString &field,
                           const QString &interval = "hour");

  /**
   * @brief 清除分组，恢复正常显示
   */
  Q_INVOKABLE void clearGrouping();

  /**
   * @brief 获取所有分组信息
   * @return QVariantList，每个元素包含 GroupInfo 的属性
   */
  Q_INVOKABLE QVariantList getGroups() const;

  /**
   * @brief 切换分组的展开/折叠状态
   * @param groupId 分组ID
   */
  Q_INVOKABLE void toggleGroupExpanded(int groupId);

  /**
   * @brief 展开所有分组
   */
  Q_INVOKABLE void expandAllGroups();

  /**
   * @brief 折叠所有分组
   */
  Q_INVOKABLE void collapseAllGroups();

  /**
   * @brief 获取指定行所属的分组ID
   * @param viewRow 视图行号
   * @return 分组ID，如果不在分组模式返回 -1
   */
  Q_INVOKABLE int getGroupIdForRow(int viewRow) const;

  // ============ Delta 时间功能 (Pro) ============

  /**
   * @brief 是否启用 Delta 时间显示
   */
  Q_INVOKABLE bool deltaTimeEnabled() const { return m_deltaTimeEnabled; }

  /**
   * @brief 设置是否启用 Delta 时间显示
   */
  Q_INVOKABLE void setDeltaTimeEnabled(bool enabled);

  /**
   * @brief 从日志行解析时间戳
   * @param rawLine 原始日志行
   * @return 解析出的 QDateTime，无效则返回 null QDateTime
   */
  Q_INVOKABLE QDateTime parseTimestamp(const QString &rawLine) const;

  /**
   * @brief 计算两行之间的时间差（毫秒）
   * @param viewRow 当前视图行号
   * @return 时间差（毫秒），无法计算返回 -1
   */
  Q_INVOKABLE qint64 getDeltaTime(int viewRow) const;

  /**
   * @brief 格式化时间差为可读字符串
   * @param deltaMs 时间差（毫秒）
   * @return 格式化的字符串（如 "+1.234s", "+5m 32s"）
   */
  Q_INVOKABLE static QString formatDeltaTime(qint64 deltaMs);

  // ============ 滚动日志功能 (Pro) ============

  /**
   * @brief 检测并返回相关的滚动日志文件
   * @param basePath 基础日志文件路径（如 app.log）
   * @return 检测到的滚动日志文件列表，按序号排序
   */
  Q_INVOKABLE static QStringList detectRollingLogs(const QString &basePath);

  /**
   * @brief 合并多个滚动日志文件到临时文件并加载
   * @param files 要合并的文件列表
   * @return 成功返回 true
   */
  Q_INVOKABLE bool loadRollingLogs(const QStringList &files);

  /**
   * @brief 获取当前文件的滚动日志列表
   * @return 相关的滚动日志文件列表
   */
  Q_INVOKABLE QStringList getRelatedRollingLogs() const;

  // ============ 原始行访问 (用于外部访问) ============

  /**
   * @brief 获取指定行的原始文本
   * @param row 行号（从 0 开始）
   * @return 该行的原始 QString 内容（可能被截断）
   */
  Q_INVOKABLE QString getRawLine(int row) const;

  /**
   * @brief 导出可见行到文件
   * @param filePath 导出文件路径
   * @param sanitizationLevel 脱敏级别 (0=不脱敏, 1=标准, 2=严格)
   * @return 成功返回 true
   */
  Q_INVOKABLE bool exportToFile(const QString &filePath,
                                int sanitizationLevel = 0) const;

  // QAbstractTableModel 接口实现
  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  int columnCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

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

  // ============ 属性变化信号 (用于 QML 绑定) ============
  void filePathChanged();
  void fileSizeChanged();
  void lineCountChanged();
  void totalLineCountChanged();
  void filterModeChanged();
  void indexingStateChanged();
  void searchResultCountChanged();
  void bookmarksChanged();
  void deltaTimeEnabledChanged();
  void logLevelLinesChanged(); ///< 日志级别行列表变化信号（仅在加载完成后触发）

  // ============ 分组信号 ============
  void groupModeChanged(); ///< 分组模式变化
  void groupsChanged();    ///< 分组列表变化

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

  /**
   * @brief 异步执行高级过滤（在后台线程执行）
   */
  void executeAdvancedFilterAsync(const QStringList &levels,
                                  const QStringList &keywords, bool andLogic,
                                  bool useRegex);

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
  mutable QStringDecoder m_decoder{
      QStringConverter::Utf8}; ///< 字符解码器（默认 UTF-8）

  // 文件监控（实时日志）
  QFileSystemWatcher *m_watcher = nullptr; ///< 文件变化监视器

  // ============ 表格模式 ============
  bool m_tableModeEnabled = false; ///< 是否启用表格解析模式

  // ============ 日志过滤 (Virtual Mapping Vector) ============
  std::vector<int> m_filteredRows; ///< 虚拟映射向量：存储匹配行的原始索引
  QString m_filterKeyword;         ///< 当前过滤关键词
  std::atomic<bool> m_isFiltering{false};           ///< 是否正在执行过滤
  std::atomic<bool> m_filterCancelRequested{false}; ///< 是否请求取消过滤
  std::atomic<bool> m_filterMode{false};            ///< 是否处于过滤模式
  QFutureWatcher<void> m_filterWatcher;             ///< 过滤任务监视器

  // ============ 书签功能 ============
  QMap<qint64, BookmarkInfo>
      m_bookmarks; ///< 书签映射（行索引 -> 书签信息，过滤时保持有效）

  // ============ Delta 时间功能 ============
  bool m_deltaTimeEnabled = false;                 ///< 是否启用 Delta 时间显示
  mutable QCache<int, QDateTime> m_timestampCache; ///< 时间戳缓存
  QList<QRegularExpression> m_timestampPatterns;   ///< 时间戳解析正则表达式列表

  // ============ 日志分组功能 ============
  bool m_isGroupMode = false;       ///< 是否处于分组模式
  QString m_groupField;             ///< 当前分组字段
  QString m_groupInterval;          ///< 时间分组间隔
  QList<GroupInfo> m_groups;        ///< 分组列表
  std::vector<int> m_rowToGroupMap; ///< 行号到分组ID的映射
};

#endif // BIGFILEMODEL_H
