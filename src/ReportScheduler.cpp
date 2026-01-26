/**
 * @file ReportScheduler.cpp
 * @brief Scheduled Report Generator implementation
 */

#include "ReportScheduler.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include <QTextStream>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHttpMultiPart>
#include <QBuffer>
#include <QtConcurrent>

ReportScheduler& ReportScheduler::instance()
{
    static ReportScheduler instance;
    return instance;
}

ReportScheduler::ReportScheduler(QObject *parent)
    : QObject(parent)
    , m_scheduleTimer(new QTimer(this))
    , m_isRunning(false)
    , m_progress(0)
{
    loadSettings();
    loadBuiltInTemplates();
    
    // Check schedules every minute
    connect(m_scheduleTimer, &QTimer::timeout, this, &ReportScheduler::checkSchedules);
    m_scheduleTimer->start(60000);
    
    // Initial check
    QTimer::singleShot(5000, this, &ReportScheduler::checkSchedules);
}

ReportScheduler::~ReportScheduler()
{
    saveSettings();
}

void ReportScheduler::loadSettings()
{
    QSettings settings;
    settings.beginGroup("ReportScheduler");
    
    // Load schedules
    int scheduleCount = settings.beginReadArray("Schedules");
    for (int i = 0; i < scheduleCount; ++i) {
        settings.setArrayIndex(i);
        auto schedule = std::make_shared<ReportSchedule>();
        
        schedule->id = settings.value("id").toString();
        schedule->name = settings.value("name").toString();
        schedule->description = settings.value("description").toString();
        schedule->enabled = settings.value("enabled", true).toBool();
        schedule->frequency = static_cast<ReportSchedule::Frequency>(
            settings.value("frequency", ReportSchedule::Daily).toInt());
        schedule->cronExpression = settings.value("cronExpression").toString();
        schedule->timeOfDay = settings.value("timeOfDay", QTime(8, 0)).toTime();
        schedule->dayOfWeek = settings.value("dayOfWeek", 1).toInt();
        schedule->dayOfMonth = settings.value("dayOfMonth", 1).toInt();
        schedule->nextRun = settings.value("nextRun").toDateTime();
        schedule->lastRun = settings.value("lastRun").toDateTime();
        
        schedule->reportType = settings.value("reportType", "summary").toString();
        schedule->templateName = settings.value("templateName", "default").toString();
        schedule->sourceFiles = settings.value("sourceFiles").toStringList();
        schedule->filterQuery = settings.value("filterQuery").toString();
        schedule->timeRangeHours = settings.value("timeRangeHours", 24).toInt();
        schedule->useRelativeTime = settings.value("useRelativeTime", true).toBool();
        
        schedule->outputFormat = static_cast<ReportSchedule::OutputFormat>(
            settings.value("outputFormat", ReportSchedule::Html).toInt());
        schedule->outputPath = settings.value("outputPath").toString();
        schedule->outputFilenamePattern = settings.value("outputFilenamePattern", 
            "report_{name}_{date}_{time}").toString();
        
        schedule->emailEnabled = settings.value("emailEnabled", false).toBool();
        schedule->emailRecipients = settings.value("emailRecipients").toStringList();
        schedule->emailSubject = settings.value("emailSubject").toString();
        schedule->emailBody = settings.value("emailBody").toString();
        schedule->webhookEnabled = settings.value("webhookEnabled", false).toBool();
        schedule->webhookUrl = settings.value("webhookUrl").toString();
        schedule->ftpEnabled = settings.value("ftpEnabled", false).toBool();
        schedule->ftpServer = settings.value("ftpServer").toString();
        schedule->ftpPath = settings.value("ftpPath").toString();
        schedule->ftpUsername = settings.value("ftpUsername").toString();
        schedule->ftpPassword = settings.value("ftpPassword").toString();
        
        if (!schedule->id.isEmpty()) {
            m_schedules[schedule->id] = schedule;
        }
    }
    settings.endArray();
    
    // Load custom templates
    int templateCount = settings.beginReadArray("Templates");
    for (int i = 0; i < templateCount; ++i) {
        settings.setArrayIndex(i);
        auto tmpl = std::make_shared<ReportTemplate>();
        
        tmpl->id = settings.value("id").toString();
        tmpl->name = settings.value("name").toString();
        tmpl->description = settings.value("description").toString();
        tmpl->category = settings.value("category", "User").toString();
        
        tmpl->includeSummary = settings.value("includeSummary", true).toBool();
        tmpl->includeTimeline = settings.value("includeTimeline", true).toBool();
        tmpl->includeErrorDistribution = settings.value("includeErrorDistribution", true).toBool();
        tmpl->includeTopMessages = settings.value("includeTopMessages", true).toBool();
        tmpl->includeSourceBreakdown = settings.value("includeSourceBreakdown", true).toBool();
        tmpl->includeKeywordHits = settings.value("includeKeywordHits", true).toBool();
        tmpl->includeBookmarks = settings.value("includeBookmarks", false).toBool();
        tmpl->includeTrends = settings.value("includeTrends", true).toBool();
        
        tmpl->cssTheme = settings.value("cssTheme", "Light").toString();
        tmpl->customCss = settings.value("customCss").toString();
        tmpl->headerHtml = settings.value("headerHtml").toString();
        tmpl->footerHtml = settings.value("footerHtml").toString();
        tmpl->logoPath = settings.value("logoPath").toString();
        tmpl->companyName = settings.value("companyName").toString();
        
        if (!tmpl->id.isEmpty() && tmpl->category != "Built-in") {
            m_templates[tmpl->id] = tmpl;
        }
    }
    settings.endArray();
    
    // Load history
    int historyCount = settings.beginReadArray("History");
    for (int i = 0; i < historyCount && i < 100; ++i) {
        settings.setArrayIndex(i);
        ReportHistoryEntry entry;
        entry.id = settings.value("id").toString();
        entry.scheduleId = settings.value("scheduleId").toString();
        entry.scheduleName = settings.value("scheduleName").toString();
        entry.timestamp = settings.value("timestamp").toDateTime();
        entry.filePath = settings.value("filePath").toString();
        entry.fileSize = settings.value("fileSize").toLongLong();
        entry.success = settings.value("success").toBool();
        entry.error = settings.value("error").toString();
        m_history.append(entry);
    }
    settings.endArray();
    
    settings.endGroup();
}

void ReportScheduler::saveSettings()
{
    QSettings settings;
    settings.beginGroup("ReportScheduler");
    
    settings.beginWriteArray("Schedules");
    int i = 0;
    for (auto it = m_schedules.constBegin(); it != m_schedules.constEnd(); ++it, ++i) {
        settings.setArrayIndex(i);
        const auto &schedule = *it.value();
        
        settings.setValue("id", schedule.id);
        settings.setValue("name", schedule.name);
        settings.setValue("description", schedule.description);
        settings.setValue("enabled", schedule.enabled);
        settings.setValue("frequency", static_cast<int>(schedule.frequency));
        settings.setValue("cronExpression", schedule.cronExpression);
        settings.setValue("timeOfDay", schedule.timeOfDay);
        settings.setValue("dayOfWeek", schedule.dayOfWeek);
        settings.setValue("dayOfMonth", schedule.dayOfMonth);
        settings.setValue("nextRun", schedule.nextRun);
        settings.setValue("lastRun", schedule.lastRun);
        
        settings.setValue("reportType", schedule.reportType);
        settings.setValue("templateName", schedule.templateName);
        settings.setValue("sourceFiles", schedule.sourceFiles);
        settings.setValue("filterQuery", schedule.filterQuery);
        settings.setValue("timeRangeHours", schedule.timeRangeHours);
        settings.setValue("useRelativeTime", schedule.useRelativeTime);
        
        settings.setValue("outputFormat", static_cast<int>(schedule.outputFormat));
        settings.setValue("outputPath", schedule.outputPath);
        settings.setValue("outputFilenamePattern", schedule.outputFilenamePattern);
        
        settings.setValue("emailEnabled", schedule.emailEnabled);
        settings.setValue("emailRecipients", schedule.emailRecipients);
        settings.setValue("emailSubject", schedule.emailSubject);
        settings.setValue("emailBody", schedule.emailBody);
        settings.setValue("webhookEnabled", schedule.webhookEnabled);
        settings.setValue("webhookUrl", schedule.webhookUrl);
        settings.setValue("ftpEnabled", schedule.ftpEnabled);
        settings.setValue("ftpServer", schedule.ftpServer);
        settings.setValue("ftpPath", schedule.ftpPath);
        settings.setValue("ftpUsername", schedule.ftpUsername);
        settings.setValue("ftpPassword", schedule.ftpPassword);
    }
    settings.endArray();
    
    // Save custom templates only
    settings.beginWriteArray("Templates");
    i = 0;
    for (auto it = m_templates.constBegin(); it != m_templates.constEnd(); ++it) {
        if (it.value()->category == "Built-in") continue;
        settings.setArrayIndex(i++);
        const auto &tmpl = *it.value();
        
        settings.setValue("id", tmpl.id);
        settings.setValue("name", tmpl.name);
        settings.setValue("description", tmpl.description);
        settings.setValue("category", tmpl.category);
        
        settings.setValue("includeSummary", tmpl.includeSummary);
        settings.setValue("includeTimeline", tmpl.includeTimeline);
        settings.setValue("includeErrorDistribution", tmpl.includeErrorDistribution);
        settings.setValue("includeTopMessages", tmpl.includeTopMessages);
        settings.setValue("includeSourceBreakdown", tmpl.includeSourceBreakdown);
        settings.setValue("includeKeywordHits", tmpl.includeKeywordHits);
        settings.setValue("includeBookmarks", tmpl.includeBookmarks);
        settings.setValue("includeTrends", tmpl.includeTrends);
        
        settings.setValue("cssTheme", tmpl.cssTheme);
        settings.setValue("customCss", tmpl.customCss);
        settings.setValue("headerHtml", tmpl.headerHtml);
        settings.setValue("footerHtml", tmpl.footerHtml);
        settings.setValue("logoPath", tmpl.logoPath);
        settings.setValue("companyName", tmpl.companyName);
    }
    settings.endArray();
    
    // Save history (keep last 100)
    settings.beginWriteArray("History");
    int limit = qMin(m_history.size(), 100);
    for (i = 0; i < limit; ++i) {
        settings.setArrayIndex(i);
        const auto &entry = m_history[i];
        settings.setValue("id", entry.id);
        settings.setValue("scheduleId", entry.scheduleId);
        settings.setValue("scheduleName", entry.scheduleName);
        settings.setValue("timestamp", entry.timestamp);
        settings.setValue("filePath", entry.filePath);
        settings.setValue("fileSize", entry.fileSize);
        settings.setValue("success", entry.success);
        settings.setValue("error", entry.error);
    }
    settings.endArray();
    
    settings.endGroup();
}

void ReportScheduler::loadBuiltInTemplates()
{
    // Default template
    auto defaultTmpl = std::make_shared<ReportTemplate>();
    defaultTmpl->id = "default";
    defaultTmpl->name = tr("Default Report");
    defaultTmpl->description = tr("Standard log analysis report with all sections");
    defaultTmpl->category = "Built-in";
    defaultTmpl->includeSummary = true;
    defaultTmpl->includeTimeline = true;
    defaultTmpl->includeErrorDistribution = true;
    defaultTmpl->includeTopMessages = true;
    defaultTmpl->includeSourceBreakdown = true;
    defaultTmpl->includeKeywordHits = true;
    defaultTmpl->includeBookmarks = false;
    defaultTmpl->includeTrends = true;
    defaultTmpl->cssTheme = "Light";
    m_templates["default"] = defaultTmpl;
    
    // Executive Summary
    auto execTmpl = std::make_shared<ReportTemplate>();
    execTmpl->id = "executive";
    execTmpl->name = tr("Executive Summary");
    execTmpl->description = tr("High-level summary for management");
    execTmpl->category = "Built-in";
    execTmpl->includeSummary = true;
    execTmpl->includeTimeline = false;
    execTmpl->includeErrorDistribution = true;
    execTmpl->includeTopMessages = false;
    execTmpl->includeSourceBreakdown = false;
    execTmpl->includeKeywordHits = false;
    execTmpl->includeBookmarks = false;
    execTmpl->includeTrends = true;
    execTmpl->cssTheme = "Corporate";
    m_templates["executive"] = execTmpl;
    
    // Technical Detail
    auto techTmpl = std::make_shared<ReportTemplate>();
    techTmpl->id = "technical";
    techTmpl->name = tr("Technical Detail");
    techTmpl->description = tr("Detailed technical report for developers");
    techTmpl->category = "Built-in";
    techTmpl->includeSummary = true;
    techTmpl->includeTimeline = true;
    techTmpl->includeErrorDistribution = true;
    techTmpl->includeTopMessages = true;
    techTmpl->includeSourceBreakdown = true;
    techTmpl->includeKeywordHits = true;
    techTmpl->includeBookmarks = true;
    techTmpl->includeTrends = true;
    techTmpl->includeCustomQueries = true;
    techTmpl->cssTheme = "Dark";
    m_templates["technical"] = techTmpl;
    
    // Error Focus
    auto errorTmpl = std::make_shared<ReportTemplate>();
    errorTmpl->id = "errors";
    errorTmpl->name = tr("Error Analysis");
    errorTmpl->description = tr("Focused report on errors and warnings");
    errorTmpl->category = "Built-in";
    errorTmpl->includeSummary = true;
    errorTmpl->includeTimeline = true;
    errorTmpl->includeErrorDistribution = true;
    errorTmpl->includeTopMessages = true;
    errorTmpl->includeSourceBreakdown = false;
    errorTmpl->includeKeywordHits = false;
    errorTmpl->includeBookmarks = false;
    errorTmpl->includeTrends = true;
    errorTmpl->cssTheme = "Light";
    m_templates["errors"] = errorTmpl;
    
    emit templatesChanged();
}

void ReportScheduler::checkSchedules()
{
    QDateTime now = QDateTime::currentDateTime();
    
    for (auto it = m_schedules.begin(); it != m_schedules.end(); ++it) {
        auto &schedule = *it.value();
        
        if (!schedule.enabled) continue;
        
        if (schedule.nextRun.isValid() && schedule.nextRun <= now) {
            executeSchedule(schedule);
        }
    }
}

void ReportScheduler::executeSchedule(ReportSchedule &schedule)
{
    emit scheduleTriggered(schedule.id);
    
    schedule.lastRun = QDateTime::currentDateTime();
    
    // Calculate next run
    QVariantMap config;
    config["frequency"] = static_cast<int>(schedule.frequency);
    config["cronExpression"] = schedule.cronExpression;
    config["timeOfDay"] = schedule.timeOfDay;
    config["dayOfWeek"] = schedule.dayOfWeek;
    config["dayOfMonth"] = schedule.dayOfMonth;
    schedule.nextRun = calculateNextRun(config);
    
    saveSettings();
    emit schedulesChanged();
    
    // Generate report asynchronously
    QtConcurrent::run([this, schedule]() {
        generateReport(schedule.id);
    });
}

QVariantMap ReportScheduler::createSchedule(const QVariantMap &config)
{
    auto schedule = std::make_shared<ReportSchedule>();
    schedule->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    schedule->name = config.value("name", tr("New Schedule")).toString();
    schedule->description = config.value("description").toString();
    schedule->enabled = config.value("enabled", true).toBool();
    
    schedule->frequency = static_cast<ReportSchedule::Frequency>(
        config.value("frequency", ReportSchedule::Daily).toInt());
    schedule->cronExpression = config.value("cronExpression").toString();
    schedule->timeOfDay = config.value("timeOfDay", QTime(8, 0)).toTime();
    schedule->dayOfWeek = config.value("dayOfWeek", 1).toInt();
    schedule->dayOfMonth = config.value("dayOfMonth", 1).toInt();
    
    schedule->reportType = config.value("reportType", "summary").toString();
    schedule->templateName = config.value("templateName", "default").toString();
    schedule->sourceFiles = config.value("sourceFiles").toStringList();
    schedule->filterQuery = config.value("filterQuery").toString();
    schedule->timeRangeHours = config.value("timeRangeHours", 24).toInt();
    schedule->useRelativeTime = config.value("useRelativeTime", true).toBool();
    
    schedule->outputFormat = static_cast<ReportSchedule::OutputFormat>(
        config.value("outputFormat", ReportSchedule::Html).toInt());
    schedule->outputPath = config.value("outputPath", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/Reports").toString();
    schedule->outputFilenamePattern = config.value("outputFilenamePattern", 
        "report_{name}_{date}_{time}").toString();
    
    schedule->emailEnabled = config.value("emailEnabled", false).toBool();
    schedule->emailRecipients = config.value("emailRecipients").toStringList();
    schedule->emailSubject = config.value("emailSubject").toString();
    schedule->emailBody = config.value("emailBody").toString();
    schedule->webhookEnabled = config.value("webhookEnabled", false).toBool();
    schedule->webhookUrl = config.value("webhookUrl").toString();
    
    // Calculate initial next run
    schedule->nextRun = calculateNextRun(config);
    
    m_schedules[schedule->id] = schedule;
    saveSettings();
    emit schedulesChanged();
    
    QVariantMap result;
    result["id"] = schedule->id;
    result["success"] = true;
    return result;
}

bool ReportScheduler::updateSchedule(const QString &id, const QVariantMap &config)
{
    auto it = m_schedules.find(id);
    if (it == m_schedules.end()) return false;
    
    auto &schedule = *it.value();
    
    if (config.contains("name")) schedule.name = config["name"].toString();
    if (config.contains("description")) schedule.description = config["description"].toString();
    if (config.contains("enabled")) schedule.enabled = config["enabled"].toBool();
    if (config.contains("frequency")) {
        schedule.frequency = static_cast<ReportSchedule::Frequency>(config["frequency"].toInt());
    }
    if (config.contains("cronExpression")) schedule.cronExpression = config["cronExpression"].toString();
    if (config.contains("timeOfDay")) schedule.timeOfDay = config["timeOfDay"].toTime();
    if (config.contains("dayOfWeek")) schedule.dayOfWeek = config["dayOfWeek"].toInt();
    if (config.contains("dayOfMonth")) schedule.dayOfMonth = config["dayOfMonth"].toInt();
    
    if (config.contains("reportType")) schedule.reportType = config["reportType"].toString();
    if (config.contains("templateName")) schedule.templateName = config["templateName"].toString();
    if (config.contains("sourceFiles")) schedule.sourceFiles = config["sourceFiles"].toStringList();
    if (config.contains("filterQuery")) schedule.filterQuery = config["filterQuery"].toString();
    if (config.contains("timeRangeHours")) schedule.timeRangeHours = config["timeRangeHours"].toInt();
    if (config.contains("useRelativeTime")) schedule.useRelativeTime = config["useRelativeTime"].toBool();
    
    if (config.contains("outputFormat")) {
        schedule.outputFormat = static_cast<ReportSchedule::OutputFormat>(config["outputFormat"].toInt());
    }
    if (config.contains("outputPath")) schedule.outputPath = config["outputPath"].toString();
    if (config.contains("outputFilenamePattern")) {
        schedule.outputFilenamePattern = config["outputFilenamePattern"].toString();
    }
    
    if (config.contains("emailEnabled")) schedule.emailEnabled = config["emailEnabled"].toBool();
    if (config.contains("emailRecipients")) schedule.emailRecipients = config["emailRecipients"].toStringList();
    if (config.contains("webhookEnabled")) schedule.webhookEnabled = config["webhookEnabled"].toBool();
    if (config.contains("webhookUrl")) schedule.webhookUrl = config["webhookUrl"].toString();
    
    // Recalculate next run
    schedule.nextRun = calculateNextRun(config);
    
    saveSettings();
    emit schedulesChanged();
    return true;
}

bool ReportScheduler::deleteSchedule(const QString &id)
{
    if (m_schedules.remove(id) > 0) {
        saveSettings();
        emit schedulesChanged();
        return true;
    }
    return false;
}

QVariantMap ReportScheduler::getSchedule(const QString &id) const
{
    auto it = m_schedules.find(id);
    if (it == m_schedules.end()) return QVariantMap();
    
    const auto &schedule = *it.value();
    QVariantMap result;
    result["id"] = schedule.id;
    result["name"] = schedule.name;
    result["description"] = schedule.description;
    result["enabled"] = schedule.enabled;
    result["frequency"] = static_cast<int>(schedule.frequency);
    result["cronExpression"] = schedule.cronExpression;
    result["timeOfDay"] = schedule.timeOfDay;
    result["dayOfWeek"] = schedule.dayOfWeek;
    result["dayOfMonth"] = schedule.dayOfMonth;
    result["nextRun"] = schedule.nextRun;
    result["lastRun"] = schedule.lastRun;
    result["reportType"] = schedule.reportType;
    result["templateName"] = schedule.templateName;
    result["sourceFiles"] = schedule.sourceFiles;
    result["filterQuery"] = schedule.filterQuery;
    result["timeRangeHours"] = schedule.timeRangeHours;
    result["useRelativeTime"] = schedule.useRelativeTime;
    result["outputFormat"] = static_cast<int>(schedule.outputFormat);
    result["outputPath"] = schedule.outputPath;
    result["outputFilenamePattern"] = schedule.outputFilenamePattern;
    result["emailEnabled"] = schedule.emailEnabled;
    result["emailRecipients"] = schedule.emailRecipients;
    result["webhookEnabled"] = schedule.webhookEnabled;
    result["webhookUrl"] = schedule.webhookUrl;
    return result;
}

void ReportScheduler::setScheduleEnabled(const QString &id, bool enabled)
{
    auto it = m_schedules.find(id);
    if (it != m_schedules.end()) {
        it.value()->enabled = enabled;
        saveSettings();
        emit schedulesChanged();
    }
}

void ReportScheduler::runScheduleNow(const QString &id)
{
    auto it = m_schedules.find(id);
    if (it != m_schedules.end()) {
        executeSchedule(*it.value());
    }
}

QVariantMap ReportScheduler::createTemplate(const QVariantMap &config)
{
    auto tmpl = std::make_shared<ReportTemplate>();
    tmpl->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tmpl->name = config.value("name", tr("New Template")).toString();
    tmpl->description = config.value("description").toString();
    tmpl->category = "User";
    
    tmpl->includeSummary = config.value("includeSummary", true).toBool();
    tmpl->includeTimeline = config.value("includeTimeline", true).toBool();
    tmpl->includeErrorDistribution = config.value("includeErrorDistribution", true).toBool();
    tmpl->includeTopMessages = config.value("includeTopMessages", true).toBool();
    tmpl->includeSourceBreakdown = config.value("includeSourceBreakdown", true).toBool();
    tmpl->includeKeywordHits = config.value("includeKeywordHits", true).toBool();
    tmpl->includeBookmarks = config.value("includeBookmarks", false).toBool();
    tmpl->includeTrends = config.value("includeTrends", true).toBool();
    
    tmpl->cssTheme = config.value("cssTheme", "Light").toString();
    tmpl->customCss = config.value("customCss").toString();
    tmpl->headerHtml = config.value("headerHtml").toString();
    tmpl->footerHtml = config.value("footerHtml").toString();
    tmpl->logoPath = config.value("logoPath").toString();
    tmpl->companyName = config.value("companyName").toString();
    
    m_templates[tmpl->id] = tmpl;
    saveSettings();
    emit templatesChanged();
    
    QVariantMap result;
    result["id"] = tmpl->id;
    result["success"] = true;
    return result;
}

bool ReportScheduler::updateTemplate(const QString &id, const QVariantMap &config)
{
    auto it = m_templates.find(id);
    if (it == m_templates.end() || it.value()->category == "Built-in") return false;
    
    auto &tmpl = *it.value();
    
    if (config.contains("name")) tmpl.name = config["name"].toString();
    if (config.contains("description")) tmpl.description = config["description"].toString();
    if (config.contains("includeSummary")) tmpl.includeSummary = config["includeSummary"].toBool();
    if (config.contains("includeTimeline")) tmpl.includeTimeline = config["includeTimeline"].toBool();
    if (config.contains("includeErrorDistribution")) tmpl.includeErrorDistribution = config["includeErrorDistribution"].toBool();
    if (config.contains("includeTopMessages")) tmpl.includeTopMessages = config["includeTopMessages"].toBool();
    if (config.contains("includeSourceBreakdown")) tmpl.includeSourceBreakdown = config["includeSourceBreakdown"].toBool();
    if (config.contains("includeKeywordHits")) tmpl.includeKeywordHits = config["includeKeywordHits"].toBool();
    if (config.contains("includeBookmarks")) tmpl.includeBookmarks = config["includeBookmarks"].toBool();
    if (config.contains("includeTrends")) tmpl.includeTrends = config["includeTrends"].toBool();
    
    if (config.contains("cssTheme")) tmpl.cssTheme = config["cssTheme"].toString();
    if (config.contains("customCss")) tmpl.customCss = config["customCss"].toString();
    if (config.contains("headerHtml")) tmpl.headerHtml = config["headerHtml"].toString();
    if (config.contains("footerHtml")) tmpl.footerHtml = config["footerHtml"].toString();
    if (config.contains("logoPath")) tmpl.logoPath = config["logoPath"].toString();
    if (config.contains("companyName")) tmpl.companyName = config["companyName"].toString();
    
    saveSettings();
    emit templatesChanged();
    return true;
}

bool ReportScheduler::deleteTemplate(const QString &id)
{
    auto it = m_templates.find(id);
    if (it == m_templates.end() || it.value()->category == "Built-in") return false;
    
    m_templates.remove(id);
    saveSettings();
    emit templatesChanged();
    return true;
}

QVariantMap ReportScheduler::getTemplate(const QString &id) const
{
    auto it = m_templates.find(id);
    if (it == m_templates.end()) return QVariantMap();
    
    const auto &tmpl = *it.value();
    QVariantMap result;
    result["id"] = tmpl.id;
    result["name"] = tmpl.name;
    result["description"] = tmpl.description;
    result["category"] = tmpl.category;
    result["includeSummary"] = tmpl.includeSummary;
    result["includeTimeline"] = tmpl.includeTimeline;
    result["includeErrorDistribution"] = tmpl.includeErrorDistribution;
    result["includeTopMessages"] = tmpl.includeTopMessages;
    result["includeSourceBreakdown"] = tmpl.includeSourceBreakdown;
    result["includeKeywordHits"] = tmpl.includeKeywordHits;
    result["includeBookmarks"] = tmpl.includeBookmarks;
    result["includeTrends"] = tmpl.includeTrends;
    result["cssTheme"] = tmpl.cssTheme;
    result["customCss"] = tmpl.customCss;
    return result;
}

QVariantList ReportScheduler::getTemplatesByCategory(const QString &category) const
{
    QVariantList result;
    for (const auto &tmpl : m_templates) {
        if (category.isEmpty() || tmpl->category == category) {
            result.append(getTemplate(tmpl->id));
        }
    }
    return result;
}

void ReportScheduler::generateReport(const QString &scheduleId)
{
    auto it = m_schedules.find(scheduleId);
    if (it == m_schedules.end()) {
        emit reportGenerationFailed(scheduleId, tr("Schedule not found"));
        return;
    }
    
    m_isRunning = true;
    m_currentReport = it.value()->name;
    m_progress = 0;
    emit runningChanged();
    emit currentReportChanged();
    emit progressChanged();
    
    const auto &schedule = *it.value();
    
    // Get template
    auto tmplIt = m_templates.find(schedule.templateName);
    const ReportTemplate *tmpl = (tmplIt != m_templates.end()) 
        ? tmplIt.value().get() 
        : m_templates["default"].get();
    
    QString outputContent;
    QString filePath;
    
    m_progress = 10;
    emit progressChanged();
    
    try {
        // Generate report based on format
        QString extension;
        switch (schedule.outputFormat) {
        case ReportSchedule::Html:
            outputContent = generateHtmlReport(schedule, *tmpl);
            extension = ".html";
            break;
        case ReportSchedule::Csv:
            outputContent = generateCsvReport(schedule);
            extension = ".csv";
            break;
        case ReportSchedule::Json:
            outputContent = generateJsonReport(schedule);
            extension = ".json";
            break;
        case ReportSchedule::Markdown:
            outputContent = generateMarkdownReport(schedule, *tmpl);
            extension = ".md";
            break;
        case ReportSchedule::Pdf:
            outputContent = generateHtmlReport(schedule, *tmpl);
            extension = ".pdf";
            break;
        case ReportSchedule::Xlsx:
            extension = ".xlsx";
            break;
        }
        
        m_progress = 60;
        emit progressChanged();
        
        // Ensure output directory exists
        QDir().mkpath(schedule.outputPath);
        
        // Generate filename
        QString filename = expandFilenamePattern(schedule.outputFilenamePattern);
        filename = filename.replace("{name}", schedule.name.simplified().replace(" ", "_"));
        filePath = schedule.outputPath + "/" + filename + extension;
        
        // Write file
        if (schedule.outputFormat == ReportSchedule::Pdf) {
            if (!generatePdfReport(outputContent, filePath)) {
                throw std::runtime_error("Failed to generate PDF");
            }
        } else if (schedule.outputFormat == ReportSchedule::Xlsx) {
            if (!generateXlsxReport(schedule, filePath)) {
                throw std::runtime_error("Failed to generate XLSX");
            }
        } else {
            QFile file(filePath);
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                throw std::runtime_error("Failed to open output file");
            }
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            stream << outputContent;
            file.close();
        }
        
        m_progress = 80;
        emit progressChanged();
        
        // Distribute report
        if (schedule.emailEnabled || schedule.webhookEnabled || 
            schedule.ftpEnabled || schedule.sharePointEnabled) {
            distributeReport(schedule, filePath);
        }
        
        m_progress = 100;
        emit progressChanged();
        
        addToHistory(schedule.id, filePath, true);
        emit reportGenerated(schedule.id, filePath);
        
    } catch (const std::exception &e) {
        addToHistory(schedule.id, QString(), false, QString::fromUtf8(e.what()));
        emit reportGenerationFailed(schedule.id, QString::fromUtf8(e.what()));
    }
    
    m_isRunning = false;
    m_currentReport.clear();
    emit runningChanged();
    emit currentReportChanged();
}

void ReportScheduler::generateReportWithConfig(const QVariantMap &config)
{
    auto result = createSchedule(config);
    QString scheduleId = result["id"].toString();
    
    // Generate immediately and then delete the schedule
    generateReport(scheduleId);
    deleteSchedule(scheduleId);
}

void ReportScheduler::cancelGeneration()
{
    // TODO: Implement cancellation logic
    m_isRunning = false;
    emit runningChanged();
}

QString ReportScheduler::previewReport(const QVariantMap &config)
{
    // Create temporary schedule for preview
    ReportSchedule schedule;
    schedule.reportType = config.value("reportType", "summary").toString();
    schedule.templateName = config.value("templateName", "default").toString();
    schedule.timeRangeHours = config.value("timeRangeHours", 24).toInt();
    schedule.useRelativeTime = true;
    
    auto tmplIt = m_templates.find(schedule.templateName);
    const ReportTemplate *tmpl = (tmplIt != m_templates.end()) 
        ? tmplIt.value().get() 
        : m_templates["default"].get();
    
    return generateHtmlReport(schedule, *tmpl);
}

QString ReportScheduler::generateHtmlReport(const ReportSchedule &schedule, const ReportTemplate &tmpl)
{
    Q_UNUSED(schedule)
    
    QString css;
    if (tmpl.cssTheme == "Dark") {
        css = R"(
            body { font-family: 'Segoe UI', sans-serif; background: #1e1e1e; color: #d4d4d4; margin: 40px; }
            h1, h2, h3 { color: #569cd6; }
            .card { background: #252526; border-radius: 8px; padding: 20px; margin: 20px 0; }
            table { width: 100%; border-collapse: collapse; }
            th { background: #383838; padding: 12px; text-align: left; }
            td { padding: 10px; border-bottom: 1px solid #383838; }
            .error { color: #f14c4c; }
            .warning { color: #cca700; }
            .info { color: #3794ff; }
        )";
    } else if (tmpl.cssTheme == "Corporate") {
        css = R"(
            body { font-family: 'Calibri', sans-serif; background: #fff; color: #333; margin: 40px; }
            h1 { color: #003366; border-bottom: 3px solid #003366; padding-bottom: 10px; }
            h2 { color: #004080; }
            .card { background: #f8f9fa; border-left: 4px solid #003366; padding: 20px; margin: 20px 0; }
            table { width: 100%; border-collapse: collapse; }
            th { background: #003366; color: white; padding: 12px; text-align: left; }
            td { padding: 10px; border-bottom: 1px solid #dee2e6; }
        )";
    } else {
        css = R"(
            body { font-family: 'Segoe UI', sans-serif; background: #f5f5f5; color: #333; margin: 40px; }
            h1 { color: #2c3e50; }
            h2 { color: #34495e; }
            .card { background: white; border-radius: 8px; padding: 20px; margin: 20px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
            table { width: 100%; border-collapse: collapse; }
            th { background: #3498db; color: white; padding: 12px; text-align: left; }
            td { padding: 10px; border-bottom: 1px solid #ecf0f1; }
            .error { color: #e74c3c; }
            .warning { color: #f39c12; }
            .info { color: #3498db; }
        )";
    }
    
    if (!tmpl.customCss.isEmpty()) {
        css += "\n" + tmpl.customCss;
    }
    
    QString html = QString(R"(<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Log Analysis Report</title>
    <style>%1</style>
</head>
<body>
)").arg(css);
    
    // Header
    if (!tmpl.headerHtml.isEmpty()) {
        html += tmpl.headerHtml;
    } else {
        html += QString("<h1>%1</h1>").arg(tr("Log Analysis Report"));
        if (!tmpl.companyName.isEmpty()) {
            html += QString("<p>%1</p>").arg(tmpl.companyName);
        }
        html += QString("<p>%1: %2</p>").arg(tr("Generated")).arg(
            QDateTime::currentDateTime().toString(Qt::ISODate));
    }
    
    // Summary section
    if (tmpl.includeSummary) {
        html += QString(R"(
<div class="card">
    <h2>%1</h2>
    <table>
        <tr><th>%2</th><td>-</td></tr>
        <tr><th>%3</th><td>-</td></tr>
        <tr><th>%4</th><td>-</td></tr>
        <tr><th>%5</th><td>-</td></tr>
    </table>
</div>
)").arg(tr("Summary"))
   .arg(tr("Total Lines"))
   .arg(tr("Errors"))
   .arg(tr("Warnings"))
   .arg(tr("Time Range"));
    }
    
    // Error distribution
    if (tmpl.includeErrorDistribution) {
        html += QString(R"(
<div class="card">
    <h2>%1</h2>
    <p>%2</p>
</div>
)").arg(tr("Error Distribution"))
   .arg(tr("No data available - connect to BigFileModel for actual statistics"));
    }
    
    // Timeline
    if (tmpl.includeTimeline) {
        html += QString(R"(
<div class="card">
    <h2>%1</h2>
    <p>%2</p>
</div>
)").arg(tr("Activity Timeline"))
   .arg(tr("Timeline visualization would be generated here"));
    }
    
    // Top messages
    if (tmpl.includeTopMessages) {
        html += QString(R"(
<div class="card">
    <h2>%1</h2>
    <table>
        <tr><th>%2</th><th>%3</th><th>%4</th></tr>
        <tr><td colspan="3">%5</td></tr>
    </table>
</div>
)").arg(tr("Top Messages"))
   .arg(tr("Count"))
   .arg(tr("Level"))
   .arg(tr("Message"))
   .arg(tr("No data available"));
    }
    
    // Bookmarks
    if (tmpl.includeBookmarks) {
        html += QString(R"(
<div class="card">
    <h2>%1</h2>
    <p>%2</p>
</div>
)").arg(tr("Bookmarks"))
   .arg(tr("Bookmarked lines would be listed here"));
    }
    
    // Footer
    if (!tmpl.footerHtml.isEmpty()) {
        html += tmpl.footerHtml;
    } else {
        html += QString("<hr><p style='text-align:center;color:#888;'>%1 BigFileViewer</p>")
            .arg(tr("Generated by"));
    }
    
    html += "</body></html>";
    return html;
}

QString ReportScheduler::generateCsvReport(const ReportSchedule &schedule)
{
    Q_UNUSED(schedule)
    
    QString csv;
    QTextStream stream(&csv);
    
    // Header
    stream << "Timestamp,Level,Source,Message\n";
    
    // Placeholder data
    stream << "\"" << QDateTime::currentDateTime().toString(Qt::ISODate) << "\","
           << "INFO,System,\"Report generated - connect to BigFileModel for actual data\"\n";
    
    return csv;
}

QString ReportScheduler::generateJsonReport(const ReportSchedule &schedule)
{
    Q_UNUSED(schedule)
    
    QJsonObject root;
    root["generated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["generator"] = "BigFileViewer";
    
    QJsonObject summary;
    summary["totalLines"] = 0;
    summary["errors"] = 0;
    summary["warnings"] = 0;
    summary["info"] = 0;
    root["summary"] = summary;
    
    root["entries"] = QJsonArray();
    
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

QString ReportScheduler::generateMarkdownReport(const ReportSchedule &schedule, const ReportTemplate &tmpl)
{
    Q_UNUSED(schedule)
    
    QString md;
    QTextStream stream(&md);
    
    stream << "# " << tr("Log Analysis Report") << "\n\n";
    
    if (!tmpl.companyName.isEmpty()) {
        stream << "**" << tmpl.companyName << "**\n\n";
    }
    
    stream << tr("Generated") << ": " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    
    if (tmpl.includeSummary) {
        stream << "## " << tr("Summary") << "\n\n";
        stream << "| " << tr("Metric") << " | " << tr("Value") << " |\n";
        stream << "|--------|-------|\n";
        stream << "| " << tr("Total Lines") << " | - |\n";
        stream << "| " << tr("Errors") << " | - |\n";
        stream << "| " << tr("Warnings") << " | - |\n\n";
    }
    
    if (tmpl.includeErrorDistribution) {
        stream << "## " << tr("Error Distribution") << "\n\n";
        stream << tr("No data available") << "\n\n";
    }
    
    stream << "---\n" << tr("Generated by") << " BigFileViewer\n";
    
    return md;
}

bool ReportScheduler::generatePdfReport(const QString &htmlContent, const QString &outputPath)
{
    // PDF generation would require QPrinter or external library
    // For now, save as HTML and note that PDF conversion is needed
    Q_UNUSED(htmlContent)
    Q_UNUSED(outputPath)
    
    // Could use wkhtmltopdf or similar external tool  
    return false;
}

bool ReportScheduler::generateXlsxReport(const ReportSchedule &schedule, const QString &outputPath)
{
    Q_UNUSED(schedule)
    Q_UNUSED(outputPath)
    
    // XLSX generation would require external library like QXlsx
    return false;
}

bool ReportScheduler::distributeReport(const ReportSchedule &schedule, const QString &filePath)
{
    bool success = true;
    
    if (schedule.emailEnabled && !schedule.emailRecipients.isEmpty()) {
        bool emailSuccess = sendReportByEmail(schedule, filePath);
        emit reportDistributed(schedule.id, "email", emailSuccess);
        success = success && emailSuccess;
    }
    
    if (schedule.webhookEnabled && !schedule.webhookUrl.isEmpty()) {
        bool webhookSuccess = sendReportToWebhook(schedule, filePath);
        emit reportDistributed(schedule.id, "webhook", webhookSuccess);
        success = success && webhookSuccess;
    }
    
    if (schedule.ftpEnabled && !schedule.ftpServer.isEmpty()) {
        bool ftpSuccess = uploadReportToFtp(schedule, filePath);
        emit reportDistributed(schedule.id, "ftp", ftpSuccess);
        success = success && ftpSuccess;
    }
    
    return success;
}

bool ReportScheduler::sendReportByEmail(const ReportSchedule &schedule, const QString &filePath)
{
    // Would integrate with SmtpAlertManager
    Q_UNUSED(schedule)
    Q_UNUSED(filePath)
    return false;
}

bool ReportScheduler::sendReportToWebhook(const ReportSchedule &schedule, const QString &filePath)
{
    Q_UNUSED(schedule)
    Q_UNUSED(filePath)
    // Would use NotificationManager webhook functionality
    return false;
}

bool ReportScheduler::uploadReportToFtp(const ReportSchedule &schedule, const QString &filePath)
{
    Q_UNUSED(schedule)
    Q_UNUSED(filePath)
    // Would use QNetworkAccessManager with FTP
    return false;
}

bool ReportScheduler::uploadReportToSharePoint(const ReportSchedule &schedule, const QString &filePath)
{
    Q_UNUSED(schedule)
    Q_UNUSED(filePath)
    // Would use MS Graph API
    return false;
}

QString ReportScheduler::expandFilenamePattern(const QString &pattern) const
{
    QDateTime now = QDateTime::currentDateTime();
    QString result = pattern;
    
    result.replace("{date}", now.toString("yyyy-MM-dd"));
    result.replace("{time}", now.toString("HHmmss"));
    result.replace("{datetime}", now.toString("yyyyMMdd_HHmmss"));
    result.replace("{year}", now.toString("yyyy"));
    result.replace("{month}", now.toString("MM"));
    result.replace("{day}", now.toString("dd"));
    result.replace("{hour}", now.toString("HH"));
    result.replace("{minute}", now.toString("mm"));
    
    return result;
}

QDateTime ReportScheduler::calculateNextRun(const QVariantMap &config) const
{
    QDateTime now = QDateTime::currentDateTime();
    auto frequency = static_cast<ReportSchedule::Frequency>(
        config.value("frequency", ReportSchedule::Daily).toInt());
    QTime timeOfDay = config.value("timeOfDay", QTime(8, 0)).toTime();
    int dayOfWeek = config.value("dayOfWeek", 1).toInt();
    int dayOfMonth = config.value("dayOfMonth", 1).toInt();
    
    QDateTime next;
    
    switch (frequency) {
    case ReportSchedule::Once:
        return QDateTime(); // No next run for one-time
        
    case ReportSchedule::Hourly:
        next = now.addSecs(3600);
        next.setTime(QTime(next.time().hour(), 0));
        break;
        
    case ReportSchedule::Daily:
        next = QDateTime(now.date(), timeOfDay);
        if (next <= now) {
            next = next.addDays(1);
        }
        break;
        
    case ReportSchedule::Weekly: {
        next = QDateTime(now.date(), timeOfDay);
        int currentDay = now.date().dayOfWeek();
        int daysUntilTarget = (dayOfWeek - currentDay + 7) % 7;
        if (daysUntilTarget == 0 && next <= now) {
            daysUntilTarget = 7;
        }
        next = next.addDays(daysUntilTarget);
        break;
    }
        
    case ReportSchedule::Monthly: {
        QDate targetDate(now.date().year(), now.date().month(), 
                         qMin(dayOfMonth, now.date().daysInMonth()));
        next = QDateTime(targetDate, timeOfDay);
        if (next <= now) {
            targetDate = targetDate.addMonths(1);
            targetDate.setDate(targetDate.year(), targetDate.month(),
                              qMin(dayOfMonth, targetDate.daysInMonth()));
            next = QDateTime(targetDate, timeOfDay);
        }
        break;
    }
        
    case ReportSchedule::Custom:
        next = parseNextCronRun(config.value("cronExpression").toString());
        break;
    }
    
    return next;
}

QDateTime ReportScheduler::parseNextCronRun(const QString &cronExpr) const
{
    // Simple cron parser - would need full implementation for production
    Q_UNUSED(cronExpr)
    return QDateTime::currentDateTime().addDays(1);
}

QVariantList ReportScheduler::getReportHistory(int limit) const
{
    QVariantList result;
    int count = qMin(limit, m_history.size());
    
    for (int i = 0; i < count; ++i) {
        const auto &entry = m_history[i];
        QVariantMap item;
        item["id"] = entry.id;
        item["scheduleId"] = entry.scheduleId;
        item["scheduleName"] = entry.scheduleName;
        item["timestamp"] = entry.timestamp;
        item["filePath"] = entry.filePath;
        item["fileSize"] = entry.fileSize;
        item["success"] = entry.success;
        item["error"] = entry.error;
        result.append(item);
    }
    
    return result;
}

bool ReportScheduler::deleteReportFromHistory(const QString &reportId)
{
    for (int i = 0; i < m_history.size(); ++i) {
        if (m_history[i].id == reportId) {
            // Also delete the file if it exists
            QFile::remove(m_history[i].filePath);
            m_history.removeAt(i);
            saveSettings();
            return true;
        }
    }
    return false;
}

void ReportScheduler::clearHistory()
{
    m_history.clear();
    saveSettings();
}

void ReportScheduler::addToHistory(const QString &scheduleId, const QString &filePath, 
                                   bool success, const QString &error)
{
    ReportHistoryEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.scheduleId = scheduleId;
    entry.timestamp = QDateTime::currentDateTime();
    entry.filePath = filePath;
    entry.success = success;
    entry.error = error;
    
    auto it = m_schedules.find(scheduleId);
    if (it != m_schedules.end()) {
        entry.scheduleName = it.value()->name;
    }
    
    if (!filePath.isEmpty()) {
        QFileInfo fi(filePath);
        entry.fileSize = fi.size();
    }
    
    m_history.prepend(entry);
    
    // Keep history size manageable
    while (m_history.size() > 100) {
        m_history.removeLast();
    }
    
    saveSettings();
}

QVariantList ReportScheduler::getSchedulesVariant() const
{
    QVariantList result;
    for (const auto &schedule : m_schedules) {
        result.append(getSchedule(schedule->id));
    }
    return result;
}

QVariantList ReportScheduler::getTemplatesVariant() const
{
    QVariantList result;
    for (const auto &tmpl : m_templates) {
        result.append(getTemplate(tmpl->id));
    }
    return result;
}

QStringList ReportScheduler::getAvailableFormats() const
{
    return {"HTML", "PDF", "CSV", "JSON", "XLSX", "Markdown"};
}

QString ReportScheduler::formatBytes(qint64 bytes) const
{
    const char* units[] = {"B", "KB", "MB", "GB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024 && unit < 3) {
        size /= 1024;
        unit++;
    }
    
    return QString::number(size, 'f', 2) + " " + units[unit];
}
