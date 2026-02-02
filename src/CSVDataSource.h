/**
 * @file CSVDataSource.h
 * @brief CSV/TSV 专用数据源 - 独立的表格视图模式
 * @description 提供类似 Excel 的表格体验，支持大文件内存映射加载、
 *              自动检测分隔符、列类型推断、列排序、冻结行列等功能。
 * @note 区别于 LogParser 的日志解析模式，这是纯数据表格视图
 */

#ifndef CSVDATASOURCE_H
#define CSVDATASOURCE_H

#include <QAbstractTableModel>
#include <QFile>
#include <QMutex>
#include <QFutureWatcher>
#include <QVariant>
#include <vector>
#include <atomic>

/**
 * @brief 列元数据信息
 */
struct ColumnMeta {
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(int type MEMBER type)
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(bool frozen MEMBER frozen)
public:
    QString name;           ///< 列名
    int type = 0;           ///< 列类型: 0=Text, 1=Number, 2=DateTime, 3=Boolean
    int width = 100;        ///< 列宽度（像素）
    bool frozen = false;    ///< 是否冻结
    
    // 统计信息
    qint64 nullCount = 0;   ///< 空值数量
    qint64 uniqueCount = 0; ///< 唯一值数量
    double minValue = 0;    ///< 数值列最小值
    double maxValue = 0;    ///< 数值列最大值
    
    ColumnMeta() = default;
    ColumnMeta(const QString &n) : name(n) {}
};

/**
 * @brief CSV数据源类 - 专用于CSV/TSV文件的表格模型
 */
class CSVDataSource : public QAbstractTableModel
{
    Q_OBJECT
    
    // QML 属性
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(qint64 fileSize READ fileSize NOTIFY fileSizeChanged)
    Q_PROPERTY(int totalRowCount READ totalRowCount NOTIFY totalRowCountChanged)
    Q_PROPERTY(int totalColumnCount READ totalColumnCount NOTIFY totalColumnCountChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingStateChanged)
    Q_PROPERTY(QChar delimiter READ delimiter WRITE setDelimiter NOTIFY delimiterChanged)
    Q_PROPERTY(bool hasHeader READ hasHeader WRITE setHasHeader NOTIFY hasHeaderChanged)
    Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder NOTIFY sortChanged)
    Q_PROPERTY(int frozenRowCount READ frozenRowCount WRITE setFrozenRowCount NOTIFY frozenRowsChanged)
    Q_PROPERTY(int frozenColumnCount READ frozenColumnCount WRITE setFrozenColumnCount NOTIFY frozenColumnsChanged)

public:
    /// 列数据类型
    enum ColumnType {
        TextType = 0,
        NumberType = 1,
        DateTimeType = 2,
        BooleanType = 3
    };
    Q_ENUM(ColumnType)
    
    /// 自定义数据角色
    enum DataRole {
        RawValueRole = Qt::UserRole + 1,    ///< 原始值（未格式化）
        ColumnTypeRole,                      ///< 列类型
        ColumnIndexRole,                     ///< 列索引
        RowIndexRole,                        ///< 原始行索引（排序前）
        IsFrozenRole                         ///< 是否冻结行/列
    };
    Q_ENUM(DataRole)
    
    explicit CSVDataSource(QObject *parent = nullptr);
    ~CSVDataSource() override;
    
    // ============ 文件操作 ============
    
    /**
     * @brief 加载CSV文件
     * @param filePath 文件路径
     * @return 成功返回 true
     */
    Q_INVOKABLE bool loadFile(const QString &filePath);
    
    /**
     * @brief 关闭文件并释放资源
     */
    Q_INVOKABLE void closeFile();
    
    /**
     * @brief 取消加载
     */
    Q_INVOKABLE void cancelLoading();
    
    // ============ 属性访问器 ============
    
    QString filePath() const { return m_filePath; }
    qint64 fileSize() const { return m_fileSize; }
    int totalRowCount() const { return static_cast<int>(m_data.size()); }
    int totalColumnCount() const { return static_cast<int>(m_columns.size()); }
    bool isLoading() const { return m_isLoading.load(); }
    
    QChar delimiter() const { return m_delimiter; }
    void setDelimiter(QChar d);
    
    bool hasHeader() const { return m_hasHeader; }
    void setHasHeader(bool h);
    
    QString sortColumn() const { return m_sortColumn; }
    Qt::SortOrder sortOrder() const { return m_sortOrder; }
    
    int frozenRowCount() const { return m_frozenRowCount; }
    void setFrozenRowCount(int count);
    
    int frozenColumnCount() const { return m_frozenColumnCount; }
    void setFrozenColumnCount(int count);
    
    // ============ 列操作 ============
    
    /**
     * @brief 获取列元数据
     */
    Q_INVOKABLE QVariantList getColumns() const;
    
    /**
     * @brief 获取指定列的元数据
     */
    Q_INVOKABLE QVariantMap getColumnMeta(int columnIndex) const;
    
    /**
     * @brief 设置列宽
     */
    Q_INVOKABLE void setColumnWidth(int columnIndex, int width);
    
    /**
     * @brief 自动调整列宽
     */
    Q_INVOKABLE void autoFitColumnWidth(int columnIndex);
    
    /**
     * @brief 冻结/解冻列
     */
    Q_INVOKABLE void setColumnFrozen(int columnIndex, bool frozen);
    
    /**
     * @brief 隐藏/显示列
     */
    Q_INVOKABLE void setColumnVisible(int columnIndex, bool visible);
    
    // ============ 排序 ============
    
    /**
     * @brief 按列排序
     * @param column 列索引
     * @param order 排序顺序
     */
    Q_INVOKABLE void sortByColumn(int column, Qt::SortOrder order = Qt::AscendingOrder);
    
    /**
     * @brief 清除排序
     */
    Q_INVOKABLE void clearSort();
    
    // ============ 搜索和过滤 ============
    
    /**
     * @brief 在指定列中搜索
     * @param column 列索引（-1表示所有列）
     * @param text 搜索文本
     * @param useRegex 是否使用正则
     * @return 匹配的行索引列表
     */
    Q_INVOKABLE QVariantList searchInColumn(int column, const QString &text, bool useRegex = false);
    
    /**
     * @brief 应用列过滤
     */
    Q_INVOKABLE void applyColumnFilter(int column, const QString &filterValue, const QString &operatorType = "contains");
    
    /**
     * @brief 清除所有过滤
     */
    Q_INVOKABLE void clearAllFilters();
    
    // ============ 导出 ============
    
    /**
     * @brief 导出为CSV
     */
    Q_INVOKABLE bool exportToCSV(const QString &path, bool includeHeader = true);
    
    /**
     * @brief 导出选中行
     */
    Q_INVOKABLE bool exportSelectedRows(const QString &path, const QVariantList &rowIndices);
    
    // ============ 统计 ============
    
    /**
     * @brief 获取列统计信息
     */
    Q_INVOKABLE QVariantMap getColumnStatistics(int columnIndex) const;
    
    /**
     * @brief 获取唯一值列表
     */
    Q_INVOKABLE QVariantList getDistinctValues(int columnIndex, int maxCount = 100) const;
    
    // ============ 单元格操作 ============
    
    /**
     * @brief 获取单元格值
     */
    Q_INVOKABLE QVariant getCellValue(int row, int column) const;
    
    /**
     * @brief 获取原始行文本
     */
    Q_INVOKABLE QString getRawRowText(int row) const;
    
    // ============ QAbstractTableModel 接口 ============
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

signals:
    void filePathChanged();
    void fileSizeChanged();
    void totalRowCountChanged();
    void totalColumnCountChanged();
    void loadingStateChanged();
    void delimiterChanged();
    void hasHeaderChanged();
    void sortChanged();
    void frozenRowsChanged();
    void frozenColumnsChanged();
    
    void loadingProgress(int percent);
    void loadingFinished(bool success, const QString &message);
    void loadingCancelled();
    void columnStatsReady(int columnIndex);

private slots:
    void onLoadingFinished();

private:
    /**
     * @brief 自动检测分隔符
     */
    QChar detectDelimiter(const QString &sample);
    
    /**
     * @brief 自动检测列类型
     */
    ColumnType detectColumnType(const QStringList &samples);
    
    /**
     * @brief 解析CSV行（处理引号内的分隔符）
     */
    QStringList parseCSVLine(const QString &line) const;
    
    /**
     * @brief 异步加载数据
     */
    void loadDataAsync();
    
    /**
     * @brief 更新列统计信息
     */
    void updateColumnStatistics(int columnIndex);
    
private:
    // 文件信息
    QString m_filePath;
    qint64 m_fileSize = 0;
    QFile m_file;
    uchar *m_mapPtr = nullptr;
    
    // CSV 配置
    QChar m_delimiter = ',';
    QChar m_quoteChar = '"';
    QChar m_escapeChar = '\\';
    bool m_hasHeader = true;
    
    // 数据存储
    std::vector<QStringList> m_data;         ///< 行数据（每行是字段列表）
    std::vector<ColumnMeta> m_columns;       ///< 列元数据
    std::vector<int> m_sortedIndices;        ///< 排序后的行索引映射
    std::vector<int> m_filteredIndices;      ///< 过滤后的行索引
    std::vector<bool> m_columnVisible;       ///< 列可见性
    
    // 排序状态
    QString m_sortColumn;
    int m_sortColumnIndex = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    
    // 冻结状态
    int m_frozenRowCount = 0;
    int m_frozenColumnCount = 0;
    
    // 过滤状态
    bool m_isFiltered = false;
    
    // 加载状态
    std::atomic<bool> m_isLoading{false};
    std::atomic<bool> m_cancelRequested{false};
    QFutureWatcher<void> m_loadWatcher;
    mutable QMutex m_dataMutex;
};

#endif // CSVDATASOURCE_H
