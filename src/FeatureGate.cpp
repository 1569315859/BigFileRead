/**
 * @file FeatureGate.cpp
 * @brief 功能授权门控系统实现
 */

#include "FeatureGate.h"
#include "BuildConfig.h"  // 功能开关宏定义
#include "LicenseManager.h"
#include "TrialManager.h"
#include <QSettings>
#include <QDebug>

// ============================================================================
// 单例实现
// ============================================================================

FeatureGate& FeatureGate::instance()
{
    static FeatureGate instance;
    return instance;
}

FeatureGate::FeatureGate()
    : QObject(nullptr)
    , m_currentTier(LicenseTier::Enterprise )  // ★★★ 测试模式：默认 Pro ★★★
{
    // 初始化时检查许可证状态
    // refreshLicenseStatus();  // 暂时禁用，测试时默认 Pro
}

// ============================================================================
// 编译时功能检查
// ============================================================================

bool FeatureGate::hasProFeatures() const
{
#ifdef BFV_PRO_FEATURES
    return true;
#else
    return false;
#endif
}

bool FeatureGate::hasRemoteFeatures() const
{
#ifdef BFV_REMOTE_FEATURES
    return true;
#else
    return false;
#endif
}

bool FeatureGate::hasLicenseSystem() const
{
#ifdef ENABLE_LICENSE_SYSTEM
    return true;
#else
    return false;
#endif
}

// ============================================================================
// 运行时授权检查
// ============================================================================

FeatureGate::LicenseTier FeatureGate::requiredTier(Feature feature) const
{
    switch (feature) {
        // Free 版功能
        case Feature::Bookmarks:
        case Feature::BookmarkComments:
        case Feature::ExportTxt:
        case Feature::ExportCsv:
        case Feature::ExportHtml:
        case Feature::ClipboardImport:
        case Feature::DirectoryMonitor:
            return LicenseTier::Free;
        
        // Pro 版功能
        case Feature::FilterTemplates:
        case Feature::Workspaces:
        case Feature::DeltaTime:
        case Feature::RollingLogs:
        case Feature::Statistics:
            return LicenseTier::Pro;
        
        // Enterprise 版功能
        case Feature::RemoteFiles:
        case Feature::SmtpAlerts:
        case Feature::JiraIntegration:
        case Feature::GitHubIntegration:
            return LicenseTier::Enterprise;
    }
    
    return LicenseTier::Enterprise;  // 默认最高等级
}

bool FeatureGate::isEnabled(Feature feature) const
{
    LicenseTier required = requiredTier(feature);
    
    qDebug() << "FeatureGate::isEnabled - feature:" << static_cast<int>(feature)
             << "required tier:" << static_cast<int>(required)
             << "current tier:" << static_cast<int>(m_currentTier)
             << "hasProFeatures:" << hasProFeatures();
    
    // ★★★ 测试模式：跳过编译时检查 ★★★
#if 1  // 改为 0 恢复正常检查
    // 测试模式下只检查授权等级
    if (required == LicenseTier::Free) {
        return true;
    }
    bool allowed = static_cast<int>(m_currentTier) >= static_cast<int>(required);
    qDebug() << "  -> TEST MODE License check:" << (allowed ? "ALLOWED" : "BLOCKED");
    return allowed;
#else
    // 1. 检查编译时功能开关
    if (required == LicenseTier::Pro && !hasProFeatures()) {
        qDebug() << "  -> BLOCKED: Pro features not compiled in";
        return false;  // Pro 功能未编译进二进制
    }
    if (required == LicenseTier::Enterprise && !hasRemoteFeatures()) {
        qDebug() << "  -> BLOCKED: Enterprise features not compiled in";
        return false;  // Enterprise 功能未编译进二进制
    }
    
    // 2. 检查运行时授权等级
    // Free 功能始终可用
    if (required == LicenseTier::Free) {
        qDebug() << "  -> ALLOWED: Free feature";
        return true;
    }
    
    // 检查当前授权等级是否足够
    bool allowed = static_cast<int>(m_currentTier) >= static_cast<int>(required);
    qDebug() << "  -> License check:" << (allowed ? "ALLOWED" : "BLOCKED");
    return allowed;
#endif
}

FeatureGate::Feature FeatureGate::parseFeatureName(const QString &name) const
{
    QString lower = name.toLower();
    
    // Free 功能
    if (lower == "bookmarks") return Feature::Bookmarks;
    if (lower == "bookmarkcomments" || lower == "bookmark_comments") return Feature::BookmarkComments;
    if (lower == "exporttxt" || lower == "export_txt") return Feature::ExportTxt;
    if (lower == "exportcsv" || lower == "export_csv") return Feature::ExportCsv;
    if (lower == "exporthtml" || lower == "export_html") return Feature::ExportHtml;
    if (lower == "clipboardimport" || lower == "clipboard_import") return Feature::ClipboardImport;
    if (lower == "directorymonitor" || lower == "directory_monitor") return Feature::DirectoryMonitor;
    
    // Pro 功能
    if (lower == "filtertemplates" || lower == "filter_templates") return Feature::FilterTemplates;
    if (lower == "workspaces") return Feature::Workspaces;
    if (lower == "deltatime" || lower == "delta_time") return Feature::DeltaTime;
    if (lower == "rollinglogs" || lower == "rolling_logs") return Feature::RollingLogs;
    if (lower == "statistics") return Feature::Statistics;
    
    // Enterprise 功能
    if (lower == "remotefiles" || lower == "remote_files") return Feature::RemoteFiles;
    if (lower == "smtpalerts" || lower == "smtp_alerts") return Feature::SmtpAlerts;
    if (lower == "jiraintegration" || lower == "jira_integration" || lower == "jira") return Feature::JiraIntegration;
    if (lower == "githubintegration" || lower == "github_integration" || lower == "github") return Feature::GitHubIntegration;
    
    // 默认返回一个需要最高权限的功能
    qWarning() << "FeatureGate: Unknown feature name:" << name;
    return Feature::GitHubIntegration;
}

bool FeatureGate::isFeatureEnabled(const QString &featureName) const
{
    Feature feature = parseFeatureName(featureName);
    return isEnabled(feature);
}

FeatureGate::LicenseTier FeatureGate::currentTier() const
{
    return m_currentTier;
}

QString FeatureGate::currentTierName() const
{
    switch (m_currentTier) {
        case LicenseTier::Free: return QStringLiteral("Free");
        case LicenseTier::Pro: return QStringLiteral("Pro");
        case LicenseTier::Enterprise: return QStringLiteral("Enterprise");
    }
    return QStringLiteral("Free");
}

bool FeatureGate::isRegistered() const
{
    return m_currentTier != LicenseTier::Free;
}

bool FeatureGate::isProUser() const
{
    return static_cast<int>(m_currentTier) >= static_cast<int>(LicenseTier::Pro);
}

bool FeatureGate::canUseFeature(const QString &featureName)
{
    if (isFeatureEnabled(featureName)) {
        return true;
    }
    // 功能不可用，显示升级提示
    showUpgradePrompt(featureName);
    return false;
}

void FeatureGate::refreshLicenseStatus()
{
    LicenseTier oldTier = m_currentTier;
    
#ifdef ENABLE_LICENSE_SYSTEM
    // 从许可证管理器读取授权等级
    if (LicenseManager::instance().isRegistered()) {
        // 从设置中读取授权等级
        QSettings settings("BigFileViewer", "BigFileViewer");
        int tierValue = settings.value("license/tier", 0).toInt();
        
        if (tierValue >= 2) {
            m_currentTier = LicenseTier::Enterprise;
        } else if (tierValue >= 1) {
            m_currentTier = LicenseTier::Pro;
        } else {
            // 已注册但未指定等级，默认 Pro
            m_currentTier = LicenseTier::Pro;
        }
    } else {
        m_currentTier = LicenseTier::Free;
    }
#else
    // 无许可证系统时，检查简单的设置标志
    QSettings settings("BigFileViewer", "BigFileViewer");
    int tierValue = settings.value("license/tier", 2).toInt();
    
    // 调试模式：允许通过设置直接设定等级（仅开发用）
#ifdef QT_DEBUG
    if (tierValue >= 2) {
        m_currentTier = LicenseTier::Enterprise;
    } else if (tierValue >= 1) {
        m_currentTier = LicenseTier::Pro;
    } else {
        m_currentTier = LicenseTier::Free;
    }
#else
    m_currentTier = LicenseTier::Free;
#endif
    
#endif
    
    if (oldTier != m_currentTier) {
        emit tierChanged();
    }
}

// ============================================================================
// 升级提示
// ============================================================================

QVariantMap FeatureGate::getFeatureInfo(const QString &featureName) const
{
    Feature feature = parseFeatureName(featureName);
    LicenseTier required = requiredTier(feature);
    
    QVariantMap info;
    info["featureName"] = featureName;
    info["requiredTier"] = static_cast<int>(required);
    
    switch (required) {
        case LicenseTier::Pro:
            info["requiredTierName"] = tr("Pro");
            info["price"] = "¥39";
            break;
        case LicenseTier::Enterprise:
            info["requiredTierName"] = tr("Enterprise");
            info["price"] = "¥199";
            break;
        default:
            info["requiredTierName"] = tr("Free");
            info["price"] = "";
            break;
    }
    
    // Feature descriptions (English for translation source)
    switch (feature) {
        case Feature::FilterTemplates:
            info["title"] = tr("Filter Templates");
            info["description"] = tr("Save and load commonly used filter conditions for quick application of preset filter schemes.");
            break;
        case Feature::Workspaces:
            info["title"] = tr("Workspaces");
            info["description"] = tr("Save current session state (open files, filter conditions, scroll position) and restore on next startup.");
            break;
        case Feature::DeltaTime:
            info["title"] = tr("Delta Time Column");
            info["description"] = tr("Display time difference between each log line and the previous one to help analyze performance bottlenecks.");
            break;
        case Feature::RollingLogs:
            info["title"] = tr("Rolling Logs");
            info["description"] = tr("Automatically merge segmented log files (log, log.1, log.2, etc.) and view as continuous logs.");
            break;
        case Feature::Statistics:
            info["title"] = tr("Statistics Panel");
            info["description"] = tr("Visualize log level distribution, time trends, keyword frequency, and other statistical charts.");
            break;
        case Feature::RemoteFiles:
            info["title"] = tr("Remote Files");
            info["description"] = tr("Connect to remote servers via SFTP/SSH to view and monitor remote log files directly.");
            break;
        case Feature::SmtpAlerts:
            info["title"] = tr("Email Alerts");
            info["description"] = tr("Automatically send email notifications when specific keywords (e.g., FATAL) are detected.");
            break;
        case Feature::JiraIntegration:
            info["title"] = tr("Jira Integration");
            info["description"] = tr("Create Jira Issues directly from selected log snippets for bug tracking.");
            break;
        case Feature::GitHubIntegration:
            info["title"] = tr("GitHub Integration");
            info["description"] = tr("Create GitHub Issues directly from selected log snippets.");
            break;
        default:
            info["title"] = featureName;
            info["description"] = "";
            break;
    }
    
    // 购买链接
    info["purchaseUrl"] = QStringLiteral("https://bigfileviewer.com/purchase");
    
    return info;
}

void FeatureGate::showUpgradePrompt(const QString &featureName)
{
    QVariantMap info = getFeatureInfo(featureName);
    emit upgradePromptRequested(info);
}
