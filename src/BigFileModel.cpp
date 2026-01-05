/**
 * @file BigFileModel.cpp
 * @brief 大文件模型类实现 - Timer-Pull 架构
 * @description 使用 QFile::map() 内存映射 + 行偏移索引，实现高性能大文件读取
 *              采用 Timer-Pull 模式：Worker 静默填充 buffer，Timer 主动拉取数据
 * @version 2.1 - Timer-Pull Pattern, 50ms refresh, memchr scan
 */

#include "BigFileModel.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtConcurrent>
#include <algorithm> // for std::search
#include <cctype>    // for std::tolower
#include <cstring>   // for memchr

BigFileModel::BigFileModel(QObject *parent) : QAbstractListModel(parent) {
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
  m_file.setFileName(filePath);

  // 以只读方式打开文件
  if (!m_file.open(QIODevice::ReadOnly)) {
    emit fileLoaded(false, tr("无法打开文件: %1").arg(m_file.errorString()));
    return false;
  }

  m_fileSize = m_file.size();

  // 空文件特殊处理
  if (m_fileSize == 0) {
    beginResetModel();
    m_lineOffsets.clear();
    endResetModel();
    emit fileLoaded(true, tr("文件为空"));
    return true;
  }

  // 内存映射整个文件（仅 64 位系统支持超大文件）
  m_mapPtr = m_file.map(0, m_fileSize);
  if (!m_mapPtr) {
    m_file.close();
    emit fileLoaded(false, tr("内存映射失败: %1").arg(m_file.errorString()));
    return false;
  }

  // 重置状态
  m_cancelRequested.store(false);
  m_isIndexing.store(true);
  m_lastReportedPercent.store(0);

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

  // *** 立即触发一次 Timer 回调，快速显示前几行 ***
  // 启动 Pull Timer（每 50ms 拉取新数据）
  m_refreshTimer->start();

  // 启动异步索引（Worker 线程）
  QFuture<void> future = QtConcurrent::run([this]() { buildIndexAsync(); });
  m_futureWatcher.setFuture(future);

  // 添加文件监控（用于实时日志追踪）
  m_watcher->addPath(filePath);

  // 通知 UI 已开始加载（第一行已可用）
  qDebug() << "File loading started, first line available";

  return true;
}

void BigFileModel::cancelIndexing() { m_cancelRequested.store(true); }

void BigFileModel::closeFile() {
  // 取消索引并等待
  cancelIndexing();
  m_futureWatcher.waitForFinished();

  // 取消过滤并等待
  cancelFilter();
  m_filterWatcher.waitForFinished();

  // 停止 Timer
  if (m_refreshTimer) {
    m_refreshTimer->stop();
  }

  // 移除文件监控
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

  // 清理过滤状态
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
  m_isIndexing.store(false);
  m_lastReportedPercent.store(0);

  endResetModel();
}

int BigFileModel::lineCount() const {
  // 过滤模式下返回过滤后的行数
  if (m_filterMode.load()) {
    return static_cast<int>(m_filteredRows.size());
  }
  return static_cast<int>(m_lineOffsets.size());
}

void BigFileModel::onUpdateTimerTimeout() {
  // === Timer-Pull 模式的核心逻辑 ===
  // 每 50ms 从 pending buffer 拉取数据
  // 定义每帧最大处理行数（防卡死阈值）
  std::vector<qint64> newData;
  bool isWorkerDone = !m_isIndexing.load(); // 检查后台是否早已收工
  bool isPendingEmpty = false;

  // === 动态变速箱逻辑 (V3.0 狂暴版) ===
  // 基础吞吐量
  size_t currentBatchLimit = 5000;

  // Step 1: 加锁取数据
  {
    QMutexLocker locker(&m_pendingMutex);
    size_t pendingSize = m_pendingOffsets.size();
    isPendingEmpty = (pendingSize == 0);

    if (!isPendingEmpty) {
      if (isWorkerDone) {
        // 【狂暴模式】
        // 后台已经扫完了，用户现在只想要结果！
        // 牺牲一点掉帧，换取极速加载。
        // 经测试，Qt处理 50万行 insert 大约耗时 100-200ms，是可以接受的。
        if (pendingSize > 500000) {
          currentBatchLimit = 100000; // 积压超多，每次吞 100万
        } else {
          currentBatchLimit = 50000; // 积压较多，每次吞 50万
        }
      } else {
        // 【平滑模式】
        // 后台还在跑，用户可能在滚动查看，必须保证鼠标绝对跟手。
        if (pendingSize > 100000) {
          currentBatchLimit = 20000; // 积压稍多，加速一点
        } else {
          currentBatchLimit = 5000; // 正常状态，极致丝滑
        }
      }

      // 执行数据搬运
      if (pendingSize > currentBatchLimit) {
        // 切片取出
        auto beginIt = m_pendingOffsets.begin();
        auto endIt = m_pendingOffsets.begin() + currentBatchLimit;
        newData.assign(beginIt, endIt);
        // 移除已取出的部分
        m_pendingOffsets.erase(beginIt, endIt);
      } else {
        // 全部取出 (Swap 最快)
        newData.swap(m_pendingOffsets);
        // 保持一定的 capacity 避免反复申请内存
        if (m_pendingOffsets.capacity() < 20000) {
          m_pendingOffsets.reserve(20000);
        }
      }
    }
  }

  // std::vector<qint64> newData;

  // // Step 1: 短暂加锁，swap 出数据（微秒级）
  // {
  //     QMutexLocker locker(&m_pendingMutex);
  //     if (m_pendingOffsets.empty()) {
  //         return;  // 无数据，直接返回
  //     }
  //     // 使用 swap 高效转移数据
  //     newData.swap(m_pendingOffsets);
  //     // 为下一批预留空间
  //     m_pendingOffsets.reserve(100000);
  // }
  // 锁已释放

  // Step 2: 在主线程安全地更新模型
  if (!newData.empty()) {
    int startRow = static_cast<int>(m_lineOffsets.size());
    int endRow = startRow + static_cast<int>(newData.size()) - 1;

    // *** 关键：使用 beginInsertRows/endInsertRows，不是 layoutChanged ***
    beginInsertRows(QModelIndex(), startRow, endRow);

    // 合并数据到主向量
    m_lineOffsets.insert(m_lineOffsets.end(), newData.begin(), newData.end());

    endInsertRows();
    // 视图和滚动条会自动更新
  }
  if (isWorkerDone && isPendingEmpty && newData.empty()) {
    m_refreshTimer->stop();
    m_lineOffsets.shrink_to_fit();
    qDebug() << "Full Load Complete. Total lines:" << m_lineOffsets.size();

    // 这里发送最终完成信号
    emit fileLoaded(true, tr("加载完成: %1 行").arg(m_lineOffsets.size()));
  }
}

void BigFileModel::buildIndexAsync() {
  // === Worker 线程：静默扫描，只填充 buffer，不发信号 ===

  QElapsedTimer timer;
  timer.start();

  const char *ptr = reinterpret_cast<const char *>(m_mapPtr);
  const char *end = ptr + m_fileSize;
  const char *fileStart = ptr;

  // 本地缓冲区（栈上）
  std::vector<qint64> localBuffer;
  localBuffer.reserve(WORKER_CHUNK_SIZE + 100);

  int linesInChunk = 0;
  bool firstBatchFlushed = false;

  // 首批快速 flush 的行数（让用户立即看到内容）
  const int FIRST_BATCH_SIZE = 100;

  // 使用 memchr 高速扫描换行符
  while (ptr < end) {
    // 检查取消请求（每 1MB）
    qint64 pos = ptr - fileStart;
    if ((pos & 0xFFFFF) == 0) { // 每 1MB 检查一次
      if (m_cancelRequested.load()) {
        emit indexingCancelled();
        m_isIndexing.store(false);
        return;
      }

      // 进度报告（限制频率，避免信号风暴）
      int percent = static_cast<int>((pos * 100) / m_fileSize);
      int lastPercent = m_lastReportedPercent.load();
      if (percent > lastPercent && percent - lastPercent >= 2) {
        m_lastReportedPercent.store(percent);
        emit indexingProgress(percent);
      }
    }

    // memchr 比逐字节扫描快 10-20 倍
    const char *newline = static_cast<const char *>(
        memchr(ptr, '\n', static_cast<size_t>(end - ptr)));

    if (!newline) {
      break; // 没有更多换行符
    }

    qint64 nextLineOffset = (newline - fileStart) + 1;
    if (nextLineOffset < m_fileSize) {
      localBuffer.push_back(nextLineOffset);
      ++linesInChunk;

      // *** 首批数据快速 flush（100 行），让用户立即看到内容 ***
      int flushThreshold =
          firstBatchFlushed ? WORKER_CHUNK_SIZE : FIRST_BATCH_SIZE;

      if (linesInChunk >= flushThreshold) {
        {
          QMutexLocker locker(&m_pendingMutex);
          m_pendingOffsets.insert(m_pendingOffsets.end(), localBuffer.begin(),
                                  localBuffer.end());
        }
        localBuffer.clear();
        linesInChunk = 0;
        firstBatchFlushed = true;
        // *** 不发送任何信号 - Timer 会自动拉取 ***
      }
    }

    ptr = newline + 1;
  }

  // Flush 剩余数据
  if (!localBuffer.empty()) {
    QMutexLocker locker(&m_pendingMutex);
    m_pendingOffsets.insert(m_pendingOffsets.end(), localBuffer.begin(),
                            localBuffer.end());
  }

  qint64 elapsed = timer.elapsed();

  // 计算最终行数
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
  m_isIndexing.store(false);

  if (!success) {
    // 如果是失败/取消，则立即停止 Timer
    m_refreshTimer->stop();
    emit fileLoaded(false, message);
  }
  // 最后一次拉取所有剩余数据
  // onUpdateTimerTimeout();

  // // 停止 Timer
  // m_refreshTimer->stop();

  // // 收缩内存
  // m_lineOffsets.shrink_to_fit();

  // m_isIndexing.store(false);
  // emit fileLoaded(success, message);
}

// === 禁用 fetchMore 机制 ===
bool BigFileModel::canFetchMore(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return false; // 我们使用 Timer-Pull 主动推送，不需要 fetchMore
}

void BigFileModel::fetchMore(const QModelIndex &parent) {
  Q_UNUSED(parent)
  // 不做任何事情
}

qint64 BigFileModel::getLineLength(int row) const {
  // 主线程访问，无需锁
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
  // 主线程访问，无需锁
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

  // 处理 Windows 风格换行 (\r\n)
  if (length > 0) {
    const uchar *lineEnd = m_mapPtr + endOffset - 1;
    if (*lineEnd == '\r') {
      --length;
    }
  }

  if (length <= 0) {
    return QString();
  }

  // 长行截断优化
  bool truncated = false;
  if (length > MAX_DISPLAY_LENGTH) {
    length = MAX_DISPLAY_LENGTH;
    truncated = true;
  }

  // 使用配置的解码器从映射内存创建 QString
  const char *lineStart =
      reinterpret_cast<const char *>(m_mapPtr + startOffset);
  QString result = m_decoder.decode(QByteArrayView(lineStart, static_cast<qsizetype>(length)));

  if (truncated) {
    result += QStringLiteral("  ... [truncated]");
  }

  return result;
}

int BigFileModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }
  // *** 关键：过滤模式下返回过滤后的行数 ***
  if (m_filterMode.load()) {
    return static_cast<int>(m_filteredRows.size());
  }
  return static_cast<int>(m_lineOffsets.size());
}

QVariant BigFileModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  int viewRow = index.row();
  
  // *** 关键：视图行号 -> 真实行号转换 ***
  int realRow = toRealRow(viewRow);
  
  if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) {
    return QVariant();
  }

  switch (role) {
  case Qt::DisplayRole:
  case Qt::EditRole:
    return getLine(realRow);

  case Qt::ToolTipRole: {
    // 显示真实行号和原始长度
    qint64 len = getLineLength(realRow);
    if (len > MAX_DISPLAY_LENGTH) {
      return tr("行 %1 (原始长度: %2 字符, 已截断)").arg(realRow + 1).arg(len);
    }
    return tr("行 %1").arg(realRow + 1);
  }

  case BookmarkRole:
    // 返回该行是否已添加书签
    return m_bookmarks.contains(static_cast<qint64>(realRow));

  default:
    return QVariant();
  }
}

// ========== 编码设置实现 ==========

void BigFileModel::setEncoding(QStringConverter::Encoding encoding) {
  // 重新创建解码器
  m_decoder = QStringDecoder(encoding);
  
  // 通知视图刷新所有数据
  beginResetModel();
  endResetModel();
  
  qDebug() << "Encoding changed to:" << (encoding == QStringConverter::Utf8 ? "UTF-8" : "System");
}

// ========== 实时日志监控实现 ==========

void BigFileModel::onFileChanged(const QString &path) {
  // 忽略正在索引时的变化
  if (m_isIndexing.load()) {
    return;
  }
  
  // 检查新文件大小
  QFileInfo fi(path);
  qint64 newSize = fi.size();
  
  // 忽略文件缩小或未变化的情况
  if (newSize <= m_fileSize) {
    // 重新添加监控（某些系统会在文件变化后移除监控）
    if (!m_watcher->files().contains(path)) {
      m_watcher->addPath(path);
    }
    return;
  }
  
  qDebug() << "File changed, new size:" << newSize << "old size:" << m_fileSize;
  
  // 保存旧的行数
  int oldRowCount = static_cast<int>(m_lineOffsets.size());
  qint64 oldSize = m_fileSize;
  
  // === 重新映射文件 ===
  if (m_mapPtr) {
    m_file.unmap(m_mapPtr);
    m_mapPtr = nullptr;
  }
  
  m_mapPtr = m_file.map(0, newSize);
  if (!m_mapPtr) {
    qWarning() << "Failed to remap file to new size:" << newSize;
    // 尝试恢复旧映射
    m_mapPtr = m_file.map(0, m_fileSize);
    return;
  }
  
  // === 增量扫描新内容 ===
  const uchar *start = m_mapPtr + oldSize;
  const uchar *end = m_mapPtr + newSize;
  
  // 使用 memchr 快速查找换行符
  const uchar *ptr = start;
  while (ptr < end) {
    const uchar *found = static_cast<const uchar *>(
        std::memchr(ptr, '\n', static_cast<size_t>(end - ptr)));
    
    if (found) {
      // 找到换行符，下一行的偏移量是换行符后一个位置
      qint64 nextLineOffset = (found - m_mapPtr) + 1;
      if (nextLineOffset < newSize) {
        m_lineOffsets.push_back(nextLineOffset);
      }
      ptr = found + 1;
    } else {
      break;  // 没有更多换行符
    }
  }
  
  int newRowCount = static_cast<int>(m_lineOffsets.size());
  
  // 通知视图有新行插入
  if (newRowCount > oldRowCount) {
    beginInsertRows(QModelIndex(), oldRowCount, newRowCount - 1);
    endInsertRows();
    
    qDebug() << "Added" << (newRowCount - oldRowCount) << "new lines";
  }
  
  // 更新文件大小
  m_fileSize = newSize;
  
  // 发射日志追加信号
  emit logAppended();
  
  // 重新添加监控（某些系统会在文件变化后移除监控）
  if (!m_watcher->files().contains(path)) {
    m_watcher->addPath(path);
  }
}

// ========== 搜索功能实现 ==========

void BigFileModel::search(const QString &text, bool useRegex) {
  // 取消之前的搜索
  if (m_isSearching.load()) {
    cancelSearch();
    m_searchWatcher.waitForFinished();
  }

  // 清空之前的结果
  m_searchResults.clear();

  // 空搜索文本
  if (text.isEmpty()) {
    emit searchFinished(m_searchResults);
    return;
  }

  // 启动异步搜索
  m_searchCancelRequested.store(false);
  m_isSearching.store(true);

  QFuture<void> future =
      QtConcurrent::run([this, text, useRegex]() { executeSearchAsync(text, useRegex); });
  m_searchWatcher.setFuture(future);
}

void BigFileModel::cancelSearch() { m_searchCancelRequested.store(true); }

void BigFileModel::executeSearchAsync(const QString &searchText, bool useRegex) {
  // === 后台线程：行级搜索（支持正则和普通字符串）===

  QElapsedTimer timer;
  timer.start();

  std::vector<int> results;
  results.reserve(10000); // 预分配，避免频繁扩容

  const int totalLines = static_cast<int>(m_lineOffsets.size());
  int lastPercent = 0;

  // === 正则表达式搜索 ===
  if (useRegex) {
    QRegularExpression regex(searchText, QRegularExpression::CaseInsensitiveOption);
    if (!regex.isValid()) {
      qWarning() << "Invalid regex pattern:" << regex.errorString();
      m_isSearching.store(false);
      emit searchFinished(results);
      return;
    }

    for (int row = 0; row < totalLines; ++row) {
      // 定期检查取消请求和报告进度
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

      // 获取行文本（需要构造 QString 用于正则匹配）
      QString lineText = getLine(row);
      
      // 使用正则匹配
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
  }
  // === 普通字符串搜索（高性能原始内存搜索）===
  else {
    // 转换搜索文本为 UTF-8 字节数组
    QByteArray searchBytes = searchText.toUtf8();
    const char *searchPtr = searchBytes.constData();
    const int searchLen = searchBytes.size();

    if (searchLen == 0) {
      m_isSearching.store(false);
      emit searchFinished(results);
      return;
    }

    const char *fileStart = reinterpret_cast<const char *>(m_mapPtr);

    // 大小写不敏感比较器
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

  // 保存结果并通知主线程
  m_searchResults = results;
  m_isSearching.store(false);
  emit searchProgress(100);
  emit searchFinished(results);
}

// ========== 虚拟行映射辅助函数 ==========

int BigFileModel::toRealRow(int viewRow) const {
  if (!m_filterMode.load()) {
    // 非过滤模式：视图行号 = 真实行号
    return viewRow;
  }
  
  // 过滤模式：通过映射向量获取真实行号
  if (viewRow >= 0 && viewRow < static_cast<int>(m_filteredRows.size())) {
    return m_filteredRows[viewRow];
  }
  
  return -1;  // 无效行号
}

// ========== 日志过滤功能实现 (Virtual Mapping Vector) ==========

void BigFileModel::applyFilter(const QString &keyword, bool useRegex) {
  // 取消之前的过滤操作
  if (m_isFiltering.load()) {
    cancelFilter();
    m_filterWatcher.waitForFinished();
  }

  // 空关键词 = 清除过滤
  if (keyword.isEmpty()) {
    clearFilter();
    return;
  }

  // 保存关键词
  m_filterKeyword = keyword;
  
  // 启动异步过滤
  m_filterCancelRequested.store(false);
  m_isFiltering.store(true);

  QFuture<void> future =
      QtConcurrent::run([this, keyword, useRegex]() { executeFilterAsync(keyword, useRegex); });
  m_filterWatcher.setFuture(future);
}

void BigFileModel::clearFilter() {
  // 取消正在进行的过滤
  if (m_isFiltering.load()) {
    cancelFilter();
    m_filterWatcher.waitForFinished();
  }

  // 如果不在过滤模式，无需操作
  if (!m_filterMode.load()) {
    return;
  }

  // 切换回非过滤模式
  beginResetModel();
  m_filterMode.store(false);
  m_filteredRows.clear();
  m_filteredRows.shrink_to_fit();
  m_filterKeyword.clear();
  endResetModel();

  qDebug() << "Filter cleared, showing all" << m_lineOffsets.size() << "lines";
  emit filterFinished(static_cast<int>(m_lineOffsets.size()));
}

void BigFileModel::cancelFilter() {
  m_filterCancelRequested.store(true);
}

void BigFileModel::executeFilterAsync(const QString &keyword, bool useRegex) {
  // === 后台线程：高性能行级过滤（支持正则和普通字符串）===
  
  QElapsedTimer timer;
  timer.start();

  std::vector<int> matchedRows;
  matchedRows.reserve(100000);  // 预分配

  const int totalLines = static_cast<int>(m_lineOffsets.size());
  int lastPercent = 0;

  // === 正则表达式过滤 ===
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

      // 获取行文本（需要构造 QString 用于正则匹配）
      QString lineText = getLine(row);
      
      if (regex.match(lineText).hasMatch()) {
        matchedRows.push_back(row);
      }
    }
  }
  // === 普通字符串过滤（高性能原始内存搜索）===
  else {
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

  // === 在主线程更新模型 ===
  QMetaObject::invokeMethod(this, [this, matchedRows = std::move(matchedRows)]() {
    beginResetModel();
    m_filteredRows = std::move(matchedRows);
    m_filterMode.store(true);
    endResetModel();

    m_isFiltering.store(false);
    emit filterProgress(100);
    emit filterFinished(static_cast<int>(m_filteredRows.size()));
  }, Qt::QueuedConnection);
}

// ========== 书签功能实现 ==========

void BigFileModel::toggleBookmark(int viewRow) {
  // 转换为真实行号
  int realRow = toRealRow(viewRow);
  if (realRow < 0 || realRow >= static_cast<int>(m_lineOffsets.size())) {
    return;
  }

  qint64 realRowKey = static_cast<qint64>(realRow);
  
  if (m_bookmarks.contains(realRowKey)) {
    // 移除书签
    m_bookmarks.remove(realRowKey);
    qDebug() << "Bookmark removed at line" << (realRow + 1);
  } else {
    // 添加书签
    m_bookmarks.insert(realRowKey);
    qDebug() << "Bookmark added at line" << (realRow + 1);
  }

  // 通知视图该行需要重绘
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
  
  // 从当前行的下一行开始搜索
  for (int viewRow = currentViewRow + 1; viewRow < totalViewRows; ++viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow >= 0 && m_bookmarks.contains(static_cast<qint64>(realRow))) {
      return viewRow;
    }
  }
  
  // 循环回到开头继续搜索
  for (int viewRow = 0; viewRow <= currentViewRow && viewRow < totalViewRows; ++viewRow) {
    int realRow = toRealRow(viewRow);
    if (realRow >= 0 && m_bookmarks.contains(static_cast<qint64>(realRow))) {
      return viewRow;
    }
  }
  
  return -1;  // 没有找到书签
}

void BigFileModel::clearAllBookmarks() {
  if (m_bookmarks.isEmpty()) {
    return;
  }
  
  m_bookmarks.clear();
  
  // 通知视图所有行都可能需要重绘
  emit dataChanged(index(0, 0), index(rowCount() - 1, 0), {BookmarkRole});
  
  qDebug() << "All bookmarks cleared";
}