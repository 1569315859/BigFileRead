/**
 * @file ReportScheduler.h
 * @brief Scheduled Report Generator - Automated report generation and distribution
 */

#ifndef REPORTSCHEDULER_H
#define REPORTSCHEDULER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariantMap>
#include <QVariantList>
#include <QTimer>
#include <QMap>
#include <memory>

/**
 * @brief Report Schedule Configuration
 */
struct ReportSchedule {
    QString id;                     // Unique schedule ID
    QString name;                   // Display name
    QString description;            // Schedule description
    bool enabled;                   // Whether schedule is active
    
    // Schedule timing
    enum Frequency {
        Once,           // One-time report
        Hourly,         // Every hour
        Daily,          // Every day
        Weekly,         // Every week
        Monthly,        // Every month
        Custom          // Custom cron-like expression
    };
    Frequency frequency;
    QString cronExpression;         // Custom cron expression (for Custom type)
    QTime timeOfDay;                // Time to run (for Daily/Weekly/Monthly)
    int dayOfWeek;                  // Day of week (0-6, for Weekly)
    int dayOfMonth;                 // Day of month (1-31, for Monthly)
    QDateTime nextRun;              // Calculated next run time
    QDateTime lastRun;              // Last execution time
    
    // Report configuration
    QString reportType;             // Type: summary, detailed, custom
    QString templateName;           // Report template to use
    QStringList sourceFiles;        // Log files to include
    QString filterQuery;            // Filter expression
    QDateTime timeRangeStart;       // Time range (if fixed)
    int timeRangeHours;             // Relative time range (last N hours)
    bool useRelativeTime;           // Use relative vs fixed time range
    
    // Output settings
    enum OutputFormat {
        Html,
        Pdf,
        Csv,
        Json,
        Xlsx,
        Markdown
    };
    OutputFormat outputFormat;
    QString outputPath;             // Where to save the report
    QString outputFilenamePattern;  // Pattern like "report_{date}_{time}.pdf"
    
    // Distribution settings
    bool emailEnabled;
    QStringList emailRecipients;
    QString emailSubject;
    QString emailBody;
    bool webhookEnabled;
    QString webhookUrl;
    bool ftpEnabled;
    QString ftpServer;
    QString ftpPath;
    QString ftpUsername;
    QString ftpPassword;
    bool sharePointEnabled;
    QString sharePointUrl;
    QString sharePointFolder;
};

/**
 * @brief Report Template for customizable reports
 */
struct ReportTemplate {
    QString id;
    QString name;
    QString description;
    QString category;               // Built-in, User, Enterprise
    
    // Sections to include
    bool includeSummary;
    bool includeTimeline;
    bool includeErrorDistribution;
    bool includeTopMessages;
    bool includeSourceBreakdown;
    bool includeKeywordHits;
    bool includeBookmarks;
    bool includeTrends;
    bool includeCustomQueries;
    
    // Custom SQL queries for additional data
    QStringList customQueries;
    
    // Styling
    QString cssTheme;               // Dark, Light, Corporate, Custom
    QString customCss;
    QString headerHtml;
    QString footerHtml;
    QString logoPath;
    QString companyName;
};

/**
 * @brief Singleton ReportScheduler for automated report generation
 */
class ReportScheduler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList schedules READ getSchedulesVariant NOTIFY schedulesChanged)
    Q_PROPERTY(QVariantList templates READ getTemplatesVariant NOTIFY templatesChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(QString currentReport READ currentReport NOTIFY currentReportChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)

public:
    /**
     * @brief Get singleton instance
     */
    static ReportScheduler& instance();
    
    // Schedule management
    Q_INVOKABLE QVariantMap createSchedule(const QVariantMap &config);
    Q_INVOKABLE bool updateSchedule(const QString &id, const QVariantMap &config);
    Q_INVOKABLE bool deleteSchedule(const QString &id);
    Q_INVOKABLE QVariantMap getSchedule(const QString &id) const;
    Q_INVOKABLE void setScheduleEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void runScheduleNow(const QString &id);
    
    // Template management
    Q_INVOKABLE QVariantMap createTemplate(const QVariantMap &config);
    Q_INVOKABLE bool updateTemplate(const QString &id, const QVariantMap &config);
    Q_INVOKABLE bool deleteTemplate(const QString &id);
    Q_INVOKABLE QVariantMap getTemplate(const QString &id) const;
    Q_INVOKABLE QVariantList getTemplatesByCategory(const QString &category) const;
    
    // Report generation
    Q_INVOKABLE void generateReport(const QString &scheduleId);
    Q_INVOKABLE void generateReportWithConfig(const QVariantMap &config);
    Q_INVOKABLE void cancelGeneration();
    Q_INVOKABLE QString previewReport(const QVariantMap &config);
    
    // Report history
    Q_INVOKABLE QVariantList getReportHistory(int limit = 50) const;
    Q_INVOKABLE bool deleteReportFromHistory(const QString &reportId);
    Q_INVOKABLE void clearHistory();
    
    // Utility
    Q_INVOKABLE QDateTime calculateNextRun(const QVariantMap &scheduleConfig) const;
    Q_INVOKABLE QStringList getAvailableFormats() const;
    Q_INVOKABLE QString formatBytes(qint64 bytes) const;
    
    // Getters
    QVariantList getSchedulesVariant() const;
    QVariantList getTemplatesVariant() const;
    bool isRunning() const { return m_isRunning; }
    QString currentReport() const { return m_currentReport; }
    int progress() const { return m_progress; }

signals:
    void schedulesChanged();
    void templatesChanged();
    void runningChanged();
    void currentReportChanged();
    void progressChanged();
    void scheduleTriggered(const QString &scheduleId);
    void reportGenerated(const QString &scheduleId, const QString &filePath);
    void reportGenerationFailed(const QString &scheduleId, const QString &error);
    void reportDistributed(const QString &scheduleId, const QString &method, bool success);
    void scheduleError(const QString &scheduleId, const QString &error);

private:
    explicit ReportScheduler(QObject *parent = nullptr);
    ~ReportScheduler();
    
    ReportScheduler(const ReportScheduler&) = delete;
    ReportScheduler& operator=(const ReportScheduler&) = delete;
    
    void loadSettings();
    void saveSettings();
    void loadBuiltInTemplates();
    
    void checkSchedules();
    void executeSchedule(ReportSchedule &schedule);
    
    QString generateHtmlReport(const ReportSchedule &schedule, const ReportTemplate &tmpl);
    QString generateCsvReport(const ReportSchedule &schedule);
    QString generateJsonReport(const ReportSchedule &schedule);
    QString generateMarkdownReport(const ReportSchedule &schedule, const ReportTemplate &tmpl);
    bool generatePdfReport(const QString &htmlContent, const QString &outputPath);
    bool generateXlsxReport(const ReportSchedule &schedule, const QString &outputPath);
    
    bool distributeReport(const ReportSchedule &schedule, const QString &filePath);
    bool sendReportByEmail(const ReportSchedule &schedule, const QString &filePath);
    bool sendReportToWebhook(const ReportSchedule &schedule, const QString &filePath);
    bool uploadReportToFtp(const ReportSchedule &schedule, const QString &filePath);
    bool uploadReportToSharePoint(const ReportSchedule &schedule, const QString &filePath);
    
    QString expandFilenamePattern(const QString &pattern) const;
    QDateTime parseNextCronRun(const QString &cronExpr) const;
    
    void addToHistory(const QString &scheduleId, const QString &filePath, 
                      bool success, const QString &error = QString());
    
    // Schedules and templates
    QMap<QString, std::shared_ptr<ReportSchedule>> m_schedules;
    QMap<QString, std::shared_ptr<ReportTemplate>> m_templates;
    
    // Timer for checking schedules
    QTimer *m_scheduleTimer;
    
    // State
    bool m_isRunning;
    QString m_currentReport;
    int m_progress;
    
    // History
    struct ReportHistoryEntry {
        QString id;
        QString scheduleId;
        QString scheduleName;
        QDateTime timestamp;
        QString filePath;
        qint64 fileSize;
        bool success;
        QString error;
        QStringList distributionResults;
    };
    QList<ReportHistoryEntry> m_history;
};

#endif // REPORTSCHEDULER_H
