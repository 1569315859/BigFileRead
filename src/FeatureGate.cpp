/**
 * @file FeatureGate.cpp
 * @brief 功能授权门控系统实现
 */

#include "FeatureGate.h"
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
    , m_currentTier(LicenseTier::Free)
{
    // 初始化时检查许可证状态
    refreshLicenseStatus();
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
    
    // 1. 检查编译时功能开关
    if (required == LicenseTier::Pro && !hasProFeatures()) {
        return false;  // Pro 功能未编译进二进制
    }
    if (required == LicenseTier::Enterprise && !hasRemoteFeatures()) {
        return false;  // Enterprise 功能未编译进二进制
    }
    
    // 2. 检查运行时授权等级
    // Free 功能始终可用
    if (required == LicenseTier::Free) {
        return true;
    }
    
    // 检查当前授权等级是否足够
    return static_cast<int>(m_currentTier) >= static_cast<int>(required);
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
    int tierValue = settings.value("license/tier", 0).toInt();
    
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
    
    // 功能描述
    switch (feature) {
        case Feature::FilterTemplates:
            info["title"] = tr("过滤模板");
            info["description"] = tr("保存和加载常用的过滤条件，快速应用预设过滤方案。");
            break;
        case Feature::Workspaces:
            info["title"] = tr("工作区");
            info["description"] = tr("保存当前会话状态（打开的文件、过滤条件、滚动位置），下次启动时恢复。");
            break;
        case Feature::DeltaTime:
            info["title"] = tr("耗时列");
            info["description"] = tr("显示每行日志与上一行的时间差，帮助分析性能瓶颈。");
            break;
        case Feature::RollingLogs:
            info["title"] = tr("滚动日志");
            info["description"] = tr("自动合并 log, log.1, log.2 等切割的日志文件，作为连续日志查看。");
            break;
        case Feature::Statistics:
            info["title"] = tr("统计面板");
            info["description"] = tr("可视化展示日志级别分布、时间趋势、关键词频率等统计图表。");
            break;
        case Feature::RemoteFiles:
            info["title"] = tr("远程文件");
            info["description"] = tr("通过 SFTP/SSH 直接连接远程服务器，查看和监控远程日志文件。");
            break;
        case Feature::SmtpAlerts:
            info["title"] = tr("邮件报警");
            info["description"] = tr("当检测到特定关键词（如 FATAL）时，自动发送邮件通知。");
            break;
        case Feature::JiraIntegration:
            info["title"] = tr("Jira 集成");
            info["description"] = tr("将选中的日志片段直接创建为 Jira Issue，便于 Bug 跟踪。");
            break;
        case Feature::GitHubIntegration:
            info["title"] = tr("GitHub 集成");
            info["description"] = tr("将选中的日志片段直接创建为 GitHub Issue。");
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
