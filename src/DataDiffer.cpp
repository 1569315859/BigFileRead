/**
 * @file DataDiffer.cpp
 * @brief 数据差异对比器实现
 */

#include "DataDiffer.h"
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QtConcurrent>
#include <QDebug>
#include <algorithm>
#include <functional>

DataDiffer& DataDiffer::instance()
{
    static DataDiffer instance;
    return instance;
}

DataDiffer::DataDiffer(QObject *parent)
    : QObject(parent)
    , m_watcher(new QFutureWatcher<void>(this))
{
    connect(m_watcher, &QFutureWatcher<void>::finished, this, [this]() {
        m_isComparing = false;
        m_progress = 100;
        emit progressChanged();
        emit comparingChanged();
        emit compareCompleted(getStats());
    });
}

DataDiffer::~DataDiffer()
{
    cancel();
}

// ===================== 文件对比 =====================

void DataDiffer::compareFiles(const QString &leftPath, const QString &rightPath,
                               bool ignoreWhitespace, bool ignoreCase)
{
    // 读取左侧文件
    QFile leftFile(leftPath);
    if (!leftFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit compareError(tr("Cannot open left file: %1").arg(leftPath));
        return;
    }
    
    // 读取右侧文件
    QFile rightFile(rightPath);
    if (!rightFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit compareError(tr("Cannot open right file: %1").arg(rightPath));
        return;
    }
    
    QString leftContent = QString::fromUtf8(leftFile.readAll());
    QString rightContent = QString::fromUtf8(rightFile.readAll());
    
    m_leftPath = leftPath;
    m_rightPath = rightPath;
    
    compareText(leftContent, rightContent, ignoreWhitespace, ignoreCase);
}

void DataDiffer::compareText(const QString &leftContent, const QString &rightContent,
                              bool ignoreWhitespace, bool ignoreCase)
{
    if (m_isComparing) {
        cancel();
    }
    
    m_cancelled = false;
    m_isComparing = true;
    m_progress = 0;
    emit comparingChanged();
    emit progressChanged();
    
    // 分割为行
    m_leftLines = leftContent.split('\n');
    m_rightLines = rightContent.split('\n');
    
    // 异步处理
    QFuture<void> future = QtConcurrent::run([this, ignoreWhitespace, ignoreCase]() {
        m_diffs = computeDiff(m_leftLines, m_rightLines, ignoreWhitespace, ignoreCase);
        m_hunks = groupIntoHunks(m_diffs, 3);
        
        // 计算统计
        m_stats = DiffStats();
        m_stats.totalLines = qMax(m_leftLines.size(), m_rightLines.size());
        
        for (const LineDiff &diff : m_diffs) {
            switch (diff.type) {
                case DiffType::Unchanged:
                    m_stats.unchangedLines++;
                    break;
                case DiffType::Added:
                    m_stats.addedLines++;
                    break;
                case DiffType::Deleted:
                    m_stats.deletedLines++;
                    break;
                case DiffType::Modified:
                    m_stats.modifiedLines++;
                    break;
            }
        }
        
        if (m_stats.totalLines > 0) {
            m_stats.similarity = static_cast<double>(m_stats.unchangedLines) / m_stats.totalLines;
        }
    });
    
    m_watcher->setFuture(future);
}

// ===================== CSV 对比 =====================

void DataDiffer::compareCSV(const QString &leftPath, const QString &rightPath,
                             const QList<int> &keyColumns, const QList<int> &ignoreColumns)
{
    // 读取 CSV 文件（简化实现，完整版应使用 CSVDataSource）
    QFile leftFile(leftPath);
    QFile rightFile(rightPath);
    
    if (!leftFile.open(QIODevice::ReadOnly | QIODevice::Text) ||
        !rightFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        emit compareError(tr("Cannot open CSV files"));
        return;
    }
    
    // 简单按行对比（完整实现应按主键匹配）
    QString leftContent = QString::fromUtf8(leftFile.readAll());
    QString rightContent = QString::fromUtf8(rightFile.readAll());
    
    m_leftPath = leftPath;
    m_rightPath = rightPath;
    
    compareText(leftContent, rightContent, false, false);
}

// ===================== 结果访问 =====================

QVariantMap DataDiffer::getStats() const
{
    QVariantMap result;
    result["totalLines"] = m_stats.totalLines;
    result["unchangedLines"] = m_stats.unchangedLines;
    result["addedLines"] = m_stats.addedLines;
    result["deletedLines"] = m_stats.deletedLines;
    result["modifiedLines"] = m_stats.modifiedLines;
    result["similarity"] = m_stats.similarity;
    result["leftPath"] = m_leftPath;
    result["rightPath"] = m_rightPath;
    return result;
}

QVariantList DataDiffer::getHunks() const
{
    QVariantList result;
    
    for (const DiffHunk &hunk : m_hunks) {
        QVariantMap hunkMap;
        hunkMap["leftStart"] = hunk.leftStart;
        hunkMap["leftCount"] = hunk.leftCount;
        hunkMap["rightStart"] = hunk.rightStart;
        hunkMap["rightCount"] = hunk.rightCount;
        
        QVariantList lines;
        for (const LineDiff &diff : hunk.lines) {
            QVariantMap lineMap;
            lineMap["leftLine"] = diff.leftLine;
            lineMap["rightLine"] = diff.rightLine;
            lineMap["type"] = static_cast<int>(diff.type);
            lineMap["leftContent"] = diff.leftContent;
            lineMap["rightContent"] = diff.rightContent;
            lines.append(lineMap);
        }
        hunkMap["lines"] = lines;
        
        result.append(hunkMap);
    }
    
    return result;
}

QVariantList DataDiffer::getAllDiffs(bool includeUnchanged) const
{
    QVariantList result;
    
    for (const LineDiff &diff : m_diffs) {
        if (!includeUnchanged && diff.type == DiffType::Unchanged) {
            continue;
        }
        
        QVariantMap lineMap;
        lineMap["leftLine"] = diff.leftLine;
        lineMap["rightLine"] = diff.rightLine;
        lineMap["type"] = static_cast<int>(diff.type);
        lineMap["typeName"] = diff.type == DiffType::Added ? "added" :
                              diff.type == DiffType::Deleted ? "deleted" :
                              diff.type == DiffType::Modified ? "modified" : "unchanged";
        lineMap["leftContent"] = diff.leftContent;
        lineMap["rightContent"] = diff.rightContent;
        result.append(lineMap);
    }
    
    return result;
}

QVariantList DataDiffer::getDiffsInRange(int startLine, int endLine) const
{
    QVariantList result;
    
    for (const LineDiff &diff : m_diffs) {
        int lineNo = diff.leftLine >= 0 ? diff.leftLine : diff.rightLine;
        if (lineNo >= startLine && lineNo <= endLine) {
            QVariantMap lineMap;
            lineMap["leftLine"] = diff.leftLine;
            lineMap["rightLine"] = diff.rightLine;
            lineMap["type"] = static_cast<int>(diff.type);
            lineMap["leftContent"] = diff.leftContent;
            lineMap["rightContent"] = diff.rightContent;
            result.append(lineMap);
        }
    }
    
    return result;
}

int DataDiffer::getNextDiff(int currentLine, int direction) const
{
    if (direction > 0) {
        for (const LineDiff &diff : m_diffs) {
            if (diff.type != DiffType::Unchanged) {
                int lineNo = diff.leftLine >= 0 ? diff.leftLine : diff.rightLine;
                if (lineNo > currentLine) {
                    return lineNo;
                }
            }
        }
    } else {
        for (int i = m_diffs.size() - 1; i >= 0; --i) {
            const LineDiff &diff = m_diffs[i];
            if (diff.type != DiffType::Unchanged) {
                int lineNo = diff.leftLine >= 0 ? diff.leftLine : diff.rightLine;
                if (lineNo < currentLine) {
                    return lineNo;
                }
            }
        }
    }
    
    return -1;
}

int DataDiffer::getDiffCount() const
{
    int count = 0;
    for (const LineDiff &diff : m_diffs) {
        if (diff.type != DiffType::Unchanged) {
            count++;
        }
    }
    return count;
}

void DataDiffer::cancel()
{
    m_cancelled = true;
    if (m_watcher->isRunning()) {
        m_watcher->cancel();
        m_watcher->waitForFinished();
    }
    m_isComparing = false;
    emit comparingChanged();
}

// ===================== 导出 =====================

void DataDiffer::exportDiff(const QString &path, const QString &format)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit compareError(tr("Cannot create output file: %1").arg(path));
        return;
    }
    
    QTextStream out(&file);
    
    if (format == "html") {
        // HTML 格式
        out << "<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n";
        out << "<title>Diff Report</title>\n";
        out << "<style>\n";
        out << "body { font-family: monospace; }\n";
        out << ".added { background-color: #e6ffec; }\n";
        out << ".deleted { background-color: #ffebe9; }\n";
        out << ".modified { background-color: #fff3cd; }\n";
        out << ".line-num { color: #666; width: 50px; display: inline-block; }\n";
        out << ".stats { margin: 20px 0; padding: 10px; background: #f0f0f0; }\n";
        out << "</style></head><body>\n";
        
        // 统计
        out << "<div class=\"stats\">\n";
        out << QString("<p>Left: %1</p>\n").arg(m_leftPath);
        out << QString("<p>Right: %1</p>\n").arg(m_rightPath);
        out << QString("<p>Added: %1 | Deleted: %2 | Modified: %3 | Unchanged: %4</p>\n")
               .arg(m_stats.addedLines).arg(m_stats.deletedLines)
               .arg(m_stats.modifiedLines).arg(m_stats.unchangedLines);
        out << QString("<p>Similarity: %1%</p>\n").arg(m_stats.similarity * 100, 0, 'f', 1);
        out << "</div>\n";
        
        // 差异内容
        out << "<div class=\"diff\">\n";
        for (const LineDiff &diff : m_diffs) {
            QString cssClass;
            QString prefix;
            switch (diff.type) {
                case DiffType::Added:
                    cssClass = "added";
                    prefix = "+";
                    break;
                case DiffType::Deleted:
                    cssClass = "deleted";
                    prefix = "-";
                    break;
                case DiffType::Modified:
                    cssClass = "modified";
                    prefix = "~";
                    break;
                default:
                    prefix = " ";
            }
            
            QString content = diff.type == DiffType::Deleted ? diff.leftContent : diff.rightContent;
            content = content.toHtmlEscaped();
            
            out << QString("<div class=\"%1\"><span class=\"line-num\">%2</span>%3 %4</div>\n")
                   .arg(cssClass)
                   .arg(diff.leftLine >= 0 ? QString::number(diff.leftLine + 1) : "-")
                   .arg(prefix)
                   .arg(content);
        }
        out << "</div>\n</body></html>\n";
        
    } else if (format == "patch") {
        // Unified diff 格式
        out << QString("--- %1\n").arg(m_leftPath);
        out << QString("+++ %1\n").arg(m_rightPath);
        
        for (const DiffHunk &hunk : m_hunks) {
            out << QString("@@ -%1,%2 +%3,%4 @@\n")
                   .arg(hunk.leftStart + 1).arg(hunk.leftCount)
                   .arg(hunk.rightStart + 1).arg(hunk.rightCount);
            
            for (const LineDiff &diff : hunk.lines) {
                switch (diff.type) {
                    case DiffType::Added:
                        out << "+" << diff.rightContent << "\n";
                        break;
                    case DiffType::Deleted:
                        out << "-" << diff.leftContent << "\n";
                        break;
                    case DiffType::Modified:
                        out << "-" << diff.leftContent << "\n";
                        out << "+" << diff.rightContent << "\n";
                        break;
                    default:
                        out << " " << diff.leftContent << "\n";
                }
            }
        }
        
    } else {
        // 纯文本格式
        out << QString("Left: %1\n").arg(m_leftPath);
        out << QString("Right: %1\n").arg(m_rightPath);
        out << QString("Added: %1 | Deleted: %2 | Modified: %3\n\n")
               .arg(m_stats.addedLines).arg(m_stats.deletedLines).arg(m_stats.modifiedLines);
        
        for (const LineDiff &diff : m_diffs) {
            if (diff.type == DiffType::Unchanged) continue;
            
            QString prefix;
            switch (diff.type) {
                case DiffType::Added:    prefix = "[+]"; break;
                case DiffType::Deleted:  prefix = "[-]"; break;
                case DiffType::Modified: prefix = "[~]"; break;
                default: break;
            }
            
            QString content = diff.type == DiffType::Deleted ? diff.leftContent : diff.rightContent;
            out << QString("%1 Line %2: %3\n")
                   .arg(prefix)
                   .arg(diff.leftLine >= 0 ? diff.leftLine + 1 : diff.rightLine + 1)
                   .arg(content);
        }
    }
}

// ===================== 差异算法 =====================

QVector<LineDiff> DataDiffer::computeDiff(const QStringList &left, const QStringList &right,
                                           bool ignoreWhitespace, bool ignoreCase) const
{
    QVector<LineDiff> result;
    
    int n = left.size();
    int m = right.size();
    
    // 使用 LCS（最长公共子序列）算法
    // 构建 DP 表
    QVector<QVector<int>> dp(n + 1, QVector<int>(m + 1, 0));
    
    // 预计算哈希
    QVector<uint> leftHashes(n);
    QVector<uint> rightHashes(m);
    
    for (int i = 0; i < n; ++i) {
        leftHashes[i] = hashLine(normalizeLine(left[i], ignoreWhitespace, ignoreCase));
    }
    for (int j = 0; j < m; ++j) {
        rightHashes[j] = hashLine(normalizeLine(right[j], ignoreWhitespace, ignoreCase));
    }
    
    // 填充 DP 表
    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {
            if (leftHashes[i-1] == rightHashes[j-1] &&
                normalizeLine(left[i-1], ignoreWhitespace, ignoreCase) ==
                normalizeLine(right[j-1], ignoreWhitespace, ignoreCase)) {
                dp[i][j] = dp[i-1][j-1] + 1;
            } else {
                dp[i][j] = qMax(dp[i-1][j], dp[i][j-1]);
            }
        }
    }
    
    // 回溯构建差异
    int i = n, j = m;
    QVector<LineDiff> tempDiffs;
    
    while (i > 0 || j > 0) {
        LineDiff diff;
        
        if (i > 0 && j > 0 &&
            normalizeLine(left[i-1], ignoreWhitespace, ignoreCase) ==
            normalizeLine(right[j-1], ignoreWhitespace, ignoreCase)) {
            // 相同
            diff.type = DiffType::Unchanged;
            diff.leftLine = i - 1;
            diff.rightLine = j - 1;
            diff.leftContent = left[i-1];
            diff.rightContent = right[j-1];
            --i;
            --j;
        } else if (j > 0 && (i == 0 || dp[i][j-1] >= dp[i-1][j])) {
            // 新增
            diff.type = DiffType::Added;
            diff.leftLine = -1;
            diff.rightLine = j - 1;
            diff.rightContent = right[j-1];
            --j;
        } else {
            // 删除
            diff.type = DiffType::Deleted;
            diff.leftLine = i - 1;
            diff.rightLine = -1;
            diff.leftContent = left[i-1];
            --i;
        }
        
        tempDiffs.prepend(diff);
    }
    
    // 检测修改（相邻的删除+新增可能是修改）
    for (int k = 0; k < tempDiffs.size() - 1; ++k) {
        if (tempDiffs[k].type == DiffType::Deleted &&
            tempDiffs[k+1].type == DiffType::Added) {
            // 计算相似度
            const QString &oldLine = tempDiffs[k].leftContent;
            const QString &newLine = tempDiffs[k+1].rightContent;
            
            // 如果行较短，或相似度超过 50%，标记为修改
            int commonLen = 0;
            int minLen = qMin(oldLine.length(), newLine.length());
            for (int c = 0; c < minLen; ++c) {
                if (oldLine[c] == newLine[c]) commonLen++;
            }
            
            double similarity = minLen > 0 ? static_cast<double>(commonLen) / minLen : 0;
            
            if (similarity > 0.3 || (oldLine.length() < 10 && newLine.length() < 10)) {
                // 合并为修改
                LineDiff modDiff;
                modDiff.type = DiffType::Modified;
                modDiff.leftLine = tempDiffs[k].leftLine;
                modDiff.rightLine = tempDiffs[k+1].rightLine;
                modDiff.leftContent = oldLine;
                modDiff.rightContent = newLine;
                modDiff.charDiffs = computeCharDiff(oldLine, newLine);
                
                result.append(modDiff);
                ++k;  // 跳过下一个
                continue;
            }
        }
        
        result.append(tempDiffs[k]);
    }
    
    if (!tempDiffs.isEmpty()) {
        result.append(tempDiffs.last());
    }
    
    return result;
}

QVector<QPair<int, int>> DataDiffer::computeLCS(const QStringList &left, const QStringList &right) const
{
    // 简化版 LCS，返回匹配的索引对
    QVector<QPair<int, int>> matches;
    // ... 完整实现略
    return matches;
}

QList<QPair<int, int>> DataDiffer::computeCharDiff(const QString &left, const QString &right) const
{
    QList<QPair<int, int>> diffs;
    
    int i = 0, j = 0;
    int n = left.length(), m = right.length();
    
    while (i < n && j < m) {
        if (left[i] == right[j]) {
            ++i;
            ++j;
        } else {
            int start = j;
            // 找到不同的区间
            while (j < m && (i >= n || left[i] != right[j])) {
                ++j;
            }
            if (j > start) {
                diffs.append({start, j});
            }
            if (i < n) ++i;
        }
    }
    
    return diffs;
}

QVector<DiffHunk> DataDiffer::groupIntoHunks(const QVector<LineDiff> &diffs, int contextLines) const
{
    QVector<DiffHunk> hunks;
    
    if (diffs.isEmpty()) return hunks;
    
    DiffHunk currentHunk;
    int unchangedCount = 0;
    bool inHunk = false;
    
    for (int i = 0; i < diffs.size(); ++i) {
        const LineDiff &diff = diffs[i];
        
        if (diff.type != DiffType::Unchanged) {
            // 开始新 hunk 或继续当前 hunk
            if (!inHunk) {
                currentHunk = DiffHunk();
                currentHunk.leftStart = diff.leftLine >= 0 ? diff.leftLine : 0;
                currentHunk.rightStart = diff.rightLine >= 0 ? diff.rightLine : 0;
                
                // 添加前面的上下文
                int contextStart = qMax(0, i - contextLines);
                for (int c = contextStart; c < i; ++c) {
                    currentHunk.lines.append(diffs[c]);
                }
                
                inHunk = true;
            }
            
            currentHunk.lines.append(diff);
            unchangedCount = 0;
            
        } else if (inHunk) {
            currentHunk.lines.append(diff);
            unchangedCount++;
            
            // 如果连续未变化行超过阈值，结束当前 hunk
            if (unchangedCount > contextLines * 2) {
                // 移除多余的未变化行
                while (currentHunk.lines.size() > 0 && 
                       currentHunk.lines.last().type == DiffType::Unchanged &&
                       unchangedCount > contextLines) {
                    currentHunk.lines.removeLast();
                    unchangedCount--;
                }
                
                // 计算行数
                for (const LineDiff &ld : currentHunk.lines) {
                    if (ld.leftLine >= 0) currentHunk.leftCount++;
                    if (ld.rightLine >= 0) currentHunk.rightCount++;
                }
                
                hunks.append(currentHunk);
                inHunk = false;
                unchangedCount = 0;
            }
        }
    }
    
    // 处理最后一个 hunk
    if (inHunk && !currentHunk.lines.isEmpty()) {
        // 移除尾部多余的未变化行
        while (currentHunk.lines.size() > 0 && 
               currentHunk.lines.last().type == DiffType::Unchanged &&
               unchangedCount > contextLines) {
            currentHunk.lines.removeLast();
            unchangedCount--;
        }
        
        for (const LineDiff &ld : currentHunk.lines) {
            if (ld.leftLine >= 0) currentHunk.leftCount++;
            if (ld.rightLine >= 0) currentHunk.rightCount++;
        }
        
        hunks.append(currentHunk);
    }
    
    return hunks;
}

QString DataDiffer::normalizeLine(const QString &line, bool ignoreWhitespace, bool ignoreCase) const
{
    QString result = line;
    
    if (ignoreWhitespace) {
        result = result.simplified();
    }
    
    if (ignoreCase) {
        result = result.toLower();
    }
    
    return result;
}

uint DataDiffer::hashLine(const QString &line) const
{
    return qHash(line);
}
