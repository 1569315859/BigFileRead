/**
 * @file JSONViewer.h
 * @brief JSON/JSONL 文件查看器组件
 * 
 * 支持：
 * - 树形视图展示JSON结构
 * - JSONL每行一个JSON对象的表格视图
 * - JSON路径搜索 (如 $.items[0].name)
 * - 大文件流式解析
 */

#ifndef JSONVIEWER_H
#define JSONVIEWER_H

#include <QObject>
#include <QAbstractItemModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QVariant>
#include <QVector>
#include <QFile>
#include <QFutureWatcher>
#include <QMutex>
#include <memory>

class JSONTreeNode;

/**
 * @class JSONTreeModel
 * @brief JSON树形结构的Qt模型，用于QML TreeView
 */
class JSONTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingStateChanged)
    Q_PROPERTY(int totalNodes READ totalNodes NOTIFY totalNodesChanged)
    Q_PROPERTY(bool isJsonl READ isJsonl NOTIFY isJsonlChanged)
    
public:
    enum Roles {
        KeyRole = Qt::UserRole + 1,
        ValueRole,
        TypeRole,
        TypeNameRole,
        PathRole,
        HasChildrenRole,
        ChildCountRole,
        DepthRole
    };
    Q_ENUM(Roles)
    
    enum JsonValueType {
        NullType = 0,
        BoolType,
        NumberType,
        StringType,
        ArrayType,
        ObjectType
    };
    Q_ENUM(JsonValueType)
    
    explicit JSONTreeModel(QObject *parent = nullptr);
    ~JSONTreeModel() override;
    
    // 文件操作
    Q_INVOKABLE bool loadFile(const QString &filePath);
    Q_INVOKABLE void closeFile();
    Q_INVOKABLE bool loadFromString(const QString &jsonString);
    
    // 搜索
    Q_INVOKABLE QVariantList searchByPath(const QString &jsonPath);
    Q_INVOKABLE QVariantList searchByValue(const QString &value, bool useRegex = false);
    Q_INVOKABLE QVariantList searchByKey(const QString &key);
    
    // 节点操作
    Q_INVOKABLE QVariant getNodeValue(const QModelIndex &index) const;
    Q_INVOKABLE QString getNodePath(const QModelIndex &index) const;
    Q_INVOKABLE QString getNodeType(const QModelIndex &index) const;
    Q_INVOKABLE void expandAll();
    Q_INVOKABLE void collapseAll();
    Q_INVOKABLE void expandToDepth(int depth);
    
    // 格式化
    Q_INVOKABLE QString formatJson(bool compact = false) const;
    Q_INVOKABLE bool exportToFile(const QString &path, bool compact = false) const;
    
    // 属性
    QString filePath() const { return m_filePath; }
    bool isLoading() const { return m_isLoading.load(); }
    int totalNodes() const { return m_totalNodes; }
    bool isJsonl() const { return m_isJsonl; }
    
    // QAbstractItemModel 接口
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
signals:
    void filePathChanged();
    void loadingStateChanged();
    void loadingProgress(int percent);
    void loadingFinished(bool success, const QString &message);
    void totalNodesChanged();
    void isJsonlChanged();
    void searchCompleted(int resultCount);
    
private:
    void parseJsonValue(const QJsonValue &value, JSONTreeNode *parent, const QString &key = QString());
    void parseJsonObject(const QJsonObject &obj, JSONTreeNode *parent);
    void parseJsonArray(const QJsonArray &arr, JSONTreeNode *parent);
    void loadDataAsync();
    void onLoadingFinished();
    bool detectJsonl(const QString &sample);
    void parseJsonl(QFile &file);
    
    JSONTreeNode *nodeFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromNode(JSONTreeNode *node) const;
    
    std::unique_ptr<JSONTreeNode> m_rootNode;
    QString m_filePath;
    QJsonDocument m_document;
    
    std::atomic<bool> m_isLoading{false};
    std::atomic<bool> m_cancelRequested{false};
    QFutureWatcher<void> m_loadWatcher;
    QMutex m_dataMutex;
    
    int m_totalNodes = 0;
    bool m_isJsonl = false;
};


/**
 * @class JSONTreeNode
 * @brief JSON树形结构的节点
 */
class JSONTreeNode
{
public:
    explicit JSONTreeNode(JSONTreeNode *parent = nullptr);
    ~JSONTreeNode();
    
    // 节点属性
    QString key;
    QJsonValue value;
    JSONTreeModel::JsonValueType type = JSONTreeModel::NullType;
    QString path;  // JSON路径，如 $.items[0].name
    
    // 树结构
    JSONTreeNode *parentNode = nullptr;
    QVector<JSONTreeNode*> children;
    
    // 辅助函数
    int row() const;
    int childCount() const { return children.size(); }
    bool hasChildren() const { return !children.isEmpty(); }
    JSONTreeNode *child(int row) const;
    void appendChild(JSONTreeNode *child);
    
    // 值获取
    QString displayKey() const;
    QString displayValue() const;
    QString typeName() const;
};


/**
 * @class JSONLTableModel
 * @brief JSONL文件的表格模型（每行一个JSON对象）
 */
class JSONLTableModel : public QAbstractTableModel
{
    Q_OBJECT
    
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingStateChanged)
    Q_PROPERTY(int totalRowCount READ totalRowCount NOTIFY totalRowCountChanged)
    Q_PROPERTY(int totalColumnCount READ totalColumnCount NOTIFY totalColumnCountChanged)
    
public:
    enum Roles {
        RawValueRole = Qt::UserRole + 1,
        ColumnNameRole,
        ValueTypeRole
    };
    
    explicit JSONLTableModel(QObject *parent = nullptr);
    ~JSONLTableModel() override;
    
    // 文件操作
    Q_INVOKABLE bool loadFile(const QString &filePath);
    Q_INVOKABLE void closeFile();
    
    // 列信息
    Q_INVOKABLE QVariantList getColumns() const;
    Q_INVOKABLE QVariant getCellValue(int row, int column) const;
    Q_INVOKABLE QString getCellJsonPath(int row, int column) const;
    
    // 搜索和过滤
    Q_INVOKABLE QVariantList searchInColumn(int column, const QString &text);
    Q_INVOKABLE void applyFilter(int column, const QString &value);
    Q_INVOKABLE void clearFilters();
    
    // 导出
    Q_INVOKABLE bool exportToCSV(const QString &path) const;
    Q_INVOKABLE bool exportToJSON(const QString &path) const;
    
    // 属性
    QString filePath() const { return m_filePath; }
    bool isLoading() const { return m_isLoading.load(); }
    int totalRowCount() const { return static_cast<int>(m_rows.size()); }
    int totalColumnCount() const { return static_cast<int>(m_columns.size()); }
    
    // QAbstractTableModel 接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    
signals:
    void filePathChanged();
    void loadingStateChanged();
    void loadingProgress(int percent);
    void loadingFinished(bool success, const QString &message);
    void totalRowCountChanged();
    void totalColumnCountChanged();
    
private:
    void loadDataAsync();
    void onLoadingFinished();
    void extractColumns(const QJsonObject &obj, const QString &prefix = QString());
    
    struct ColumnInfo {
        QString name;       // 显示名
        QString jsonPath;   // JSON路径 (如 user.address.city)
        int type = 0;       // 推断的值类型
    };
    
    QString m_filePath;
    std::vector<ColumnInfo> m_columns;
    std::vector<QJsonObject> m_rows;
    std::vector<int> m_filteredIndices;
    bool m_isFiltered = false;
    
    std::atomic<bool> m_isLoading{false};
    std::atomic<bool> m_cancelRequested{false};
    QFutureWatcher<void> m_loadWatcher;
    QMutex m_dataMutex;
};

#endif // JSONVIEWER_H
