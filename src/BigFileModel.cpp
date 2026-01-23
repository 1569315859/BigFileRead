/**
 * @file BigFileModel.cpp
 * @brief 大文件模型类实现 - Timer-Pull 架构 + 结构化日志解析
 * @description 使用 QFile::map() 内存映射 + 行偏移索引，实现高性能大文件读取
 *              采用 Timer-Pull 模式：Worker 静默填充 buffer，Timer 主动拉取数据
 *              支持 QAbstractTableModel 多列显示和懒加载解析
 * @version 2.0 - Upgraded to TableModel with lazy log parsing
 */

#include "BigFileModel.h"
#include "LogParser.h"
#include "DataSanitizer.h"
#include <QCache>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QtConcurrent>
#include <algorithm> // for std::search
#include <cctype>    // for std::tolower
#include <cstring>   // for memchr

BigFileModel::BigFileModel(QObject *parent) : QAbstractTableModel(parent),
    m_timestampCache(1000) {
  // 创建 Pull Timer（仅在主线程运行，每 50ms 触发）
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(UI_UPDATE_INTERVAL_MS);
  connect(m_refreshTimer, &QTimer::timeout, this,
          &BigFileModel::onUpdateTimerTimeout);

  // 连接索引完成信号（跨线程）
  connect(this, &BigFileModel::indexingFinished, this,
          &BigFileModel::onIndexingFinished, Qt::QueuedConnection);

  // 创建文件监视器（用于实时日志监控）
  m_watcher = new QFileSystemWatcher(this);
  connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
          &BigFileModel::onFileChanged);

  // 初始化常见时间戳解析模式
  m_timestampPatterns = {
    // ISO 8601: 2024-01-15T10:30:45.123Z 或 2024-01-15 10:30:45.123
    QRegularExpression(R"((\d{4}-\d{2}-\d{2})[T ](\d{2}:\d{2}:\d{2})(?:\.(\d{3}))?(?:Z|[+-]\d{2}:?\d{2})?)"),
    // 常见日志格式: [2024-01-15 10:30:45]
    QRegularExpression(R"(\[(\d{4}-\d{2}-\d{2}) (\d{2}:\d{2}:\d{2})(?:\.(\d{3}))?\])"),
    // Spring Boot: 2024-01-15 10:30:45.123
    QRegularExpression(R"((\d{4}-\d{2}-\d{2}) (\d{2}:\d{2}:\d{2})\.(\d{3}))"),
    // Syslog: Jan 15 10:30:45
    QRegularExpression(R"((Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dec)\s+(\d{1,2})\s+(\d{2}:\d{2}:\d{2}))"),
    // Unix timestamp (毫秒): 1705315845123
    QRegularExpression(R"(\b(\d{13})\b)"),
    // Unix timestamp (秒): 1705315845
    QRegularExpression(R"(\b(\d{10})\b)")
  };
}

BigFileModel::~BigFileModel() {
  cancelIndexing();
  m_futureWatcher.waitForFinished();
  closeFile();
}

bool BigFileModel::loadFile(const QString &filePath) {
  // 如果正在索引，先取消
  if (m_isIndexing.load()) {
    cancelIndexing();
    m_futureWatcher.waitForFinished();
  }

  // 停止之前的 Timer
  if (m_refreshTimer) {
    m_refreshTimer->stop();
  }

  // 关闭之前的文件（不重置模型）
  if (m_mapPtr) {
    m_file.unmap(m_mapPtr);
    m_mapPtr = nullptr;
  }
  if (m_file.isOpen()) {
    m_file.close();
  }

  m_filePath = filePath;
  emit filePathChanged();
  m_file.setFileName(filePath);

  // 以只读方式打开文件
  if (!m_file.open(QIODevice::ReadOnly)) {
    emit fileLoaded(false, tr("Cannot open file: %1").arg(m_file.errorString()));
    return false;
  }

  m_fileSize = m_file.size();
  emit fileSizeChanged();

  // 空文件特殊处理
  if (m_fileSize == 0) {
    beginResetModel();
    m_lineOffsets.clear();
    endResetModel();
    emit fileLoaded(true, tr("File is empty"));
    return true;
  }

  // 内存映射整个文件（仅 64 位系统支持超大文件）
  m_mapPtr = m_file.map(0, m_fileSize);
  if (!m_mapPtr) {
    m_file.close();
    emit fileLoaded(false, tr("Memory mapping failed: %1").arg(m_file.errorString()));
    return false;
  }

  // 重置状态
  m_cancelRequested.store(false);
  m_isIndexing.store(true);
  m_lastReportedPercent.store(0);

  // 清除 LogParser 缓存
  LogParser::instance().clearCache();

  // *** 关键：先重置模型，然后立即显示第一行 ***
  beginResetModel();
  m_lineOffsets.clear();
  m_lineOffsets.reserve(static_cast<size_t>(m_fileSize / 40));
  m_lineOffsets.push_back(0); // 第一行偏移为 0，立即可见
  endResetModel();

  // 清空 pending buffer
  {
    QMutexLocker locker(&m_pendingMutex);
    m_pendingOffsets.clear();
    m_pendingOffsets.reserve(100000);
  }

  // 启动 Pull Timer（每 50ms 拉取新数据）
  m_refreshTimer->start();

  // 启动异步索引（Worker 线程）
  QFuture<void> future = QtConcurrent::run([this]() { buildIndexAsync(); });
  m_futureWatcher.setFuture(future);

  // 添加文件监控（用于实时日志追踪）
  m_watcher->addPath(filePath);

  qDebug() << "File loading started, first line available";

  return true;
}

void BigFileModel::cancelIndexing() { m_cancelRequested.store(true); }

void BigFileModel::closeFile() {
  // 关闭前自动保存书签
  autoSaveBookmarks();
  
  cancelIndexing();
  m_futureWatcher.waitForFinished();
  cancelFilter();
  m_filterWatcher.waitForFinished();

  if (m_refreshTimer) {
    m_refreshTimer->stop();
  }

  if (!m_filePath.isEmpty()) {
    m_watcher->removePath(m_filePath);
  }

  beginResetModel();

  if (m_mapPtr) {
    m_file.unmap(m_mapPtr);
    m_mapPtr = nullptr;
  }

  if (m_file.isOpen()) {
    m_file.close();
  }

  m_lineOffsets.clear();
  m_lineOffsets.shrink_to_fit();
  m_filteredRows.clear();
  m_filteredRows.shrink_to_fit();
  m_filterKeyword.clear();
  m_filterMode.store(false);

  {
    QMutexLocker locker(&m_pendingMutex);
    m_pendingOffsets.clear();
    m_pendingOffsets.shrink_to_fit();
  }

  m_fileSize = 0;
  m_filePath.clear();
  emit filePathChanged();
  emit fileSizeChanged();
  emit lineCountChanged();
  emit totalLineCountChanged();
  emit filterModeChanged();
  m_isIndexing.store(false);
  emit indexingStateChanged();
  m_lastReportedPercent.store(0);

  // 清除解析缓存
  LogParser::instance().clearCache();

  endResetModel();
}

int BigFileModel::lineCount() const {
  if (m_filterMode.load()) {
    return static_cast<int>(m_filteredRows.size());
  }
  return static_cast<int>(m_lineOffsets.size());
}

void BigFileModel::onUpdateTimerTimeout() {
  std::vector<qint64> newData;
  bool isWorkerDone = !m_isIndexing.load();
  bool isPendingEmpty = false;

  size_t currentBatchLimit = 5000;

  {
    QMutexLocker locker(&m_pendingMutex);
    size_t pendingSize = m_pendingOffsets.size();
    isPendingEmpty = (pendingSize == 0);

    if (!isPendingEmpty) {
      if (isWorkerDone) {
        if (pendingSize > 500000) {
          currentBatchLimit = 100000;
        } else {
          currentBatchLimit = 50000;
        }
      } else {
        if (pendingSize > 100000) {
          currentBatchLimit = 20000;
        } else {
          currentBatchLimit = 5000;
        }
      }

      if (pendingSize > currentBatchLimit) {
        auto beginIt = m_pendingOffsets.begin();
        auto endIt = m_pendingOffsets.begin() + currentBatchLimit;
        newData.assign(beginIt, endIt);
        m_pendingOffsets.erase(beginIt, endIt);
      } else {
        newData.swap(m_pendingOffsets);
        if (m_pendingOffsets.capacity() < 20000) {
          m_pendingOffsets.reserve(20000);
        }
      }
    }
  }

  if (!newData.empty()) {
    int startRow = static_cast<int>(m_lineOffsets.size());
    int endRow = startRow + static_cast<int>(newData.size()) - 1;

    beginInsertRows(QModelIndex(), startRow, endRow);
    m_lineOffsets.insert(m_lineOffsets.end(), newData.begin(), newData.end());
    endInsertRows();
    
    // 发射行数变化信号以更新 UI
    emit lineCountChanged();
    emit totalLineCountChanged();
  }

  if (isWorkerDone && isPendingEmpty && newData.empty()) {
    m_refreshTimer->stop();
    m_lineOffsets.shrink_to_fit();
    qDebug() << "Full Load Complete. Total lines:" << m_lineOffsets.size();
    emit fileLoaded(true, tr("Load complete: %1 lines").arg(m_lineOffsets.size()));
  }
}

void BigFileModel::buildIndexAsync() {
  QElapsedTimer timer;
  timer.start();

  const char *ptr = reinterpret_cast<const char *>(m_mapPtr);
  const char *end = ptr + m_fileSize;
  const char *fileStart = ptr;

  std::vector<qint64> localBuffer;
  localBuffer.reserve(WORKER_CHUNK_SIZE + 100);

  int linesInChunk = 0;
  bool firstBatchFlushed = false;
  const int FIRST_BATCH_SIZE = 100;

  while (ptr < end) {
    qint64 pos = ptr - fileStart;
    if ((pos & 0xFFFFF) == 0) {
      if (m_cancelRequested.load()) {
        emit indexingCancelled();
        m_isIndexing.store(false);
        return;
      }

      int percent = static_cast<int>((pos * 100) / m_fileSize);
      int lastPercent = m_lastReportedPercent.load();
      if (percent > lastPercent && percent - lastPercent >= 2) {
        m_lastReportedPercent.store(percent);
        emit indexingProgress(percent);
      }
    }

    const char *newline = static_cast<const char *>(
        memchr(ptr, '\n', static_cast<size_t>(end - ptr)));

    if (!newline) {
      break;
    }

    qint64 nextLineOffset = (newline - fileStart) + 1;
    if (nextLineOffset < m_fileSize) {
      localBuffer.push_back(nextLineOffset);
      ++linesInChunk;

      int flushThreshold = firstBatchFlushed ? WORKER_CHUNK_SIZE : FIRST_BATCH_SIZE;

      if (linesInChunk >= flushThreshold) {
        {
          QMutexLocker locker(&m_pendingMutex);
          m_pendingOffsets.insert(m_pendingOffsets.end(), localBuffer.begin(),
                                  localBuffer.end());
        }
        localBuffer.clear();
        linesInChunk = 0;
        firstBatchFlushed = true;
      }
    }

    ptr = newline + 1;
  }

  if (!localBuffer.empty()) {
    QMutexLocker locker(&m_pendingMutex);
    m_pendingOffsets.insert(m_pendingOffsets.end(), localBuffer.begin(),
                            localBuffer.end());
  }

  qint64 elapsed = timer.elapsed();

  int pendingCount = 0;
  {
    QMutexLocker locker(&m_pendingMutex);
    pendingCount = static_cast<int>(m_pendingOffsets.size());
  }
  int currentCount = static_cast<int>(m_lineOffsets.size());
  int totalLines = currentCount + pendingCount;

  QString message = tr("Done: %1 lines, %2 MB, %3 ms")
                        .arg(totalLines)
                        .arg(m_fileSize / (1024.0 * 1024.0), 0, 'f', 2)
                        .arg(elapsed);

  qDebug() << message;

  emit indexingProgress(100);
  emit indexingFinished(true, message);
}

void BigFileModel::onIndexingFinished(bool success, const QString &message) {
  // *** 关键：确保索引标记被正确重置 ***
  m_isIndexing.store(false);
  emit indexingStateChanged();
  
  // 停止刷新定时器
  if (m_refreshTimer && m_refreshTimer->isActive()) {
    onUpdateTimerTimeout();  // 最后一次拉取剩余数据
    m_refreshTimer->stop();
  }

  // 发出行数变化信号
  emit lineCountChanged();
  emit totalLineCountChanged();

  // 自动加载书签（如果有）
  if (success) {
    autoLoadBookmarks();
  }

  // *** 无论成功失败都发送 fileLoaded 信号 ***
  emit fileLoaded(success, message);
  
  // *** 只在加载完成后触发日志级别行列表变化信号（优化性能）***
  if (success) {
    emit logLevelLinesChanged();
  }
  
  qDebug() << "[BigFileModel] Indexing finished:" << success << message;
}

bool BigFileModel::canFetchMore(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return false;
}

void BigFileModel::fetchMore(const QModelIndex &parent) {
  Q_UNUSED(parent)
}

qint64 BigFileModel::getLineLength(int row) const {
  if (!m_mapPtr || row < 0 || row >= static_cast<int>(m_lineOffsets.size())) {
    return 0;
  }

  qint64 startOffset = m_lineOffsets[row];
  qint64 endOffset;

  if (row + 1 < static_cast<int>(m_lineOffsets.size())) {
    endOffset = m_lineOffsets[row + 1] - 1;
  } else {
    endOffset = m_fileSize;
  }

  return endOffset - startOffset;
}

QString BigFileModel::getLine(int row) const {
  if (!m_mapPtr || row < 0 || row >= static_cast<int>(m_lineOffsets.size())) {
    return QString();
  }

  qint64 startOffset = m_lineOffsets[row];
  qint64 endOffset;

  if (row + 1 < static_cast<int>(m_lineOffsets.size())) {
    endOffset = m_lineOffsets[row + 1] - 1;
  } else {
    endOffset = m_fileSize;
  }

  qint64 length = endOffset - startOffset;

  if (length > 0) {
    const uchar *lineEnd = m_mapPtr + endOffset - 1;
    if (*lineEnd == '\r') {
      --length;
    }
  }

  if (length <= 0) {
    return QString();
  }

  bool truncated = false;
  if (length > MAX_DISPLAY_LENGTH) {
    length = MAX_DISPLAY_LENGTH;
    truncated = true;
  }

  const char *lineStart = reinterpret_cast<const char *>(m_mapPtr + startOffset);
  QString result = m_decoder.decode(QByteArrayView(lineStart, static_cast<qsizetype>(length)));

  if (truncated) {
    result += QStringLiteral("  ... [truncated]");
  }

  return result;
}

// ========== QAbstractTableModel 接口实现 ==========

QHash<int, QByteArray> BigFileModel::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[Qt::DisplayRole] = "display";
  roles[BookmarkRole] = "isBookmarked";
  roles[RealRowRole] = "realRow";
  roles[RawLineRole] = "rawLine";
  roles[LogLevelRole] = "logLevel";
  return roles;
}

int BigFileModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  if (m_filterMode.load()) {
    return static_cast<int>(m_filteredRows.size());
  }
  return static_cast<int>(m_lineOffsets.size());
}

int BigFileModel::columnCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  
  if (m_tableModeEnabled) {
    return LogParser::instance().columnCount();
  }
  
  return 1;
}

QVariant BigFileModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (role != Qt::DisplayRole) {
    return QVariant();
  }
  
  if (orientation == Qt::Horizontal) {
    if (m_tableModeEnabled) {
      QStringList headers = LogParser::instance().columnHeaders();
      if (section >= 0 && section < headers.size()) {
        return headers.at(section);
      }
    }
    return tr("Content");
  } else {
    int realRow = toRealRow(section);
    return realRow + 1;
  }
}

QVariant BigFileModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  int viewRow = index.row();
  int column = index.column();
  
  int realRow = toRealRow(viewRow);
  
  if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) {
    return QVariant();
  }

  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole: {
    QString rawLine = getLine(realRow);
    
    if (m_tableModeEnabled && column >= 0) {
      QString field = LogParser::instance().getFieldWithCache(
          static_cast<qint64>(realRow), rawLine, column);
      // 如果解析失败（返回空），对于第一列回退显示原始内容
      if (field.isEmpty() && column == 0) {
        return rawLine;
      }
      return field;
    }
    
    return rawLine;
  }

  case Qt::ToolTipRole: {
    qint64 len = getLineLength(realRow);
    if (len > MAX_DISPLAY_LENGTH) {
      return tr("Line %1 (Original length: %2 chars, truncated)").arg(realRow + 1).arg(len);
    }
    return tr("Line %1").arg(realRow + 1);
  }

  case BookmarkRole:
    return m_bookmarks.contains(static_cast<qint64>(realRow));

  case RealRowRole:
    return realRow;

  case RawLineRole:
    return getLine(realRow);

  case LogLevelRole: {
    QString rawLine = getLine(realRow);
    LogParser::LogLevel level = LogParser::instance().detectLevel(rawLine);
    return static_cast<int>(level);
  }

  case DeltaTimeRole: {
    if (!m_deltaTimeEnabled) {
      return QVariant();
    }
    qint64 delta = getDeltaTime(viewRow);
    return delta >= 0 ? QVariant(delta) : QVariant();
  }

  case TimestampRole: {
    QString rawLine = getLine(realRow);
    QDateTime ts = parseTimestamp(rawLine);
    return ts.isValid() ? QVariant(ts) : QVariant();
  }

  default:
    return QVariant();
  }
}

// ========== 表格模式控制 ==========

void BigFileModel::setTableModeEnabled(bool enabled) {
  if (m_tableModeEnabled != enabled) {
    beginResetModel();
    m_tableModeEnabled = enabled;
    LogParser::instance().clearCache();
    endResetModel();
    qDebug() << "Table mode:" << (enabled ? "ENABLED" : "DISABLED");
  }
}

void BigFileModel::refreshTableStructure() {
  beginResetModel();
  LogParser::instance().clearCache();
  endResetModel();
}

QString BigFileModel::getRawLine(int row) const {
  return getLine(row);
}

// ========== 编码设置实现 ==========

void BigFileModel::setEncoding(QStringConverter::Encoding encoding) {
  m_decoder = QStringDecoder(encoding);
  beginResetModel();
  endResetModel();
  qDebug() << "Encoding changed to:" << (encoding == QStringConverter::Utf8 ? "UTF-8" : "System");
}

// ========== 实时日志监控实现 ==========

void BigFileModel::onFileChanged(const QString &path) {
  if (m_isIndexing.load()) {
    return;
  }
  
  QFileInfo fi(path);
  qint64 newSize = fi.size();
  
  if (newSize <= m_fileSize) {
    if (!m_watcher->files().contains(path)) {
      m_watcher->addPath(path);
    }
    return;
  }
  
  qDebug() << "File changed, new size:" << newSize << "old size:" << m_fileSize;
  
  qint64 oldSize = m_fileSize;
  
  if (m_mapPtr) {
    m_file.unmap(m_mapPtr);
    m_mapPtr = nullptr;
  }
  
  m_mapPtr = m_file.map(0, newSize);
  if (!m_mapPtr) {
    qWarning() << "Failed to remap file to new size:" << newSize;
    m_mapPtr = m_file.map(0, m_fileSize);
    return;
  }
  
  const uchar *start = m_mapPtr + oldSize;
  const uchar *end = m_mapPtr + newSize;
  
  const uchar *ptr = start;
  while (ptr < end) {
    const uchar *found = static_cast<const uchar *>(
        std::memchr(ptr, '\n', static_cast<size_t>(end - ptr)));
    
    if (found) {
      qint64 nextLineOffset = (found - m_mapPtr) + 1;
      if (nextLineOffset < newSize) {
        m_lineOffsets.push_back(nextLineOffset);
      }
      ptr = found + 1;
    } else {
      break;
    }
  }
  
  m_fileSize = newSize;
  emit fileSizeChanged();
  emit lineCountChanged();
  emit totalLineCountChanged();
  emit logAppended();
  emit layoutChanged();
}

// ========== 搜索功能实现 ==========

void BigFileModel::search(const QString &text, bool useRegex) {
  if (m_isSearching.load()) {
    cancelSearch();
    m_searchWatcher.waitForFinished();
  }

  m_searchResults.clear();

  if (text.isEmpty()) {
    emit searchFinished(m_searchResults);
    return;
  }

  m_searchCancelRequested.store(false);
  m_isSearching.store(true);

  QFuture<void> future =
      QtConcurrent::run([this, text, useRegex]() { executeSearchAsync(text, useRegex); });
  m_searchWatcher.setFuture(future);
}

void BigFileModel::cancelSearch() { m_searchCancelRequested.store(true); }

void BigFileModel::executeSearchAsync(const QString &searchText, bool useRegex) {
  QElapsedTimer timer;
  timer.start();

  std::vector<int> results;
  results.reserve(10000);

  const int totalLines = static_cast<int>(m_lineOffsets.size());
  int lastPercent = 0;

  if (useRegex) {
    QRegularExpression regex(searchText, QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) {
      qWarning() << "Invalid regex pattern:" << regex.errorString();
      m_isSearching.store(false);
      emit searchFinished(results);
      return;
    }

    for (int row = 0; row < totalLines; ++row) {
      if (row % 1000 == 0) {
        if (m_searchCancelRequested.load()) {
          qDebug() << "Search cancelled by user";
          m_isSearching.store(false);
          return;
        }
        int percent = (row * 100) / totalLines;
        if (percent > lastPercent) {
          lastPercent = percent;
          emit searchProgress(percent);
        }
      }

      QString lineText = getLine(row);
      
      QRegularExpressionMatchIterator it = regex.globalMatch(lineText);
      while (it.hasNext()) {
        it.next();
        results.push_back(row);
        
        if (static_cast<int>(results.size()) >= MAX_SEARCH_RESULTS) {
          break;
        }
      }

      if (static_cast<int>(results.size()) >= MAX_SEARCH_RESULTS) {
        qDebug() << "Search limit reached:" << MAX_SEARCH_RESULTS << "results";
        break;
      }
    }
  } else {
    QByteArray searchBytes = searchText.toUtf8();
    const char *searchPtr = searchBytes.constData();
    const int searchLen = searchBytes.size();

    if (searchLen == 0) {
      m_isSearching.store(false);
      emit searchFinished(results);
      return;
    }

    const char *fileStart = reinterpret_cast<const char *>(m_mapPtr);

    auto caseInsensitiveEqual = [](char a, char b) {
      return std::tolower(static_cast<unsigned char>(a)) ==
             std::tolower(static_cast<unsigned char>(b));
    };

    for (int row = 0; row < totalLines; ++row) {
      if (row % 1000 == 0) {
        if (m_searchCancelRequested.load()) {
          qDebug() << "Search cancelled by user";
          m_isSearching.store(false);
          return;
        }
        int percent = (row * 100) / totalLines;
        if (percent > lastPercent) {
          lastPercent = percent;
          emit searchProgress(percent);
        }
      }

      const qint64 lineStart = m_lineOffsets[row];
      qint64 lineEnd = (row + 1 < totalLines) ? m_lineOffsets[row + 1] : m_fileSize;
      qint64 lineLength = lineEnd - lineStart;

      if (lineLength <= 0) continue;

      const char *linePtr = fileStart + lineStart;
      if (lineLength > 0 && linePtr[lineLength - 1] == '\n') --lineLength;
      if (lineLength > 0 && linePtr[lineLength - 1] == '\r') --lineLength;
      if (lineLength < searchLen) continue;

      const char *searchStart = linePtr;
      const char *lineEndPtr = linePtr + lineLength;

      while (searchStart < lineEndPtr) {
        const char *found = std::search(searchStart, lineEndPtr, 
                                        searchPtr, searchPtr + searchLen,
                                        caseInsensitiveEqual);
        if (found != lineEndPtr) {
          results.push_back(row);
          if (static_cast<int>(results.size()) >= MAX_SEARCH_RESULTS) break;
          searchStart = found + searchLen;
        } else {
          break;
        }
      }

      if (static_cast<int>(results.size()) >= MAX_SEARCH_RESULTS) {
        qDebug() << "Search limit reached:" << MAX_SEARCH_RESULTS << "results";
        break;
      }
    }
  }

  qint64 elapsed = timer.elapsed();
  qDebug() << "Search completed:" << results.size() << "total occurrences in"
           << elapsed << "ms (" << (totalLines / (elapsed + 1)) << "lines/ms)"
           << (useRegex ? "[Regex]" : "[Plain]");

  m_searchResults = results;
  m_isSearching.store(false);
  emit searchProgress(100);
  emit searchResultCountChanged();
  emit searchFinished(results);
}

int BigFileModel::nextSearchResult(int currentViewRow) const {
  if (m_searchResults.empty()) return -1;
  
  int currentRealRow = toRealRow(currentViewRow);
  
  // Find first result > currentRealRow
  auto it = std::upper_bound(m_searchResults.begin(), m_searchResults.end(), currentRealRow);
  
  // Wrap around if needed
  if (it == m_searchResults.end()) {
    it = m_searchResults.begin();
  }
  
  // Iterate to find a visible match
  auto startIt = it;
  do {
    int matchRealRow = *it;
    
    if (!m_filterMode.load()) {
      return matchRealRow;
    } else {
      // Check if matchRealRow is visible
      auto fit = std::lower_bound(m_filteredRows.begin(), m_filteredRows.end(), matchRealRow);
      if (fit != m_filteredRows.end() && *fit == matchRealRow) {
        // Found visible match, return view index
        return static_cast<int>(std::distance(m_filteredRows.begin(), fit));
      }
    }
    
    ++it;
    if (it == m_searchResults.end()) {
      it = m_searchResults.begin();
    }
  } while (it != startIt);
  
  return -1;
}

int BigFileModel::prevSearchResult(int currentViewRow) const {
  if (m_searchResults.empty()) return -1;
  
  int currentRealRow = toRealRow(currentViewRow);
  
  // Find first result >= currentRealRow
  auto it = std::lower_bound(m_searchResults.begin(), m_searchResults.end(), currentRealRow);
  
  // Move back to get < currentRealRow
  if (it == m_searchResults.begin()) {
    it = m_searchResults.end();
  }
  --it;
  
  // Iterate backwards to find a visible match
  auto startIt = it;
  do {
    int matchRealRow = *it;
    
    if (!m_filterMode.load()) {
      return matchRealRow;
    } else {
      auto fit = std::lower_bound(m_filteredRows.begin(), m_filteredRows.end(), matchRealRow);
      if (fit != m_filteredRows.end() && *fit == matchRealRow) {
        return static_cast<int>(std::distance(m_filteredRows.begin(), fit));
      }
    }
    
    if (it == m_searchResults.begin()) {
      it = m_searchResults.end();
    }
    --it;
  } while (it != startIt);
  
  return -1;
}

// ========== 虚拟行映射辅助函数 ==========

int BigFileModel::toRealRow(int viewRow) const {
  if (!m_filterMode.load()) {
    return viewRow;
  }
  
  if (viewRow >= 0 && viewRow < static_cast<int>(m_filteredRows.size())) {
    return m_filteredRows[viewRow];
  }
  
  return -1;
}

// ========== 日志过滤功能实现 ==========

void BigFileModel::applyFilter(const QString &keyword, bool useRegex) {
  if (m_isFiltering.load()) {
    cancelFilter();
    m_filterWatcher.waitForFinished();
  }

  if (keyword.isEmpty()) {
    clearFilter();
    return;
  }

  m_filterKeyword = keyword;
  
  m_filterCancelRequested.store(false);
  m_isFiltering.store(true);

  QFuture<void> future =
      QtConcurrent::run([this, keyword, useRegex]() { executeFilterAsync(keyword, useRegex); });
  m_filterWatcher.setFuture(future);
}

void BigFileModel::applyAdvancedFilter(const QString &level, 
                                        const QStringList &keywords,
                                        bool andLogic,
                                        bool useRegex) {
  if (m_isFiltering.load()) {
    cancelFilter();
    m_filterWatcher.waitForFinished();
  }

  if (level.isEmpty() && keywords.isEmpty()) {
    clearFilter();
    return;
  }

  m_filterKeyword = keywords.join(QStringLiteral(" "));
  
  m_filterCancelRequested.store(false);
  m_isFiltering.store(true);

  QFuture<void> future =
      QtConcurrent::run([this, level, keywords, andLogic, useRegex]() { 
        executeAdvancedFilterAsync(level, keywords, andLogic, useRegex); 
      });
  m_filterWatcher.setFuture(future);
}

void BigFileModel::clearFilter() {
  if (m_isFiltering.load()) {
    cancelFilter();
    m_filterWatcher.waitForFinished();
  }

  if (!m_filterMode.load()) {
    return;
  }

  beginResetModel();
  m_filterMode.store(false);
  m_filteredRows.clear();
  m_filteredRows.shrink_to_fit();
  m_filterKeyword.clear();
  endResetModel();

  emit filterModeChanged();
  emit lineCountChanged();
  emit logLevelLinesChanged();  // 清除过滤后更新日志级别行列表

  qDebug() << "Filter cleared, showing all" << m_lineOffsets.size() << "lines";
  emit filterFinished(static_cast<int>(m_lineOffsets.size()));
}

void BigFileModel::cancelFilter() {
  m_filterCancelRequested.store(true);
}

void BigFileModel::executeFilterAsync(const QString &keyword, bool useRegex) {
  QElapsedTimer timer;
  timer.start();

  std::vector<int> matchedRows;
  matchedRows.reserve(100000);

  const int totalLines = static_cast<int>(m_lineOffsets.size());
  int lastPercent = 0;

  if (useRegex) {
    QRegularExpression regex(keyword, QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) {
      qWarning() << "Invalid regex pattern:" << regex.errorString();
      m_isFiltering.store(false);
      return;
    }

    for (int row = 0; row < totalLines; ++row) {
      if (row % 5000 == 0) {
        if (m_filterCancelRequested.load()) {
          qDebug() << "Filter cancelled by user";
          m_isFiltering.store(false);
          return;
        }
        int percent = (row * 100) / totalLines;
        if (percent > lastPercent) {
          lastPercent = percent;
          emit filterProgress(percent);
        }
      }

      QString lineText = getLine(row);
      
      if (regex.match(lineText).hasMatch()) {
        matchedRows.push_back(row);
      }
    }
  } else {
    QByteArray keywordBytes = keyword.toUtf8();
    const char *keywordPtr = keywordBytes.constData();
    const int keywordLen = keywordBytes.size();

    if (keywordLen == 0) {
      m_isFiltering.store(false);
      return;
    }

    const char *fileStart = reinterpret_cast<const char *>(m_mapPtr);

    auto caseInsensitiveEqual = [](char a, char b) {
      return std::tolower(static_cast<unsigned char>(a)) ==
             std::tolower(static_cast<unsigned char>(b));
    };

    for (int row = 0; row < totalLines; ++row) {
      if (row % 5000 == 0) {
        if (m_filterCancelRequested.load()) {
          qDebug() << "Filter cancelled by user";
          m_isFiltering.store(false);
          return;
        }
        int percent = (row * 100) / totalLines;
        if (percent > lastPercent) {
          lastPercent = percent;
          emit filterProgress(percent);
        }
      }

      const qint64 lineStart = m_lineOffsets[row];
      qint64 lineEnd = (row + 1 < totalLines) ? m_lineOffsets[row + 1] : m_fileSize;
      qint64 lineLength = lineEnd - lineStart;

      if (lineLength <= 0 || lineLength < keywordLen) continue;

      const char *linePtr = fileStart + lineStart;
      if (lineLength > 0 && linePtr[lineLength - 1] == '\n') --lineLength;
      if (lineLength > 0 && linePtr[lineLength - 1] == '\r') --lineLength;
      if (lineLength < keywordLen) continue;

      const char *lineEndPtr = linePtr + lineLength;
      const char *found = std::search(linePtr, lineEndPtr, 
                                      keywordPtr, keywordPtr + keywordLen,
                                      caseInsensitiveEqual);
      if (found != lineEndPtr) {
        matchedRows.push_back(row);
      }
    }
  }

  qint64 elapsed = timer.elapsed();
  qDebug() << "Filter completed:" << matchedRows.size() << "matching rows out of"
           << totalLines << "in" << elapsed << "ms"
           << (useRegex ? "[Regex]" : "[Plain]");

  QMetaObject::invokeMethod(this, [this, matchedRows = std::move(matchedRows)]() {
    beginResetModel();
    m_filteredRows = std::move(matchedRows);
    m_filterMode.store(true);
    endResetModel();

    m_isFiltering.store(false);
    emit filterModeChanged();
    emit lineCountChanged();
    emit logLevelLinesChanged();  // 过滤后更新日志级别行列表
    emit filterProgress(100);
    emit filterFinished(static_cast<int>(m_filteredRows.size()));
  }, Qt::QueuedConnection);
}

void BigFileModel::executeAdvancedFilterAsync(const QString &level,
                                               const QStringList &keywords,
                                               bool andLogic,
                                               bool useRegex) {
  QElapsedTimer timer;
  timer.start();

  std::vector<int> matchedRows;
  matchedRows.reserve(100000);

  const int totalLines = static_cast<int>(m_lineOffsets.size());
  int lastPercent = 0;

  // Prepare keyword matchers
  QList<QRegularExpression> keywordRegexes;
  if (useRegex) {
    for (const QString &kw : keywords) {
      QRegularExpression re(kw, QRegularExpression::CaseInsensitiveOption);
      if (re.isValid()) {
        keywordRegexes.append(re);
      }
    }
  }

  for (int row = 0; row < totalLines; ++row) {
    if (row % 5000 == 0) {
      if (m_filterCancelRequested.load()) {
        qDebug() << "Advanced filter cancelled by user";
        m_isFiltering.store(false);
        return;
      }
      int percent = (row * 100) / totalLines;
      if (percent > lastPercent) {
        lastPercent = percent;
        emit filterProgress(percent);
      }
    }

    QString lineText = getLine(row);
    
    // Level filter
    bool levelMatch = level.isEmpty();
    if (!level.isEmpty()) {
      levelMatch = lineText.contains(level, Qt::CaseInsensitive);
    }
    
    if (!levelMatch) continue;
    
    // Keyword filter
    bool keywordMatch = keywords.isEmpty();
    
    if (!keywords.isEmpty()) {
      if (useRegex) {
        if (andLogic) {
          keywordMatch = true;
          for (const QRegularExpression &re : keywordRegexes) {
            if (!re.match(lineText).hasMatch()) {
              keywordMatch = false;
              break;
            }
          }
        } else {
          keywordMatch = false;
          for (const QRegularExpression &re : keywordRegexes) {
            if (re.match(lineText).hasMatch()) {
              keywordMatch = true;
              break;
            }
          }
        }
      } else {
        if (andLogic) {
          keywordMatch = true;
          for (const QString &kw : keywords) {
            if (!lineText.contains(kw, Qt::CaseInsensitive)) {
              keywordMatch = false;
              break;
            }
          }
        } else {
          keywordMatch = false;
          for (const QString &kw : keywords) {
            if (lineText.contains(kw, Qt::CaseInsensitive)) {
              keywordMatch = true;
              break;
            }
          }
        }
      }
    }
    
    if (keywordMatch) {
      matchedRows.push_back(row);
    }
  }

  qint64 elapsed = timer.elapsed();
  qDebug() << "Advanced filter completed:" << matchedRows.size() << "matching rows in" << elapsed << "ms";

  QMetaObject::invokeMethod(this, [this, matchedRows = std::move(matchedRows)]() {
    beginResetModel();
    m_filteredRows = std::move(matchedRows);
    m_filterMode.store(true);
    endResetModel();

    m_isFiltering.store(false);
    emit filterModeChanged();
    emit lineCountChanged();
    emit logLevelLinesChanged();  // 过滤后更新日志级别行列表
    emit filterProgress(100);
    emit filterFinished(static_cast<int>(m_filteredRows.size()));
  }, Qt::QueuedConnection);
}

// ========== 书签功能实现 ==========

void BigFileModel::toggleBookmark(int viewRow) {
  int realRow = toRealRow(viewRow);
  if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) {
    return;
  }

  qint64 realRowKey = static_cast<qint64>(realRow);
  
  if (m_bookmarks.contains(realRowKey)) {
    m_bookmarks.remove(realRowKey);
    qDebug() << "Bookmark removed at line" << (realRow + 1);
  } else {
    m_bookmarks.insert(realRowKey, BookmarkInfo(realRowKey));
    qDebug() << "Bookmark added at line" << (realRow + 1);
  }

  QModelIndex idx = index(viewRow, 0);
  emit dataChanged(idx, idx, {BookmarkRole});
  emit bookmarksChanged();
}

bool BigFileModel::isBookmarked(int viewRow) const {
  int realRow = toRealRow(viewRow);
  if (realRow < 0) {
    return false;
  }
  return m_bookmarks.contains(static_cast<qint64>(realRow));
}

int BigFileModel::getNextBookmark(int currentViewRow) const {
  if (m_bookmarks.isEmpty()) {
    return -1;
  }

  int totalViewRows = rowCount();
  
  for (int viewRow = currentViewRow + 1; viewRow < totalViewRows; ++viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow >= 0 && m_bookmarks.contains(static_cast<qint64>(realRow))) {
      return viewRow;
    }
  }
  
  for (int viewRow = 0; viewRow <= currentViewRow && viewRow < totalViewRows; ++viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow >= 0 && m_bookmarks.contains(static_cast<qint64>(realRow))) {
      return viewRow;
    }
  }
  
  return -1;
}

int BigFileModel::getPrevBookmark(int currentViewRow) const {
  if (m_bookmarks.isEmpty()) {
    return -1;
  }

  int totalViewRows = rowCount();
  
  // 从当前行向前搜索
  for (int viewRow = currentViewRow - 1; viewRow >= 0; --viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow >= 0 && m_bookmarks.contains(static_cast<qint64>(realRow))) {
      return viewRow;
    }
  }
  
  // 如果没找到，从尾部开始循环搜索
  for (int viewRow = totalViewRows - 1; viewRow > currentViewRow; --viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow >= 0 && m_bookmarks.contains(static_cast<qint64>(realRow))) {
      return viewRow;
    }
  }
  
  return -1;
}

void BigFileModel::clearAllBookmarks() {
  if (m_bookmarks.isEmpty()) {
    return;
  }
  
  m_bookmarks.clear();
  
  emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {BookmarkRole});
  emit bookmarksChanged();
  
  qDebug() << "All bookmarks cleared";
}

void BigFileModel::setBookmarkComment(int viewRow, const QString &comment) {
  int realRow = toRealRow(viewRow);
  if (realRow < 0) {
    return;
  }
  
  qint64 realRowKey = static_cast<qint64>(realRow);
  
  if (!m_bookmarks.contains(realRowKey)) {
    // 如果不存在书签，先创建
    m_bookmarks.insert(realRowKey, BookmarkInfo(realRowKey, comment));
    QModelIndex idx = index(viewRow, 0);
    emit dataChanged(idx, idx, {BookmarkRole});
  } else {
    m_bookmarks[realRowKey].comment = comment;
  }
  
  emit bookmarksChanged();
  qDebug() << "Bookmark comment updated at line" << (realRow + 1) << ":" << comment;
}

QString BigFileModel::getBookmarkComment(int viewRow) const {
  int realRow = toRealRow(viewRow);
  if (realRow < 0) {
    return QString();
  }
  
  qint64 realRowKey = static_cast<qint64>(realRow);
  if (m_bookmarks.contains(realRowKey)) {
    return m_bookmarks[realRowKey].comment;
  }
  return QString();
}

QVariantList BigFileModel::getAllBookmarks() const {
  QVariantList result;
  for (auto it = m_bookmarks.constBegin(); it != m_bookmarks.constEnd(); ++it) {
    QVariantMap bookmarkMap;
    const BookmarkInfo &info = it.value();
    
    // 计算视图行号
    int viewRow = static_cast<int>(info.lineIndex);
    if (m_filterMode.load() && !m_filteredRows.empty()) {
      auto filterIt = std::lower_bound(m_filteredRows.begin(), m_filteredRows.end(), viewRow);
      if (filterIt != m_filteredRows.end() && *filterIt == viewRow) {
        viewRow = static_cast<int>(std::distance(m_filteredRows.begin(), filterIt));
      } else {
        viewRow = -1; // 在过滤模式下不可见
      }
    }
    
    bookmarkMap["lineIndex"] = info.lineIndex;
    bookmarkMap["viewRow"] = viewRow;
    bookmarkMap["displayLine"] = info.lineIndex + 1; // 1-based for display
    bookmarkMap["comment"] = info.comment;
    bookmarkMap["createdAt"] = info.createdAt.toString(Qt::ISODate);
    
    // 获取行内容预览（前100个字符）
    if (info.lineIndex >= 0 && info.lineIndex < static_cast<qint64>(m_lineOffsets.size())) {
      QString lineContent = getLine(static_cast<int>(info.lineIndex));
      if (lineContent.length() > 100) {
        lineContent = lineContent.left(100) + "...";
      }
      bookmarkMap["preview"] = lineContent.trimmed();
    }
    
    result.append(bookmarkMap);
  }
  return result;
}

bool BigFileModel::saveBookmarksToFile(const QString &path) {
  if (path.isEmpty()) {
    qWarning() << "Cannot save bookmarks: empty path";
    return false;
  }
  
  QJsonArray bookmarksArray;
  for (auto it = m_bookmarks.constBegin(); it != m_bookmarks.constEnd(); ++it) {
    const BookmarkInfo &info = it.value();
    QJsonObject bookmarkObj;
    bookmarkObj["lineIndex"] = static_cast<qint64>(info.lineIndex);
    bookmarkObj["comment"] = info.comment;
    bookmarkObj["createdAt"] = info.createdAt.toString(Qt::ISODate);
    bookmarksArray.append(bookmarkObj);
  }
  
  QJsonObject rootObj;
  rootObj["version"] = 1;
  rootObj["sourceFile"] = m_filePath;
  rootObj["bookmarks"] = bookmarksArray;
  
  QJsonDocument doc(rootObj);
  
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Cannot open file for writing:" << path;
    return false;
  }
  
  file.write(doc.toJson(QJsonDocument::Indented));
  file.close();
  
  qDebug() << "Bookmarks saved to:" << path << "count:" << m_bookmarks.size();
  return true;
}

bool BigFileModel::loadBookmarksFromFile(const QString &path) {
  if (path.isEmpty()) {
    qWarning() << "Cannot load bookmarks: empty path";
    return false;
  }
  
  QFile file(path);
  if (!file.exists()) {
    qDebug() << "Bookmarks file does not exist:" << path;
    return false;
  }
  
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "Cannot open bookmarks file:" << path;
    return false;
  }
  
  QByteArray data = file.readAll();
  file.close();
  
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
  
  if (parseError.error != QJsonParseError::NoError) {
    qWarning() << "JSON parse error:" << parseError.errorString();
    return false;
  }
  
  QJsonObject rootObj = doc.object();
  
  // 可选：检查源文件匹配
  if (rootObj.contains("sourceFile")) {
    QString sourceFile = rootObj["sourceFile"].toString();
    if (!sourceFile.isEmpty() && sourceFile != m_filePath) {
      qDebug() << "Warning: Bookmarks were created for different file:" << sourceFile;
    }
  }
  
  // 清除现有书签
  m_bookmarks.clear();
  
  QJsonArray bookmarksArray = rootObj["bookmarks"].toArray();
  for (const QJsonValue &val : bookmarksArray) {
    QJsonObject bookmarkObj = val.toObject();
    qint64 lineIndex = static_cast<qint64>(bookmarkObj["lineIndex"].toInteger());
    QString comment = bookmarkObj["comment"].toString();
    QString createdAtStr = bookmarkObj["createdAt"].toString();
    
    BookmarkInfo info(lineIndex, comment);
    if (!createdAtStr.isEmpty()) {
      info.createdAt = QDateTime::fromString(createdAtStr, Qt::ISODate);
    }
    
    // 验证行索引是否有效
    if (lineIndex >= 0 && lineIndex < static_cast<qint64>(m_lineOffsets.size())) {
      m_bookmarks.insert(lineIndex, info);
    }
  }
  
  emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {BookmarkRole});
  emit bookmarksChanged();
  
  qDebug() << "Bookmarks loaded from:" << path << "count:" << m_bookmarks.size();
  return true;
}

bool BigFileModel::autoSaveBookmarks() {
  if (m_filePath.isEmpty() || m_bookmarks.isEmpty()) {
    return false;
  }
  
  // 书签文件存储在与源文件相同的目录，文件名为 .{filename}.bookmarks.json
  QFileInfo fi(m_filePath);
  QString bookmarkPath = fi.absolutePath() + "/." + fi.fileName() + ".bookmarks.json";
  
  return saveBookmarksToFile(bookmarkPath);
}

bool BigFileModel::autoLoadBookmarks() {
  if (m_filePath.isEmpty()) {
    return false;
  }
  
  QFileInfo fi(m_filePath);
  QString bookmarkPath = fi.absolutePath() + "/." + fi.fileName() + ".bookmarks.json";
  
  return loadBookmarksFromFile(bookmarkPath);
}

bool BigFileModel::exportToCSV(const QString &path, int startRow, int endRow, bool includeBookmarksOnly, int sanitizationLevel) {
  if (path.isEmpty() || m_lineOffsets.empty()) {
    qWarning() << "Cannot export: empty path or no data";
    return false;
  }
  
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Cannot open file for writing:" << path;
    return false;
  }
  
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  
  // CSV header
  out << "\"Line Number\",\"Timestamp\",\"Level\",\"Component\",\"Message\",\"Bookmarked\",\"Bookmark Comment\"\n";
  
  int totalRows = rowCount();
  int end = (endRow < 0 || endRow >= totalRows) ? totalRows : endRow + 1;
  int exportedCount = 0;
  
  // Sanitization helper - use DataSanitizer singleton
  auto sanitizeText = [sanitizationLevel](const QString &text) -> QString {
    if (sanitizationLevel < 0) return text;
    return DataSanitizer::instance().sanitize(text, sanitizationLevel);
  };
  
  for (int viewRow = startRow; viewRow < end; ++viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow < 0) continue;
    
    // Check bookmark filter
    bool isBookmarked = m_bookmarks.contains(static_cast<qint64>(realRow));
    if (includeBookmarksOnly && !isBookmarked) {
      continue;
    }
    
    // Get data from model
    QModelIndex idx = index(viewRow, 0);
    QString lineNumber = QString::number(realRow + 1);
    QString timestamp = data(idx, Qt::UserRole + 1).toString();  // TimestampRole
    QString level = data(idx, Qt::UserRole + 2).toString();       // LevelRole
    QString component = data(idx, Qt::UserRole + 3).toString();   // ComponentRole
    QString message = sanitizeText(data(idx, Qt::UserRole + 4).toString());     // MessageRole
    QString bookmarkComment = isBookmarked ? m_bookmarks[static_cast<qint64>(realRow)].comment : QString();
    
    // Escape CSV values
    auto escapeCSV = [](const QString &value) -> QString {
      QString escaped = value;
      escaped.replace("\"", "\"\"");
      return "\"" + escaped + "\"";
    };
    
    out << escapeCSV(lineNumber) << ","
        << escapeCSV(timestamp) << ","
        << escapeCSV(level) << ","
        << escapeCSV(sanitizeText(component)) << ","
        << escapeCSV(message) << ","
        << (isBookmarked ? "Yes" : "No") << ","
        << escapeCSV(bookmarkComment) << "\n";
    
    ++exportedCount;
  }
  
  file.close();
  qDebug() << "Exported" << exportedCount << "lines to CSV:" << path 
           << (sanitizationLevel >= 0 ? QString("(sanitization level: %1)").arg(sanitizationLevel) : "");
  return true;
}

bool BigFileModel::exportToHTML(const QString &path, int startRow, int endRow, bool includeBookmarksOnly, int sanitizationLevel) {
  if (path.isEmpty() || m_lineOffsets.empty()) {
    qWarning() << "Cannot export: empty path or no data";
    return false;
  }
  
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Cannot open file for writing:" << path;
    return false;
  }
  
  QTextStream out(&file);
  out.setEncoding(QStringConverter::Utf8);
  
  // Sanitization helper - use DataSanitizer singleton
  auto sanitizeText = [sanitizationLevel](const QString &text) -> QString {
    if (sanitizationLevel < 0) return text;
    return DataSanitizer::instance().sanitize(text, sanitizationLevel);
  };
  
  // HTML header with styling
  out << R"(<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Log Export - )" << QFileInfo(m_filePath).fileName() << R"(</title>
  <style>
    body { font-family: 'Segoe UI', Arial, sans-serif; margin: 20px; background: #1e1e1e; color: #d4d4d4; }
    h1 { color: #569cd6; font-size: 18px; }
    .meta { color: #6a9955; font-size: 12px; margin-bottom: 15px; }
    table { border-collapse: collapse; width: 100%; font-size: 12px; }
    th { background: #252526; color: #569cd6; padding: 8px; text-align: left; border-bottom: 2px solid #3c3c3c; }
    td { padding: 6px 8px; border-bottom: 1px solid #3c3c3c; vertical-align: top; }
    tr:hover { background: #2a2d2e; }
    .line-num { color: #858585; font-family: Consolas, monospace; width: 60px; }
    .timestamp { color: #b5cea8; font-family: Consolas, monospace; width: 180px; }
    .level { width: 80px; font-weight: bold; }
    .level-error { color: #f14c4c; }
    .level-warn { color: #cca700; }
    .level-info { color: #3794ff; }
    .level-debug { color: #b5cea8; }
    .component { color: #9cdcfe; width: 120px; }
    .message { font-family: Consolas, monospace; white-space: pre-wrap; word-break: break-all; }
    .bookmarked { background: #3d3d00; }
    .bookmark-icon { color: #ffd700; }
    .bookmark-comment { color: #ce9178; font-style: italic; font-size: 11px; margin-top: 4px; }
    .sanitized { color: #ce9178; font-style: italic; }
  </style>
</head>
<body>
  <h1>🔖 Log Export</h1>
  <div class="meta">
    Source: )" << m_filePath << R"(<br>
    Exported: )" << QDateTime::currentDateTime().toString(Qt::ISODate) << R"(<br>
    )" << (includeBookmarksOnly ? "Bookmarks only" : "All lines");
  
  if (sanitizationLevel >= 0) {
    QString levelName = sanitizationLevel == 0 ? "Minimal" : (sanitizationLevel == 1 ? "Standard" : "Strict");
    out << R"(<br>
    <span class="sanitized">Data sanitization: )" << levelName << R"(</span>)";
  }
  
  out << R"(
  </div>
  <table>
    <thead>
      <tr>
        <th>Line</th>
        <th>Timestamp</th>
        <th>Level</th>
        <th>Component</th>
        <th>Message</th>
      </tr>
    </thead>
    <tbody>
)";
  
  int totalRows = rowCount();
  int end = (endRow < 0 || endRow >= totalRows) ? totalRows : endRow + 1;
  int exportedCount = 0;
  
  for (int viewRow = startRow; viewRow < end; ++viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow < 0) continue;
    
    bool isBookmarked = m_bookmarks.contains(static_cast<qint64>(realRow));
    if (includeBookmarksOnly && !isBookmarked) {
      continue;
    }
    
    QModelIndex idx = index(viewRow, 0);
    QString lineNumber = QString::number(realRow + 1);
    QString timestamp = data(idx, Qt::UserRole + 1).toString();
    QString level = data(idx, Qt::UserRole + 2).toString();
    QString component = sanitizeText(data(idx, Qt::UserRole + 3).toString());
    QString message = sanitizeText(data(idx, Qt::UserRole + 4).toString());
    QString bookmarkComment = isBookmarked ? m_bookmarks[static_cast<qint64>(realRow)].comment : QString();
    
    // Escape HTML
    auto escapeHTML = [](const QString &value) -> QString {
      QString escaped = value;
      escaped.replace("&", "&amp;");
      escaped.replace("<", "&lt;");
      escaped.replace(">", "&gt;");
      escaped.replace("\"", "&quot;");
      return escaped;
    };
    
    // Determine level class
    QString levelClass = "level-debug";
    QString levelLower = level.toLower();
    if (levelLower.contains("error") || levelLower.contains("fatal") || levelLower.contains("critical")) {
      levelClass = "level-error";
    } else if (levelLower.contains("warn")) {
      levelClass = "level-warn";
    } else if (levelLower.contains("info")) {
      levelClass = "level-info";
    }
    
    QString rowClass = isBookmarked ? " class=\"bookmarked\"" : "";
    QString bookmarkIcon = isBookmarked ? " <span class=\"bookmark-icon\">🔖</span>" : "";
    QString commentHtml = !bookmarkComment.isEmpty() 
        ? QString("<div class=\"bookmark-comment\">📝 %1</div>").arg(escapeHTML(bookmarkComment)) 
        : QString();
    
    out << "      <tr" << rowClass << ">\n"
        << "        <td class=\"line-num\">" << lineNumber << bookmarkIcon << "</td>\n"
        << "        <td class=\"timestamp\">" << escapeHTML(timestamp) << "</td>\n"
        << "        <td class=\"level " << levelClass << "\">" << escapeHTML(level) << "</td>\n"
        << "        <td class=\"component\">" << escapeHTML(component) << "</td>\n"
        << "        <td class=\"message\">" << escapeHTML(message) << commentHtml << "</td>\n"
        << "      </tr>\n";
    
    ++exportedCount;
  }
  
  out << R"(    </tbody>
  </table>
  <div class="meta" style="margin-top: 15px;">
    Total exported: )" << exportedCount << R"( lines
  </div>
</body>
</html>
)";
  
  file.close();
  qDebug() << "Exported" << exportedCount << "lines to HTML:" << path
           << (sanitizationLevel >= 0 ? QString("(sanitization level: %1)").arg(sanitizationLevel) : "");
  return true;
}

bool BigFileModel::loadFromClipboard() {
  QClipboard *clipboard = QGuiApplication::clipboard();
  if (!clipboard) {
    qWarning() << "Cannot access clipboard";
    return false;
  }
  
  QString text = clipboard->text();
  if (text.isEmpty()) {
    qWarning() << "Clipboard is empty or contains no text";
    return false;
  }
  
  // 创建临时文件
  QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
  QString tempPath = tempDir + "/clipboard_" + timestamp + ".log";
  
  QFile tempFile(tempPath);
  if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Cannot create temp file:" << tempPath;
    return false;
  }
  
  QTextStream out(&tempFile);
  out.setEncoding(QStringConverter::Utf8);
  out << text;
  tempFile.close();
  
  qDebug() << "Clipboard content saved to temp file:" << tempPath << "size:" << text.size();
  
  // 加载临时文件
  return loadFile(tempPath);
}

QVariantList BigFileModel::bookmarkLines() const {
  QVariantList result;
  for (auto it = m_bookmarks.constBegin(); it != m_bookmarks.constEnd(); ++it) {
    qint64 bookmark = it.key();
    // 转换为视图行号（如果在过滤模式下）
    if (m_filterMode.load() && !m_filteredRows.empty()) {
      // 在过滤索引中查找
      auto filterIt = std::lower_bound(m_filteredRows.begin(), m_filteredRows.end(), static_cast<int>(bookmark));
      if (filterIt != m_filteredRows.end() && *filterIt == static_cast<int>(bookmark)) {
        result.append(static_cast<int>(std::distance(m_filteredRows.begin(), filterIt)));
      }
    } else {
      result.append(static_cast<int>(bookmark));
    }
  }
  return result;
}

QVariantList BigFileModel::searchResultLines() const {
  QVariantList result;
  // 限制返回数量，避免太多标记影响性能
  int count = std::min(static_cast<int>(m_searchResults.size()), 10000);
  for (int i = 0; i < count; ++i) {
    result.append(m_searchResults[i]);
  }
  return result;
}

QVariantList BigFileModel::errorLines() const {
  static const QStringList errorKeywords = {
    "FATAL", "CRITICAL", "ERROR", "FAIL", "FAILED", "EXCEPTION", "PANIC", "ABORT"
  };
  return findLinesWithKeywords(errorKeywords, 10000);  // 增加到 10000
}

QVariantList BigFileModel::warningLines() const {
  static const QStringList warningKeywords = {
    "WARN", "WARNING", "ALERT", "CAUTION"
  };
  return findLinesWithKeywords(warningKeywords, 10000);  // 增加到 10000
}

QVariantList BigFileModel::infoLines() const {
  static const QStringList infoKeywords = {
    "INFO", "NOTICE"
  };
  return findLinesWithKeywords(infoKeywords, 5000);  // 增加到 5000
}

QVariantMap BigFileModel::getLogStatistics() const {
  QVariantMap result;
  result["error"] = 0;
  result["warn"] = 0;
  result["info"] = 0;
  result["debug"] = 0;
  result["trace"] = 0;
  result["other"] = 0;
  
  if (!m_mapPtr) return result;
  
  int errorCount = 0, warnCount = 0, infoCount = 0, debugCount = 0, traceCount = 0;
  
  int totalRows = m_filterMode.load() ? static_cast<int>(m_filteredRows.size()) 
                                       : static_cast<int>(m_lineOffsets.size());
  
  // 采样扫描，限制性能开销
  int step = 1;
  int sampleSize = totalRows;
  if (totalRows > 100000) {
    step = totalRows / 100000 + 1;
    sampleSize = totalRows / step;
  }
  
  for (int viewRow = 0; viewRow < totalRows; viewRow += step) {
    int realRow = m_filterMode.load() ? m_filteredRows[viewRow] : viewRow;
    
    if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) continue;
    
    qint64 start = m_lineOffsets[realRow];
    qint64 end = (realRow + 1 < static_cast<int>(m_lineOffsets.size())) 
                 ? m_lineOffsets[realRow + 1] 
                 : m_fileSize;
    qint64 len = std::min(end - start, static_cast<qint64>(150));
    
    QByteArray lineData(reinterpret_cast<const char*>(m_mapPtr + start), static_cast<int>(len));
    QString line = QString::fromUtf8(lineData).toUpper();
    
    // 检查日志级别
    if (line.contains("FATAL") || line.contains("CRITICAL") || line.contains("ERROR") ||
        line.contains("FAIL") || line.contains("EXCEPTION") || line.contains("PANIC")) {
      errorCount++;
    } else if (line.contains("WARN") || line.contains("ALERT") || line.contains("CAUTION")) {
      warnCount++;
    } else if (line.contains("INFO") || line.contains("NOTICE")) {
      infoCount++;
    } else if (line.contains("DEBUG")) {
      debugCount++;
    } else if (line.contains("TRACE") || line.contains("VERBOSE")) {
      traceCount++;
    }
  }
  
  // 如果采样了，按比例扩展
  if (step > 1) {
    errorCount = errorCount * step;
    warnCount = warnCount * step;
    infoCount = infoCount * step;
    debugCount = debugCount * step;
    traceCount = traceCount * step;
  }
  
  int classified = errorCount + warnCount + infoCount + debugCount + traceCount;
  int otherCount = std::max(0, totalRows - classified);
  
  result["error"] = errorCount;
  result["warn"] = warnCount;
  result["info"] = infoCount;
  result["debug"] = debugCount;
  result["trace"] = traceCount;
  result["other"] = otherCount;
  
  return result;
}

QVariantList BigFileModel::findLinesWithKeywords(const QStringList &keywords, int maxResults) const {
  QVariantList result;
  if (!m_mapPtr || keywords.isEmpty()) return result;
  
  int totalRows = m_filterMode.load() ? static_cast<int>(m_filteredRows.size()) 
                                       : static_cast<int>(m_lineOffsets.size());
  
  // 为了性能，大文件时采用均匀采样，确保覆盖整个文件
  // 计算采样步长：每种级别最多保留 maxResults 个标记
  int step = 1;
  if (totalRows > maxResults * 10) {
    // 大文件时采样，但确保均匀分布在整个文件
    step = totalRows / (maxResults * 10) + 1;
  }
  
  for (int viewRow = 0; viewRow < totalRows && result.size() < maxResults; viewRow += step) {
    int realRow = m_filterMode.load() ? m_filteredRows[viewRow] : viewRow;
    
    if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) continue;
    
    qint64 start = m_lineOffsets[realRow];
    qint64 end = (realRow + 1 < static_cast<int>(m_lineOffsets.size())) 
                 ? m_lineOffsets[realRow + 1] 
                 : m_fileSize;
    qint64 len = end - start;
    
    // 只检查行首部分（前 200 字符，日志级别通常在行首）
    len = std::min(len, static_cast<qint64>(200));
    
    QByteArray lineData(reinterpret_cast<const char*>(m_mapPtr + start), static_cast<int>(len));
    QString line = QString::fromUtf8(lineData).toUpper();
    
    for (const QString &keyword : keywords) {
      if (line.contains(keyword)) {
        result.append(viewRow);
        break;
      }
    }
  }
  
  return result;
}

bool BigFileModel::exportToFile(const QString &filePath, int sanitizationLevel) const {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for export:" << file.errorString();
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  // 获取脱敏器实例
  DataSanitizer* sanitizer = nullptr;
  if (sanitizationLevel > 0) {
    sanitizer = &DataSanitizer::instance();
  }

  int totalRows = rowCount();
  for (int row = 0; row < totalRows; ++row) {
    QString lineText = data(index(row, 0), Qt::DisplayRole).toString();
    
    // 应用脱敏
    if (sanitizer) {
      lineText = sanitizer->sanitize(lineText, sanitizationLevel);
    }
    
    stream << lineText << "\n";
  }

  file.close();
  
  qDebug() << "Exported" << totalRows << "lines to" << filePath << "(sanitization level:" << sanitizationLevel << ")";
  return true;
}

// ========== Delta 时间功能实现 ==========

void BigFileModel::setDeltaTimeEnabled(bool enabled) {
  if (m_deltaTimeEnabled != enabled) {
    m_deltaTimeEnabled = enabled;
    m_timestampCache.clear();
    emit deltaTimeEnabledChanged();
    // 刷新显示
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
  }
}

QDateTime BigFileModel::parseTimestamp(const QString &rawLine) const {
  if (rawLine.isEmpty()) {
    return QDateTime();
  }
  
  // 尝试各种时间戳格式
  for (const QRegularExpression &pattern : m_timestampPatterns) {
    QRegularExpressionMatch match = pattern.match(rawLine);
    if (match.hasMatch()) {
      QString captured = match.captured(0);
      
      // Unix 时间戳（毫秒）
      if (captured.length() == 13 && captured.at(0).isDigit()) {
        bool ok;
        qint64 ms = captured.toLongLong(&ok);
        if (ok) {
          return QDateTime::fromMSecsSinceEpoch(ms);
        }
      }
      
      // Unix 时间戳（秒）
      if (captured.length() == 10 && captured.at(0).isDigit()) {
        bool ok;
        qint64 sec = captured.toLongLong(&ok);
        if (ok) {
          return QDateTime::fromSecsSinceEpoch(sec);
        }
      }
      
      // ISO 8601 或常见格式
      QString dateStr = match.captured(1);
      QString timeStr = match.captured(2);
      QString msStr = match.capturedLength(3) > 0 ? match.captured(3) : "000";
      
      // Syslog 格式特殊处理
      if (dateStr.length() == 3) { // 月份缩写
        static const QMap<QString, int> months = {
          {"Jan", 1}, {"Feb", 2}, {"Mar", 3}, {"Apr", 4},
          {"May", 5}, {"Jun", 6}, {"Jul", 7}, {"Aug", 8},
          {"Sep", 9}, {"Oct", 10}, {"Nov", 11}, {"Dec", 12}
        };
        int month = months.value(dateStr, 0);
        int day = match.captured(2).toInt();
        timeStr = match.captured(3);
        if (month > 0) {
          QDate date(QDate::currentDate().year(), month, day);
          QTime time = QTime::fromString(timeStr, "HH:mm:ss");
          if (date.isValid() && time.isValid()) {
            return QDateTime(date, time);
          }
        }
        continue;
      }
      
      // 标准日期时间格式
      QDate date = QDate::fromString(dateStr, "yyyy-MM-dd");
      QTime time = QTime::fromString(timeStr, "HH:mm:ss");
      
      if (date.isValid() && time.isValid()) {
        int ms = msStr.leftJustified(3, '0').left(3).toInt();
        time = time.addMSecs(ms);
        return QDateTime(date, time);
      }
    }
  }
  
  return QDateTime();
}

qint64 BigFileModel::getDeltaTime(int viewRow) const {
  if (viewRow <= 0) {
    return 0; // 第一行没有 delta
  }
  
  int realRow = toRealRow(viewRow);
  int prevRealRow = toRealRow(viewRow - 1);
  
  if (realRow < 0 || prevRealRow < 0) {
    return -1;
  }
  
  // 尝试从缓存获取
  QDateTime *cachedCurrent = m_timestampCache.object(realRow);
  QDateTime *cachedPrev = m_timestampCache.object(prevRealRow);
  
  QDateTime currentTs, prevTs;
  
  if (cachedCurrent) {
    currentTs = *cachedCurrent;
  } else {
    QString currentLine = getLine(realRow);
    currentTs = parseTimestamp(currentLine);
    if (currentTs.isValid()) {
      m_timestampCache.insert(realRow, new QDateTime(currentTs));
    }
  }
  
  if (cachedPrev) {
    prevTs = *cachedPrev;
  } else {
    QString prevLine = getLine(prevRealRow);
    prevTs = parseTimestamp(prevLine);
    if (prevTs.isValid()) {
      m_timestampCache.insert(prevRealRow, new QDateTime(prevTs));
    }
  }
  
  if (currentTs.isValid() && prevTs.isValid()) {
    return prevTs.msecsTo(currentTs);
  }
  
  return -1;
}

QString BigFileModel::formatDeltaTime(qint64 deltaMs) {
  if (deltaMs < 0) {
    return QString();
  }
  
  if (deltaMs == 0) {
    return "+0ms";
  }
  
  QString sign = deltaMs >= 0 ? "+" : "-";
  qint64 absMs = qAbs(deltaMs);
  
  if (absMs < 1000) {
    return QString("%1%2ms").arg(sign).arg(absMs);
  }
  
  if (absMs < 60000) {
    double sec = absMs / 1000.0;
    return QString("%1%2s").arg(sign).arg(sec, 0, 'f', 3);
  }
  
  if (absMs < 3600000) {
    int min = absMs / 60000;
    int sec = (absMs % 60000) / 1000;
    return QString("%1%2m %3s").arg(sign).arg(min).arg(sec);
  }
  
  int hour = absMs / 3600000;
  int min = (absMs % 3600000) / 60000;
  int sec = (absMs % 60000) / 1000;
  return QString("%1%2h %3m %4s").arg(sign).arg(hour).arg(min).arg(sec);
}

// ========== 滚动日志功能实现 ==========

QStringList BigFileModel::detectRollingLogs(const QString &basePath) {
  QStringList result;
  QFileInfo baseInfo(basePath);
  
  if (!baseInfo.exists()) {
    return result;
  }
  
  QString dir = baseInfo.absolutePath();
  QString baseName = baseInfo.completeBaseName();
  QString suffix = baseInfo.suffix();
  
  QDir directory(dir);
  QStringList filters;
  
  // 常见的滚动日志命名模式：
  // app.log.1, app.log.2, ...
  // app.1.log, app.2.log, ...
  // app.log.2024-01-15, app.log.2024-01-14, ...
  // app-2024-01-15.log, app-2024-01-14.log, ...
  
  filters << QString("%1.%2.*").arg(baseName).arg(suffix);  // app.log.1
  filters << QString("%1.*.%2").arg(baseName).arg(suffix);  // app.1.log
  filters << QString("%1-*.%2").arg(baseName).arg(suffix);  // app-2024-01-15.log
  
  QStringList entries = directory.entryList(filters, QDir::Files, QDir::Name);
  
  // 按文件名中的数字排序
  QMap<int, QString> numberedFiles;
  QStringList datedFiles;
  
  for (const QString &entry : entries) {
    QString fullPath = directory.absoluteFilePath(entry);
    
    // 提取数字后缀
    QRegularExpression numRe(R"(\.(\d+)(?:\.[^.]+)?$|\.(\d+)$)");
    QRegularExpressionMatch numMatch = numRe.match(entry);
    
    if (numMatch.hasMatch()) {
      int num = numMatch.captured(1).isEmpty() 
                ? numMatch.captured(2).toInt() 
                : numMatch.captured(1).toInt();
      numberedFiles[num] = fullPath;
    } else {
      // 日期格式的文件
      datedFiles.append(fullPath);
    }
  }
  
  // 按序号排序添加（从大到小，因为 .1 通常是最新的）
  QList<int> nums = numberedFiles.keys();
  std::sort(nums.begin(), nums.end(), std::greater<int>());
  for (int num : nums) {
    result.append(numberedFiles[num]);
  }
  
  // 日期文件按名称排序（通常日期越早的文件名越小）
  std::sort(datedFiles.begin(), datedFiles.end());
  result.append(datedFiles);
  
  // 最后添加基础文件（最新的）
  result.append(basePath);
  
  qDebug() << "Detected rolling logs for" << basePath << ":" << result.size() << "files";
  return result;
}

bool BigFileModel::loadRollingLogs(const QStringList &files) {
  if (files.isEmpty()) {
    return false;
  }
  
  if (files.size() == 1) {
    return loadFile(files.first());
  }
  
  // 创建临时合并文件
  QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QString tempFile = tempDir + "/BigFileViewer_merged_" + 
                     QString::number(QDateTime::currentMSecsSinceEpoch()) + ".log";
  
  QFile output(tempFile);
  if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to create temp file for merging:" << tempFile;
    return false;
  }
  
  qint64 totalSize = 0;
  for (const QString &filePath : files) {
    QFile input(filePath);
    if (!input.open(QIODevice::ReadOnly)) {
      qWarning() << "Failed to open file for merging:" << filePath;
      continue;
    }
    
    // 添加文件分隔注释
    QString separator = QString("\n=== %1 ===\n").arg(QFileInfo(filePath).fileName());
    output.write(separator.toUtf8());
    
    // 分块复制（避免大文件内存问题）
    const qint64 chunkSize = 8 * 1024 * 1024; // 8MB
    while (!input.atEnd()) {
      QByteArray chunk = input.read(chunkSize);
      output.write(chunk);
      totalSize += chunk.size();
    }
    
    input.close();
  }
  
  output.close();
  
  qDebug() << "Merged" << files.size() << "rolling logs into" << tempFile 
           << "total size:" << totalSize;
  
  return loadFile(tempFile);
}

QStringList BigFileModel::getRelatedRollingLogs() const {
  if (m_filePath.isEmpty()) {
    return QStringList();
  }
  return detectRollingLogs(m_filePath);
}

// ============================================================================
// 时间统计功能实现（用于仪表盘）
// ============================================================================

QVariantList BigFileModel::getTimeBasedStatistics(const QString &interval, int maxBuckets) const {
  QVariantList result;
  
  if (!m_mapPtr || m_lineOffsets.empty()) return result;
  
  // 确定时间间隔（毫秒）
  qint64 intervalMs;
  if (interval == "minute") {
    intervalMs = 60 * 1000;
  } else if (interval == "hour") {
    intervalMs = 60 * 60 * 1000;
  } else if (interval == "day") {
    intervalMs = 24 * 60 * 60 * 1000;
  } else {
    intervalMs = 60 * 60 * 1000; // 默认按小时
  }
  
  // 数据结构: 时间桶 -> 统计数据
  struct BucketStats {
    int error = 0;
    int warn = 0;
    int info = 0;
    int debug = 0;
    int trace = 0;
    int other = 0;
  };
  QMap<qint64, BucketStats> buckets;
  
  int totalRows = m_filterMode.load() ? static_cast<int>(m_filteredRows.size()) 
                                       : static_cast<int>(m_lineOffsets.size());
  
  // 采样扫描（大文件限制）
  int step = 1;
  if (totalRows > 50000) {
    step = totalRows / 50000 + 1;
  }
  
  qint64 minTime = LLONG_MAX, maxTime = 0;
  
  for (int viewRow = 0; viewRow < totalRows; viewRow += step) {
    int realRow = m_filterMode.load() ? m_filteredRows[viewRow] : viewRow;
    if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) continue;
    
    // 获取行内容
    QString rawLine = getRawLine(realRow);
    if (rawLine.isEmpty()) continue;
    
    // 解析时间戳
    QDateTime timestamp = parseTimestamp(rawLine);
    if (!timestamp.isValid()) continue;
    
    qint64 msecs = timestamp.toMSecsSinceEpoch();
    qint64 bucketKey = (msecs / intervalMs) * intervalMs;
    
    minTime = qMin(minTime, bucketKey);
    maxTime = qMax(maxTime, bucketKey);
    
    // 检测日志级别
    QString upperLine = rawLine.left(200).toUpper();
    BucketStats &stats = buckets[bucketKey];
    
    if (upperLine.contains("FATAL") || upperLine.contains("CRITICAL") || 
        upperLine.contains("ERROR") || upperLine.contains("FAIL") || 
        upperLine.contains("EXCEPTION") || upperLine.contains("PANIC")) {
      stats.error += step;
    } else if (upperLine.contains("WARN") || upperLine.contains("ALERT") || 
               upperLine.contains("CAUTION")) {
      stats.warn += step;
    } else if (upperLine.contains("INFO") || upperLine.contains("NOTICE")) {
      stats.info += step;
    } else if (upperLine.contains("DEBUG")) {
      stats.debug += step;
    } else if (upperLine.contains("TRACE") || upperLine.contains("VERBOSE")) {
      stats.trace += step;
    } else {
      stats.other += step;
    }
  }
  
  // 限制桶数量
  QList<qint64> sortedKeys = buckets.keys();
  if (sortedKeys.size() > maxBuckets) {
    // 合并相邻桶
    qint64 newIntervalMs = ((maxTime - minTime) / maxBuckets) / intervalMs * intervalMs;
    if (newIntervalMs < intervalMs) newIntervalMs = intervalMs;
    
    QMap<qint64, BucketStats> mergedBuckets;
    for (auto it = buckets.begin(); it != buckets.end(); ++it) {
      qint64 newKey = (it.key() / newIntervalMs) * newIntervalMs;
      BucketStats &merged = mergedBuckets[newKey];
      merged.error += it->error;
      merged.warn += it->warn;
      merged.info += it->info;
      merged.debug += it->debug;
      merged.trace += it->trace;
      merged.other += it->other;
    }
    buckets = mergedBuckets;
  }
  
  // 转换为 QVariantList
  for (auto it = buckets.begin(); it != buckets.end(); ++it) {
    QVariantMap entry;
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(it.key());
    
    // 格式化时间标签
    QString timeLabel;
    if (interval == "minute") {
      timeLabel = dt.toString("HH:mm");
    } else if (interval == "hour") {
      timeLabel = dt.toString("MM-dd HH:00");
    } else {
      timeLabel = dt.toString("yyyy-MM-dd");
    }
    
    entry["timestamp"] = it.key();
    entry["timeLabel"] = timeLabel;
    entry["datetime"] = dt.toString(Qt::ISODate);
    entry["error"] = it->error;
    entry["warn"] = it->warn;
    entry["info"] = it->info;
    entry["debug"] = it->debug;
    entry["trace"] = it->trace;
    entry["other"] = it->other;
    entry["total"] = it->error + it->warn + it->info + it->debug + it->trace + it->other;
    
    result.append(entry);
  }
  
  return result;
}

QVariantMap BigFileModel::getLogTimeRange() const {
  QVariantMap result;
  result["startTime"] = QDateTime();
  result["endTime"] = QDateTime();
  
  if (!m_mapPtr || m_lineOffsets.empty()) return result;
  
  int totalRows = static_cast<int>(m_lineOffsets.size());
  
  // 扫描前 100 行找起始时间
  QDateTime startTime;
  for (int i = 0; i < qMin(100, totalRows); ++i) {
    QString line = getRawLine(i);
    startTime = parseTimestamp(line);
    if (startTime.isValid()) break;
  }
  
  // 扫描后 100 行找结束时间
  QDateTime endTime;
  for (int i = totalRows - 1; i >= qMax(0, totalRows - 100); --i) {
    QString line = getRawLine(i);
    endTime = parseTimestamp(line);
    if (endTime.isValid()) break;
  }
  
  result["startTime"] = startTime;
  result["endTime"] = endTime;
  
  return result;
}

// ============================================================================
// 脱敏导出功能实现
// ============================================================================

#include "DataSanitizer.h"

bool BigFileModel::exportWithSanitization(const QString &path, const QString &format, 
                                           const QVariantMap &sanitizeOptions,
                                           int startRow, int endRow) {
  if (!m_mapPtr) return false;
  
  int totalRows = m_filterMode.load() ? static_cast<int>(m_filteredRows.size()) 
                                       : static_cast<int>(m_lineOffsets.size());
  if (totalRows == 0) return false;
  
  if (endRow < 0 || endRow >= totalRows) endRow = totalRows - 1;
  if (startRow < 0) startRow = 0;
  if (startRow > endRow) return false;
  
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Cannot open file for writing:" << path;
    return false;
  }
  
  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  
  DataSanitizer &sanitizer = DataSanitizer::instance();
  
  // HTML 格式头部
  if (format == "html") {
    stream << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n";
    stream << "<title>Log Export (Sanitized)</title>\n";
    stream << "<style>body{font-family:Consolas,monospace;font-size:12px;background:#1e1e1e;color:#d4d4d4;padding:20px;}\n";
    stream << ".error{color:#f44336;}.warn{color:#ff9800;}.info{color:#2196f3;}.debug{color:#9c27b0;}\n";
    stream << "pre{margin:2px 0;white-space:pre-wrap;word-wrap:break-word;}</style></head><body>\n";
  }
  
  // CSV 格式头部
  if (format == "csv") {
    stream << "\"Line\",\"Level\",\"Content\"\n";
  }
  
  // 导出行
  for (int viewRow = startRow; viewRow <= endRow; ++viewRow) {
    int realRow = m_filterMode.load() ? m_filteredRows[viewRow] : viewRow;
    if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) continue;
    
    QString rawLine = getRawLine(realRow);
    
    // 应用脱敏
    QString sanitizedLine = sanitizer.sanitizeWithOptions(rawLine, sanitizeOptions);
    
    // 检测日志级别
    QString level = "other";
    QString upperLine = rawLine.left(100).toUpper();
    if (upperLine.contains("ERROR") || upperLine.contains("FATAL")) level = "error";
    else if (upperLine.contains("WARN")) level = "warn";
    else if (upperLine.contains("INFO")) level = "info";
    else if (upperLine.contains("DEBUG")) level = "debug";
    
    if (format == "html") {
      QString escapedLine = sanitizedLine.toHtmlEscaped();
      stream << "<pre class=\"" << level << "\">" << escapedLine << "</pre>\n";
    } else if (format == "csv") {
      // 转义双引号
      QString escaped = sanitizedLine.replace("\"", "\"\"");
      stream << "\"" << (realRow + 1) << "\",\"" << level << "\",\"" << escaped << "\"\n";
    } else {
      // 纯文本
      stream << sanitizedLine << "\n";
    }
  }
  
  // HTML 格式尾部
  if (format == "html") {
    stream << "</body></html>\n";
  }
  
  file.close();
  qDebug() << "Exported" << (endRow - startRow + 1) << "lines with sanitization to" << path;
  return true;
}