/**
 * @file FeatureGate.h
 * @brief 功能授权门控系统
 * @details 管理 Free/Pro/Enterprise 版本的功能访问权限
 *          - 检查编译时功能开关（BFV_PRO_FEATURES, BFV_REMOTE_FEATURES）
 *          - 检查运行时许可证状态
 *          - 提供升级提示对话框触发
 */

#ifndef FEATUREGATE_H
#define FEATUREGATE_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include "BuildConfig.h"

/**
 * @class FeatureGate
 * @brief 功能授权门控单例类
 * 
 * 使用方式：
 * - C++: FeatureGate::instance().isEnabled(Feature::Statistics)
 * - QML: _featureGate.isFeatureEnabled("statistics")
 */
class FeatureGate : public QObject
{
    Q_OBJECT
    
    // ========================================================================
    // QML 属性 - 编译时功能标志
    // ========================================================================
    
    /** @brief Pro 版功能是否编译进二进制 */
    Q_PROPERTY(bool hasProFeatures READ hasProFeatures CONSTANT)
    
    /** @brief Enterprise 远程功能是否编译进二进制 */
    Q_PROPERTY(bool hasRemoteFeatures READ hasRemoteFeatures CONSTANT)
    
    /** @brief 许可证系统是否启用 */
    Q_PROPERTY(bool hasLicenseSystem READ hasLicenseSystem CONSTANT)
    
    // ========================================================================
    // QML 属性 - 运行时授权状态
    // ========================================================================
    
    /** @brief 当前授权等级名称 (Free/Pro/Enterprise) */
    Q_PROPERTY(QString currentTier READ currentTierName NOTIFY tierChanged)
    
    /** @brief 是否已注册（非试用） */
    Q_PROPERTY(bool isRegistered READ isRegistered NOTIFY tierChanged)
    
    /** @brief 是否为 Pro 用户（Pro 或 Enterprise） */
    Q_PROPERTY(bool isProUser READ isProUser NOTIFY tierChanged)

public:
    /**
     * @brief 功能枚举 - 所有可控功能列表
     */
    enum class Feature {
        // === Free 版功能（始终可用）===
        Bookmarks,              ///< 书签功能
        BookmarkComments,       ///< 书签评论
        ExportTxt,              ///< 导出 TXT
        ExportCsv,              ///< 导出 CSV
        ExportHtml,             ///< 导出 HTML
        ClipboardImport,        ///< 剪贴板导入
        DirectoryMonitor,       ///< 目录监控
        
        // === Pro 版功能 ===
        FilterTemplates,        ///< 过滤模板
        Workspaces,             ///< 工作区/会话保存
        DeltaTime,              ///< 经过时间列
        RollingLogs,            ///< 滚动日志合并
        Statistics,             ///< 统计面板
        
        // === Enterprise 版功能 ===
        RemoteFiles,            ///< SFTP/SSH 远程文件
        SmtpAlerts,             ///< SMTP 邮件报警
        JiraIntegration,        ///< Jira 集成
        GitHubIntegration       ///< GitHub 集成
    };
    Q_ENUM(Feature)
    
    /**
     * @brief 授权等级枚举
     */
    enum class LicenseTier {
        Free = 0,           ///< 免费版
        Pro = 1,            ///< Pro 版 (¥39)
        Enterprise = 2      ///< Enterprise 版 (¥199)
    };
    Q_ENUM(LicenseTier)

    /**
     * @brief 获取单例实例
     */
    static FeatureGate& instance();
    
    // 禁用拷贝和移动
    FeatureGate(const FeatureGate&) = delete;
    FeatureGate& operator=(const FeatureGate&) = delete;

    // ========================================================================
    // 编译时功能检查
    // ========================================================================
    
    /** @brief 检查 Pro 功能是否编译进二进制 */
    bool hasProFeatures() const;
    
    /** @brief 检查 Enterprise 远程功能是否编译进二进制 */
    bool hasRemoteFeatures() const;
    
    /** @brief 检查许可证系统是否启用 */
    bool hasLicenseSystem() const;
    
    // ========================================================================
    // 运行时授权检查
    // ========================================================================
    
    /**
     * @brief 检查功能是否可用（综合编译时+运行时）
     * @param feature 功能枚举值
     * @return true 如果功能可用
     */
    bool isEnabled(Feature feature) const;
    
    /**
     * @brief QML 调用 - 通过字符串检查功能
     * @param featureName 功能名称字符串
     * @return true 如果功能可用
     */
    Q_INVOKABLE bool isFeatureEnabled(const QString &featureName) const;
    
    /**
     * @brief 获取当前授权等级
     */
    LicenseTier currentTier() const;
    
    /**
     * @brief 获取当前授权等级名称
     */
    QString currentTierName() const;
    
    /**
     * @brief 是否已注册（非试用）
     */
    bool isRegistered() const;
    
    /**
     * @brief 是否为 Pro 用户（Pro 或 Enterprise）
     */
    bool isProUser() const;
    
    /**
     * @brief 检查功能是否可用，如不可用则显示升级提示
     * @param featureName 功能名称字符串
     * @return true 如果功能可用
     */
    Q_INVOKABLE bool canUseFeature(const QString &featureName);
    
    // ========================================================================
    // 升级提示
    // ========================================================================
    
    /**
     * @brief 显示升级提示对话框
     * @param feature 需要的功能
     * @details 发射 upgradePromptRequested 信号，由 QML 处理显示对话框
     */
    Q_INVOKABLE void showUpgradePrompt(const QString &featureName);
    
    /**
     * @brief 获取功能的详细信息（用于升级提示）
     * @param featureName 功能名称
     * @return 包含 title, description, requiredTier 的 QVariantMap
     */
    Q_INVOKABLE QVariantMap getFeatureInfo(const QString &featureName) const;
    
    /**
     * @brief 获取功能所需的最低授权等级
     * @param feature 功能枚举值
     * @return 所需的授权等级
     */
    LicenseTier requiredTier(Feature feature) const;
    
    /**
     * @brief 刷新授权状态（重新检查许可证）
     */
    Q_INVOKABLE void refreshLicenseStatus();

signals:
    /**
     * @brief 授权等级变化信号
     */
    void tierChanged();
    
    /**
     * @brief 升级提示请求信号
     * @param featureInfo 功能信息（title, description, requiredTier, purchaseUrl）
     */
    void upgradePromptRequested(const QVariantMap &featureInfo);

private:
    FeatureGate();
    ~FeatureGate() = default;
    
    /**
     * @brief 从字符串解析功能枚举
     */
    Feature parseFeatureName(const QString &name) const;
    
    /**
     * @brief 缓存的当前授权等级
     */
    LicenseTier m_currentTier;
};

#endif // FEATUREGATE_H
