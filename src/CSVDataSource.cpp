/**
 * @file CSVDataSource.cpp
 * @brief CSV/TSV 专用数据源实现
 */

#include "CSVDataSource.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>
#include <QtConcurrent>
#include <QSet>
#include <algorithm>
#include <cmath>

CSVDataSource::CSVDataSource(QObject *parent)
    : QAbstractTableModel(parent)
{
    connect(&m_loadWatcher, &QFutureWatcher<void>::finished,
            this, &CSVDataSource::onLoadingFinished);
}

CSVDataSource::~CSVDataSource()
{
    closeFile();
}

bool CSVDataSource::loadFile(const QString &filePath)
{
    if (m_isLoading.load()) {
        return false;
    }
    
    closeFile();
    
    m_file.setFileName(filePath);
    if (!m_file.open(QIODevice::ReadOnly)) {
        emit loadingFinished(false, tr("Cannot open file: %1").arg(m_file.errorString()));
        return false;
    }
    
    m_filePath = filePath;
    m_fileSize = m_file.size();
    emit filePathChanged();
    emit fileSizeChanged();
    
    // 读取样本检测分隔符
    QString sample = QString::fromUtf8(m_file.read(qMin(m_fileSize, qint64(8192))));
    m_file.seek(0);
    
    m_delimiter = detectDelimiter(sample);
    emit delimiterChanged();
    
    m_isLoading.store(true);
    m_cancelRequested.store(false);
    emit loadingStateChanged();
    
    // 异步加载
    auto future = QtConcurrent::run([this]() { loadDataAsync(); });
    m_loadWatcher.setFuture(future);
    
    return true;
}

void CSVDataSource::closeFile()
{
    cancelLoading();
    
    m_file.close();
    
    beginResetModel();
    m_data.clear();
    m_columns.clear();
    m_sortedIndices.clear();
    m_filteredIndices.clear();
    m_columnVisible.clear();
    endResetModel();
    
    m_filePath.clear();
    m_fileSize = 0;
    m_sortColumnIndex = -1;
    m_sortColumn.clear();
    m_isFiltered = false;
    
    emit filePathChanged();
    emit fileSizeChanged();
    emit totalRowCountChanged();
    emit totalColumnCountChanged();
}

void CSVDataSource::cancelLoading()
{
    if (m_isLoading.load()) {
        m_cancelRequested.store(true);
        m_loadWatcher.waitForFinished();
    }
}

void CSVDataSource::setDelimiter(QChar d)
{
    if (m_delimiter != d) {
        m_delimiter = d;
        emit delimiterChanged();
        
        // 如果已加载文件，重新解析
        if (!m_filePath.isEmpty() && !m_isLoading.load()) {
            loadFile(m_filePath);
        }
    }
}

void CSVDataSource::setHasHeader(bool h)
{
    if (m_hasHeader != h) {
        m_hasHeader = h;
        emit hasHeaderChanged();
        
        // 重新解析
        if (!m_filePath.isEmpty() && !m_isLoading.load()) {
            loadFile(m_filePath);
        }
    }
}

void CSVDataSource::setFrozenRowCount(int count)
{
    if (m_frozenRowCount != count) {
        m_frozenRowCount = count;
        emit frozenRowsChanged();
    }
}

void CSVDataSource::setFrozenColumnCount(int count)
{
    if (m_frozenColumnCount != count) {
        m_frozenColumnCount = count;
        emit frozenColumnsChanged();
    }
}

QVariantList CSVDataSource::getColumns() const
{
    QVariantList result;
    for (size_t i = 0; i < m_columns.size(); ++i) {
        if (i < m_columnVisible.size() && !m_columnVisible[i]) {
            continue;
        }
        QVariantMap col;
        col["index"] = static_cast<int>(i);
        col["name"] = m_columns[i].name;
        col["type"] = m_columns[i].type;
        col["width"] = m_columns[i].width;
        col["frozen"] = m_columns[i].frozen;
        result.append(col);
    }
    return result;
}

QVariantMap CSVDataSource::getColumnMeta(int columnIndex) const
{
    QVariantMap result;
    if (columnIndex >= 0 && columnIndex < static_cast<int>(m_columns.size())) {
        const auto &col = m_columns[columnIndex];
        result["name"] = col.name;
        result["type"] = col.type;
        result["width"] = col.width;
        result["frozen"] = col.frozen;
        result["nullCount"] = col.nullCount;
        result["uniqueCount"] = col.uniqueCount;
        result["minValue"] = col.minValue;
        result["maxValue"] = col.maxValue;
    }
    return result;
}

void CSVDataSource::setColumnWidth(int columnIndex, int width)
{
    if (columnIndex >= 0 && columnIndex < static_cast<int>(m_columns.size())) {
        m_columns[columnIndex].width = width;
    }
}

void CSVDataSource::autoFitColumnWidth(int columnIndex)
{
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) {
        return;
    }
    
    int maxWidth = m_columns[columnIndex].name.length() * 8 + 20;
    
    // 采样前100行计算最大宽度
    int sampleCount = qMin(100, static_cast<int>(m_data.size()));
    for (int i = 0; i < sampleCount; ++i) {
        if (columnIndex < m_data[i].size()) {
            int width = m_data[i][columnIndex].length() * 8 + 20;
            maxWidth = qMax(maxWidth, width);
        }
    }
    
    m_columns[columnIndex].width = qMin(maxWidth, 400);  // 最大400像素
}

void CSVDataSource::setColumnFrozen(int columnIndex, bool frozen)
{
    if (columnIndex >= 0 && columnIndex < static_cast<int>(m_columns.size())) {
        m_columns[columnIndex].frozen = frozen;
        emit frozenColumnsChanged();
    }
}

void CSVDataSource::setColumnVisible(int columnIndex, bool visible)
{
    if (columnIndex >= 0 && columnIndex < static_cast<int>(m_columnVisible.size())) {
        m_columnVisible[columnIndex] = visible;
        emit totalColumnCountChanged();
    }
}

void CSVDataSource::sortByColumn(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= static_cast<int>(m_columns.size())) {
        return;
    }
    
    m_sortColumnIndex = column;
    m_sortColumn = m_columns[column].name;
    m_sortOrder = order;
    
    sort(column, order);
    emit sortChanged();
}

void CSVDataSource::clearSort()
{
    m_sortColumnIndex = -1;
    m_sortColumn.clear();
    
    // 恢复原始顺序
    m_sortedIndices.clear();
    m_sortedIndices.reserve(m_data.size());
    for (size_t i = 0; i < m_data.size(); ++i) {
        m_sortedIndices.push_back(static_cast<int>(i));
    }
    
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
    emit sortChanged();
}

QVariantList CSVDataSource::searchInColumn(int column, const QString &text, bool useRegex)
{
    QVariantList results;
    
    if (text.isEmpty()) {
        return results;
    }
    
    QRegularExpression regex;
    if (useRegex) {
        regex.setPattern(text);
        regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        if (!regex.isValid()) {
            return results;
        }
    }
    
    for (size_t i = 0; i < m_data.size(); ++i) {
        const auto &row = m_data[i];
        
        int startCol = (column < 0) ? 0 : column;
        int endCol = (column < 0) ? static_cast<int>(row.size()) : column + 1;
        
        for (int c = startCol; c < endCol && c < row.size(); ++c) {
            bool match = false;
            if (useRegex) {
                match = regex.match(row[c]).hasMatch();
            } else {
                match = row[c].contains(text, Qt::CaseInsensitive);
            }
            
            if (match) {
                results.append(static_cast<int>(i));
                break;
            }
        }
    }
    
    return results;
}

void CSVDataSource::applyColumnFilter(int column, const QString &filterValue, const QString &operatorType)
{
    if (column < 0 || column >= static_cast<int>(m_columns.size())) {
        return;
    }
    
    beginResetModel();
    
    m_filteredIndices.clear();
    
    for (size_t i = 0; i < m_data.size(); ++i) {
        if (column >= m_data[i].size()) {
            continue;
        }
        
        const QString &value = m_data[i][column];
        bool match = false;
        
        if (operatorType == "equals") {
            match = (value.compare(filterValue, Qt::CaseInsensitive) == 0);
        } else if (operatorType == "contains") {
            match = value.contains(filterValue, Qt::CaseInsensitive);
        } else if (operatorType == "startsWith") {
            match = value.startsWith(filterValue, Qt::CaseInsensitive);
        } else if (operatorType == "endsWith") {
            match = value.endsWith(filterValue, Qt::CaseInsensitive);
        } else if (operatorType == "greaterThan") {
            bool ok1, ok2;
            double v1 = value.toDouble(&ok1);
            double v2 = filterValue.toDouble(&ok2);
            match = ok1 && ok2 && v1 > v2;
        } else if (operatorType == "lessThan") {
            bool ok1, ok2;
            double v1 = value.toDouble(&ok1);
            double v2 = filterValue.toDouble(&ok2);
            match = ok1 && ok2 && v1 < v2;
        } else if (operatorType == "isEmpty") {
            match = value.trimmed().isEmpty();
        } else if (operatorType == "isNotEmpty") {
            match = !value.trimmed().isEmpty();
        }
        
        if (match) {
            m_filteredIndices.push_back(static_cast<int>(i));
        }
    }
    
    m_isFiltered = true;
    endResetModel();
    emit totalRowCountChanged();
}

void CSVDataSource::clearAllFilters()
{
    if (!m_isFiltered) {
        return;
    }
    
    beginResetModel();
    m_filteredIndices.clear();
    m_isFiltered = false;
    endResetModel();
    emit totalRowCountChanged();
}

bool CSVDataSource::exportToCSV(const QString &path, bool includeHeader)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    // 写入表头
    if (includeHeader && !m_columns.empty()) {
        QStringList headers;
        for (const auto &col : m_columns) {
            QString header = col.name;
            if (header.contains(m_delimiter) || header.contains('"') || header.contains('\n')) {
                header = '"' + header.replace('"', "\"\"") + '"';
            }
            headers.append(header);
        }
        out << headers.join(m_delimiter) << "\n";
    }
    
    // 写入数据
    const auto &indices = m_isFiltered ? m_filteredIndices : m_sortedIndices;
    for (int idx : indices) {
        if (idx >= 0 && idx < static_cast<int>(m_data.size())) {
            QStringList fields;
            for (const auto &field : m_data[idx]) {
                QString value = field;
                if (value.contains(m_delimiter) || value.contains('"') || value.contains('\n')) {
                    value = '"' + value.replace('"', "\"\"") + '"';
                }
                fields.append(value);
            }
            out << fields.join(m_delimiter) << "\n";
        }
    }
    
    file.close();
    return true;
}

bool CSVDataSource::exportSelectedRows(const QString &path, const QVariantList &rowIndices)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    
    // 写入表头
    if (!m_columns.empty()) {
        QStringList headers;
        for (const auto &col : m_columns) {
            QString header = col.name;
            if (header.contains(m_delimiter) || header.contains('"') || header.contains('\n')) {
                header = '"' + header.replace('"', "\"\"") + '"';
            }
            headers.append(header);
        }
        out << headers.join(m_delimiter) << "\n";
    }
    
    // 写入选中行
    for (const auto &v : rowIndices) {
        int idx = v.toInt();
        if (idx >= 0 && idx < static_cast<int>(m_data.size())) {
            QStringList fields;
            for (const auto &field : m_data[idx]) {
                QString value = field;
                if (value.contains(m_delimiter) || value.contains('"') || value.contains('\n')) {
                    value = '"' + value.replace('"', "\"\"") + '"';
                }
                fields.append(value);
            }
            out << fields.join(m_delimiter) << "\n";
        }
    }
    
    file.close();
    return true;
}

QVariantMap CSVDataSource::getColumnStatistics(int columnIndex) const
{
    QVariantMap result;
    
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) {
        return result;
    }
    
    const auto &col = m_columns[columnIndex];
    result["name"] = col.name;
    result["type"] = col.type;
    result["totalCount"] = static_cast<int>(m_data.size());
    result["nullCount"] = col.nullCount;
    result["uniqueCount"] = col.uniqueCount;
    
    if (col.type == NumberType) {
        result["minValue"] = col.minValue;
        result["maxValue"] = col.maxValue;
        
        // 计算平均值和标准差
        double sum = 0;
        int count = 0;
        for (const auto &row : m_data) {
            if (columnIndex < row.size()) {
                bool ok;
                double v = row[columnIndex].toDouble(&ok);
                if (ok) {
                    sum += v;
                    count++;
                }
            }
        }
        if (count > 0) {
            double mean = sum / count;
            result["mean"] = mean;
            
            double variance = 0;
            for (const auto &row : m_data) {
                if (columnIndex < row.size()) {
                    bool ok;
                    double v = row[columnIndex].toDouble(&ok);
                    if (ok) {
                        variance += (v - mean) * (v - mean);
                    }
                }
            }
            result["stdDev"] = std::sqrt(variance / count);
        }
    }
    
    return result;
}

QVariantList CSVDataSource::getDistinctValues(int columnIndex, int maxCount) const
{
    QVariantList result;
    
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) {
        return result;
    }
    
    QMap<QString, int> valueCounts;
    for (const auto &row : m_data) {
        if (columnIndex < row.size()) {
            valueCounts[row[columnIndex]]++;
        }
    }
    
    // 按频率排序
    QList<QPair<QString, int>> sorted;
    for (auto it = valueCounts.begin(); it != valueCounts.end(); ++it) {
        sorted.append(qMakePair(it.key(), it.value()));
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });
    
    // 取前 maxCount 个
    for (int i = 0; i < qMin(maxCount, sorted.size()); ++i) {
        QVariantMap item;
        item["value"] = sorted[i].first;
        item["count"] = sorted[i].second;
        result.append(item);
    }
    
    return result;
}

QVariant CSVDataSource::getCellValue(int row, int column) const
{
    int actualRow = row;
    if (!m_sortedIndices.empty() && row < static_cast<int>(m_sortedIndices.size())) {
        actualRow = m_sortedIndices[row];
    }
    if (m_isFiltered && row < static_cast<int>(m_filteredIndices.size())) {
        actualRow = m_filteredIndices[row];
    }
    
    if (actualRow >= 0 && actualRow < static_cast<int>(m_data.size())) {
        if (column >= 0 && column < m_data[actualRow].size()) {
            return m_data[actualRow][column];
        }
    }
    return QVariant();
}

QString CSVDataSource::getRawRowText(int row) const
{
    int actualRow = row;
    if (!m_sortedIndices.empty() && row < static_cast<int>(m_sortedIndices.size())) {
        actualRow = m_sortedIndices[row];
    }
    
    if (actualRow >= 0 && actualRow < static_cast<int>(m_data.size())) {
        return m_data[actualRow].join(m_delimiter);
    }
    return QString();
}

int CSVDataSource::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    
    if (m_isFiltered) {
        return static_cast<int>(m_filteredIndices.size());
    }
    return static_cast<int>(m_data.size());
}

int CSVDataSource::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    
    int visibleCount = 0;
    for (size_t i = 0; i < m_columns.size(); ++i) {
        if (i >= m_columnVisible.size() || m_columnVisible[i]) {
            visibleCount++;
        }
    }
    return visibleCount;
}

QVariant CSVDataSource::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return QVariant();
    
    int row = index.row();
    int column = index.column();
    
    // 计算实际行索引
    int actualRow = row;
    if (m_isFiltered && row < static_cast<int>(m_filteredIndices.size())) {
        actualRow = m_filteredIndices[row];
    } else if (!m_sortedIndices.empty() && row < static_cast<int>(m_sortedIndices.size())) {
        actualRow = m_sortedIndices[row];
    }
    
    if (actualRow < 0 || actualRow >= static_cast<int>(m_data.size())) {
        return QVariant();
    }
    
    // 计算实际列索引（跳过隐藏列）
    int actualColumn = -1;
    int visibleCount = 0;
    for (size_t i = 0; i < m_columns.size(); ++i) {
        if (i >= m_columnVisible.size() || m_columnVisible[i]) {
            if (visibleCount == column) {
                actualColumn = static_cast<int>(i);
                break;
            }
            visibleCount++;
        }
    }
    
    if (actualColumn < 0 || actualColumn >= m_data[actualRow].size()) {
        return QVariant();
    }
    
    switch (role) {
    case Qt::DisplayRole:
    case Qt::EditRole:
    case RawValueRole:
        return m_data[actualRow][actualColumn];
        
    case ColumnTypeRole:
        return actualColumn < static_cast<int>(m_columns.size()) ? m_columns[actualColumn].type : 0;
        
    case ColumnIndexRole:
        return actualColumn;
        
    case RowIndexRole:
        return actualRow;
        
    case IsFrozenRole:
        return (row < m_frozenRowCount) || 
               (actualColumn < static_cast<int>(m_columns.size()) && m_columns[actualColumn].frozen);
        
    default:
        return QVariant();
    }
}

QVariant CSVDataSource::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) return QVariant();
    
    if (orientation == Qt::Horizontal) {
        // 计算实际列索引
        int visibleCount = 0;
        for (size_t i = 0; i < m_columns.size(); ++i) {
            if (i >= m_columnVisible.size() || m_columnVisible[i]) {
                if (visibleCount == section) {
                    return m_columns[i].name;
                }
                visibleCount++;
            }
        }
    } else {
        return section + 1;  // 行号从1开始
    }
    
    return QVariant();
}

QHash<int, QByteArray> CSVDataSource::roleNames() const
{
    auto roles = QAbstractTableModel::roleNames();
    roles[RawValueRole] = "rawValue";
    roles[ColumnTypeRole] = "columnType";
    roles[ColumnIndexRole] = "columnIndex";
    roles[RowIndexRole] = "rowIndex";
    roles[IsFrozenRole] = "isFrozen";
    return roles;
}

void CSVDataSource::sort(int column, Qt::SortOrder order)
{
    // 计算实际列索引
    int actualColumn = -1;
    int visibleCount = 0;
    for (size_t i = 0; i < m_columns.size(); ++i) {
        if (i >= m_columnVisible.size() || m_columnVisible[i]) {
            if (visibleCount == column) {
                actualColumn = static_cast<int>(i);
                break;
            }
            visibleCount++;
        }
    }
    
    if (actualColumn < 0) return;
    
    // 初始化排序索引
    if (m_sortedIndices.empty()) {
        m_sortedIndices.reserve(m_data.size());
        for (size_t i = 0; i < m_data.size(); ++i) {
            m_sortedIndices.push_back(static_cast<int>(i));
        }
    }
    
    // 获取列类型
    ColumnType colType = (actualColumn < static_cast<int>(m_columns.size())) 
                         ? static_cast<ColumnType>(m_columns[actualColumn].type) 
                         : TextType;
    
    // 排序
    std::stable_sort(m_sortedIndices.begin(), m_sortedIndices.end(),
        [this, actualColumn, order, colType](int a, int b) {
            QString va = (actualColumn < m_data[a].size()) ? m_data[a][actualColumn] : QString();
            QString vb = (actualColumn < m_data[b].size()) ? m_data[b][actualColumn] : QString();
            
            bool less = false;
            
            if (colType == NumberType) {
                bool okA, okB;
                double da = va.toDouble(&okA);
                double db = vb.toDouble(&okB);
                if (okA && okB) {
                    less = da < db;
                } else if (okA) {
                    less = true;
                } else if (okB) {
                    less = false;
                } else {
                    less = va.compare(vb, Qt::CaseInsensitive) < 0;
                }
            } else {
                less = va.compare(vb, Qt::CaseInsensitive) < 0;
            }
            
            return (order == Qt::AscendingOrder) ? less : !less;
        });
    
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void CSVDataSource::onLoadingFinished()
{
    m_isLoading.store(false);
    emit loadingStateChanged();
    
    if (m_cancelRequested.load()) {
        emit loadingCancelled();
    } else {
        emit loadingFinished(true, tr("Loaded %1 rows, %2 columns")
                           .arg(m_data.size()).arg(m_columns.size()));
    }
    
    emit totalRowCountChanged();
    emit totalColumnCountChanged();
}

QChar CSVDataSource::detectDelimiter(const QString &sample)
{
    // 统计各分隔符出现次数
    QMap<QChar, int> counts;
    counts[','] = 0;
    counts['\t'] = 0;
    counts[';'] = 0;
    counts['|'] = 0;
    
    bool inQuotes = false;
    for (const QChar &c : sample) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes && counts.contains(c)) {
            counts[c]++;
        }
    }
    
    // 找出现次数最多的分隔符
    QChar best = ',';
    int maxCount = 0;
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        if (it.value() > maxCount) {
            maxCount = it.value();
            best = it.key();
        }
    }
    
    return best;
}

CSVDataSource::ColumnType CSVDataSource::detectColumnType(const QStringList &samples)
{
    int numericCount = 0;
    int dateCount = 0;
    int boolCount = 0;
    int nonEmptyCount = 0;
    
    static QRegularExpression dateRegex(
        R"(^\d{4}[-/]\d{1,2}[-/]\d{1,2}(\s+\d{1,2}:\d{2}(:\d{2})?)?$|)"
        R"(^\d{1,2}[-/]\d{1,2}[-/]\d{2,4}$)"
    );
    
    for (const QString &s : samples) {
        QString trimmed = s.trimmed();
        if (trimmed.isEmpty()) continue;
        
        nonEmptyCount++;
        
        // 检查布尔
        QString lower = trimmed.toLower();
        if (lower == "true" || lower == "false" || lower == "yes" || lower == "no" ||
            lower == "1" || lower == "0") {
            boolCount++;
        }
        
        // 检查数字
        bool ok;
        trimmed.toDouble(&ok);
        if (ok) {
            numericCount++;
        }
        
        // 检查日期
        if (dateRegex.match(trimmed).hasMatch()) {
            dateCount++;
        }
    }
    
    if (nonEmptyCount == 0) return TextType;
    
    double threshold = 0.8;
    if (static_cast<double>(numericCount) / nonEmptyCount >= threshold) {
        return NumberType;
    }
    if (static_cast<double>(dateCount) / nonEmptyCount >= threshold) {
        return DateTimeType;
    }
    if (static_cast<double>(boolCount) / nonEmptyCount >= threshold) {
        return BooleanType;
    }
    
    return TextType;
}

QStringList CSVDataSource::parseCSVLine(const QString &line) const
{
    QStringList fields;
    QString field;
    bool inQuotes = false;
    
    for (int i = 0; i < line.length(); ++i) {
        QChar c = line[i];
        
        if (inQuotes) {
            if (c == m_quoteChar) {
                // 检查是否是转义引号
                if (i + 1 < line.length() && line[i + 1] == m_quoteChar) {
                    field += m_quoteChar;
                    i++;  // 跳过下一个引号
                } else {
                    inQuotes = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == m_quoteChar) {
                inQuotes = true;
            } else if (c == m_delimiter) {
                fields.append(field.trimmed());
                field.clear();
            } else {
                field += c;
            }
        }
    }
    
    fields.append(field.trimmed());
    return fields;
}

void CSVDataSource::loadDataAsync()
{
    QTextStream stream(&m_file);
    
    // 读取第一行作为表头或数据
    QString firstLine = stream.readLine();
    if (firstLine.isEmpty()) {
        return;
    }
    
    QStringList headers = parseCSVLine(firstLine);
    int columnCount = headers.size();
    
    // 初始化列元数据
    {
        QMutexLocker lock(&m_dataMutex);
        m_columns.clear();
        m_columnVisible.clear();
        
        for (int i = 0; i < columnCount; ++i) {
            ColumnMeta col;
            if (m_hasHeader) {
                col.name = headers[i].isEmpty() ? QString("Column %1").arg(i + 1) : headers[i];
            } else {
                col.name = QString("Column %1").arg(i + 1);
            }
            m_columns.push_back(col);
            m_columnVisible.push_back(true);
        }
        
        // 如果没有表头，第一行就是数据
        if (!m_hasHeader) {
            m_data.push_back(headers);
        }
    }
    
    // 读取数据行
    qint64 bytesRead = firstLine.toUtf8().size();
    int lastPercent = 0;
    
    // 用于类型检测的样本
    std::vector<QStringList> typeSamples(columnCount);
    const int MAX_TYPE_SAMPLES = 100;
    
    while (!stream.atEnd() && !m_cancelRequested.load()) {
        QString line = stream.readLine();
        bytesRead += line.toUtf8().size() + 1;
        
        if (line.trimmed().isEmpty()) continue;
        
        QStringList fields = parseCSVLine(line);
        
        // 填充或截断到正确列数
        while (fields.size() < columnCount) {
            fields.append(QString());
        }
        if (fields.size() > columnCount) {
            fields = fields.mid(0, columnCount);
        }
        
        {
            QMutexLocker lock(&m_dataMutex);
            m_data.push_back(fields);
            
            // 收集类型检测样本
            for (int i = 0; i < columnCount && i < fields.size(); ++i) {
                if (typeSamples[i].size() < MAX_TYPE_SAMPLES) {
                    typeSamples[i].append(fields[i]);
                }
            }
        }
        
        // 报告进度
        int percent = static_cast<int>(bytesRead * 100 / m_fileSize);
        if (percent > lastPercent) {
            lastPercent = percent;
            emit loadingProgress(percent);
        }
    }
    
    // 检测列类型
    {
        QMutexLocker lock(&m_dataMutex);
        for (int i = 0; i < columnCount; ++i) {
            m_columns[i].type = detectColumnType(typeSamples[i]);
        }
    }
    
    // 初始化排序索引
    {
        QMutexLocker lock(&m_dataMutex);
        m_sortedIndices.clear();
        m_sortedIndices.reserve(m_data.size());
        for (size_t i = 0; i < m_data.size(); ++i) {
            m_sortedIndices.push_back(static_cast<int>(i));
        }
    }
    
    // 更新列统计信息
    for (int i = 0; i < columnCount; ++i) {
        updateColumnStatistics(i);
    }
}

void CSVDataSource::updateColumnStatistics(int columnIndex)
{
    if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size())) {
        return;
    }
    
    QSet<QString> uniqueValues;
    qint64 nullCount = 0;
    double minVal = std::numeric_limits<double>::max();
    double maxVal = std::numeric_limits<double>::lowest();
    bool isNumeric = (m_columns[columnIndex].type == NumberType);
    
    for (const auto &row : m_data) {
        if (columnIndex >= row.size()) {
            nullCount++;
            continue;
        }
        
        const QString &value = row[columnIndex];
        if (value.trimmed().isEmpty()) {
            nullCount++;
        } else {
            uniqueValues.insert(value);
            
            if (isNumeric) {
                bool ok;
                double v = value.toDouble(&ok);
                if (ok) {
                    minVal = qMin(minVal, v);
                    maxVal = qMax(maxVal, v);
                }
            }
        }
    }
    
    QMutexLocker lock(&m_dataMutex);
    m_columns[columnIndex].nullCount = nullCount;
    m_columns[columnIndex].uniqueCount = uniqueValues.size();
    if (isNumeric && minVal <= maxVal) {
        m_columns[columnIndex].minValue = minVal;
        m_columns[columnIndex].maxValue = maxVal;
    }
    
    emit columnStatsReady(columnIndex);
}
