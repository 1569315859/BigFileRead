/**
 * @file DistinctValueAnalyzer.h
 * @brief Distinct Value Analyzer - Analyze unique values in log columns
 */

#ifndef DISTINCTVALUEANALYZER_H
#define DISTINCTVALUEANALYZER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QHash>
#include <QVector>
#include <QRegularExpression>
#include <QDateTime>

/**
 * @brief Result of distinct value analysis for a column/field
 */
struct DistinctValueResult {
    QString columnName;
    int totalCount;
    int distinctCount;
    double uniquenessRatio;  // distinctCount / totalCount
    QList<QPair<QString, int>> topValues;  // value -> count, sorted by count
    QList<QPair<QString, int>> bottomValues;  // least frequent values
    QString mostCommon;
    QString leastCommon;
    int nullCount;
    int emptyCount;
    
    // Statistics for numeric columns
    bool isNumeric;
    double minValue;
    double maxValue;
    double avgValue;
    double stdDev;
    double median;
    
    // Statistics for datetime columns
    bool isDateTime;
    QDateTime earliestTime;
    QDateTime latestTime;
    
    // Pattern analysis
    QList<QPair<QString, int>> patterns;  // regex pattern -> count
};

/**
 * @brief Column extraction configuration
 */
struct ColumnConfig {
    QString name;
    enum Type {
        Auto,       // Auto-detect type
        String,
        Integer,
        Float,
        DateTime,
        Boolean,
        Regex       // Extract using regex
    };
    Type type;
    QString regexPattern;  // For Regex type
    int captureGroup;      // Which capture group to use (default 1)
    QString dateFormat;    // For DateTime type
};

/**
 * @brief Singleton DistinctValueAnalyzer for analyzing unique values
 */
class DistinctValueAnalyzer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isAnalyzing READ isAnalyzing NOTIFY analyzingChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QVariantList results READ resultsVariant NOTIFY resultsChanged)
    Q_PROPERTY(QVariantList columns READ columnsVariant NOTIFY columnsChanged)

public:
    static DistinctValueAnalyzer& instance();
    
    // Column configuration
    Q_INVOKABLE void addColumn(const QVariantMap &config);
    Q_INVOKABLE void removeColumn(const QString &name);
    Q_INVOKABLE void clearColumns();
    Q_INVOKABLE QVariantList detectColumns(const QStringList &sampleLines);
    
    // Analysis
    Q_INVOKABLE void analyzeLines(const QStringList &lines);
    Q_INVOKABLE void analyzeLineRange(int startLine, int endLine);
    Q_INVOKABLE void analyzeWithFilter(const QString &filterPattern);
    Q_INVOKABLE void cancelAnalysis();
    
    // Results
    Q_INVOKABLE QVariantMap getResult(const QString &columnName) const;
    Q_INVOKABLE QVariantList getTopValues(const QString &columnName, int limit = 10) const;
    Q_INVOKABLE QVariantList getBottomValues(const QString &columnName, int limit = 10) const;
    Q_INVOKABLE QVariantMap getStatistics(const QString &columnName) const;
    Q_INVOKABLE QVariantList getPatterns(const QString &columnName) const;
    
    // Export
    Q_INVOKABLE QString exportToCsv() const;
    Q_INVOKABLE QString exportToJson() const;
    Q_INVOKABLE bool exportToFile(const QString &filePath);
    
    // Utility
    Q_INVOKABLE QVariantMap analyzePattern(const QString &value) const;
    Q_INVOKABLE QString suggestType(const QStringList &sampleValues) const;
    
    // Getters
    bool isAnalyzing() const { return m_isAnalyzing; }
    int progress() const { return m_progress; }
    QVariantList resultsVariant() const;
    QVariantList columnsVariant() const;

signals:
    void analyzingChanged();
    void progressChanged();
    void resultsChanged();
    void columnsChanged();
    void analysisStarted();
    void analysisCompleted();
    void analysisError(const QString &error);
    void columnDetected(const QString &name, const QString &type);

private:
    explicit DistinctValueAnalyzer(QObject *parent = nullptr);
    ~DistinctValueAnalyzer();
    
    DistinctValueAnalyzer(const DistinctValueAnalyzer&) = delete;
    DistinctValueAnalyzer& operator=(const DistinctValueAnalyzer&) = delete;
    
    QString extractValue(const QString &line, const ColumnConfig &config) const;
    void processLine(const QString &line);
    void computeStatistics();
    void detectPatterns(DistinctValueResult &result, const QHash<QString, int> &values);
    
    bool isNumericValue(const QString &value) const;
    bool isDateTimeValue(const QString &value) const;
    double parseNumeric(const QString &value) const;
    QDateTime parseDateTime(const QString &value) const;
    
    QString generatePattern(const QString &value) const;
    
    QList<ColumnConfig> m_columns;
    QHash<QString, QHash<QString, int>> m_valueCounters;  // column -> (value -> count)
    QHash<QString, QVector<double>> m_numericValues;      // column -> numeric values
    QHash<QString, DistinctValueResult> m_results;
    
    bool m_isAnalyzing;
    int m_progress;
    int m_totalLines;
    int m_processedLines;
    bool m_cancelRequested;
};

#endif // DISTINCTVALUEANALYZER_H
