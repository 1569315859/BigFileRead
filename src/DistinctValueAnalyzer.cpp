/**
 * @file DistinctValueAnalyzer.cpp
 * @brief Distinct Value Analyzer implementation
 */

#include "DistinctValueAnalyzer.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

DistinctValueAnalyzer& DistinctValueAnalyzer::instance()
{
    static DistinctValueAnalyzer instance;
    return instance;
}

DistinctValueAnalyzer::DistinctValueAnalyzer(QObject *parent)
    : QObject(parent)
    , m_isAnalyzing(false)
    , m_progress(0)
    , m_totalLines(0)
    , m_processedLines(0)
    , m_cancelRequested(false)
{
}

DistinctValueAnalyzer::~DistinctValueAnalyzer()
{
}

void DistinctValueAnalyzer::addColumn(const QVariantMap &config)
{
    ColumnConfig col;
    col.name = config.value("name").toString();
    col.type = static_cast<ColumnConfig::Type>(config.value("type", ColumnConfig::Auto).toInt());
    col.regexPattern = config.value("regexPattern").toString();
    col.captureGroup = config.value("captureGroup", 1).toInt();
    col.dateFormat = config.value("dateFormat").toString();
    
    // Remove existing column with same name
    m_columns.erase(std::remove_if(m_columns.begin(), m_columns.end(),
        [&col](const ColumnConfig &c) { return c.name == col.name; }), m_columns.end());
    
    m_columns.append(col);
    emit columnsChanged();
}

void DistinctValueAnalyzer::removeColumn(const QString &name)
{
    m_columns.erase(std::remove_if(m_columns.begin(), m_columns.end(),
        [&name](const ColumnConfig &c) { return c.name == name; }), m_columns.end());
    m_valueCounters.remove(name);
    m_results.remove(name);
    emit columnsChanged();
    emit resultsChanged();
}

void DistinctValueAnalyzer::clearColumns()
{
    m_columns.clear();
    m_valueCounters.clear();
    m_results.clear();
    m_numericValues.clear();
    emit columnsChanged();
    emit resultsChanged();
}

QVariantList DistinctValueAnalyzer::detectColumns(const QStringList &sampleLines)
{
    QVariantList detected;
    
    if (sampleLines.isEmpty()) return detected;
    
    // Try common log formats
    
    // 1. Check for key=value patterns
    QRegularExpression kvRegex(R"((\w+)=([^\s,]+))");
    QHash<QString, int> keyFrequency;
    
    for (const QString &line : sampleLines) {
        auto it = kvRegex.globalMatch(line);
        while (it.hasNext()) {
            auto match = it.next();
            keyFrequency[match.captured(1)]++;
        }
    }
    
    // Add frequently occurring keys as columns
    for (auto it = keyFrequency.constBegin(); it != keyFrequency.constEnd(); ++it) {
        if (it.value() >= sampleLines.size() * 0.5) {  // At least 50% occurrence
            QVariantMap col;
            col["name"] = it.key();
            col["type"] = ColumnConfig::Regex;
            col["regexPattern"] = QString(R"(%1=([^\s,]+))").arg(QRegularExpression::escape(it.key()));
            col["captureGroup"] = 1;
            detected.append(col);
            emit columnDetected(it.key(), "key-value");
        }
    }
    
    // 2. Check for JSON fields
    if (sampleLines.first().contains('{')) {
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(sampleLines.first().toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            for (const QString &key : obj.keys()) {
                QVariantMap col;
                col["name"] = key;
                col["type"] = ColumnConfig::Regex;
                // Pattern: "key"\s*:\s*"?([^",}]+)"?
                col["regexPattern"] = QString("\"%1\"\\s*:\\s*\"?([^\",}]+)\"?").arg(QRegularExpression::escape(key));
                col["captureGroup"] = 1;
                detected.append(col);
                emit columnDetected(key, "json");
            }
        }
    }
    
    // 3. Check for common fields
    struct CommonPattern {
        QString name;
        QString pattern;
    };
    QList<CommonPattern> commonPatterns = {
        {"timestamp", "(\\d{4}-\\d{2}-\\d{2}[T ]\\d{2}:\\d{2}:\\d{2})"},
        {"level", "\\b(DEBUG|INFO|WARN|WARNING|ERROR|FATAL|TRACE)\\b"},
        {"ip_address", "\\b(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})\\b"},
        {"http_status", "\\b([1-5]\\d{2})\\b"},
        {"http_method", "\\b(GET|POST|PUT|DELETE|PATCH|HEAD|OPTIONS)\\b"},
        {"url_path", "\"(/[^\"\\s]+)\""},
        {"uuid", "([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})"},
        {"thread", "\\[([^\\]]+)\\]"},
        {"class_name", "\\b([A-Z][a-zA-Z0-9_]+(?:\\.[A-Z][a-zA-Z0-9_]+)+)\\b"}
    };
    
    for (const auto &pattern : commonPatterns) {
        QRegularExpression regex(pattern.pattern, QRegularExpression::CaseInsensitiveOption);
        int matches = 0;
        for (const QString &line : sampleLines) {
            if (regex.match(line).hasMatch()) matches++;
        }
        
        if (matches >= sampleLines.size() * 0.3) {  // At least 30% occurrence
            // Check if already added
            bool exists = std::any_of(detected.begin(), detected.end(),
                [&pattern](const QVariant &v) { return v.toMap()["name"] == pattern.name; });
            
            if (!exists) {
                QVariantMap col;
                col["name"] = pattern.name;
                col["type"] = ColumnConfig::Regex;
                col["regexPattern"] = pattern.pattern;
                col["captureGroup"] = 1;
                detected.append(col);
                emit columnDetected(pattern.name, "common");
            }
        }
    }
    
    return detected;
}

void DistinctValueAnalyzer::analyzeLines(const QStringList &lines)
{
    if (lines.isEmpty() || m_columns.isEmpty()) {
        emit analysisError(tr("No lines or columns to analyze"));
        return;
    }
    
    m_isAnalyzing = true;
    m_progress = 0;
    m_totalLines = lines.size();
    m_processedLines = 0;
    m_cancelRequested = false;
    
    // Clear previous results
    m_valueCounters.clear();
    m_numericValues.clear();
    m_results.clear();
    
    emit analyzingChanged();
    emit analysisStarted();
    
    // Initialize counters
    for (const auto &col : m_columns) {
        m_valueCounters[col.name] = QHash<QString, int>();
        m_numericValues[col.name] = QVector<double>();
    }
    
    // Process lines
    for (const QString &line : lines) {
        if (m_cancelRequested) break;
        
        processLine(line);
        m_processedLines++;
        
        int newProgress = (m_processedLines * 100) / m_totalLines;
        if (newProgress != m_progress) {
            m_progress = newProgress;
            emit progressChanged();
        }
    }
    
    if (!m_cancelRequested) {
        computeStatistics();
    }
    
    m_isAnalyzing = false;
    m_progress = 100;
    emit analyzingChanged();
    emit progressChanged();
    emit resultsChanged();
    emit analysisCompleted();
}

void DistinctValueAnalyzer::analyzeLineRange(int startLine, int endLine)
{
    Q_UNUSED(startLine)
    Q_UNUSED(endLine)
    // Would integrate with BigFileModel to get lines
    emit analysisError(tr("Line range analysis requires integration with BigFileModel"));
}

void DistinctValueAnalyzer::analyzeWithFilter(const QString &filterPattern)
{
    Q_UNUSED(filterPattern)
    // Would integrate with BigFileModel filtering
    emit analysisError(tr("Filtered analysis requires integration with BigFileModel"));
}

void DistinctValueAnalyzer::cancelAnalysis()
{
    m_cancelRequested = true;
}

QString DistinctValueAnalyzer::extractValue(const QString &line, const ColumnConfig &config) const
{
    if (config.type == ColumnConfig::Regex && !config.regexPattern.isEmpty()) {
        QRegularExpression regex(config.regexPattern);
        auto match = regex.match(line);
        if (match.hasMatch()) {
            return match.captured(config.captureGroup);
        }
    }
    
    // Fallback: return whole line
    return line;
}

void DistinctValueAnalyzer::processLine(const QString &line)
{
    for (const auto &col : m_columns) {
        QString value = extractValue(line, col);
        
        if (!value.isEmpty()) {
            m_valueCounters[col.name][value]++;
            
            // Track numeric values for statistics
            if (isNumericValue(value)) {
                m_numericValues[col.name].append(parseNumeric(value));
            }
        } else {
            // Track empty/null values
            m_valueCounters[col.name]["<empty>"]++;
        }
    }
}

void DistinctValueAnalyzer::computeStatistics()
{
    for (const auto &col : m_columns) {
        DistinctValueResult result;
        result.columnName = col.name;
        
        const auto &counter = m_valueCounters[col.name];
        const auto &numericVals = m_numericValues[col.name];
        
        result.distinctCount = counter.size();
        result.totalCount = 0;
        result.nullCount = 0;
        result.emptyCount = counter.value("<empty>", 0);
        
        // Compute total and find top/bottom values
        QList<QPair<QString, int>> sortedValues;
        for (auto it = counter.constBegin(); it != counter.constEnd(); ++it) {
            if (it.key() != "<empty>") {
                result.totalCount += it.value();
                sortedValues.append({it.key(), it.value()});
            }
        }
        result.totalCount += result.emptyCount;
        
        // Sort by count descending
        std::sort(sortedValues.begin(), sortedValues.end(),
            [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
                return a.second > b.second;
            });
        
        // Top 10 values
        for (int i = 0; i < qMin(10, sortedValues.size()); ++i) {
            result.topValues.append(sortedValues[i]);
        }
        
        // Bottom 10 values
        for (int i = qMax(0, sortedValues.size() - 10); i < sortedValues.size(); ++i) {
            result.bottomValues.append(sortedValues[i]);
        }
        
        if (!sortedValues.isEmpty()) {
            result.mostCommon = sortedValues.first().first;
            result.leastCommon = sortedValues.last().first;
        }
        
        result.uniquenessRatio = result.totalCount > 0 
            ? double(result.distinctCount) / result.totalCount 
            : 0.0;
        
        // Numeric statistics
        result.isNumeric = !numericVals.isEmpty() && numericVals.size() >= result.totalCount * 0.8;
        if (result.isNumeric && !numericVals.isEmpty()) {
            QVector<double> sorted = numericVals;
            std::sort(sorted.begin(), sorted.end());
            
            result.minValue = sorted.first();
            result.maxValue = sorted.last();
            
            double sum = 0;
            for (double v : numericVals) sum += v;
            result.avgValue = sum / numericVals.size();
            
            // Median
            int mid = sorted.size() / 2;
            result.median = (sorted.size() % 2 == 0) 
                ? (sorted[mid-1] + sorted[mid]) / 2.0 
                : sorted[mid];
            
            // Standard deviation
            double sqSum = 0;
            for (double v : numericVals) {
                sqSum += (v - result.avgValue) * (v - result.avgValue);
            }
            result.stdDev = std::sqrt(sqSum / numericVals.size());
        }
        
        // Check for datetime
        result.isDateTime = false;
        if (!sortedValues.isEmpty() && isDateTimeValue(sortedValues.first().first)) {
            result.isDateTime = true;
            QDateTime earliest, latest;
            for (const auto &pair : sortedValues) {
                QDateTime dt = parseDateTime(pair.first);
                if (dt.isValid()) {
                    if (!earliest.isValid() || dt < earliest) earliest = dt;
                    if (!latest.isValid() || dt > latest) latest = dt;
                }
            }
            result.earliestTime = earliest;
            result.latestTime = latest;
        }
        
        // Detect patterns
        detectPatterns(result, counter);
        
        m_results[col.name] = result;
    }
}

void DistinctValueAnalyzer::detectPatterns(DistinctValueResult &result, 
                                           const QHash<QString, int> &values)
{
    QHash<QString, int> patternCounts;
    
    for (auto it = values.constBegin(); it != values.constEnd(); ++it) {
        if (it.key() == "<empty>") continue;
        QString pattern = generatePattern(it.key());
        patternCounts[pattern] += it.value();
    }
    
    // Sort patterns by count
    QList<QPair<QString, int>> sortedPatterns;
    for (auto it = patternCounts.constBegin(); it != patternCounts.constEnd(); ++it) {
        sortedPatterns.append({it.key(), it.value()});
    }
    std::sort(sortedPatterns.begin(), sortedPatterns.end(),
        [](const QPair<QString, int> &a, const QPair<QString, int> &b) {
            return a.second > b.second;
        });
    
    // Keep top 5 patterns
    for (int i = 0; i < qMin(5, sortedPatterns.size()); ++i) {
        result.patterns.append(sortedPatterns[i]);
    }
}

QString DistinctValueAnalyzer::generatePattern(const QString &value) const
{
    QString pattern;
    for (const QChar &ch : value) {
        if (ch.isDigit()) {
            if (pattern.isEmpty() || pattern.back() != 'N') {
                pattern += 'N';
            }
        } else if (ch.isLetter()) {
            if (ch.isUpper()) {
                if (pattern.isEmpty() || pattern.back() != 'A') {
                    pattern += 'A';
                }
            } else {
                if (pattern.isEmpty() || pattern.back() != 'a') {
                    pattern += 'a';
                }
            }
        } else {
            pattern += ch;
        }
    }
    return pattern;
}

bool DistinctValueAnalyzer::isNumericValue(const QString &value) const
{
    bool ok;
    value.toDouble(&ok);
    return ok;
}

bool DistinctValueAnalyzer::isDateTimeValue(const QString &value) const
{
    // Check common datetime patterns
    static QList<QRegularExpression> datePatterns = {
        QRegularExpression(R"(\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2})"),
        QRegularExpression(R"(\d{2}/\d{2}/\d{4})"),
        QRegularExpression(R"(\d{2}-\w{3}-\d{4})"),
        QRegularExpression(R"(\w{3}\s+\d{1,2}\s+\d{2}:\d{2}:\d{2})")
    };
    
    for (const auto &pattern : datePatterns) {
        if (pattern.match(value).hasMatch()) return true;
    }
    return false;
}

double DistinctValueAnalyzer::parseNumeric(const QString &value) const
{
    return value.toDouble();
}

QDateTime DistinctValueAnalyzer::parseDateTime(const QString &value) const
{
    // Try common formats
    static QStringList formats = {
        "yyyy-MM-dd HH:mm:ss",
        "yyyy-MM-ddTHH:mm:ss",
        "dd/MM/yyyy HH:mm:ss",
        "MM/dd/yyyy HH:mm:ss",
        "dd-MMM-yyyy HH:mm:ss"
    };
    
    for (const QString &fmt : formats) {
        QDateTime dt = QDateTime::fromString(value, fmt);
        if (dt.isValid()) return dt;
    }
    
    return QDateTime::fromString(value, Qt::ISODate);
}

QVariantMap DistinctValueAnalyzer::getResult(const QString &columnName) const
{
    auto it = m_results.find(columnName);
    if (it == m_results.end()) return QVariantMap();
    
    const auto &r = it.value();
    QVariantMap result;
    result["columnName"] = r.columnName;
    result["totalCount"] = r.totalCount;
    result["distinctCount"] = r.distinctCount;
    result["uniquenessRatio"] = r.uniquenessRatio;
    result["mostCommon"] = r.mostCommon;
    result["leastCommon"] = r.leastCommon;
    result["nullCount"] = r.nullCount;
    result["emptyCount"] = r.emptyCount;
    result["isNumeric"] = r.isNumeric;
    result["isDateTime"] = r.isDateTime;
    
    if (r.isNumeric) {
        result["minValue"] = r.minValue;
        result["maxValue"] = r.maxValue;
        result["avgValue"] = r.avgValue;
        result["stdDev"] = r.stdDev;
        result["median"] = r.median;
    }
    
    if (r.isDateTime) {
        result["earliestTime"] = r.earliestTime;
        result["latestTime"] = r.latestTime;
    }
    
    return result;
}

QVariantList DistinctValueAnalyzer::getTopValues(const QString &columnName, int limit) const
{
    QVariantList result;
    auto it = m_results.find(columnName);
    if (it == m_results.end()) return result;
    
    for (int i = 0; i < qMin(limit, it.value().topValues.size()); ++i) {
        QVariantMap item;
        item["value"] = it.value().topValues[i].first;
        item["count"] = it.value().topValues[i].second;
        result.append(item);
    }
    return result;
}

QVariantList DistinctValueAnalyzer::getBottomValues(const QString &columnName, int limit) const
{
    QVariantList result;
    auto it = m_results.find(columnName);
    if (it == m_results.end()) return result;
    
    const auto &bottom = it.value().bottomValues;
    for (int i = qMax(0, bottom.size() - limit); i < bottom.size(); ++i) {
        QVariantMap item;
        item["value"] = bottom[i].first;
        item["count"] = bottom[i].second;
        result.append(item);
    }
    return result;
}

QVariantMap DistinctValueAnalyzer::getStatistics(const QString &columnName) const
{
    auto it = m_results.find(columnName);
    if (it == m_results.end() || !it.value().isNumeric) return QVariantMap();
    
    const auto &r = it.value();
    QVariantMap stats;
    stats["min"] = r.minValue;
    stats["max"] = r.maxValue;
    stats["avg"] = r.avgValue;
    stats["stdDev"] = r.stdDev;
    stats["median"] = r.median;
    return stats;
}

QVariantList DistinctValueAnalyzer::getPatterns(const QString &columnName) const
{
    QVariantList result;
    auto it = m_results.find(columnName);
    if (it == m_results.end()) return result;
    
    for (const auto &pair : it.value().patterns) {
        QVariantMap item;
        item["pattern"] = pair.first;
        item["count"] = pair.second;
        result.append(item);
    }
    return result;
}

QString DistinctValueAnalyzer::exportToCsv() const
{
    QString csv;
    QTextStream stream(&csv);
    
    stream << "Column,Total,Distinct,Uniqueness,MostCommon,LeastCommon,Empty\n";
    
    for (const auto &r : m_results) {
        stream << "\"" << r.columnName << "\","
               << r.totalCount << ","
               << r.distinctCount << ","
               << QString::number(r.uniquenessRatio, 'f', 4) << ","
               << "\"" << r.mostCommon << "\","
               << "\"" << r.leastCommon << "\","
               << r.emptyCount << "\n";
    }
    
    return csv;
}

QString DistinctValueAnalyzer::exportToJson() const
{
    QJsonObject root;
    QJsonArray columns;
    
    for (const auto &r : m_results) {
        QJsonObject col;
        col["name"] = r.columnName;
        col["totalCount"] = r.totalCount;
        col["distinctCount"] = r.distinctCount;
        col["uniquenessRatio"] = r.uniquenessRatio;
        col["mostCommon"] = r.mostCommon;
        col["leastCommon"] = r.leastCommon;
        col["emptyCount"] = r.emptyCount;
        
        if (r.isNumeric) {
            QJsonObject stats;
            stats["min"] = r.minValue;
            stats["max"] = r.maxValue;
            stats["avg"] = r.avgValue;
            stats["stdDev"] = r.stdDev;
            stats["median"] = r.median;
            col["numericStats"] = stats;
        }
        
        QJsonArray topArr;
        for (const auto &pair : r.topValues) {
            QJsonObject item;
            item["value"] = pair.first;
            item["count"] = pair.second;
            topArr.append(item);
        }
        col["topValues"] = topArr;
        
        columns.append(col);
    }
    
    root["columns"] = columns;
    root["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool DistinctValueAnalyzer::exportToFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    if (filePath.endsWith(".json", Qt::CaseInsensitive)) {
        stream << exportToJson();
    } else {
        stream << exportToCsv();
    }
    
    return true;
}

QVariantMap DistinctValueAnalyzer::analyzePattern(const QString &value) const
{
    QVariantMap result;
    result["value"] = value;
    result["pattern"] = generatePattern(value);
    result["isNumeric"] = isNumericValue(value);
    result["isDateTime"] = isDateTimeValue(value);
    result["length"] = value.length();
    return result;
}

QString DistinctValueAnalyzer::suggestType(const QStringList &sampleValues) const
{
    int numericCount = 0;
    int dateCount = 0;
    
    for (const QString &val : sampleValues) {
        if (isNumericValue(val)) numericCount++;
        if (isDateTimeValue(val)) dateCount++;
    }
    
    if (numericCount >= sampleValues.size() * 0.8) return "numeric";
    if (dateCount >= sampleValues.size() * 0.8) return "datetime";
    return "string";
}

QVariantList DistinctValueAnalyzer::resultsVariant() const
{
    QVariantList result;
    for (const QString &col : m_results.keys()) {
        result.append(getResult(col));
    }
    return result;
}

QVariantList DistinctValueAnalyzer::columnsVariant() const
{
    QVariantList result;
    for (const auto &col : m_columns) {
        QVariantMap item;
        item["name"] = col.name;
        item["type"] = static_cast<int>(col.type);
        item["regexPattern"] = col.regexPattern;
        result.append(item);
    }
    return result;
}
