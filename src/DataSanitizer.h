/**
 * @file DataSanitizer.h
 * @brief 数据脱敏处理器 - 检测并替换敏感信息
 * @description 支持 IP、邮箱、手机号、身份证、域名、端口、API Key 等敏感数据的自动检测与脱敏
 *              用于 AI 询问和数据导出时保护用户隐私
 */

#ifndef DATASANITIZER_H
#define DATASANITIZER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QVariantMap>
#include <QVariantList>
#include <QMap>
#include <QPair>

/**
 * @brief 脱敏规则结构体
 */
struct SanitizeRule {
    QString name;               ///< 规则名称（如 "IP地址"）
    QString pattern;            ///< 正则表达式模式
    QString replacement;        ///< 替换文本（支持 $1 等占位符）
    bool enabled;               ///< 是否启用
    bool isBuiltin;             ///< 是否为内置规则
    
    SanitizeRule() : enabled(true), isBuiltin(true) {}
    SanitizeRule(const QString &n, const QString &p, const QString &r, bool e = true, bool builtin = true)
        : name(n), pattern(p), replacement(r), enabled(e), isBuiltin(builtin) {}
};

/**
 * @brief 脱敏预览项 - 用于在 UI 中显示将被脱敏的内容
 */
struct SanitizePreviewItem {
    Q_GADGET
    Q_PROPERTY(int start MEMBER start)
    Q_PROPERTY(int end MEMBER end)
    Q_PROPERTY(QString original MEMBER original)
    Q_PROPERTY(QString replacement MEMBER replacement)
    Q_PROPERTY(QString ruleName MEMBER ruleName)
public:
    int start;                  ///< 起始位置
    int end;                    ///< 结束位置
    QString original;           ///< 原始文本
    QString replacement;        ///< 替换后的文本
    QString ruleName;           ///< 匹配的规则名称
};

/**
 * @class DataSanitizer
 * @brief 数据脱敏处理器
 * 
 * 功能特性:
 * - 内置多种常见敏感数据模式（IP、邮箱、手机号、身份证、API Key 等）
 * - 支持自定义正则表达式规则
 * - 提供脱敏预览功能，高亮显示将被替换的内容
 * - 线程安全
 */
class DataSanitizer : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QVariantList rules READ getRulesVariant NOTIFY rulesChanged)
    Q_PROPERTY(bool ipEnabled READ isIpEnabled WRITE setIpEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool emailEnabled READ isEmailEnabled WRITE setEmailEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool phoneEnabled READ isPhoneEnabled WRITE setPhoneEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool idCardEnabled READ isIdCardEnabled WRITE setIdCardEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool domainEnabled READ isDomainEnabled WRITE setDomainEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool portEnabled READ isPortEnabled WRITE setPortEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool apiKeyEnabled READ isApiKeyEnabled WRITE setApiKeyEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool usernameEnabled READ isUsernameEnabled WRITE setUsernameEnabled NOTIFY rulesChanged)
    Q_PROPERTY(bool pathEnabled READ isPathEnabled WRITE setPathEnabled NOTIFY rulesChanged)

public:
    /**
     * @brief 单例访问器
     */
    static DataSanitizer& instance();
    
    // 禁用拷贝
    DataSanitizer(const DataSanitizer&) = delete;
    DataSanitizer& operator=(const DataSanitizer&) = delete;
    
    // ===================== 脱敏操作 =====================
    
    /**
     * @brief 对文本进行脱敏处理
     * @param text 原始文本
     * @return 脱敏后的文本
     */
    Q_INVOKABLE QString sanitize(const QString &text) const;
    
    /**
     * @brief 对文本进行脱敏处理（按级别）
     * @param text 原始文本
     * @param level 脱敏级别：0=Minimal, 1=Standard, 2=Strict
     * @return 脱敏后的文本
     */
    Q_INVOKABLE QString sanitize(const QString &text, int level) const;
    
    /**
     * @brief 对多行文本进行脱敏处理
     * @param lines 原始文本列表
     * @return 脱敏后的文本列表
     */
    Q_INVOKABLE QStringList sanitizeLines(const QStringList &lines) const;
    
    /**
     * @brief 获取脱敏预览（用于 UI 显示）
     * @param text 原始文本
     * @return 预览项列表，包含匹配位置和替换信息
     */
    Q_INVOKABLE QVariantList getSanitizePreview(const QString &text) const;
    
    /**
     * @brief 使用指定选项进行脱敏
     * @param text 原始文本
     * @param options 脱敏选项 (键: 规则名称, 值: 是否启用)
     * @return 脱敏后的文本
     */
    Q_INVOKABLE QString sanitizeWithOptions(const QString &text, const QVariantMap &options) const;
    
    // ===================== 规则管理 =====================
    
    /**
     * @brief 添加自定义脱敏规则
     * @param name 规则名称
     * @param pattern 正则表达式模式
     * @param replacement 替换文本
     * @return 成功返回 true
     */
    Q_INVOKABLE bool addCustomRule(const QString &name, const QString &pattern, const QString &replacement);
    
    /**
     * @brief 移除自定义规则
     * @param name 规则名称
     * @return 成功返回 true
     */
    Q_INVOKABLE bool removeCustomRule(const QString &name);
    
    /**
     * @brief 启用/禁用指定规则
     * @param name 规则名称
     * @param enabled 是否启用
     */
    Q_INVOKABLE void setRuleEnabled(const QString &name, bool enabled);
    
    /**
     * @brief 检查规则是否启用
     * @param name 规则名称
     * @return 是否启用
     */
    Q_INVOKABLE bool isRuleEnabled(const QString &name) const;
    
    /**
     * @brief 获取所有规则列表（用于 QML）
     * @return 规则列表 QVariantList
     */
    Q_INVOKABLE QVariantList getRulesVariant() const;
    
    /**
     * @brief 获取所有自定义规则
     * @return 自定义规则列表
     */
    Q_INVOKABLE QVariantList getCustomRules() const;
    
    /**
     * @brief 重置为默认规则
     */
    Q_INVOKABLE void resetToDefaults();
    
    /**
     * @brief 检查是否有启用的脱敏规则
     * @return 如果有任何脱敏规则启用则返回 true
     */
    Q_INVOKABLE bool hasEnabledRules() const;
    
    // ===================== 便捷开关 =====================
    
    bool isIpEnabled() const;
    void setIpEnabled(bool enabled);
    
    bool isEmailEnabled() const;
    void setEmailEnabled(bool enabled);
    
    bool isPhoneEnabled() const;
    void setPhoneEnabled(bool enabled);
    
    bool isIdCardEnabled() const;
    void setIdCardEnabled(bool enabled);
    
    bool isDomainEnabled() const;
    void setDomainEnabled(bool enabled);
    
    bool isPortEnabled() const;
    void setPortEnabled(bool enabled);
    
    bool isApiKeyEnabled() const;
    void setApiKeyEnabled(bool enabled);
    
    bool isUsernameEnabled() const;
    void setUsernameEnabled(bool enabled);
    
    bool isPathEnabled() const;
    void setPathEnabled(bool enabled);
    
    // ===================== 持久化 =====================
    
    /**
     * @brief 保存规则配置到设置
     */
    Q_INVOKABLE void saveSettings();
    
    /**
     * @brief 从设置加载规则配置
     */
    Q_INVOKABLE void loadSettings();

signals:
    /**
     * @brief 规则变更信号
     */
    void rulesChanged();

private:
    DataSanitizer(QObject *parent = nullptr);
    ~DataSanitizer() = default;
    
    /**
     * @brief 初始化内置规则
     */
    void initBuiltinRules();
    
    /**
     * @brief 编译正则表达式
     */
    void compilePatterns();
    
    // 规则存储
    QMap<QString, SanitizeRule> m_rules;
    
    // 编译后的正则表达式缓存
    mutable QMap<QString, QRegularExpression> m_compiledPatterns;
    
    // 内置规则名称常量
    static const QString RULE_IP;
    static const QString RULE_IPV6;
    static const QString RULE_EMAIL;
    static const QString RULE_PHONE_CN;
    static const QString RULE_PHONE_INTL;
    static const QString RULE_IDCARD_CN;
    static const QString RULE_DOMAIN;
    static const QString RULE_PORT;
    static const QString RULE_API_KEY;
    static const QString RULE_TOKEN;
    static const QString RULE_PASSWORD;
    static const QString RULE_USERNAME;
    static const QString RULE_PATH_WIN;
    static const QString RULE_PATH_UNIX;
    static const QString RULE_MAC_ADDRESS;
    static const QString RULE_CREDIT_CARD;
    static const QString RULE_SSN;
};

#endif // DATASANITIZER_H
