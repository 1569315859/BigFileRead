/**
 * @file DataDiffer.h
 * @brief 数据对比器 - 支持文件/数据集的差异对比
 * @description 对比两个文件或数据集，识别新增、删除、修改的行
 */

#ifndef DATADIFFER_H
#define DATADIFFER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>
#include <QVector>
#include <QFuture>
#include <QFutureWatcher>
#include <memory>

/**
 * @brief 行差异类型
 */
enum class DiffType {
    Unchanged,  ///< 未变化
    Added,      ///< 新增（仅在右侧）
    Deleted,    ///< 删除（仅在左侧）
    Modified    ///< 修改（两侧都有但内容不同）
};

/**
 * @brief 单行差异信息
 */
struct LineDiff {
    int leftLine = -1;      ///< 左侧行号（-1 表示不存在）
    int rightLine = -1;     ///< 右侧行号（-1 表示不存在）
    DiffType type;          ///< 差异类型
    QString leftContent;    ///< 左侧内容
    QString rightContent;   ///< 右侧内容
    QList<QPair<int, int>> charDiffs;  ///< 字符级差异范围（修改行内的变化）
};

/**
 * @brief 差异统计
 */
struct DiffStats {
    int totalLines = 0;     ///< 总行数
    int unchangedLines = 0; ///< 未变化行数
    int addedLines = 0;     ///< 新增行数
    int deletedLines = 0;   ///< 删除行数
    int modifiedLines = 0;  ///< 修改行数
    double similarity = 0;  ///< 相似度 (0-1)
};

/**
 * @brief 差异块（连续的差异区域）
 */
struct DiffHunk {
    int leftStart = 0;      ///< 左侧起始行
    int leftCount = 0;      ///< 左侧行数
    int rightStart = 0;     ///< 右侧起始行
    int rightCount = 0;     ///< 右侧行数
    QVector<LineDiff> lines;  ///< 块内的行差异
};

/**
 * @class DataDiffer
 * @brief 数据差异对比器
 * 
 * 功能：
 * - 对比两个文本文件
 * - 对比两个 CSV 数据集
 * - 基于主键的记录匹配
 * - 字符级差异高亮
 * - 异步处理大文件
 */
class DataDiffer : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isComparing READ isComparing NOTIFY comparingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

public:
    /**
     * @brief 单例访问器
     */
    static DataDiffer& instance();
    
    // 禁用拷贝
    DataDiffer(const DataDiffer&) = delete;
    DataDiffer& operator=(const DataDiffer&) = delete;
    
    // ===================== 文件对比 =====================
    
    /**
     * @brief 对比两个文件
     * @param leftPath 左侧文件路径
     * @param rightPath 右侧文件路径
     * @param ignoreWhitespace 忽略空白差异
     * @param ignoreCase 忽略大小写
     */
    Q_INVOKABLE void compareFiles(const QString &leftPath, const QString &rightPath,
                                   bool ignoreWhitespace = false, bool ignoreCase = false);
    
    /**
     * @brief 对比两个文本内容
     * @param leftContent 左侧文本
     * @param rightContent 右侧文本
     * @param ignoreWhitespace 忽略空白差异
     * @param ignoreCase 忽略大小写
     */
    Q_INVOKABLE void compareText(const QString &leftContent, const QString &rightContent,
                                  bool ignoreWhitespace = false, bool ignoreCase = false);
    
    // ===================== CSV 对比 =====================
    
    /**
     * @brief 对比两个 CSV 文件
     * @param leftPath 左侧 CSV 路径
     * @param rightPath 右侧 CSV 路径
     * @param keyColumns 主键列索引列表（用于匹配行）
     * @param ignoreColumns 忽略的列索引列表
     */
    Q_INVOKABLE void compareCSV(const QString &leftPath, const QString &rightPath,
                                 const QList<int> &keyColumns = QList<int>(),
                                 const QList<int> &ignoreColumns = QList<int>());
    
    // ===================== 结果访问 =====================
    
    /**
     * @brief 获取差异统计
     */
    Q_INVOKABLE QVariantMap getStats() const;
    
    /**
     * @brief 获取差异块列表
     * @return [{leftStart, leftCount, rightStart, rightCount, lines: [...]}, ...]
     */
    Q_INVOKABLE QVariantList getHunks() const;
    
    /**
     * @brief 获取所有差异行
     * @param includeUnchanged 是否包含未变化的行
     * @return [{leftLine, rightLine, type, leftContent, rightContent}, ...]
     */
    Q_INVOKABLE QVariantList getAllDiffs(bool includeUnchanged = false) const;
    
    /**
     * @brief 获取指定范围的差异
     * @param startLine 起始行（左侧行号）
     * @param endLine 结束行
     */
    Q_INVOKABLE QVariantList getDiffsInRange(int startLine, int endLine) const;
    
    /**
     * @brief 获取下一个差异位置
     * @param currentLine 当前行号
     * @param direction 方向（1=向下，-1=向上）
     * @return 下一个差异的行号，-1 表示没有更多
     */
    Q_INVOKABLE int getNextDiff(int currentLine, int direction = 1) const;
    
    /**
     * @brief 获取差异总数
     */
    Q_INVOKABLE int getDiffCount() const;
    
    // ===================== 状态 =====================
    
    Q_INVOKABLE bool isComparing() const { return m_isComparing; }
    Q_INVOKABLE int progress() const { return m_progress; }
    
    /**
     * @brief 取消当前对比
     */
    Q_INVOKABLE void cancel();
    
    // ===================== 导出 =====================
    
    /**
     * @brief 导出差异报告
     * @param path 输出路径
     * @param format 格式（"html", "text", "patch"）
     */
    Q_INVOKABLE void exportDiff(const QString &path, const QString &format = "html");

signals:
    /**
     * @brief 对比完成
     * @param stats 统计信息
     */
    void compareCompleted(const QVariantMap &stats);
    
    /**
     * @brief 对比错误
     * @param error 错误信息
     */
    void compareError(const QString &error);
    
    void comparingChanged();
    void progressChanged();

private:
    DataDiffer(QObject *parent = nullptr);
    ~DataDiffer();
    
    /**
     * @brief Myers 差异算法实现
     */
    QVector<LineDiff> computeDiff(const QStringList &left, const QStringList &right,
                                   bool ignoreWhitespace, bool ignoreCase) const;
    
    /**
     * @brief LCS（最长公共子序列）算法
     */
    QVector<QPair<int, int>> computeLCS(const QStringList &left, const QStringList &right) const;
    
    /**
     * @brief 计算字符级差异
     */
    QList<QPair<int, int>> computeCharDiff(const QString &left, const QString &right) const;
    
    /**
     * @brief 将差异分组为块
     */
    QVector<DiffHunk> groupIntoHunks(const QVector<LineDiff> &diffs, int contextLines = 3) const;
    
    /**
     * @brief 规范化行（用于忽略空白/大小写）
     */
    QString normalizeLine(const QString &line, bool ignoreWhitespace, bool ignoreCase) const;
    
    /**
     * @brief 计算哈希
     */
    uint hashLine(const QString &line) const;
    
    // 结果存储
    QVector<LineDiff> m_diffs;
    QVector<DiffHunk> m_hunks;
    DiffStats m_stats;
    
    // 源数据
    QStringList m_leftLines;
    QStringList m_rightLines;
    QString m_leftPath;
    QString m_rightPath;
    
    // 状态
    bool m_isComparing = false;
    int m_progress = 0;
    bool m_cancelled = false;
    
    // 异步处理
    QFutureWatcher<void> *m_watcher = nullptr;
};

#endif // DATADIFFER_H
