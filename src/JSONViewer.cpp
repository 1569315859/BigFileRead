/**
 * @file JSONViewer.cpp
 * @brief JSON/JSONL 文件查看器实现
 */

#include "JSONViewer.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QtConcurrent>
#include <QDebug>

// ============================================================================
// JSONTreeNode 实现
// ============================================================================

JSONTreeNode::JSONTreeNode(JSONTreeNode *parent)
    : parentNode(parent)
{
}

JSONTreeNode::~JSONTreeNode()
{
    qDeleteAll(children);
}

int JSONTreeNode::row() const
{
    if (parentNode) {
        return parentNode->children.indexOf(const_cast<JSONTreeNode*>(this));
    }
    return 0;
}

JSONTreeNode *JSONTreeNode::child(int row) const
{
    if (row >= 0 && row < children.size()) {
        return children[row];
    }
    return nullptr;
}

void JSONTreeNode::appendChild(JSONTreeNode *child)
{
    child->parentNode = this;
    children.append(child);
}

QString JSONTreeNode::displayKey() const
{
    if (key.isEmpty() && parentNode && parentNode->type == JSONTreeModel::ArrayType) {
        return QString("[%1]").arg(row());
    }
    return key;
}

QString JSONTreeNode::displayValue() const
{
    switch (type) {
    case JSONTreeModel::NullType:
        return "null";
    case JSONTreeModel::BoolType:
        return value.toBool() ? "true" : "false";
    case JSONTreeModel::NumberType:
        return QString::number(value.toDouble(), 'g', 15);
    case JSONTreeModel::StringType:
        return QString("\"%1\"").arg(value.toString());
    case JSONTreeModel::ArrayType:
        return QString("[%1 items]").arg(children.size());
    case JSONTreeModel::ObjectType:
        return QString("{%1 keys}").arg(children.size());
    default:
        return QString();
    }
}

QString JSONTreeNode::typeName() const
{
    switch (type) {
    case JSONTreeModel::NullType: return "null";
    case JSONTreeModel::BoolType: return "boolean";
    case JSONTreeModel::NumberType: return "number";
    case JSONTreeModel::StringType: return "string";
    case JSONTreeModel::ArrayType: return "array";
    case JSONTreeModel::ObjectType: return "object";
    default: return "unknown";
    }
}

// ============================================================================
// JSONTreeModel 实现
// ============================================================================

JSONTreeModel::JSONTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_rootNode(std::make_unique<JSONTreeNode>())
{
    connect(&m_loadWatcher, &QFutureWatcher<void>::finished,
            this, &JSONTreeModel::onLoadingFinished);
}

JSONTreeModel::~JSONTreeModel()
{
    if (m_isLoading.load()) {
        m_cancelRequested.store(true);
        m_loadWatcher.waitForFinished();
    }
}

bool JSONTreeModel::loadFile(const QString &filePath)
{
    if (m_isLoading.load()) {
        return false;
    }
    
    closeFile();
    
    m_filePath = filePath;
    emit filePathChanged();
    
    m_isLoading.store(true);
    m_cancelRequested.store(false);
    emit loadingStateChanged();
    
    auto future = QtConcurrent::run([this]() { loadDataAsync(); });
    m_loadWatcher.setFuture(future);
    
    return true;
}

void JSONTreeModel::closeFile()
{
    if (m_isLoading.load()) {
        m_cancelRequested.store(true);
        m_loadWatcher.waitForFinished();
    }
    
    beginResetModel();
    m_rootNode = std::make_unique<JSONTreeNode>();
    m_document = QJsonDocument();
    m_totalNodes = 0;
    m_isJsonl = false;
    endResetModel();
    
    m_filePath.clear();
    emit filePathChanged();
    emit totalNodesChanged();
    emit isJsonlChanged();
}

bool JSONTreeModel::loadFromString(const QString &jsonString)
{
    closeFile();
    
    QJsonParseError error;
    m_document = QJsonDocument::fromJson(jsonString.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit loadingFinished(false, tr("JSON parse error: %1 at offset %2")
                           .arg(error.errorString()).arg(error.offset));
        return false;
    }
    
    beginResetModel();
    m_rootNode = std::make_unique<JSONTreeNode>();
    m_rootNode->path = "$";
    m_totalNodes = 0;
    
    if (m_document.isObject()) {
        m_rootNode->type = ObjectType;
        parseJsonObject(m_document.object(), m_rootNode.get());
    } else if (m_document.isArray()) {
        m_rootNode->type = ArrayType;
        parseJsonArray(m_document.array(), m_rootNode.get());
    }
    
    endResetModel();
    
    emit totalNodesChanged();
    emit loadingFinished(true, tr("Loaded %1 nodes").arg(m_totalNodes));
    return true;
}

QVariantList JSONTreeModel::searchByPath(const QString &jsonPath)
{
    QVariantList results;
    
    // 简单的JSON路径解析
    // 支持: $.key, $.key.subkey, $.array[0], $.array[*].field
    QString path = jsonPath;
    if (path.startsWith("$.")) {
        path = path.mid(2);
    } else if (path.startsWith("$")) {
        path = path.mid(1);
    }
    
    QStringList parts;
    QRegularExpression tokenRegex(R"((\w+)|\[(\d+|\*)\])");
    auto matches = tokenRegex.globalMatch(path);
    while (matches.hasNext()) {
        auto match = matches.next();
        if (!match.captured(1).isEmpty()) {
            parts.append(match.captured(1));
        } else if (!match.captured(2).isEmpty()) {
            parts.append("[" + match.captured(2) + "]");
        }
    }
    
    std::function<void(JSONTreeNode*, int)> searchRecursive;
    searchRecursive = [&](JSONTreeNode *node, int partIndex) {
        if (partIndex >= parts.size()) {
            // 找到匹配
            QVariantMap item;
            item["path"] = node->path;
            item["value"] = node->displayValue();
            item["type"] = node->typeName();
            results.append(item);
            return;
        }
        
        QString part = parts[partIndex];
        
        if (part.startsWith("[")) {
            // 数组索引
            QString indexStr = part.mid(1, part.length() - 2);
            if (indexStr == "*") {
                // 通配符，搜索所有子节点
                for (auto *child : node->children) {
                    searchRecursive(child, partIndex + 1);
                }
            } else {
                int index = indexStr.toInt();
                if (index >= 0 && index < node->children.size()) {
                    searchRecursive(node->children[index], partIndex + 1);
                }
            }
        } else {
            // 对象键名
            for (auto *child : node->children) {
                if (child->key == part) {
                    searchRecursive(child, partIndex + 1);
                    break;
                }
            }
        }
    };
    
    searchRecursive(m_rootNode.get(), 0);
    emit searchCompleted(results.size());
    return results;
}

QVariantList JSONTreeModel::searchByValue(const QString &value, bool useRegex)
{
    QVariantList results;
    
    QRegularExpression regex;
    if (useRegex) {
        regex.setPattern(value);
        regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid()) {
            return results;
        }
    }
    
    std::function<void(JSONTreeNode*)> searchRecursive;
    searchRecursive = [&](JSONTreeNode *node) {
        if (node->type == StringType || node->type == NumberType) {
            QString nodeValue = node->value.toVariant().toString();
            bool match = useRegex 
                ? regex.match(nodeValue).hasMatch()
                : nodeValue.contains(value, Qt::CaseInsensitive);
            
            if (match) {
                QVariantMap item;
                item["path"] = node->path;
                item["value"] = node->displayValue();
                item["type"] = node->typeName();
                item["key"] = node->key;
                results.append(item);
            }
        }
        
        for (auto *child : node->children) {
            searchRecursive(child);
        }
    };
    
    searchRecursive(m_rootNode.get());
    emit searchCompleted(results.size());
    return results;
}

QVariantList JSONTreeModel::searchByKey(const QString &key)
{
    QVariantList results;
    
    std::function<void(JSONTreeNode*)> searchRecursive;
    searchRecursive = [&](JSONTreeNode *node) {
        if (node->key.contains(key, Qt::CaseInsensitive)) {
            QVariantMap item;
            item["path"] = node->path;
            item["value"] = node->displayValue();
            item["type"] = node->typeName();
            item["key"] = node->key;
            results.append(item);
        }
        
        for (auto *child : node->children) {
            searchRecursive(child);
        }
    };
    
    searchRecursive(m_rootNode.get());
    emit searchCompleted(results.size());
    return results;
}

QVariant JSONTreeModel::getNodeValue(const QModelIndex &index) const
{
    if (!index.isValid()) return QVariant();
    
    JSONTreeNode *node = nodeFromIndex(index);
    if (!node) return QVariant();
    
    return node->value.toVariant();
}

QString JSONTreeModel::getNodePath(const QModelIndex &index) const
{
    if (!index.isValid()) return QString();
    
    JSONTreeNode *node = nodeFromIndex(index);
    return node ? node->path : QString();
}

QString JSONTreeModel::getNodeType(const QModelIndex &index) const
{
    if (!index.isValid()) return QString();
    
    JSONTreeNode *node = nodeFromIndex(index);
    return node ? node->typeName() : QString();
}

void JSONTreeModel::expandAll()
{
    // 通过信号通知QML视图展开所有节点
    // 这个功能需要在QML端实现
}

void JSONTreeModel::collapseAll()
{
    // 通过信号通知QML视图折叠所有节点
}

void JSONTreeModel::expandToDepth(int depth)
{
    Q_UNUSED(depth)
    // 展开到指定深度
}

QString JSONTreeModel::formatJson(bool compact) const
{
    if (m_document.isNull()) return QString();
    
    return QString::fromUtf8(m_document.toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented));
}

bool JSONTreeModel::exportToFile(const QString &path, bool compact) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    file.write(m_document.toJson(compact ? QJsonDocument::Compact : QJsonDocument::Indented));
    file.close();
    return true;
}

QModelIndex JSONTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return QModelIndex();
    }
    
    JSONTreeNode *parentNode = parent.isValid() 
        ? nodeFromIndex(parent) 
        : m_rootNode.get();
    
    if (parentNode && row < parentNode->children.size()) {
        return createIndex(row, column, parentNode->children[row]);
    }
    
    return QModelIndex();
}

QModelIndex JSONTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid()) {
        return QModelIndex();
    }
    
    JSONTreeNode *childNode = nodeFromIndex(child);
    if (!childNode || childNode == m_rootNode.get()) {
        return QModelIndex();
    }
    
    JSONTreeNode *parentNode = childNode->parentNode;
    if (!parentNode || parentNode == m_rootNode.get()) {
        return QModelIndex();
    }
    
    return createIndex(parentNode->row(), 0, parentNode);
}

int JSONTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) {
        return 0;
    }
    
    JSONTreeNode *parentNode = parent.isValid() 
        ? nodeFromIndex(parent) 
        : m_rootNode.get();
    
    return parentNode ? parentNode->children.size() : 0;
}

int JSONTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;  // 单列树形视图
}

QVariant JSONTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }
    
    JSONTreeNode *node = nodeFromIndex(index);
    if (!node) {
        return QVariant();
    }
    
    switch (role) {
    case Qt::DisplayRole:
        if (node->key.isEmpty()) {
            return node->displayValue();
        }
        return QString("%1: %2").arg(node->displayKey(), node->displayValue());
        
    case KeyRole:
        return node->displayKey();
        
    case ValueRole:
        return node->displayValue();
        
    case TypeRole:
        return static_cast<int>(node->type);
        
    case TypeNameRole:
        return node->typeName();
        
    case PathRole:
        return node->path;
        
    case HasChildrenRole:
        return node->hasChildren();
        
    case ChildCountRole:
        return node->childCount();
        
    case DepthRole: {
        int depth = 0;
        JSONTreeNode *p = node->parentNode;
        while (p && p != m_rootNode.get()) {
            depth++;
            p = p->parentNode;
        }
        return depth;
    }
        
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> JSONTreeModel::roleNames() const
{
    auto roles = QAbstractItemModel::roleNames();
    roles[KeyRole] = "key";
    roles[ValueRole] = "value";
    roles[TypeRole] = "type";
    roles[TypeNameRole] = "typeName";
    roles[PathRole] = "path";
    roles[HasChildrenRole] = "hasChildren";
    roles[ChildCountRole] = "childCount";
    roles[DepthRole] = "depth";
    return roles;
}

void JSONTreeModel::parseJsonValue(const QJsonValue &value, JSONTreeNode *parent, const QString &key)
{
    if (m_cancelRequested.load()) return;
    
    auto *node = new JSONTreeNode(parent);
    node->key = key;
    node->value = value;
    
    // 构建路径
    if (parent->path.isEmpty()) {
        node->path = "$";
    } else if (key.isEmpty()) {
        // 数组元素
        node->path = QString("%1[%2]").arg(parent->path).arg(parent->children.size());
    } else {
        node->path = QString("%1.%2").arg(parent->path, key);
    }
    
    m_totalNodes++;
    
    switch (value.type()) {
    case QJsonValue::Null:
        node->type = NullType;
        break;
    case QJsonValue::Bool:
        node->type = BoolType;
        break;
    case QJsonValue::Double:
        node->type = NumberType;
        break;
    case QJsonValue::String:
        node->type = StringType;
        break;
    case QJsonValue::Array:
        node->type = ArrayType;
        parseJsonArray(value.toArray(), node);
        break;
    case QJsonValue::Object:
        node->type = ObjectType;
        parseJsonObject(value.toObject(), node);
        break;
    default:
        break;
    }
    
    parent->appendChild(node);
}

void JSONTreeModel::parseJsonObject(const QJsonObject &obj, JSONTreeNode *parent)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (m_cancelRequested.load()) return;
        parseJsonValue(it.value(), parent, it.key());
    }
}

void JSONTreeModel::parseJsonArray(const QJsonArray &arr, JSONTreeNode *parent)
{
    for (int i = 0; i < arr.size(); ++i) {
        if (m_cancelRequested.load()) return;
        parseJsonValue(arr[i], parent, QString());
    }
}

void JSONTreeModel::loadDataAsync()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    qint64 fileSize = file.size();
    
    // 读取样本检测是否为JSONL
    QByteArray sample = file.read(qMin(fileSize, qint64(4096)));
    file.seek(0);
    
    m_isJsonl = detectJsonl(QString::fromUtf8(sample));
    
    if (m_isJsonl) {
        parseJsonl(file);
    } else {
        // 标准JSON
        QByteArray data = file.readAll();
        emit loadingProgress(50);
        
        if (m_cancelRequested.load()) {
            file.close();
            return;
        }
        
        QJsonParseError error;
        m_document = QJsonDocument::fromJson(data, &error);
        
        if (error.error != QJsonParseError::NoError) {
            qWarning() << "JSON parse error:" << error.errorString() << "at" << error.offset;
            file.close();
            return;
        }
        
        emit loadingProgress(75);
        
        QMutexLocker lock(&m_dataMutex);
        m_rootNode = std::make_unique<JSONTreeNode>();
        m_rootNode->path = "$";
        m_totalNodes = 0;
        
        if (m_document.isObject()) {
            m_rootNode->type = ObjectType;
            parseJsonObject(m_document.object(), m_rootNode.get());
        } else if (m_document.isArray()) {
            m_rootNode->type = ArrayType;
            parseJsonArray(m_document.array(), m_rootNode.get());
        }
    }
    
    file.close();
    emit loadingProgress(100);
}

void JSONTreeModel::onLoadingFinished()
{
    m_isLoading.store(false);
    
    // 关键：在主线程中通知视图模型数据已更新
    beginResetModel();
    endResetModel();
    
    emit loadingStateChanged();
    emit totalNodesChanged();
    emit isJsonlChanged();
    
    if (m_cancelRequested.load()) {
        emit loadingFinished(false, tr("Loading cancelled"));
    } else {
        emit loadingFinished(true, tr("Loaded %1 nodes").arg(m_totalNodes));
    }
}

bool JSONTreeModel::detectJsonl(const QString &sample)
{
    // JSONL: 每行是一个独立的JSON对象
    QStringList lines = sample.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) {
        return false;
    }
    
    // 检查前几行是否都以 { 开头，以 } 结尾
    int jsonObjectCount = 0;
    for (int i = 0; i < qMin(5, lines.size()); ++i) {
        QString line = lines[i].trimmed();
        if (line.startsWith('{') && line.endsWith('}')) {
            jsonObjectCount++;
        }
    }
    
    return jsonObjectCount >= 2;
}

void JSONTreeModel::parseJsonl(QFile &file)
{
    QMutexLocker lock(&m_dataMutex);
    m_rootNode = std::make_unique<JSONTreeNode>();
    m_rootNode->path = "$";
    m_rootNode->type = ArrayType;
    m_totalNodes = 0;
    
    qint64 fileSize = file.size();
    qint64 bytesRead = 0;
    int lastPercent = 0;
    int lineIndex = 0;
    
    QTextStream stream(&file);
    while (!stream.atEnd() && !m_cancelRequested.load()) {
        QString line = stream.readLine();
        bytesRead += line.toUtf8().size() + 1;
        
        line = line.trimmed();
        if (line.isEmpty()) continue;
        
        QJsonParseError error;
        QJsonDocument lineDoc = QJsonDocument::fromJson(line.toUtf8(), &error);
        
        if (error.error == QJsonParseError::NoError && lineDoc.isObject()) {
            auto *node = new JSONTreeNode(m_rootNode.get());
            node->type = ObjectType;
            node->path = QString("$[%1]").arg(lineIndex);
            node->value = QJsonValue(lineDoc.object());
            
            parseJsonObject(lineDoc.object(), node);
            m_rootNode->appendChild(node);
            m_totalNodes++;
            lineIndex++;
        }
        
        int percent = static_cast<int>(bytesRead * 100 / fileSize);
        if (percent > lastPercent) {
            lastPercent = percent;
            emit loadingProgress(percent);
        }
    }
}

JSONTreeNode *JSONTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return m_rootNode.get();
    }
    return static_cast<JSONTreeNode*>(index.internalPointer());
}

QModelIndex JSONTreeModel::indexFromNode(JSONTreeNode *node) const
{
    if (!node || node == m_rootNode.get()) {
        return QModelIndex();
    }
    return createIndex(node->row(), 0, node);
}


// ============================================================================
// JSONLTableModel 实现
// ============================================================================

JSONLTableModel::JSONLTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(&m_loadWatcher, &QFutureWatcher<void>::finished,
            this, &JSONLTableModel::onLoadingFinished);
}

JSONLTableModel::~JSONLTableModel()
{
    if (m_isLoading.load()) {
        m_cancelRequested.store(true);
        m_loadWatcher.waitForFinished();
    }
}

bool JSONLTableModel::loadFile(const QString &filePath)
{
    if (m_isLoading.load()) {
        return false;
    }
    
    closeFile();
    
    m_filePath = filePath;
    emit filePathChanged();
    
    m_isLoading.store(true);
    m_cancelRequested.store(false);
    emit loadingStateChanged();
    
    auto future = QtConcurrent::run([this]() { loadDataAsync(); });
    m_loadWatcher.setFuture(future);
    
    return true;
}

void JSONLTableModel::closeFile()
{
    if (m_isLoading.load()) {
        m_cancelRequested.store(true);
        m_loadWatcher.waitForFinished();
    }
    
    beginResetModel();
    m_rows.clear();
    m_columns.clear();
    m_filteredIndices.clear();
    m_isFiltered = false;
    endResetModel();
    
    m_filePath.clear();
    emit filePathChanged();
    emit totalRowCountChanged();
    emit totalColumnCountChanged();
}

QVariantList JSONLTableModel::getColumns() const
{
    QVariantList result;
    for (size_t i = 0; i < m_columns.size(); ++i) {
        QVariantMap col;
        col["index"] = static_cast<int>(i);
        col["name"] = m_columns[i].name;
        col["path"] = m_columns[i].jsonPath;
        col["type"] = m_columns[i].type;
        result.append(col);
    }
    return result;
}

QVariant JSONLTableModel::getCellValue(int row, int column) const
{
    if (row < 0 || row >= static_cast<int>(m_rows.size())) {
        return QVariant();
    }
    if (column < 0 || column >= static_cast<int>(m_columns.size())) {
        return QVariant();
    }
    
    int actualRow = m_isFiltered ? m_filteredIndices[row] : row;
    const QJsonObject &obj = m_rows[actualRow];
    
    // 支持嵌套路径
    QStringList pathParts = m_columns[column].jsonPath.split('.');
    QJsonValue current = obj;
    
    for (const QString &part : pathParts) {
        if (current.isObject()) {
            current = current.toObject().value(part);
        } else {
            return QVariant();
        }
    }
    
    return current.toVariant();
}

QString JSONLTableModel::getCellJsonPath(int row, int column) const
{
    if (column < 0 || column >= static_cast<int>(m_columns.size())) {
        return QString();
    }
    return QString("$[%1].%2").arg(row).arg(m_columns[column].jsonPath);
}

QVariantList JSONLTableModel::searchInColumn(int column, const QString &text)
{
    QVariantList results;
    
    if (column < 0 || column >= static_cast<int>(m_columns.size())) {
        return results;
    }
    
    for (size_t i = 0; i < m_rows.size(); ++i) {
        QVariant value = getCellValue(static_cast<int>(i), column);
        if (value.toString().contains(text, Qt::CaseInsensitive)) {
            results.append(static_cast<int>(i));
        }
    }
    
    return results;
}

void JSONLTableModel::applyFilter(int column, const QString &value)
{
    if (column < 0 || column >= static_cast<int>(m_columns.size())) {
        return;
    }
    
    beginResetModel();
    m_filteredIndices.clear();
    
    for (size_t i = 0; i < m_rows.size(); ++i) {
        QVariant cellValue = getCellValue(static_cast<int>(i), column);
        if (cellValue.toString().contains(value, Qt::CaseInsensitive)) {
            m_filteredIndices.push_back(static_cast<int>(i));
        }
    }
    
    m_isFiltered = true;
    endResetModel();
    emit totalRowCountChanged();
}

void JSONLTableModel::clearFilters()
{
    if (!m_isFiltered) return;
    
    beginResetModel();
    m_filteredIndices.clear();
    m_isFiltered = false;
    endResetModel();
    emit totalRowCountChanged();
}

bool JSONLTableModel::exportToCSV(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    // 写入表头
    QStringList headers;
    for (const auto &col : m_columns) {
        headers.append(col.name);
    }
    out << headers.join(',') << "\n";
    
    // 写入数据
    for (size_t i = 0; i < m_rows.size(); ++i) {
        QStringList values;
        for (size_t j = 0; j < m_columns.size(); ++j) {
            QVariant value = getCellValue(static_cast<int>(i), static_cast<int>(j));
            QString str = value.toString();
            if (str.contains(',') || str.contains('"') || str.contains('\n')) {
                str = '"' + str.replace('"', "\"\"") + '"';
            }
            values.append(str);
        }
        out << values.join(',') << "\n";
    }
    
    file.close();
    return true;
}

bool JSONLTableModel::exportToJSON(const QString &path) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QJsonArray arr;
    for (const auto &obj : m_rows) {
        arr.append(obj);
    }
    
    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

int JSONLTableModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_isFiltered ? static_cast<int>(m_filteredIndices.size()) 
                        : static_cast<int>(m_rows.size());
}

int JSONLTableModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(m_columns.size());
}

QVariant JSONLTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();
    
    switch (role) {
    case Qt::DisplayRole:
    case RawValueRole:
        return getCellValue(index.row(), index.column());
        
    case ColumnNameRole:
        if (index.column() >= 0 && index.column() < static_cast<int>(m_columns.size())) {
            return m_columns[index.column()].name;
        }
        break;
        
    case ValueTypeRole:
        if (index.column() >= 0 && index.column() < static_cast<int>(m_columns.size())) {
            return m_columns[index.column()].type;
        }
        break;
    }
    
    return QVariant();
}

QVariant JSONLTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    
    if (orientation == Qt::Horizontal) {
        if (section >= 0 && section < static_cast<int>(m_columns.size())) {
            return m_columns[section].name;
        }
    } else {
        return section + 1;
    }
    
    return QVariant();
}

QHash<int, QByteArray> JSONLTableModel::roleNames() const
{
    auto roles = QAbstractTableModel::roleNames();
    roles[RawValueRole] = "rawValue";
    roles[ColumnNameRole] = "columnName";
    roles[ValueTypeRole] = "valueType";
    return roles;
}

void JSONLTableModel::loadDataAsync()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    qint64 fileSize = file.size();
    qint64 bytesRead = 0;
    int lastPercent = 0;
    
    QSet<QString> seenColumns;
    
    QTextStream stream(&file);
    while (!stream.atEnd() && !m_cancelRequested.load()) {
        QString line = stream.readLine();
        bytesRead += line.toUtf8().size() + 1;
        
        line = line.trimmed();
        if (line.isEmpty()) continue;
        
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8(), &error);
        
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            
            QMutexLocker lock(&m_dataMutex);
            m_rows.push_back(obj);
            
            // 提取列（只在前100行做）
            if (m_rows.size() <= 100) {
                extractColumns(obj, QString());
            }
        }
        
        int percent = static_cast<int>(bytesRead * 100 / fileSize);
        if (percent > lastPercent) {
            lastPercent = percent;
            emit loadingProgress(percent);
        }
    }
    
    file.close();
}

void JSONLTableModel::onLoadingFinished()
{
    m_isLoading.store(false);
    emit loadingStateChanged();
    emit totalRowCountChanged();
    emit totalColumnCountChanged();
    
    if (m_cancelRequested.load()) {
        emit loadingFinished(false, tr("Loading cancelled"));
    } else {
        emit loadingFinished(true, tr("Loaded %1 rows, %2 columns")
                           .arg(m_rows.size()).arg(m_columns.size()));
    }
}

void JSONLTableModel::extractColumns(const QJsonObject &obj, const QString &prefix)
{
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        QString path = prefix.isEmpty() ? it.key() : (prefix + "." + it.key());
        
        if (it.value().isObject()) {
            // 递归提取嵌套对象的列
            extractColumns(it.value().toObject(), path);
        } else {
            // 检查是否已存在
            bool exists = false;
            for (const auto &col : m_columns) {
                if (col.jsonPath == path) {
                    exists = true;
                    break;
                }
            }
            
            if (!exists) {
                ColumnInfo col;
                col.jsonPath = path;
                col.name = path;  // 可以简化显示名
                
                // 推断类型
                if (it.value().isDouble()) {
                    col.type = 1;  // Number
                } else if (it.value().isBool()) {
                    col.type = 3;  // Boolean
                } else {
                    col.type = 0;  // String
                }
                
                m_columns.push_back(col);
            }
        }
    }
}
