/**
 * @file AIAnalysisManager.h
 * @brief AI 分析服务管理器 - 集成多种 AI API 用于日志分析
 * @description 支持 OpenAI、Azure OpenAI、Claude、通义千问、文心一言、讯飞星火、DeepSeek、Moonshot 等
 *              以及自定义 API endpoint，支持代理配置
 */

#ifndef AIANALYSISMANAGER_H
#define AIANALYSISMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>

/**
 * @brief AI 服务提供商枚举
 */
enum class AIProvider {
    OpenAI,         ///< OpenAI (GPT-4, GPT-3.5)
    AzureOpenAI,    ///< Azure OpenAI Service
    Claude,         ///< Anthropic Claude
    Qwen,           ///< 阿里云通义千问
    Ernie,          ///< 百度文心一言
    Spark,          ///< 讯飞星火
    DeepSeek,       ///< DeepSeek
    Moonshot,       ///< Moonshot (Kimi)
    Custom          ///< 自定义 API
};

/**
 * @brief AI 服务配置结构
 */
struct AIServiceConfig {
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString endpoint MEMBER endpoint)
    Q_PROPERTY(QString model MEMBER model)
public:
    QString id;             ///< 服务 ID
    QString name;           ///< 显示名称
    QString endpoint;       ///< API 端点
    QString model;          ///< 当前选中的模型
    QStringList models;     ///< 可用模型列表
    QString apiKeyHeader;   ///< API Key 请求头名称
    QString apiKeyPrefix;   ///< API Key 前缀 (如 "Bearer ")
    bool isCustom;          ///< 是否为自定义服务
};

/**
 * @brief 代理配置
 */
struct ProxyConfig {
    enum Type { NoProxy, SystemProxy, CustomProxy };
    
    Type type = SystemProxy;
    QString host;
    int port = 0;
    QString username;
    QString password;
    bool isSocks5 = false;
};

/**
 * @class AIAnalysisManager
 * @brief AI 分析服务管理器
 * 
 * 功能特性:
 * - 内置多种国内外 AI 服务配置
 * - 支持自定义 API endpoint
 * - API Key 加密存储
 * - 代理支持（系统代理 / 自定义代理 / 直连）
 * - 流式响应支持
 */
class AIAnalysisManager : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QVariantList availableServices READ getAvailableServices CONSTANT)
    Q_PROPERTY(QString currentService READ currentService WRITE setCurrentService NOTIFY currentServiceChanged)
    Q_PROPERTY(bool isAnalyzing READ isAnalyzing NOTIFY analyzingStateChanged)
    Q_PROPERTY(QString proxyType READ proxyType WRITE setProxyType NOTIFY proxyChanged)

public:
    /**
     * @brief 单例访问器
     */
    static AIAnalysisManager& instance();
    
    // 禁用拷贝
    AIAnalysisManager(const AIAnalysisManager&) = delete;
    AIAnalysisManager& operator=(const AIAnalysisManager&) = delete;
    
    // ===================== 服务管理 =====================
    
    /**
     * @brief 获取所有可用的 AI 服务列表
     * @return 服务列表 [{id, name, endpoint, model, hasApiKey}, ...]
     */
    Q_INVOKABLE QVariantList getAvailableServices() const;
    
    /**
     * @brief 获取当前选中的服务 ID
     */
    Q_INVOKABLE QString currentService() const { return m_currentServiceId; }
    
    /**
     * @brief 设置当前服务
     * @param serviceId 服务 ID
     */
    Q_INVOKABLE void setCurrentService(const QString &serviceId);
    
    /**
     * @brief 添加自定义 AI 服务
     * @param name 服务名称
     * @param endpoint API 端点 URL
     * @param model 模型名称
     * @param apiKeyHeader API Key 请求头 (默认 "Authorization")
     * @param apiKeyPrefix API Key 前缀 (默认 "Bearer ")
     * @return 服务 ID
     */
    Q_INVOKABLE QString addCustomService(const QString &name, const QString &endpoint,
                                          const QString &model, const QString &apiKeyHeader = "Authorization",
                                          const QString &apiKeyPrefix = "Bearer ");
    
    /**
     * @brief 移除自定义服务
     * @param serviceId 服务 ID
     * @return 成功返回 true
     */
    Q_INVOKABLE bool removeCustomService(const QString &serviceId);
    
    /**
     * @brief 获取服务的可用模型列表
     * @param serviceId 服务 ID
     * @return 模型名称列表
     */
    Q_INVOKABLE QStringList getModelsForService(const QString &serviceId) const;
    
    /**
     * @brief 获取服务当前选中的模型
     * @param serviceId 服务 ID
     * @return 模型名称
     */
    Q_INVOKABLE QString getCurrentModel(const QString &serviceId) const;
    
    /**
     * @brief 设置服务的模型
     * @param serviceId 服务 ID
     * @param model 模型名称
     */
    Q_INVOKABLE void setModel(const QString &serviceId, const QString &model);
    
    // ===================== API Key 管理 =====================
    
    /**
     * @brief 设置 API Key（加密存储）
     * @param serviceId 服务 ID
     * @param apiKey API Key
     */
    Q_INVOKABLE void setApiKey(const QString &serviceId, const QString &apiKey);
    
    /**
     * @brief 获取 API Key（解密）
     * @param serviceId 服务 ID
     * @return API Key（用于显示时应掩码处理）
     */
    Q_INVOKABLE QString getApiKey(const QString &serviceId) const;
    
    /**
     * @brief 检查服务是否已配置 API Key
     * @param serviceId 服务 ID
     * @return 是否已配置
     */
    Q_INVOKABLE bool hasApiKey(const QString &serviceId) const;
    
    /**
     * @brief 获取掩码后的 API Key（用于 UI 显示）
     * @param serviceId 服务 ID
     * @return 如 "sk-****...****"
     */
    Q_INVOKABLE QString getMaskedApiKey(const QString &serviceId) const;
    
    // ===================== 代理配置 =====================
    
    /**
     * @brief 获取代理类型
     * @return "none", "system", "custom"
     */
    Q_INVOKABLE QString proxyType() const;
    
    /**
     * @brief 设置代理类型
     * @param type "none", "system", "custom"
     */
    Q_INVOKABLE void setProxyType(const QString &type);
    
    /**
     * @brief 设置自定义代理
     * @param host 代理主机
     * @param port 代理端口
     * @param isSocks5 是否为 SOCKS5 代理
     * @param username 用户名（可选）
     * @param password 密码（可选）
     */
    Q_INVOKABLE void setCustomProxy(const QString &host, int port, bool isSocks5 = false,
                                     const QString &username = QString(), const QString &password = QString());
    
    /**
     * @brief 获取自定义代理配置
     * @return {host, port, isSocks5, username}
     */
    Q_INVOKABLE QVariantMap getCustomProxy() const;
    
    // ===================== AI 分析 =====================
    
    /**
     * @brief 分析日志内容
     * @param logContent 日志内容（应已脱敏）
     * @param question 用户问题
     * @param systemPrompt 系统提示词（可选，有默认值）
     */
    Q_INVOKABLE void analyzeLog(const QString &logContent, const QString &question,
                                 const QString &systemPrompt = QString());
    
    /**
     * @brief 取消当前分析
     */
    Q_INVOKABLE void cancelAnalysis();
    
    /**
     * @brief 是否正在分析中
     */
    Q_INVOKABLE bool isAnalyzing() const { return m_isAnalyzing; }
    
    /**
     * @brief 测试服务连接
     * @param serviceId 服务 ID
     */
    Q_INVOKABLE void testConnection(const QString &serviceId);
    
    // ===================== 持久化 =====================
    
    /**
     * @brief 保存配置
     */
    Q_INVOKABLE void saveSettings();
    
    /**
     * @brief 加载配置
     */
    Q_INVOKABLE void loadSettings();

signals:
    /**
     * @brief 分析响应（流式）
     * @param chunk 响应片段
     * @param isComplete 是否完成
     */
    void analysisResponse(const QString &chunk, bool isComplete);
    
    /**
     * @brief 分析完成
     * @param fullResponse 完整响应
     */
    void analysisCompleted(const QString &fullResponse);
    
    /**
     * @brief 分析错误
     * @param error 错误信息
     */
    void analysisError(const QString &error);
    
    /**
     * @brief 连接测试结果
     * @param serviceId 服务 ID
     * @param success 是否成功
     * @param message 消息
     */
    void connectionTestResult(const QString &serviceId, bool success, const QString &message);
    
    void currentServiceChanged();
    void analyzingStateChanged();
    void proxyChanged();

private slots:
    void onNetworkReply(QNetworkReply *reply);
    void onReadyRead();
    void onSslErrors(const QList<QSslError> &errors);

private:
    AIAnalysisManager(QObject *parent = nullptr);
    ~AIAnalysisManager();
    
    /**
     * @brief 初始化内置服务
     */
    void initBuiltinServices();
    
    /**
     * @brief 配置网络代理
     */
    void configureProxy();
    
    /**
     * @brief 加密 API Key
     */
    QString encryptApiKey(const QString &apiKey) const;
    
    /**
     * @brief 解密 API Key
     */
    QString decryptApiKey(const QString &encrypted) const;
    
    /**
     * @brief 构建请求体
     */
    QByteArray buildRequestBody(const QString &logContent, const QString &question,
                                 const QString &systemPrompt) const;
    
    /**
     * @brief 解析响应
     */
    QString parseResponse(const QByteArray &data, const QString &serviceId) const;
    
    // 网络管理
    QNetworkAccessManager *m_networkManager = nullptr;
    QNetworkReply *m_currentReply = nullptr;
    
    // 服务配置
    QMap<QString, AIServiceConfig> m_services;
    QString m_currentServiceId;
    
    // API Keys (加密存储)
    QMap<QString, QString> m_apiKeys;
    
    // 代理配置
    ProxyConfig m_proxyConfig;
    
    // 状态
    bool m_isAnalyzing = false;
    QString m_accumulatedResponse;
    
    // 加密密钥（简单混淆，非安全级加密）
    static const QByteArray ENCRYPTION_KEY;
    
    // 默认系统提示词
    static const QString DEFAULT_SYSTEM_PROMPT;
};

#endif // AIANALYSISMANAGER_H
