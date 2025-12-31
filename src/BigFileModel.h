/**
 * @file BigFileModel.h
 * @brief 大文件模型类 - 使用内存映射实现高性能大文件读取
 * @description 继承 QAbstractListModel，通过 QFile::map() 将文件映射到虚拟内存，
 *              配合行偏移索引实现按需读取，支持打开 10GB+ 文本文件。
 * @note 目标架构：仅支持 64 位系统，假设虚拟内存空间足够映射整个文件
 * @version 1.5 - 增加异步索引、分块更新、长行截断
 */

#ifndef BIGFILEMODEL_H
#define BIGFILEMODEL_H

#include <QAbstractListModel>
#include <QFile>
#include <QString>
#include <QMutex>
#include <QFutureWatcher>
#include <QTimer>
#include <vector>
#include <atomic>

class BigFileModel : public QAbstractListModel
{
    Q_OBJECT

public:
    /// UI 刷新间隔（毫秒）- 每秒 10 次更新
    static constexpr int UI_UPDATE_INTERVAL_MS = 200;
    /// Worker 每批次 flush 的行数
    static constexpr int WORKER_CHUNK_SIZE = 1000;
    /// 长行截断阈值
    static constexpr int MAX_DISPLAY_LENGTH = 2000;

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
     * @brief 获取当前已索引的行数
     */
    int lineCount() const;

    // QAbstractListModel 接口实现
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    
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

private slots:
    /**
     * @brief Timer 超时处理 - 将 pending 数据合并到主模型
     */
    void onUpdateTimerTimeout();

    /**
     * @brief 处理索引完成
     */
    void onIndexingFinished(bool success, const QString &message);

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

private:
    QFile m_file;                           ///< 文件对象
    QString m_filePath;                     ///< 文件路径
    qint64 m_fileSize = 0;                  ///< 文件大小
    uchar *m_mapPtr = nullptr;              ///< 内存映射指针

    std::vector<qint64> m_lineOffsets;      ///< 主数据：每行起始偏移量（仅主线程访问）
    std::vector<qint64> m_pendingOffsets;   ///< 待合并数据：后台线程写入
    mutable QMutex m_pendingMutex;          ///< 仅保护 m_pendingOffsets

    std::atomic<bool> m_isIndexing{false};  ///< 是否正在索引
    std::atomic<bool> m_cancelRequested{false}; ///< 是否请求取消
    std::atomic<int> m_lastReportedPercent{0}; ///< 上次报告的进度百分比

    QTimer *m_refreshTimer = nullptr;       ///< Pull Timer（每 50ms 触发）
    QFutureWatcher<void> m_futureWatcher;   ///< 异步任务监视器
};

#endif // BIGFILEMODEL_H
