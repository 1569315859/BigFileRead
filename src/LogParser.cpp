/**
 * @file LogParser.cpp
 * @brief Log Line Parser Engine Implementation
 */

#include "LogParser.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QMutexLocker>

// ============================================================================
// Singleton
// ============================================================================

LogParser& LogParser::instance()
{
    static LogParser instance;
    return instance;
}

LogParser::LogParser()
    : m_cache(m_cacheSize)
{
    // Default JSON keys for common log formats
    m_jsonKeys = QStringList{
        QStringLiteral("timestamp"),
        QStringLiteral("level"),
        QStringLiteral("thread"),
        QStringLiteral("logger"),
        QStringLiteral("message")
    };
    
    // *** 应用通用格式作为默认配置 ***
    // 这样用户打开日志时默认就能看到结构化列
    applyPreset(Preset::Generic);
}

// ============================================================================
// Configuration
// ============================================================================

bool LogParser::setPattern(const QString &pattern)
{
    if (pattern.isEmpty()) {
        m_patternString.clear();
        m_regex = QRegularExpression();
        // Reset to single column mode
        if (m_columnHeaders.isEmpty()) {
            m_columnHeaders = QStringList{QStringLiteral("Content")};
        }
        clearCache();
        return true;
    }
    
    QRegularExpression testRegex(pattern);
    if (!testRegex.isValid()) {
        qWarning() << "[LogParser] Invalid regex pattern:" << testRegex.errorString();
        return false;
    }
    
    m_patternString = pattern;
    m_regex = testRegex;
    m_regex.optimize();  // Pre-compile for performance
    
    // Clear cache when pattern changes
    clearCache();
    
    qDebug() << "[LogParser] Pattern set:" << pattern 
             << "Capture groups:" << m_regex.captureCount();
    
    return true;
}

void LogParser::setColumnHeaders(const QStringList &headers)
{
    m_columnHeaders = headers;
    if (m_columnHeaders.isEmpty()) {
        m_columnHeaders = QStringList{QStringLiteral("Content")};
    }
    clearCache();
}

void LogParser::setJsonKeys(const QStringList &keys)
{
    m_jsonKeys = keys;
    clearCache();
}

void LogParser::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_cache.clear();
}

void LogParser::setCacheSize(int size)
{
    QMutexLocker locker(&m_cacheMutex);
    m_cacheSize = qMax(100, size);
    m_cache.setMaxCost(m_cacheSize);
}

// ============================================================================
// Parsing
// ============================================================================

QStringList LogParser::parseLine(const QString &rawLine) const
{
    if (rawLine.isEmpty()) {
        return QStringList(m_columnHeaders.size(), QString());
    }
    
    // Try JSON first if enabled and line starts with '{'
    if (m_jsonParsingEnabled && rawLine.trimmed().startsWith(QLatin1Char('{'))) {
        QStringList jsonResult = parseJsonLine(rawLine);
        if (!jsonResult.isEmpty()) {
            return jsonResult;
        }
        // Fall through to regex if JSON parsing fails
    }
    
    // Try regex parsing
    if (m_regex.isValid() && !m_patternString.isEmpty()) {
        return parseRegexLine(rawLine);
    }
    
    // No parser configured - return raw line as single column
    return QStringList{rawLine};
}

QStringList LogParser::parseLineWithCache(qint64 lineIndex, const QString &rawLine) const
{
    // Check cache first
    {
        QMutexLocker locker(&m_cacheMutex);
        if (QStringList *cached = m_cache.object(lineIndex)) {
            return *cached;
        }
    }
    
    // Parse the line
    QStringList result = parseLine(rawLine);
    
    // Store in cache
    {
        QMutexLocker locker(&m_cacheMutex);
        m_cache.insert(lineIndex, new QStringList(result), 1);
    }
    
    return result;
}

QString LogParser::getField(const QString &rawLine, int column) const
{
    if (column < 0) {
        return QString();
    }
    
    QStringList fields = parseLine(rawLine);
    
    if (column < fields.size()) {
        return fields.at(column);
    }
    
    return QString();
}

QString LogParser::getFieldWithCache(qint64 lineIndex, const QString &rawLine, int column) const
{
    if (column < 0) {
        return QString();
    }
    
    QStringList fields = parseLineWithCache(lineIndex, rawLine);
    
    if (column < fields.size()) {
        return fields.at(column);
    }
    
    return QString();
}

QStringList LogParser::parseJsonLine(const QString &rawLine) const
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(rawLine.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return QStringList();  // Not valid JSON
    }
    
    QJsonObject obj = doc.object();
    QStringList result;
    result.reserve(m_columnHeaders.size());
    
    // Extract values for each column header based on JSON keys
    for (int i = 0; i < m_columnHeaders.size(); ++i) {
        QString key = (i < m_jsonKeys.size()) ? m_jsonKeys.at(i) : m_columnHeaders.at(i);
        
        // Handle nested keys (e.g., "context.user")
        QStringList keyParts = key.split(QLatin1Char('.'));
        QJsonValue value = obj.value(keyParts.first());
        
        for (int j = 1; j < keyParts.size() && value.isObject(); ++j) {
            value = value.toObject().value(keyParts.at(j));
        }
        
        // Convert to string
        if (value.isString()) {
            result.append(value.toString());
        } else if (value.isDouble()) {
            result.append(QString::number(value.toDouble()));
        } else if (value.isBool()) {
            result.append(value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        } else if (value.isNull() || value.isUndefined()) {
            result.append(QString());
        } else {
            // For arrays/objects, convert back to JSON string
            result.append(QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact)));
        }
    }
    
    return result;
}

QStringList LogParser::parseRegexLine(const QString &rawLine) const
{
    QRegularExpressionMatch match = m_regex.match(rawLine);
    
    if (!match.hasMatch()) {
        // *** 关键修复：匹配失败时，将完整行内容放入最后一列（通常是 Message）***
        // 这样用户至少能看到数据，而不是空白的前面列
        QStringList result;
        result.reserve(m_columnHeaders.size());
        
        // 前面的列都为空
        for (int i = 0; i < m_columnHeaders.size() - 1; ++i) {
            result.append(QString());
        }
        // 最后一列放完整行内容（通常是 Message 列）
        result.append(rawLine);
        
        return result;
    }
    
    // Extract captured groups
    QStringList result;
    result.reserve(m_columnHeaders.size());
    
    // captured(0) is full match, captured(1+) are groups
    for (int i = 1; i <= m_regex.captureCount() && result.size() < m_columnHeaders.size(); ++i) {
        result.append(match.captured(i));
    }
    
    // Pad with empty strings if needed
    while (result.size() < m_columnHeaders.size()) {
        result.append(QString());
    }
    
    return result;
}

// ============================================================================
// Log Level Detection
// ============================================================================

LogParser::LogLevel LogParser::detectLevel(const QString &rawLine) const
{
    // Fast detection using string contains (case-insensitive)
    QString upper = rawLine.toUpper();
    
    // Check for common level strings
    if (upper.contains(QLatin1String("FATAL")) || upper.contains(QLatin1String("CRITICAL"))) {
        return LogLevel::Fatal;
    }
    if (upper.contains(QLatin1String("ERROR")) || upper.contains(QLatin1String(" ERR "))) {
        return LogLevel::Error;
    }
    if (upper.contains(QLatin1String("WARN")) || upper.contains(QLatin1String("WARNING"))) {
        return LogLevel::Warn;
    }
    if (upper.contains(QLatin1String("INFO")) || upper.contains(QLatin1String(" INF "))) {
        return LogLevel::Info;
    }
    if (upper.contains(QLatin1String("DEBUG")) || upper.contains(QLatin1String(" DBG "))) {
        return LogLevel::Debug;
    }
    if (upper.contains(QLatin1String("TRACE")) || upper.contains(QLatin1String("VERBOSE"))) {
        return LogLevel::Trace;
    }
    
    return LogLevel::Unknown;
}

QString LogParser::levelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return QStringLiteral("TRACE");
    case LogLevel::Debug: return QStringLiteral("DEBUG");
    case LogLevel::Info:  return QStringLiteral("INFO");
    case LogLevel::Warn:  return QStringLiteral("WARN");
    case LogLevel::Error: return QStringLiteral("ERROR");
    case LogLevel::Fatal: return QStringLiteral("FATAL");
    default:              return QStringLiteral("UNKNOWN");
    }
}

LogParser::LogLevel LogParser::stringToLevel(const QString &str)
{
    QString upper = str.toUpper().trimmed();
    
    if (upper == QLatin1String("TRACE") || upper == QLatin1String("VERBOSE")) return LogLevel::Trace;
    if (upper == QLatin1String("DEBUG") || upper == QLatin1String("DBG")) return LogLevel::Debug;
    if (upper == QLatin1String("INFO") || upper == QLatin1String("INF")) return LogLevel::Info;
    if (upper == QLatin1String("WARN") || upper == QLatin1String("WARNING")) return LogLevel::Warn;
    if (upper == QLatin1String("ERROR") || upper == QLatin1String("ERR")) return LogLevel::Error;
    if (upper == QLatin1String("FATAL") || upper == QLatin1String("CRITICAL")) return LogLevel::Fatal;
    
    return LogLevel::Unknown;
}

// ============================================================================
// Presets
// ============================================================================

void LogParser::applyPreset(Preset preset)
{
    switch (preset) {
    case Preset::Generic:
        // *** 通用格式：匹配大多数常见日志 ***
        // 支持格式示例:
        // - 2024-01-06 15:30:45 INFO  Some message here
        // - [2024-01-06 15:30:45] [INFO] message
        // - 2024-01-06T15:30:45.123 DEBUG: message
        // - INFO | 2024-01-06 | message
        setPattern(QStringLiteral(
            R"(^[\[\s]*(\d{4}[-\/]\d{2}[-\/]\d{2}[\sT][\d:.,]+)[\]\s]*[\[\s]*(TRACE|DEBUG|INFO|WARN(?:ING)?|ERROR|FATAL|CRITICAL)[\]\s:|\-]*(.*)$)"
        ));
        setColumnHeaders(QStringList{
            QStringLiteral("Timestamp"),
            QStringLiteral("Level"),
            QStringLiteral("Message")
        });
        m_presetName = QStringLiteral("Generic");
        setLevelColumn(1);
        break;
        
    case Preset::SpringBoot:
        // Spring Boot default format:
        // 2024-01-06T15:30:45.123+08:00  INFO 12345 --- [main] c.example.MyClass : Message here
        setPattern(QStringLiteral(R"(^(\d{4}-\d{2}-\d{2}T[\d:\.]+[+\-\d:]+)\s+(\w+)\s+(\d+)\s+---\s+\[(.*?)\]\s+([\w\.]+)\s*:\s*(.*)$)"));
        setColumnHeaders(QStringList{
            QStringLiteral("Timestamp"),
            QStringLiteral("Level"),
            QStringLiteral("PID"),
            QStringLiteral("Thread"),
            QStringLiteral("Logger"),
            QStringLiteral("Message")
        });
        m_presetName = QStringLiteral("Spring Boot");
        setLevelColumn(1);  // Level is column 1
        break;
        
    case Preset::Logback:
        // Logback default: %d{HH:mm:ss.SSS} [%thread] %-5level %logger{36} - %msg%n
        setPattern(QStringLiteral(R"(^([\d:\.]+)\s+\[(.*?)\]\s+(\w+)\s+([\w\.]+)\s+-\s+(.*)$)"));
        setColumnHeaders(QStringList{
            QStringLiteral("Time"),
            QStringLiteral("Thread"),
            QStringLiteral("Level"),
            QStringLiteral("Logger"),
            QStringLiteral("Message")
        });
        m_presetName = QStringLiteral("Logback");
        setLevelColumn(2);
        break;
        
    case Preset::Log4j:
        // Log4j: 2024-01-06 15:30:45,123 [main] INFO  MyClass - Message
        setPattern(QStringLiteral(R"(^(\d{4}-\d{2}-\d{2}\s+[\d:,]+)\s+\[(.*?)\]\s+(\w+)\s+([\w\.]+)\s+-\s+(.*)$)"));
        setColumnHeaders(QStringList{
            QStringLiteral("Timestamp"),
            QStringLiteral("Thread"),
            QStringLiteral("Level"),
            QStringLiteral("Logger"),
            QStringLiteral("Message")
        });
        m_presetName = QStringLiteral("Log4j");
        setLevelColumn(2);
        break;
        
    case Preset::Syslog:
        // Syslog RFC 5424: <priority>version timestamp hostname app-name procid msgid msg
        setPattern(QStringLiteral(R"(^<(\d+)>(\d*)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(.*)$)"));
        setColumnHeaders(QStringList{
            QStringLiteral("Priority"),
            QStringLiteral("Version"),
            QStringLiteral("Timestamp"),
            QStringLiteral("Hostname"),
            QStringLiteral("App"),
            QStringLiteral("ProcID"),
            QStringLiteral("MsgID"),
            QStringLiteral("Message")
        });
        m_presetName = QStringLiteral("Syslog");
        setLevelColumn(-1);  // Need to derive from priority
        break;
        
    case Preset::ApacheAccess:
        // Apache Combined Log Format
        setPattern(QStringLiteral(R"(^(\S+)\s+(\S+)\s+(\S+)\s+\[(.*?)\]\s+\"(.*?)\"\s+(\d+)\s+(\d+)\s+\"(.*?)\"\s+\"(.*?)\"$)"));
        setColumnHeaders(QStringList{
            QStringLiteral("IP"),
            QStringLiteral("Ident"),
            QStringLiteral("User"),
            QStringLiteral("Time"),
            QStringLiteral("Request"),
            QStringLiteral("Status"),
            QStringLiteral("Size"),
            QStringLiteral("Referer"),
            QStringLiteral("UserAgent")
        });
        m_presetName = QStringLiteral("Apache Access");
        setLevelColumn(-1);
        break;
        
    case Preset::Custom:
    default:
        // Clear pattern, single column mode
        setPattern(QString());
        setColumnHeaders(QStringList{QStringLiteral("Content")});
        m_presetName = QStringLiteral("Custom");
        setLevelColumn(-1);
        break;
    }
    
    qDebug() << "[LogParser] Applied preset:" << m_presetName;
}
