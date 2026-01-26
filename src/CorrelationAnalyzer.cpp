/**
 * @file CorrelationAnalyzer.cpp
 * @brief Log Correlation Analysis Implementation
 */

#include "CorrelationAnalyzer.h"
#include "BigFileModel.h"

#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <algorithm>

CorrelationAnalyzer& CorrelationAnalyzer::instance()
{
    static CorrelationAnalyzer instance;
    return instance;
}

CorrelationAnalyzer::CorrelationAnalyzer(QObject* parent)
    : QObject(parent)
    , m_isAnalyzing(false)
    , m_cancelRequested(false)
{
}

void CorrelationAnalyzer::setAnalyzing(bool analyzing)
{
    if (m_isAnalyzing != analyzing) {
        m_isAnalyzing = analyzing;
        emit analyzingChanged();
    }
}

void CorrelationAnalyzer::setError(const QString& error)
{
    m_lastError = error;
    emit errorOccurred(error);
}

void CorrelationAnalyzer::addSource(const QString& name, BigFileModel* model)
{
    if (!model) {
        setError(tr("Invalid model"));
        return;
    }
    
    m_sources[name] = model;
    emit sourceAdded(name);
}

void CorrelationAnalyzer::removeSource(const QString& name)
{
    if (m_sources.remove(name)) {
        emit sourceRemoved(name);
    }
}

void CorrelationAnalyzer::clearSources()
{
    m_sources.clear();
}

QStringList CorrelationAnalyzer::getSources() const
{
    return m_sources.keys();
}

QDateTime CorrelationAnalyzer::parseTimestamp(const QString& line)
{
    // Common timestamp patterns
    static QList<QPair<QRegularExpression, QString>> patterns = {
        // ISO 8601: 2024-01-15T10:30:45.123Z
        {QRegularExpression(R"((\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:?\d{2})?))"), 
         "yyyy-MM-ddTHH:mm:ss.zzz"},
        // Standard: 2024-01-15 10:30:45.123
        {QRegularExpression(R"((\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}(?:\.\d+)?))"), 
         "yyyy-MM-dd HH:mm:ss.zzz"},
        // Log4j: 15 Jan 2024 10:30:45,123
        {QRegularExpression(R"((\d{2} \w{3} \d{4} \d{2}:\d{2}:\d{2},\d+))"), 
         "dd MMM yyyy HH:mm:ss,zzz"},
        // Unix timestamp: [1705315845.123]
        {QRegularExpression(R"(\[(\d{10}(?:\.\d+)?)\])"), "unix"},
    };
    
    for (const auto& [regex, format] : patterns) {
        auto match = regex.match(line);
        if (match.hasMatch()) {
            QString captured = match.captured(1);
            
            if (format == "unix") {
                // Unix timestamp
                double unixTime = captured.toDouble();
                return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(unixTime * 1000));
            } else {
                // Try parsing with format
                QDateTime dt = QDateTime::fromString(captured, Qt::ISODateWithMs);
                if (dt.isValid()) return dt;
                
                dt = QDateTime::fromString(captured, "yyyy-MM-dd HH:mm:ss.zzz");
                if (dt.isValid()) return dt;
                
                dt = QDateTime::fromString(captured, "yyyy-MM-dd HH:mm:ss");
                if (dt.isValid()) return dt;
            }
        }
    }
    
    return QDateTime();
}

QString CorrelationAnalyzer::extractCorrelationId(const QString& line, const QRegularExpression& pattern)
{
    auto match = pattern.match(line);
    if (match.hasMatch()) {
        return match.captured(1).isEmpty() ? match.captured(0) : match.captured(1);
    }
    return QString();
}

QList<QPair<QString, QRegularExpression>> CorrelationAnalyzer::getCommonIdPatterns()
{
    return {
        // UUID
        {"uuid", QRegularExpression(R"(([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}))")},
        // Request ID patterns
        {"requestId", QRegularExpression(R"((?:request[_-]?id|req[_-]?id|x-request-id)[=:\s]+([^\s,\]]+))", QRegularExpression::CaseInsensitiveOption)},
        // Transaction ID
        {"transactionId", QRegularExpression(R"((?:transaction[_-]?id|txn[_-]?id|tx[_-]?id)[=:\s]+([^\s,\]]+))", QRegularExpression::CaseInsensitiveOption)},
        // Session ID
        {"sessionId", QRegularExpression(R"((?:session[_-]?id|sess[_-]?id)[=:\s]+([^\s,\]]+))", QRegularExpression::CaseInsensitiveOption)},
        // Trace ID (distributed tracing)
        {"traceId", QRegularExpression(R"((?:trace[_-]?id|traceid)[=:\s]+([^\s,\]]+))", QRegularExpression::CaseInsensitiveOption)},
        // Span ID
        {"spanId", QRegularExpression(R"((?:span[_-]?id|spanid)[=:\s]+([^\s,\]]+))", QRegularExpression::CaseInsensitiveOption)},
        // Correlation ID
        {"correlationId", QRegularExpression(R"((?:correlation[_-]?id|corr[_-]?id)[=:\s]+([^\s,\]]+))", QRegularExpression::CaseInsensitiveOption)},
    };
}

void CorrelationAnalyzer::analyzeByTimeWindow(int windowSeconds, 
                                               const QString& anchorFile,
                                               int anchorLine)
{
    if (m_sources.isEmpty()) {
        setError(tr("No sources added for analysis"));
        return;
    }
    
    setAnalyzing(true);
    m_cancelRequested = false;
    m_correlationGroups.clear();
    m_correlationIdToGroupIndex.clear();
    
    // Collect all events with timestamps
    QList<CorrelatedEvent> allEvents;
    
    for (auto it = m_sources.begin(); it != m_sources.end(); ++it) {
        QString sourceName = it.key();
        BigFileModel* model = it.value();
        
        int totalLines = model->totalLineCount();
        for (int i = 0; i < totalLines && !m_cancelRequested; ++i) {
            QString line = model->getRawLine(i);
            QDateTime timestamp = parseTimestamp(line);
            
            if (timestamp.isValid()) {
                CorrelatedEvent evt;
                evt.sourceFile = sourceName;
                evt.lineNumber = i + 1;
                evt.timestamp = timestamp;
                evt.message = line;
                
                // Extract level
                static QRegularExpression levelRegex(R"(\b(TRACE|DEBUG|INFO|WARN(?:ING)?|ERROR|FATAL|CRITICAL)\b)", 
                                                      QRegularExpression::CaseInsensitiveOption);
                auto levelMatch = levelRegex.match(line);
                if (levelMatch.hasMatch()) {
                    evt.level = levelMatch.captured(1).toUpper();
                }
                
                allEvents.append(evt);
            }
            
            if (i % 10000 == 0) {
                emit analysisProgress(i, totalLines);
                QCoreApplication::processEvents();
            }
        }
    }
    
    if (m_cancelRequested) {
        setAnalyzing(false);
        return;
    }
    
    // Sort by timestamp
    std::sort(allEvents.begin(), allEvents.end(), 
              [](const CorrelatedEvent& a, const CorrelatedEvent& b) {
                  return a.timestamp < b.timestamp;
              });
    
    // If anchor point specified, find events within window
    if (!anchorFile.isEmpty() && anchorLine >= 0) {
        // Find anchor event
        CorrelatedEvent* anchorEvent = nullptr;
        for (auto& evt : allEvents) {
            if (evt.sourceFile == anchorFile && evt.lineNumber == anchorLine) {
                anchorEvent = &evt;
                break;
            }
        }
        
        if (anchorEvent) {
            CorrelationGroup group;
            group.correlationId = QString("time_%1").arg(anchorEvent->timestamp.toString(Qt::ISODate));
            group.correlationType = "time";
            group.startTime = anchorEvent->timestamp.addSecs(-windowSeconds);
            group.endTime = anchorEvent->timestamp.addSecs(windowSeconds);
            
            for (const auto& evt : allEvents) {
                if (evt.timestamp >= group.startTime && evt.timestamp <= group.endTime) {
                    CorrelatedEvent eventCopy = evt;
                    eventCopy.correlationId = group.correlationId;
                    group.events.append(eventCopy);
                }
            }
            
            group.durationMs = group.startTime.msecsTo(group.endTime);
            m_correlationGroups.append(group);
            m_correlationIdToGroupIndex[group.correlationId] = 0;
        }
    } else {
        // Create time-based clusters
        // Group events that are within windowSeconds of each other
        int groupId = 0;
        QSet<int> processed;
        
        for (int i = 0; i < allEvents.size() && !m_cancelRequested; ++i) {
            if (processed.contains(i)) continue;
            
            CorrelationGroup group;
            group.correlationId = QString("time_group_%1").arg(groupId++);
            group.correlationType = "time";
            group.startTime = allEvents[i].timestamp;
            
            QDateTime windowEnd = allEvents[i].timestamp.addSecs(windowSeconds);
            
            for (int j = i; j < allEvents.size(); ++j) {
                if (allEvents[j].timestamp <= windowEnd) {
                    CorrelatedEvent eventCopy = allEvents[j];
                    eventCopy.correlationId = group.correlationId;
                    group.events.append(eventCopy);
                    processed.insert(j);
                    
                    // Extend window
                    windowEnd = allEvents[j].timestamp.addSecs(windowSeconds);
                    group.endTime = allEvents[j].timestamp;
                } else {
                    break;
                }
            }
            
            if (group.events.size() > 1) {  // Only keep groups with multiple events
                group.durationMs = group.startTime.msecsTo(group.endTime);
                m_correlationIdToGroupIndex[group.correlationId] = m_correlationGroups.size();
                m_correlationGroups.append(group);
            }
        }
    }
    
    setAnalyzing(false);
    emit analysisCompleted(m_correlationGroups.size());
}

void CorrelationAnalyzer::analyzeByIdPattern(const QString& idPattern, const QString& idName)
{
    if (m_sources.isEmpty()) {
        setError(tr("No sources added for analysis"));
        return;
    }
    
    QRegularExpression regex(idPattern);
    if (!regex.isValid()) {
        setError(tr("Invalid regex pattern: %1").arg(regex.errorString()));
        return;
    }
    
    setAnalyzing(true);
    m_cancelRequested = false;
    m_correlationGroups.clear();
    m_correlationIdToGroupIndex.clear();
    
    // Group events by correlation ID
    QMap<QString, QList<CorrelatedEvent>> groupedEvents;
    
    for (auto it = m_sources.begin(); it != m_sources.end(); ++it) {
        QString sourceName = it.key();
        BigFileModel* model = it.value();
        
        int totalLines = model->totalLineCount();
        for (int i = 0; i < totalLines && !m_cancelRequested; ++i) {
            QString line = model->getRawLine(i);
            QString correlationId = extractCorrelationId(line, regex);
            
            if (!correlationId.isEmpty()) {
                CorrelatedEvent evt;
                evt.sourceFile = sourceName;
                evt.lineNumber = i + 1;
                evt.timestamp = parseTimestamp(line);
                evt.message = line;
                evt.correlationId = correlationId;
                
                // Extract level
                static QRegularExpression levelRegex(R"(\b(TRACE|DEBUG|INFO|WARN(?:ING)?|ERROR|FATAL|CRITICAL)\b)", 
                                                      QRegularExpression::CaseInsensitiveOption);
                auto levelMatch = levelRegex.match(line);
                if (levelMatch.hasMatch()) {
                    evt.level = levelMatch.captured(1).toUpper();
                }
                
                groupedEvents[correlationId].append(evt);
            }
            
            if (i % 10000 == 0) {
                emit analysisProgress(i, totalLines);
                QCoreApplication::processEvents();
            }
        }
    }
    
    if (m_cancelRequested) {
        setAnalyzing(false);
        return;
    }
    
    // Convert to correlation groups
    for (auto it = groupedEvents.begin(); it != groupedEvents.end(); ++it) {
        if (it.value().size() > 1) {  // Only groups with multiple events
            CorrelationGroup group;
            group.correlationId = it.key();
            group.correlationType = idName;
            group.events = it.value();
            
            // Sort events by timestamp
            std::sort(group.events.begin(), group.events.end(),
                      [](const CorrelatedEvent& a, const CorrelatedEvent& b) {
                          return a.timestamp < b.timestamp;
                      });
            
            if (!group.events.isEmpty()) {
                group.startTime = group.events.first().timestamp;
                group.endTime = group.events.last().timestamp;
                group.durationMs = group.startTime.msecsTo(group.endTime);
            }
            
            m_correlationIdToGroupIndex[group.correlationId] = m_correlationGroups.size();
            m_correlationGroups.append(group);
        }
    }
    
    // Sort groups by start time
    std::sort(m_correlationGroups.begin(), m_correlationGroups.end(),
              [](const CorrelationGroup& a, const CorrelationGroup& b) {
                  return a.startTime < b.startTime;
              });
    
    // Update index map
    m_correlationIdToGroupIndex.clear();
    for (int i = 0; i < m_correlationGroups.size(); ++i) {
        m_correlationIdToGroupIndex[m_correlationGroups[i].correlationId] = i;
    }
    
    setAnalyzing(false);
    emit analysisCompleted(m_correlationGroups.size());
}

void CorrelationAnalyzer::analyzeWithAutoDetection()
{
    if (m_sources.isEmpty()) {
        setError(tr("No sources added for analysis"));
        return;
    }
    
    setAnalyzing(true);
    m_cancelRequested = false;
    m_correlationGroups.clear();
    m_correlationIdToGroupIndex.clear();
    
    auto patterns = getCommonIdPatterns();
    QMap<QString, QList<CorrelatedEvent>> groupedEvents;
    
    for (auto it = m_sources.begin(); it != m_sources.end(); ++it) {
        QString sourceName = it.key();
        BigFileModel* model = it.value();
        
        int totalLines = model->totalLineCount();
        for (int i = 0; i < totalLines && !m_cancelRequested; ++i) {
            QString line = model->getRawLine(i);
            
            // Try each pattern
            for (const auto& [patternName, regex] : patterns) {
                QString correlationId = extractCorrelationId(line, regex);
                
                if (!correlationId.isEmpty()) {
                    CorrelatedEvent evt;
                    evt.sourceFile = sourceName;
                    evt.lineNumber = i + 1;
                    evt.timestamp = parseTimestamp(line);
                    evt.message = line;
                    evt.correlationId = QString("%1:%2").arg(patternName, correlationId);
                    
                    // Extract level
                    static QRegularExpression levelRegex(R"(\b(TRACE|DEBUG|INFO|WARN(?:ING)?|ERROR|FATAL|CRITICAL)\b)", 
                                                          QRegularExpression::CaseInsensitiveOption);
                    auto levelMatch = levelRegex.match(line);
                    if (levelMatch.hasMatch()) {
                        evt.level = levelMatch.captured(1).toUpper();
                    }
                    
                    groupedEvents[evt.correlationId].append(evt);
                }
            }
            
            if (i % 10000 == 0) {
                emit analysisProgress(i, totalLines);
                QCoreApplication::processEvents();
            }
        }
    }
    
    if (m_cancelRequested) {
        setAnalyzing(false);
        return;
    }
    
    // Convert to correlation groups (only those with multiple events)
    for (auto it = groupedEvents.begin(); it != groupedEvents.end(); ++it) {
        if (it.value().size() > 1) {
            CorrelationGroup group;
            group.correlationId = it.key();
            
            // Extract correlation type from the prefixed ID
            int colonPos = it.key().indexOf(':');
            group.correlationType = colonPos > 0 ? it.key().left(colonPos) : "auto";
            
            group.events = it.value();
            
            std::sort(group.events.begin(), group.events.end(),
                      [](const CorrelatedEvent& a, const CorrelatedEvent& b) {
                          return a.timestamp < b.timestamp;
                      });
            
            if (!group.events.isEmpty()) {
                group.startTime = group.events.first().timestamp;
                group.endTime = group.events.last().timestamp;
                group.durationMs = group.startTime.msecsTo(group.endTime);
            }
            
            m_correlationIdToGroupIndex[group.correlationId] = m_correlationGroups.size();
            m_correlationGroups.append(group);
        }
    }
    
    setAnalyzing(false);
    emit analysisCompleted(m_correlationGroups.size());
}

void CorrelationAnalyzer::cancelAnalysis()
{
    m_cancelRequested = true;
}

QVariantList CorrelationAnalyzer::getCorrelationGroups()
{
    QVariantList result;
    for (const auto& group : m_correlationGroups) {
        result.append(group.toVariantMap());
    }
    return result;
}

QVariantMap CorrelationAnalyzer::getCorrelationGroup(const QString& correlationId)
{
    auto it = m_correlationIdToGroupIndex.find(correlationId);
    if (it != m_correlationIdToGroupIndex.end()) {
        return m_correlationGroups[it.value()].toVariantMap();
    }
    return QVariantMap();
}

QVariantList CorrelationAnalyzer::getCorrelatedEvents(const QString& correlationId)
{
    QVariantList result;
    auto it = m_correlationIdToGroupIndex.find(correlationId);
    if (it != m_correlationIdToGroupIndex.end()) {
        for (const auto& evt : m_correlationGroups[it.value()].events) {
            result.append(evt.toVariantMap());
        }
    }
    return result;
}

QVariantList CorrelationAnalyzer::getTimelineData()
{
    QVariantList result;
    
    // Collect all events from all groups
    QList<CorrelatedEvent> allEvents;
    for (const auto& group : m_correlationGroups) {
        allEvents.append(group.events);
    }
    
    // Sort by timestamp
    std::sort(allEvents.begin(), allEvents.end(),
              [](const CorrelatedEvent& a, const CorrelatedEvent& b) {
                  return a.timestamp < b.timestamp;
              });
    
    for (const auto& evt : allEvents) {
        result.append(evt.toVariantMap());
    }
    
    return result;
}

QVariantList CorrelationAnalyzer::getTimelineDataInRange(const QDateTime& start, 
                                                          const QDateTime& end)
{
    QVariantList result;
    
    for (const auto& group : m_correlationGroups) {
        for (const auto& evt : group.events) {
            if (evt.timestamp >= start && evt.timestamp <= end) {
                result.append(evt.toVariantMap());
            }
        }
    }
    
    // Sort by timestamp
    std::sort(result.begin(), result.end(),
              [](const QVariant& a, const QVariant& b) {
                  return a.toMap()["timestamp"].toDateTime() < b.toMap()["timestamp"].toDateTime();
              });
    
    return result;
}

void CorrelationAnalyzer::exportResults(const QString& filePath, const QString& format)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(tr("Cannot open file for writing: %1").arg(filePath));
        return;
    }
    
    QTextStream out(&file);
    
    if (format.toLower() == "json") {
        QJsonArray groups;
        for (const auto& group : m_correlationGroups) {
            groups.append(QJsonObject::fromVariantMap(group.toVariantMap()));
        }
        QJsonDocument doc(groups);
        out << doc.toJson(QJsonDocument::Indented);
    }
    else if (format.toLower() == "csv") {
        // Header
        out << "CorrelationId,Type,SourceFile,LineNumber,Timestamp,Level,Message\n";
        
        for (const auto& group : m_correlationGroups) {
            for (const auto& evt : group.events) {
                QString escapedMessage = evt.message;
                escapedMessage.replace("\"", "\"\"");
                out << "\"" << group.correlationId << "\","
                    << "\"" << group.correlationType << "\","
                    << "\"" << evt.sourceFile << "\","
                    << evt.lineNumber << ","
                    << "\"" << evt.timestamp.toString(Qt::ISODate) << "\","
                    << "\"" << evt.level << "\","
                    << "\"" << escapedMessage << "\"\n";
            }
        }
    }
    else if (format.toLower() == "html") {
        out << "<!DOCTYPE html>\n<html>\n<head>\n<style>\n";
        out << "body { font-family: sans-serif; }\n";
        out << ".group { margin: 20px 0; border: 1px solid #ddd; padding: 10px; }\n";
        out << ".group-header { font-weight: bold; background: #f5f5f5; padding: 5px; }\n";
        out << "table { border-collapse: collapse; width: 100%; margin-top: 10px; }\n";
        out << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
        out << "th { background: #4CAF50; color: white; }\n";
        out << ".ERROR { color: red; font-weight: bold; }\n";
        out << ".WARN, .WARNING { color: orange; }\n";
        out << "</style>\n</head>\n<body>\n";
        out << "<h1>Log Correlation Analysis</h1>\n";
        
        for (const auto& group : m_correlationGroups) {
            out << "<div class='group'>\n";
            out << "<div class='group-header'>Correlation: " << group.correlationId.toHtmlEscaped() 
                << " (" << group.correlationType << ") - " << group.events.size() << " events</div>\n";
            out << "<table>\n<tr><th>Source</th><th>Line</th><th>Timestamp</th><th>Level</th><th>Message</th></tr>\n";
            
            for (const auto& evt : group.events) {
                out << "<tr><td>" << evt.sourceFile.toHtmlEscaped() << "</td>"
                    << "<td>" << evt.lineNumber << "</td>"
                    << "<td>" << evt.timestamp.toString(Qt::ISODate) << "</td>"
                    << "<td class='" << evt.level << "'>" << evt.level << "</td>"
                    << "<td>" << evt.message.toHtmlEscaped().left(200) << "</td></tr>\n";
            }
            
            out << "</table>\n</div>\n";
        }
        
        out << "</body>\n</html>\n";
    }
    
    file.close();
}

QVariantMap CorrelationAnalyzer::getStatistics()
{
    QVariantMap stats;
    stats["totalGroups"] = m_correlationGroups.size();
    
    int totalEvents = 0;
    qint64 totalDuration = 0;
    QMap<QString, int> typeCount;
    QMap<QString, int> sourceCount;
    
    for (const auto& group : m_correlationGroups) {
        totalEvents += group.events.size();
        totalDuration += group.durationMs;
        typeCount[group.correlationType]++;
        
        for (const auto& evt : group.events) {
            sourceCount[evt.sourceFile]++;
        }
    }
    
    stats["totalEvents"] = totalEvents;
    stats["averageDurationMs"] = m_correlationGroups.isEmpty() ? 0 : totalDuration / m_correlationGroups.size();
    stats["sourceCount"] = m_sources.size();
    
    QVariantMap types;
    for (auto it = typeCount.begin(); it != typeCount.end(); ++it) {
        types[it.key()] = it.value();
    }
    stats["byType"] = types;
    
    QVariantMap sources;
    for (auto it = sourceCount.begin(); it != sourceCount.end(); ++it) {
        sources[it.key()] = it.value();
    }
    stats["bySource"] = sources;
    
    return stats;
}

QVariantList CorrelationAnalyzer::findRelatedEvents(const QString& sourceFile,
                                                     int lineNumber,
                                                     int maxResults)
{
    QVariantList result;
    
    // Find the correlation group containing this event
    for (const auto& group : m_correlationGroups) {
        for (const auto& evt : group.events) {
            if (evt.sourceFile == sourceFile && evt.lineNumber == lineNumber) {
                // Found the event, return all events in this group
                for (const auto& relatedEvt : group.events) {
                    if (result.size() >= maxResults) break;
                    result.append(relatedEvt.toVariantMap());
                }
                return result;
            }
        }
    }
    
    return result;
}
