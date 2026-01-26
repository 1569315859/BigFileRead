/**
 * @file CorrelationAnalyzer.h
 * @brief Log Correlation Analysis for BigFileViewer
 * 
 * Analyzes correlations between log entries across multiple files
 * based on time windows, transaction IDs, request IDs, etc.
 */

#ifndef CORRELATIONANALYZER_H
#define CORRELATIONANALYZER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QDateTime>
#include <QMap>
#include <QSet>

class BigFileModel;

/**
 * @struct CorrelatedEvent
 * @brief Represents a single correlated log event
 */
struct CorrelatedEvent {
    QString sourceFile;     // Source file name
    int lineNumber;         // Line number in file
    QDateTime timestamp;    // Event timestamp
    QString level;          // Log level
    QString message;        // Log message
    QString correlationId;  // ID used for correlation (transaction/request/session)
    
    QVariantMap toVariantMap() const {
        QVariantMap map;
        map["sourceFile"] = sourceFile;
        map["lineNumber"] = lineNumber;
        map["timestamp"] = timestamp;
        map["level"] = level;
        map["message"] = message;
        map["correlationId"] = correlationId;
        return map;
    }
};

/**
 * @struct CorrelationGroup
 * @brief A group of correlated events
 */
struct CorrelationGroup {
    QString correlationId;  // The ID that links these events
    QString correlationType;// Type: "transaction", "request", "session", "time"
    QList<CorrelatedEvent> events;
    QDateTime startTime;
    QDateTime endTime;
    qint64 durationMs;
    
    QVariantMap toVariantMap() const {
        QVariantMap map;
        map["correlationId"] = correlationId;
        map["correlationType"] = correlationType;
        map["eventCount"] = events.size();
        map["startTime"] = startTime;
        map["endTime"] = endTime;
        map["durationMs"] = durationMs;
        
        QVariantList eventList;
        for (const auto& evt : events) {
            eventList.append(evt.toVariantMap());
        }
        map["events"] = eventList;
        return map;
    }
};

/**
 * @class CorrelationAnalyzer
 * @brief Analyzes correlations between log entries
 * 
 * Features:
 * - Time window correlation (events within ±N seconds)
 * - ID-based correlation (transaction/request/session IDs)
 * - Cross-file correlation
 * - Timeline visualization support
 */
class CorrelationAnalyzer : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isAnalyzing READ isAnalyzing NOTIFY analyzingChanged)
    Q_PROPERTY(int groupCount READ groupCount NOTIFY analysisCompleted)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    static CorrelationAnalyzer& instance();
    
    // Properties
    bool isAnalyzing() const { return m_isAnalyzing; }
    int groupCount() const { return m_correlationGroups.size(); }
    QString lastError() const { return m_lastError; }
    
    /**
     * @brief Add a log source for correlation analysis
     * @param name Source name/identifier
     * @param model The BigFileModel to analyze
     */
    Q_INVOKABLE void addSource(const QString& name, BigFileModel* model);
    
    /**
     * @brief Remove a log source
     */
    Q_INVOKABLE void removeSource(const QString& name);
    
    /**
     * @brief Clear all sources
     */
    Q_INVOKABLE void clearSources();
    
    /**
     * @brief Get list of added sources
     */
    Q_INVOKABLE QStringList getSources() const;
    
    /**
     * @brief Analyze correlation by time window
     * @param windowSeconds Time window in seconds (±)
     * @param anchorFile Optional: anchor file to correlate around
     * @param anchorLine Optional: anchor line number
     */
    Q_INVOKABLE void analyzeByTimeWindow(int windowSeconds, 
                                          const QString& anchorFile = QString(),
                                          int anchorLine = -1);
    
    /**
     * @brief Analyze correlation by ID pattern
     * @param idPattern Regex pattern to extract correlation ID
     * @param idName Name of the ID type (e.g., "transactionId", "requestId")
     */
    Q_INVOKABLE void analyzeByIdPattern(const QString& idPattern, 
                                         const QString& idName = "correlationId");
    
    /**
     * @brief Analyze using common ID patterns
     * Automatically detects: UUID, request-id, transaction-id, session-id, trace-id
     */
    Q_INVOKABLE void analyzeWithAutoDetection();
    
    /**
     * @brief Cancel ongoing analysis
     */
    Q_INVOKABLE void cancelAnalysis();
    
    /**
     * @brief Get all correlation groups
     */
    Q_INVOKABLE QVariantList getCorrelationGroups();
    
    /**
     * @brief Get a specific correlation group by ID
     */
    Q_INVOKABLE QVariantMap getCorrelationGroup(const QString& correlationId);
    
    /**
     * @brief Get events for a specific correlation ID
     */
    Q_INVOKABLE QVariantList getCorrelatedEvents(const QString& correlationId);
    
    /**
     * @brief Get timeline data for visualization
     * @return List of events sorted by time with source info
     */
    Q_INVOKABLE QVariantList getTimelineData();
    
    /**
     * @brief Get timeline data for a specific time range
     */
    Q_INVOKABLE QVariantList getTimelineDataInRange(const QDateTime& start, 
                                                     const QDateTime& end);
    
    /**
     * @brief Export correlation analysis results
     * @param filePath Output file path
     * @param format Export format (json, csv, html)
     */
    Q_INVOKABLE void exportResults(const QString& filePath, const QString& format);
    
    /**
     * @brief Get statistics about the correlation analysis
     */
    Q_INVOKABLE QVariantMap getStatistics();
    
    /**
     * @brief Find related events for a specific log line
     * @param sourceFile Source file name
     * @param lineNumber Line number
     * @param maxResults Maximum results to return
     */
    Q_INVOKABLE QVariantList findRelatedEvents(const QString& sourceFile,
                                                int lineNumber,
                                                int maxResults = 50);

signals:
    void analyzingChanged();
    void analysisCompleted(int groupCount);
    void analysisProgress(int current, int total);
    void errorOccurred(const QString& error);
    void sourceAdded(const QString& name);
    void sourceRemoved(const QString& name);

private:
    explicit CorrelationAnalyzer(QObject* parent = nullptr);
    ~CorrelationAnalyzer() = default;
    
    // Disable copy
    CorrelationAnalyzer(const CorrelationAnalyzer&) = delete;
    CorrelationAnalyzer& operator=(const CorrelationAnalyzer&) = delete;
    
    void setAnalyzing(bool analyzing);
    void setError(const QString& error);
    
    // Parse timestamp from log line
    QDateTime parseTimestamp(const QString& line);
    
    // Extract correlation ID using pattern
    QString extractCorrelationId(const QString& line, const QRegularExpression& pattern);
    
    // Common ID patterns
    QList<QPair<QString, QRegularExpression>> getCommonIdPatterns();
    
    // State
    bool m_isAnalyzing;
    bool m_cancelRequested;
    QString m_lastError;
    
    // Sources
    QMap<QString, BigFileModel*> m_sources;
    
    // Results
    QList<CorrelationGroup> m_correlationGroups;
    QMap<QString, int> m_correlationIdToGroupIndex;
};

#endif // CORRELATIONANALYZER_H
