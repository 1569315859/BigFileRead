/**
 * @file LogParser.cpp
 * @brief Log Line Parser Engine Implementation
 */

#include "LogParser.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QXmlStreamReader>
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
    
    // Try DSV parsing first if enabled
    if (m_dsvParsingEnabled) {
        QStringList dsvResult = parseDsvLine(rawLine);
        if (!dsvResult.isEmpty()) {
            return dsvResult;
        }
    }
    
    // Try Log4j XML first if enabled
    if (m_log4jXmlMode && rawLine.trimmed().contains(QLatin1String("<log4j:"))) {
        QStringList xmlResult = parseLog4jXmlLine(rawLine);
        if (!xmlResult.isEmpty()) {
            return xmlResult;
        }
    }
    
    // Try generic XML if enabled and line starts with '<'
    if (m_xmlParsingEnabled && rawLine.trimmed().startsWith(QLatin1Char('<'))) {
        QStringList xmlResult = parseXmlLine(rawLine);
        if (!xmlResult.isEmpty()) {
            return xmlResult;
        }
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
        
    case Preset::XmlGeneric:
        // Generic XML log format
        setXmlParsingEnabled(true);
        setLog4jXmlMode(false);
        setJsonParsingEnabled(false);
        setPattern(QString());
        setXmlElementPaths(QStringList{
            QStringLiteral("timestamp"),
            QStringLiteral("level"),
            QStringLiteral("thread"),
            QStringLiteral("logger"),
            QStringLiteral("message")
        });
        setXmlAttributePaths(QStringList{
            QStringLiteral("event@timestamp"),
            QStringLiteral("event@level")
        });
        setColumnHeaders(QStringList{
            QStringLiteral("Timestamp"),
            QStringLiteral("Level"),
            QStringLiteral("Thread"),
            QStringLiteral("Logger"),
            QStringLiteral("Message")
        });
        m_presetName = QStringLiteral("XML Generic");
        setLevelColumn(1);
        break;
        
    case Preset::Log4jXml:
        // Log4j XML layout format
        setXmlParsingEnabled(true);
        setLog4jXmlMode(true);
        setJsonParsingEnabled(false);
        setPattern(QString());
        setColumnHeaders(QStringList{
            QStringLiteral("Timestamp"),
            QStringLiteral("Level"),
            QStringLiteral("Logger"),
            QStringLiteral("Thread"),
            QStringLiteral("Message"),
            QStringLiteral("Throwable")
        });
        m_presetName = QStringLiteral("Log4j XML");
        setLevelColumn(1);
        break;
        
    case Preset::Dsv:
        // Delimiter-separated values (default: comma)
        setXmlParsingEnabled(false);
        setJsonParsingEnabled(false);
        setDsvParsingEnabled(true);
        // Set default CSV configuration
        {
            DsvConfig config;
            config.delimiter = QLatin1Char(',');
            config.quoteChar = QLatin1Char('"');
            config.escapeChar = QLatin1Char('\\');
            config.hasHeader = true;
            config.trimFields = true;
            setDsvConfig(config);
        }
        // DSV uses special parsing, pattern will be set by setDsvConfig()
        setPattern(QString());
        setColumnHeaders(QStringList{QStringLiteral("Content")});
        m_presetName = QStringLiteral("DSV");
        setLevelColumn(-1);
        break;
    }
    
    qDebug() << "[LogParser] Applied preset:" << m_presetName;
}

// ============================================================================
// XML Configuration
// ============================================================================

void LogParser::setXmlElementPaths(const QStringList &paths)
{
    m_xmlElementPaths = paths;
    clearCache();
}

void LogParser::setXmlAttributePaths(const QStringList &paths)
{
    m_xmlAttributePaths = paths;
    clearCache();
}

// ============================================================================
// XML Parsing
// ============================================================================

QStringList LogParser::parseXmlLine(const QString &rawLine) const
{
    QStringList result;
    QString trimmedLine = rawLine.trimmed();
    
    // Quick check if it looks like XML
    if (!trimmedLine.startsWith(QLatin1Char('<'))) {
        return result;
    }
    
    QXmlStreamReader xml(trimmedLine);
    QMap<QString, QString> elementValues;
    QMap<QString, QString> attributeValues;
    QString currentPath;
    QStringList pathStack;
    
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        
        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();
            pathStack.append(elementName);
            currentPath = pathStack.join(QLatin1Char('/'));
            
            // Extract attributes
            for (const QXmlStreamAttribute &attr : xml.attributes()) {
                QString attrPath = elementName + QLatin1Char('@') + attr.name().toString();
                QString fullAttrPath = currentPath + QLatin1Char('@') + attr.name().toString();
                attributeValues[attrPath] = attr.value().toString();
                attributeValues[fullAttrPath] = attr.value().toString();
            }
        }
        else if (xml.isCharacters() && !xml.isWhitespace()) {
            // Store element text content
            if (!currentPath.isEmpty()) {
                QString text = xml.text().toString().trimmed();
                if (!text.isEmpty()) {
                    elementValues[currentPath] = text;
                    // Also store just the element name for simpler lookup
                    if (!pathStack.isEmpty()) {
                        elementValues[pathStack.last()] = text;
                    }
                }
            }
        }
        else if (xml.isEndElement()) {
            if (!pathStack.isEmpty()) {
                pathStack.removeLast();
                currentPath = pathStack.join(QLatin1Char('/'));
            }
        }
    }
    
    if (xml.hasError()) {
        return result;  // Return empty on parse error
    }
    
    // Build result from configured element paths
    for (const QString &path : m_xmlElementPaths) {
        if (elementValues.contains(path)) {
            result.append(elementValues[path]);
        } else {
            result.append(QString());
        }
    }
    
    // Add attribute values
    for (const QString &path : m_xmlAttributePaths) {
        if (attributeValues.contains(path)) {
            result.append(attributeValues[path]);
        } else {
            result.append(QString());
        }
    }
    
    // If no paths configured, try to extract common fields
    if (result.isEmpty() && !elementValues.isEmpty()) {
        // Return all found values
        result = elementValues.values();
    }
    
    return result;
}

QStringList LogParser::parseLog4jXmlLine(const QString &rawLine) const
{
    QStringList result(6, QString());  // timestamp, level, logger, thread, message, throwable
    QString trimmedLine = rawLine.trimmed();
    
    // Quick check for Log4j XML format
    if (!trimmedLine.contains(QLatin1String("<log4j:event"))) {
        return QStringList();
    }
    
    // Ensure we have a complete event
    if (!trimmedLine.contains(QLatin1String("</log4j:event>"))) {
        return QStringList();
    }
    
    QXmlStreamReader xml(trimmedLine);
    
    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
        
        if (xml.isStartElement()) {
            QString elementName = xml.name().toString();
            
            if (elementName == QLatin1String("event")) {
                // Extract event attributes
                QXmlStreamAttributes attrs = xml.attributes();
                
                // timestamp (milliseconds since epoch)
                if (attrs.hasAttribute(QLatin1String("timestamp"))) {
                    qint64 timestamp = attrs.value(QLatin1String("timestamp")).toLongLong();
                    QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
                    result[0] = dt.toString(Qt::ISODateWithMs);
                }
                
                // level
                if (attrs.hasAttribute(QLatin1String("level"))) {
                    result[1] = attrs.value(QLatin1String("level")).toString();
                }
                
                // logger
                if (attrs.hasAttribute(QLatin1String("logger"))) {
                    result[2] = attrs.value(QLatin1String("logger")).toString();
                }
                
                // thread
                if (attrs.hasAttribute(QLatin1String("thread"))) {
                    result[3] = attrs.value(QLatin1String("thread")).toString();
                }
            }
            else if (elementName == QLatin1String("message")) {
                result[4] = xml.readElementText();
            }
            else if (elementName == QLatin1String("throwable")) {
                result[5] = xml.readElementText();
            }
        }
    }
    
    // Check if we got any useful data
    if (result[0].isEmpty() && result[1].isEmpty() && result[4].isEmpty()) {
        return QStringList();  // No useful data found
    }
    
    return result;
}

// ============================================================================
// DSV Parsing
// ============================================================================

void LogParser::setDsvConfig(const DsvConfig &config)
{
    m_dsvConfig = config;
    m_dsvLinesParsed = 0;  // Reset counter when config changes
    
    // If no column headers set and we have column count, generate default headers
    if (m_columnHeaders.isEmpty() && config.columnCount > 0) {
        m_columnHeaders.clear();
        for (int i = 0; i < config.columnCount; ++i) {
            m_columnHeaders.append(QString("Column%1").arg(i + 1));
        }
    }
}

QStringList LogParser::parseDsvLine(const QString &rawLine) const
{
    if (rawLine.isEmpty()) {
        return QStringList();
    }
    
    QStringList result;
    const QChar delimiter = m_dsvConfig.delimiter;
    const QChar quote = m_dsvConfig.quoteChar;
    const QChar escape = m_dsvConfig.escapeChar;
    const bool trim = m_dsvConfig.trimFields;
    
    // For first 1000 lines, use full RFC4180-compliant parsing
    // After that, use fast split for better performance
    const int FULL_PARSE_THRESHOLD = 1000;
    
    if (m_dsvLinesParsed < FULL_PARSE_THRESHOLD) {
        // Full RFC4180 parsing with proper quote handling
        QString field;
        bool inQuotes = false;
        bool fieldStarted = false;
        
        for (int i = 0; i < rawLine.length(); ++i) {
            QChar c = rawLine.at(i);
            
            if (!fieldStarted) {
                fieldStarted = true;
                if (c == quote) {
                    inQuotes = true;
                    continue;  // Don't add the opening quote
                }
            }
            
            if (inQuotes) {
                // Inside quoted field
                if (c == escape && i + 1 < rawLine.length()) {
                    // Escape sequence
                    QChar next = rawLine.at(i + 1);
                    if (next == quote || next == escape) {
                        field += next;
                        ++i;
                        continue;
                    }
                }
                
                if (c == quote) {
                    // Check for doubled quote (RFC4180 escape)
                    if (i + 1 < rawLine.length() && rawLine.at(i + 1) == quote) {
                        field += quote;
                        ++i;
                        continue;
                    }
                    // End of quoted field
                    inQuotes = false;
                    continue;
                }
                
                field += c;
            }
            else {
                // Outside quoted field
                if (c == delimiter) {
                    // End of field
                    if (trim) {
                        result.append(field.trimmed());
                    } else {
                        result.append(field);
                    }
                    field.clear();
                    fieldStarted = false;
                }
                else {
                    field += c;
                }
            }
        }
        
        // Add last field
        if (trim) {
            result.append(field.trimmed());
        } else {
            result.append(field);
        }
        
        ++m_dsvLinesParsed;
    }
    else {
        // Fast split for remaining lines (after structure is established)
        // This is much faster but doesn't handle complex quoting perfectly
        
        // Quick check if line contains quotes - if so, use full parsing
        if (rawLine.contains(quote)) {
            // Fall back to full parsing for lines with quotes
            QString field;
            bool inQuotes = false;
            
            for (int i = 0; i < rawLine.length(); ++i) {
                QChar c = rawLine.at(i);
                
                if (c == quote) {
                    inQuotes = !inQuotes;
                } else if (c == delimiter && !inQuotes) {
                    if (trim) {
                        result.append(field.trimmed());
                    } else {
                        result.append(field);
                    }
                    field.clear();
                } else {
                    field += c;
                }
            }
            
            if (trim) {
                result.append(field.trimmed());
            } else {
                result.append(field);
            }
        }
        else {
            // Simple fast split - no quotes present
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            result = rawLine.split(delimiter, Qt::KeepEmptyParts);
#else
            result = rawLine.split(delimiter, QString::KeepEmptyParts);
#endif
            if (trim) {
                for (int i = 0; i < result.size(); ++i) {
                    result[i] = result[i].trimmed();
                }
            }
        }
    }
    
    // Ensure consistent column count
    int expectedColumns = m_dsvConfig.columnCount > 0 ? m_dsvConfig.columnCount : m_columnHeaders.size();
    if (expectedColumns > 0) {
        while (result.size() < expectedColumns) {
            result.append(QString());
        }
        while (result.size() > expectedColumns) {
            result.removeLast();
        }
    }
    
    return result;
}

// ============================================================================
// Auto Format Detection
// ============================================================================

LogParser::FormatDetectionResult LogParser::detectFormat(const QString &sampleContent)
{
    FormatDetectionResult result;
    result.preset = Preset::Generic;
    result.confidence = 0;
    result.formatName = QStringLiteral("Generic");
    result.description = QStringLiteral("Auto-detect generic log format");
    
    if (sampleContent.isEmpty()) {
        return result;
    }
    
    // Take first 4KB or full content if smaller
    QString sample = sampleContent.left(4096);
    
    // Split into lines for analysis
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList lines = sample.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
#else
    QStringList lines = sample.split(QRegularExpression(QStringLiteral("[\r\n]+")), QString::SkipEmptyParts);
#endif
    
    if (lines.isEmpty()) {
        return result;
    }
    
    QString firstLine = lines.first().trimmed();
    
    // ========== 1. JSON Detection ==========
    if (firstLine.startsWith(QLatin1Char('{')) || firstLine.startsWith(QLatin1Char('['))) {
        // Count lines that look like JSON
        int jsonLines = 0;
        for (const QString &line : lines) {
            QString trimmed = line.trimmed();
            if (trimmed.startsWith(QLatin1Char('{')) && trimmed.endsWith(QLatin1Char('}'))) {
                jsonLines++;
            }
        }
        if (jsonLines > 0 && jsonLines >= lines.size() * 0.7) {
            result.preset = Preset::Custom;  // JSON uses custom parsing
            result.confidence = 90;
            result.formatName = QStringLiteral("JSON");
            result.description = QStringLiteral("JSON Lines format (JSONL)");
            return result;
        }
    }
    
    // ========== 2. XML Detection ==========
    if (firstLine.startsWith(QLatin1String("<?xml")) || 
        firstLine.startsWith(QLatin1String("<log4j:")) ||
        firstLine.startsWith(QLatin1String("<event")) ||
        firstLine.startsWith(QLatin1String("<log"))) {
        
        // Check for Log4j XML specifically
        if (sample.contains(QLatin1String("<log4j:event")) || 
            sample.contains(QLatin1String("log4j:configuration"))) {
            result.preset = Preset::Log4jXml;
            result.confidence = 95;
            result.formatName = QStringLiteral("Log4j XML");
            result.description = QStringLiteral("Log4j XML layout format");
            return result;
        }
        
        // Generic XML
        result.preset = Preset::XmlGeneric;
        result.confidence = 85;
        result.formatName = QStringLiteral("XML");
        result.description = QStringLiteral("Generic XML log format");
        return result;
    }
    
    // ========== 3. Syslog Detection ==========
    // RFC 5424: <priority>version timestamp hostname...
    static QRegularExpression syslogPattern(QStringLiteral(R"(^<\d{1,3}>(\d)?\s+\d{4}-\d{2}-\d{2})"));
    if (syslogPattern.match(firstLine).hasMatch()) {
        result.preset = Preset::Syslog;
        result.confidence = 90;
        result.formatName = QStringLiteral("Syslog");
        result.description = QStringLiteral("Syslog RFC 5424 format");
        return result;
    }
    
    // ========== 4. DSV Detection ==========
    // Analyze delimiter frequency and column consistency
    QMap<QChar, int> delimiterCounts;
    QList<QChar> possibleDelimiters = {QLatin1Char(','), QLatin1Char('\t'), QLatin1Char(';'), QLatin1Char('|'), QLatin1Char(':')};
    
    for (const QChar &delim : possibleDelimiters) {
        int totalCount = 0;
        int lineCount = 0;
        QList<int> columnCounts;
        
        for (const QString &line : lines) {
            if (line.trimmed().isEmpty()) continue;
            
            // Count delimiter occurrences (not inside quotes)
            int count = 0;
            bool inQuotes = false;
            for (int i = 0; i < line.length(); ++i) {
                if (line.at(i) == QLatin1Char('"')) {
                    inQuotes = !inQuotes;
                } else if (line.at(i) == delim && !inQuotes) {
                    count++;
                }
            }
            
            if (count > 0) {
                totalCount += count;
                lineCount++;
                columnCounts.append(count + 1);  // columns = delimiters + 1
            }
        }
        
        // Check column consistency
        if (!columnCounts.isEmpty() && lineCount > 0) {
            int firstColCount = columnCounts.first();
            int consistentLines = 0;
            for (int cols : columnCounts) {
                if (cols == firstColCount) {
                    consistentLines++;
                }
            }
            
            // If >70% lines have same column count and has reasonable delimiters
            double consistency = static_cast<double>(consistentLines) / columnCounts.size();
            double avgDelimiters = static_cast<double>(totalCount) / lineCount;
            
            if (consistency > 0.7 && avgDelimiters >= 2) {
                // Good candidate for DSV
                int score = static_cast<int>(consistency * 80 + (avgDelimiters > 5 ? 10 : 0));
                if (score > delimiterCounts.value(delim, 0)) {
                    delimiterCounts[delim] = score;
                }
            }
        }
    }
    
    // Find best delimiter
    QChar bestDelimiter = QLatin1Char(',');
    int bestScore = 0;
    for (auto it = delimiterCounts.begin(); it != delimiterCounts.end(); ++it) {
        if (it.value() > bestScore) {
            bestScore = it.value();
            bestDelimiter = it.key();
        }
    }
    
    if (bestScore >= 70) {
        result.preset = Preset::Dsv;
        result.confidence = bestScore;
        result.dsvDelimiter = bestDelimiter;
        
        // Detect if first line is header (non-numeric, unique values)
        QString headerLine = lines.first();
        bool looksLikeHeader = true;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        QStringList headerFields = headerLine.split(bestDelimiter, Qt::KeepEmptyParts);
#else
        QStringList headerFields = headerLine.split(bestDelimiter, QString::KeepEmptyParts);
#endif
        QSet<QString> uniqueFields;
        for (const QString &field : headerFields) {
            QString trimmed = field.trimmed();
            // Check if it looks like a numeric value
            bool isNumeric = false;
            trimmed.toDouble(&isNumeric);
            if (isNumeric && !trimmed.isEmpty()) {
                looksLikeHeader = false;
                break;
            }
            uniqueFields.insert(trimmed.toLower());
        }
        // All unique values suggest header
        result.hasHeader = looksLikeHeader && uniqueFields.size() == headerFields.size();
        
        QString delimName;
        if (bestDelimiter == QLatin1Char(',')) delimName = QStringLiteral("CSV");
        else if (bestDelimiter == QLatin1Char('\t')) delimName = QStringLiteral("TSV");
        else if (bestDelimiter == QLatin1Char(';')) delimName = QStringLiteral("Semicolon-separated");
        else if (bestDelimiter == QLatin1Char('|')) delimName = QStringLiteral("Pipe-separated");
        else delimName = QStringLiteral("Delimiter-separated");
        
        result.formatName = delimName;
        result.description = QString(QStringLiteral("%1 values (delimiter: '%2')"))
            .arg(delimName).arg(bestDelimiter == QLatin1Char('\t') ? QStringLiteral("TAB") : QString(bestDelimiter));
        return result;
    }
    
    // ========== 5. Known Log Format Detection ==========
    
    // Spring Boot: 2024-01-06T15:30:45.123+08:00  INFO 12345 --- [main] ...
    static QRegularExpression springBootPattern(
        QStringLiteral(R"(^\d{4}-\d{2}-\d{2}T[\d:\.]+[+\-\d:]+\s+\w+\s+\d+\s+---\s+\[)"));
    if (springBootPattern.match(firstLine).hasMatch()) {
        result.preset = Preset::SpringBoot;
        result.confidence = 95;
        result.formatName = QStringLiteral("Spring Boot");
        result.description = QStringLiteral("Spring Boot default log format");
        return result;
    }
    
    // Logback: 15:30:45.123 [main] INFO com.example - Message
    static QRegularExpression logbackPattern(
        QStringLiteral(R"(^[\d:\.]+\s+\[.*?\]\s+\w+\s+[\w\.]+\s+-\s+)"));
    if (logbackPattern.match(firstLine).hasMatch()) {
        result.preset = Preset::Logback;
        result.confidence = 85;
        result.formatName = QStringLiteral("Logback");
        result.description = QStringLiteral("Logback default pattern");
        return result;
    }
    
    // Log4j: 2024-01-06 15:30:45,123 [main] INFO MyClass - Message
    static QRegularExpression log4jPattern(
        QStringLiteral(R"(^\d{4}-\d{2}-\d{2}\s+[\d:,]+\s+\[.*?\]\s+\w+\s+[\w\.]+\s+-\s+)"));
    if (log4jPattern.match(firstLine).hasMatch()) {
        result.preset = Preset::Log4j;
        result.confidence = 85;
        result.formatName = QStringLiteral("Log4j");
        result.description = QStringLiteral("Log4j text format");
        return result;
    }
    
    // Apache Access Log
    static QRegularExpression apachePattern(
        QStringLiteral(R"(^\S+\s+\S+\s+\S+\s+\[.*?\]\s+".*?"\s+\d+\s+\d+\s+)"));
    if (apachePattern.match(firstLine).hasMatch()) {
        result.preset = Preset::ApacheAccess;
        result.confidence = 90;
        result.formatName = QStringLiteral("Apache Access");
        result.description = QStringLiteral("Apache Combined Log Format");
        return result;
    }
    
    // ========== 6. Generic Format ==========
    // Check if it looks like a timestamped log
    static QRegularExpression genericTimestamp(
        QStringLiteral(R"(^\d{4}[-\/]\d{2}[-\/]\d{2}[\sT][\d:\.]+)"));
    if (genericTimestamp.match(firstLine).hasMatch()) {
        result.preset = Preset::Generic;
        result.confidence = 60;
        result.formatName = QStringLiteral("Generic");
        result.description = QStringLiteral("Generic timestamped log");
        return result;
    }
    
    // Fallback
    result.preset = Preset::Custom;
    result.confidence = 20;
    result.formatName = QStringLiteral("Plain Text");
    result.description = QStringLiteral("Unstructured text");
    return result;
}

bool LogParser::autoDetectAndConfigure(const QString &sampleContent)
{
    FormatDetectionResult detection = detectFormat(sampleContent);
    
    if (detection.confidence < 30) {
        // Too low confidence, don't change config
        return false;
    }
    
    // Apply detected preset
    applyPreset(detection.preset);
    
    // Additional configuration for DSV
    if (detection.preset == Preset::Dsv) {
        DsvConfig config;
        config.delimiter = detection.dsvDelimiter;
        config.hasHeader = detection.hasHeader;
        config.quoteChar = QLatin1Char('"');
        config.escapeChar = QLatin1Char('\\');
        config.trimFields = true;
        setDsvConfig(config);
        setDsvParsingEnabled(true);
        
        // If has header, try to extract column names
        if (detection.hasHeader) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            QStringList lines = sampleContent.split(QRegularExpression(QStringLiteral("[\r\n]+")), Qt::SkipEmptyParts);
#else
            QStringList lines = sampleContent.split(QRegularExpression(QStringLiteral("[\r\n]+")), QString::SkipEmptyParts);
#endif
            if (!lines.isEmpty()) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                QStringList headers = lines.first().split(detection.dsvDelimiter, Qt::KeepEmptyParts);
#else
                QStringList headers = lines.first().split(detection.dsvDelimiter, QString::KeepEmptyParts);
#endif
                for (int i = 0; i < headers.size(); ++i) {
                    headers[i] = headers[i].trimmed();
                    // Remove quotes
                    if (headers[i].startsWith(QLatin1Char('"')) && headers[i].endsWith(QLatin1Char('"'))) {
                        headers[i] = headers[i].mid(1, headers[i].length() - 2);
                    }
                }
                setColumnHeaders(headers);
            }
        }
    }
    
    // Enable JSON parsing for JSON format
    if (detection.formatName == QLatin1String("JSON")) {
        setJsonParsingEnabled(true);
        setXmlParsingEnabled(false);
        setDsvParsingEnabled(false);
    }
    
    qDebug() << "[LogParser] Auto-detected format:" << detection.formatName 
             << "with confidence:" << detection.confidence << "%";
    
    return true;
}

// ===================== Multiline Configuration Implementation =====================

void LogParser::setMultilineConfig(const MultilineConfig &config)
{
    m_multilineConfig = config;
    
    // Compile the start pattern regex if provided
    if (!config.startPattern.isEmpty() && 
        (config.mode == MultilineMode::Regex || config.mode == MultilineMode::Both)) {
        m_multilineStartRegex = QRegularExpression(config.startPattern);
        if (!m_multilineStartRegex.isValid()) {
            qWarning() << "[LogParser] Invalid multiline start pattern:" << config.startPattern;
            m_multilineStartRegex = QRegularExpression();
        }
    } else {
        m_multilineStartRegex = QRegularExpression();
    }
}

void LogParser::setMultilineEnabled(bool enabled)
{
    m_multilineEnabled = enabled;
}

bool LogParser::isContinuationLine(const QString &line) const
{
    if (!m_multilineEnabled || line.isEmpty()) {
        return false;
    }
    
    switch (m_multilineConfig.mode) {
        case MultilineMode::None:
            return false;
            
        case MultilineMode::Indent: {
            // Check if line starts with whitespace (space or tab)
            if (line.isEmpty()) return false;
            QChar first = line.at(0);
            if (first == QLatin1Char(' ') || first == QLatin1Char('\t')) {
                // Count leading whitespace
                int indent = 0;
                for (int i = 0; i < line.length(); ++i) {
                    QChar ch = line.at(i);
                    if (ch == QLatin1Char(' ')) {
                        indent++;
                    } else if (ch == QLatin1Char('\t')) {
                        indent += 4;  // Tab = 4 spaces
                    } else {
                        break;
                    }
                }
                return indent >= m_multilineConfig.minIndent;
            }
            return false;
        }
            
        case MultilineMode::Regex: {
            // Continuation if line does NOT match start pattern
            if (!m_multilineStartRegex.isValid()) return false;
            return !m_multilineStartRegex.match(line).hasMatch();
        }
            
        case MultilineMode::Both: {
            // Must satisfy both: indented AND not matching start pattern
            // (Either condition alone doesn't make it a continuation in Both mode)
            // Actually, in "Both" mode, a line is a new entry if it matches EITHER:
            // - Starts without indent, OR
            // - Matches the start pattern
            // So continuation = has indent AND doesn't match start pattern
            
            // Check indent
            if (line.isEmpty()) return true;  // Empty lines are continuations
            QChar first = line.at(0);
            bool hasIndent = (first == QLatin1Char(' ') || first == QLatin1Char('\t'));
            
            // Check start pattern
            bool matchesStart = false;
            if (m_multilineStartRegex.isValid()) {
                matchesStart = m_multilineStartRegex.match(line).hasMatch();
            }
            
            // If it matches start pattern, it's a new entry (not continuation)
            if (matchesStart) return false;
            
            // If it has indent, it's a continuation
            if (hasIndent) return true;
            
            // No indent and no match - this is a heuristic, treat as continuation
            // (might be stack trace without indent)
            return false;
        }
    }
    
    return false;
}
