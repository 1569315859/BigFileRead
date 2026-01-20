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
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QtConcurrent>
#include <algorithm> // for std::search
#include <cctype>    // for std::tolower
#include <cstring>   // for memchr

BigFileModel::BigFileModel(QObject *parent) : QAbstractTableModel(parent) {
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

  // *** 无论成功失败都发送 fileLoaded 信号 ***
  emit fileLoaded(success, message);
  
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
      return LogParser::instance().getFieldWithCache(
          static_cast<qint64>(realRow), rawLine, column);
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
  
  int oldRowCount = static_cast<int>(m_lineOffsets.size());
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
    m_bookmarks.insert(realRowKey);
    qDebug() << "Bookmark added at line" << (realRow + 1);
  }

  QModelIndex idx = index(viewRow, 0);
  emit dataChanged(idx, idx, {BookmarkRole});
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
  
  qDebug() << "All bookmarks cleared";
}

bool BigFileModel::exportToFile(const QString &filePath) const {
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "Failed to open file for export:" << file.errorString();
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  int totalRows = rowCount();
  for (int row = 0; row < totalRows; ++row) {
    QString lineText = data(index(row, 0), Qt::DisplayRole).toString();
    stream << lineText << "\n";
  }

  file.close();
  
  qDebug() << "Exported" << totalRows << "lines to" << filePath;
  return true;
}