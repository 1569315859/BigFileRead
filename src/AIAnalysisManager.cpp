/**
 * @file AIAnalysisManager.cpp
 * @brief AI 分析服务管理器实现
 */

#include "AIAnalysisManager.h"
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QCryptographicHash>
#include <QDebug>
#include <QUuid>

// 简单的混淆密钥（非安全级加密，仅防止明文存储）
const QByteArray AIAnalysisManager::ENCRYPTION_KEY = "BigFileViewer_AI_Key_2024";

const QString AIAnalysisManager::DEFAULT_SYSTEM_PROMPT = 
    "You are an expert log analysis assistant. Analyze the provided log content and answer the user's question. "
    "Focus on identifying errors, warnings, patterns, root causes, and providing actionable insights. "
    "Be concise but thorough in your analysis.";

AIAnalysisManager& AIAnalysisManager::instance()
{
    static AIAnalysisManager instance;
    return instance;
}

AIAnalysisManager::AIAnalysisManager(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &AIAnalysisManager::onNetworkReply);
    
    initBuiltinServices();
    loadSettings();
    configureProxy();
}

AIAnalysisManager::~AIAnalysisManager()
{
    cancelAnalysis();
}

void AIAnalysisManager::initBuiltinServices()
{
    // OpenAI
    {
        AIServiceConfig config;
        config.id = "openai";
        config.name = "OpenAI";
        config.endpoint = "https://api.openai.com/v1/chat/completions";
        config.model = "gpt-4-turbo-preview";
        config.models = QStringList{"gpt-4-turbo-preview", "gpt-4", "gpt-4o", "gpt-4o-mini", "gpt-3.5-turbo"};
        config.apiKeyHeader = "Authorization";
        config.apiKeyPrefix = "Bearer ";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // Azure OpenAI
    {
        AIServiceConfig config;
        config.id = "azure_openai";
        config.name = "Azure OpenAI";
        config.endpoint = "https://{resource}.openai.azure.com/openai/deployments/{deployment}/chat/completions?api-version=2024-02-15-preview";
        config.model = "gpt-4";
        config.models = QStringList{"gpt-4", "gpt-4-turbo", "gpt-35-turbo"};
        config.apiKeyHeader = "api-key";
        config.apiKeyPrefix = "";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // Claude
    {
        AIServiceConfig config;
        config.id = "claude";
        config.name = "Anthropic Claude";
        config.endpoint = "https://api.anthropic.com/v1/messages";
        config.model = "claude-sonnet-4-20250514";
        config.models = QStringList{"claude-sonnet-4-20250514", "claude-opus-4-20250514", "claude-3-5-sonnet-20241022", "claude-3-opus-20240229", "claude-3-haiku-20240307"};
        config.apiKeyHeader = "x-api-key";
        config.apiKeyPrefix = "";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // 通义千问 (Qwen)
    {
        AIServiceConfig config;
        config.id = "qwen";
        config.name = tr("Qwen (Tongyi Qianwen)");
        config.endpoint = "https://dashscope.aliyuncs.com/api/v1/services/aigc/text-generation/generation";
        config.model = "qwen-max";
        config.models = QStringList{"qwen-max", "qwen-max-longcontext", "qwen-plus", "qwen-turbo"};
        config.apiKeyHeader = "Authorization";
        config.apiKeyPrefix = "Bearer ";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // 文心一言 (Ernie)
    {
        AIServiceConfig config;
        config.id = "ernie";
        config.name = tr("Ernie (Wenxin Yiyan)");
        config.endpoint = "https://aip.baidubce.com/rpc/2.0/ai_custom/v1/wenxinworkshop/chat/completions_pro";
        config.model = "ernie-4.0";
        config.models = QStringList{"ernie-4.0", "ernie-4.0-turbo", "ernie-3.5-turbo"};
        config.apiKeyHeader = "Authorization";
        config.apiKeyPrefix = "Bearer ";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // 讯飞星火 (Spark)
    {
        AIServiceConfig config;
        config.id = "spark";
        config.name = tr("Spark (iFlytek)");
        config.endpoint = "https://spark-api-open.xf-yun.com/v1/chat/completions";
        config.model = "generalv3.5";
        config.models = QStringList{"generalv3.5", "generalv3", "generalv2"};
        config.apiKeyHeader = "Authorization";
        config.apiKeyPrefix = "Bearer ";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // DeepSeek
    {
        AIServiceConfig config;
        config.id = "deepseek";
        config.name = "DeepSeek";
        config.endpoint = "https://api.deepseek.com/chat/completions";
        config.model = "deepseek-chat";
        config.models = QStringList{"deepseek-chat", "deepseek-coder", "deepseek-reasoner"};
        config.apiKeyHeader = "Authorization";
        config.apiKeyPrefix = "Bearer ";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // Moonshot (Kimi)
    {
        AIServiceConfig config;
        config.id = "moonshot";
        config.name = "Moonshot (Kimi)";
        config.endpoint = "https://api.moonshot.cn/v1/chat/completions";
        config.model = "moonshot-v1-128k";
        config.models = QStringList{"moonshot-v1-128k", "moonshot-v1-32k", "moonshot-v1-8k"};
        config.apiKeyHeader = "Authorization";
        config.apiKeyPrefix = "Bearer ";
        config.isCustom = false;
        m_services[config.id] = config;
    }
    
    // 默认选中 OpenAI
    if (m_currentServiceId.isEmpty()) {
        m_currentServiceId = "openai";
    }
}

QVariantList AIAnalysisManager::getAvailableServices() const
{
    QVariantList result;
    
    for (auto it = m_services.begin(); it != m_services.end(); ++it) {
        QVariantMap map;
        map["id"] = it->id;
        map["name"] = it->name;
        map["endpoint"] = it->endpoint;
        map["model"] = it->model;
        map["isCustom"] = it->isCustom;
        map["hasApiKey"] = hasApiKey(it->id);
        result.append(map);
    }
    
    return result;
}

void AIAnalysisManager::setCurrentService(const QString &serviceId)
{
    if (m_services.contains(serviceId) && m_currentServiceId != serviceId) {
        m_currentServiceId = serviceId;
        saveSettings();
        emit currentServiceChanged();
    }
}

QString AIAnalysisManager::addCustomService(const QString &name, const QString &endpoint,
                                             const QString &model, const QString &apiKeyHeader,
                                             const QString &apiKeyPrefix)
{
    QString id = "custom_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    
    AIServiceConfig config;
    config.id = id;
    config.name = name;
    config.endpoint = endpoint;
    config.model = model;
    config.apiKeyHeader = apiKeyHeader.isEmpty() ? "Authorization" : apiKeyHeader;
    config.apiKeyPrefix = apiKeyPrefix;
    config.isCustom = true;
    
    m_services[id] = config;
    saveSettings();
    
    return id;
}

bool AIAnalysisManager::removeCustomService(const QString &serviceId)
{
    if (m_services.contains(serviceId) && m_services[serviceId].isCustom) {
        m_services.remove(serviceId);
        m_apiKeys.remove(serviceId);
        
        if (m_currentServiceId == serviceId) {
            m_currentServiceId = "openai";
            emit currentServiceChanged();
        }
        
        saveSettings();
        return true;
    }
    return false;
}

void AIAnalysisManager::setApiKey(const QString &serviceId, const QString &apiKey)
{
    if (apiKey.isEmpty()) {
        m_apiKeys.remove(serviceId);
    } else {
        m_apiKeys[serviceId] = encryptApiKey(apiKey);
    }
    saveSettings();
}

QString AIAnalysisManager::getApiKey(const QString &serviceId) const
{
    if (m_apiKeys.contains(serviceId)) {
        return decryptApiKey(m_apiKeys[serviceId]);
    }
    return QString();
}

bool AIAnalysisManager::hasApiKey(const QString &serviceId) const
{
    return m_apiKeys.contains(serviceId) && !m_apiKeys[serviceId].isEmpty();
}

QString AIAnalysisManager::getMaskedApiKey(const QString &serviceId) const
{
    QString key = getApiKey(serviceId);
    if (key.isEmpty()) return QString();
    
    if (key.length() <= 8) {
        return QString(key.length(), '*');
    }
    
    return key.left(4) + "..." + key.right(4);
}

QString AIAnalysisManager::proxyType() const
{
    switch (m_proxyConfig.type) {
        case ProxyConfig::NoProxy: return "none";
        case ProxyConfig::SystemProxy: return "system";
        case ProxyConfig::CustomProxy: return "custom";
    }
    return "system";
}

void AIAnalysisManager::setProxyType(const QString &type)
{
    ProxyConfig::Type newType = ProxyConfig::SystemProxy;
    if (type == "none") newType = ProxyConfig::NoProxy;
    else if (type == "custom") newType = ProxyConfig::CustomProxy;
    
    if (m_proxyConfig.type != newType) {
        m_proxyConfig.type = newType;
        configureProxy();
        saveSettings();
        emit proxyChanged();
    }
}

void AIAnalysisManager::setCustomProxy(const QString &host, int port, bool isSocks5,
                                        const QString &username, const QString &password)
{
    m_proxyConfig.host = host;
    m_proxyConfig.port = port;
    m_proxyConfig.isSocks5 = isSocks5;
    m_proxyConfig.username = username;
    m_proxyConfig.password = password;
    
    if (m_proxyConfig.type == ProxyConfig::CustomProxy) {
        configureProxy();
    }
    saveSettings();
}

QVariantMap AIAnalysisManager::getCustomProxy() const
{
    QVariantMap map;
    map["host"] = m_proxyConfig.host;
    map["port"] = m_proxyConfig.port;
    map["isSocks5"] = m_proxyConfig.isSocks5;
    map["username"] = m_proxyConfig.username;
    return map;
}

void AIAnalysisManager::configureProxy()
{
    QNetworkProxy proxy;
    
    switch (m_proxyConfig.type) {
        case ProxyConfig::NoProxy:
            proxy.setType(QNetworkProxy::NoProxy);
            break;
            
        case ProxyConfig::SystemProxy:
            proxy.setType(QNetworkProxy::DefaultProxy);
            break;
            
        case ProxyConfig::CustomProxy:
            proxy.setType(m_proxyConfig.isSocks5 ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy);
            proxy.setHostName(m_proxyConfig.host);
            proxy.setPort(m_proxyConfig.port);
            if (!m_proxyConfig.username.isEmpty()) {
                proxy.setUser(m_proxyConfig.username);
                proxy.setPassword(m_proxyConfig.password);
            }
            break;
    }
    
    m_networkManager->setProxy(proxy);
}

void AIAnalysisManager::analyzeLog(const QString &logContent, const QString &question,
                                    const QString &systemPrompt)
{
    if (m_isAnalyzing) {
        emit analysisError(tr("Analysis already in progress"));
        return;
    }
    
    if (!m_services.contains(m_currentServiceId)) {
        emit analysisError(tr("Invalid service selected"));
        return;
    }
    
    QString apiKey = getApiKey(m_currentServiceId);
    if (apiKey.isEmpty()) {
        emit analysisError(tr("API Key not configured for service: %1").arg(m_services[m_currentServiceId].name));
        return;
    }
    
    const AIServiceConfig &config = m_services[m_currentServiceId];
    
    // 构建请求
    QNetworkRequest request(QUrl(config.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(config.apiKeyHeader.toUtf8(), 
                         (config.apiKeyPrefix + apiKey).toUtf8());
    
    // Claude 需要额外头
    if (m_currentServiceId == "claude") {
        request.setRawHeader("anthropic-version", "2023-06-01");
    }
    
    // 构建请求体
    QByteArray body = buildRequestBody(logContent, question, 
                                        systemPrompt.isEmpty() ? DEFAULT_SYSTEM_PROMPT : systemPrompt);
    
    m_isAnalyzing = true;
    m_accumulatedResponse.clear();
    emit analyzingStateChanged();
    
    m_currentReply = m_networkManager->post(request, body);
    
    // 流式读取
    connect(m_currentReply, &QNetworkReply::readyRead, this, &AIAnalysisManager::onReadyRead);
    connect(m_currentReply, &QNetworkReply::sslErrors, this, &AIAnalysisManager::onSslErrors);
}

void AIAnalysisManager::cancelAnalysis()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    
    if (m_isAnalyzing) {
        m_isAnalyzing = false;
        emit analyzingStateChanged();
    }
}

void AIAnalysisManager::testConnection(const QString &serviceId)
{
    if (!m_services.contains(serviceId)) {
        emit connectionTestResult(serviceId, false, tr("Unknown service"));
        return;
    }
    
    QString apiKey = getApiKey(serviceId);
    if (apiKey.isEmpty()) {
        emit connectionTestResult(serviceId, false, tr("API Key not configured"));
        return;
    }
    
    // 发送简单测试请求
    const AIServiceConfig &config = m_services[serviceId];
    
    QNetworkRequest request(QUrl(config.endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader(config.apiKeyHeader.toUtf8(), 
                         (config.apiKeyPrefix + apiKey).toUtf8());
    
    if (serviceId == "claude") {
        request.setRawHeader("anthropic-version", "2023-06-01");
    }
    
    // 简单的测试消息
    QJsonObject body;
    if (serviceId == "claude") {
        body["model"] = config.model;
        body["max_tokens"] = 10;
        body["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", "Hi"}}};
    } else if (serviceId == "qwen") {
        body["model"] = config.model;
        QJsonObject input;
        input["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", "Hi"}}};
        body["input"] = input;
    } else {
        body["model"] = config.model;
        body["max_tokens"] = 10;
        body["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", "Hi"}}};
    }
    
    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(body).toJson());
    
    // 存储服务 ID 用于回调
    reply->setProperty("testServiceId", serviceId);
}

QByteArray AIAnalysisManager::buildRequestBody(const QString &logContent, const QString &question,
                                                 const QString &systemPrompt) const
{
    QJsonObject body;
    const AIServiceConfig &config = m_services[m_currentServiceId];
    
    QString userMessage = QString("Log Content:\n```\n%1\n```\n\nQuestion: %2").arg(logContent, question);
    
    if (m_currentServiceId == "claude") {
        // Claude API 格式
        body["model"] = config.model;
        body["max_tokens"] = 4096;
        body["system"] = systemPrompt;
        body["messages"] = QJsonArray{
            QJsonObject{{"role", "user"}, {"content", userMessage}}
        };
    } else if (m_currentServiceId == "qwen") {
        // 通义千问格式
        body["model"] = config.model;
        QJsonObject input;
        input["messages"] = QJsonArray{
            QJsonObject{{"role", "system"}, {"content", systemPrompt}},
            QJsonObject{{"role", "user"}, {"content", userMessage}}
        };
        body["input"] = input;
        QJsonObject parameters;
        parameters["result_format"] = "message";
        body["parameters"] = parameters;
    } else {
        // OpenAI 兼容格式 (OpenAI, Azure, DeepSeek, Moonshot, Spark 等)
        body["model"] = config.model;
        body["max_tokens"] = 4096;
        body["stream"] = false;  // 暂不使用流式
        body["messages"] = QJsonArray{
            QJsonObject{{"role", "system"}, {"content", systemPrompt}},
            QJsonObject{{"role", "user"}, {"content", userMessage}}
        };
    }
    
    return QJsonDocument(body).toJson();
}

QString AIAnalysisManager::parseResponse(const QByteArray &data, const QString &serviceId) const
{
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return QString();
    
    QJsonObject obj = doc.object();
    
    // 检查错误
    if (obj.contains("error")) {
        QJsonObject error = obj["error"].toObject();
        return tr("API Error: %1").arg(error["message"].toString());
    }
    
    // Claude 格式
    if (serviceId == "claude") {
        QJsonArray content = obj["content"].toArray();
        if (!content.isEmpty()) {
            return content[0].toObject()["text"].toString();
        }
    }
    // 通义千问格式
    else if (serviceId == "qwen") {
        QJsonObject output = obj["output"].toObject();
        QJsonArray choices = output["choices"].toArray();
        if (!choices.isEmpty()) {
            return choices[0].toObject()["message"].toObject()["content"].toString();
        }
        // 兼容旧格式
        return output["text"].toString();
    }
    // OpenAI 兼容格式
    else {
        QJsonArray choices = obj["choices"].toArray();
        if (!choices.isEmpty()) {
            QJsonObject choice = choices[0].toObject();
            QJsonObject message = choice["message"].toObject();
            return message["content"].toString();
        }
    }
    
    return QString();
}

void AIAnalysisManager::onNetworkReply(QNetworkReply *reply)
{
    // 检查是否是连接测试
    QString testServiceId = reply->property("testServiceId").toString();
    if (!testServiceId.isEmpty()) {
        if (reply->error() == QNetworkReply::NoError) {
            emit connectionTestResult(testServiceId, true, tr("Connection successful"));
        } else {
            emit connectionTestResult(testServiceId, false, reply->errorString());
        }
        reply->deleteLater();
        return;
    }
    
    // 正常的分析请求
    if (reply != m_currentReply) {
        reply->deleteLater();
        return;
    }
    
    m_isAnalyzing = false;
    emit analyzingStateChanged();
    
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        
        // 尝试解析错误响应体
        QByteArray data = reply->readAll();
        if (!data.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("error")) {
                    QJsonObject error = obj["error"].toObject();
                    errorMsg = error["message"].toString();
                }
            }
        }
        
        emit analysisError(errorMsg);
    } else {
        QByteArray data = reply->readAll();
        QString response = parseResponse(data, m_currentServiceId);
        
        if (response.isEmpty()) {
            emit analysisError(tr("Failed to parse response"));
        } else {
            m_accumulatedResponse = response;
            emit analysisResponse(response, true);
            emit analysisCompleted(response);
        }
    }
    
    m_currentReply = nullptr;
    reply->deleteLater();
}

void AIAnalysisManager::onReadyRead()
{
    // 流式读取（如果启用了 stream）
    if (!m_currentReply) return;
    
    QByteArray data = m_currentReply->readAll();
    // 简化处理，等待完整响应
}

void AIAnalysisManager::onSslErrors(const QList<QSslError> &errors)
{
    qWarning() << "SSL Errors:";
    for (const QSslError &error : errors) {
        qWarning() << "  " << error.errorString();
    }
}

QString AIAnalysisManager::encryptApiKey(const QString &apiKey) const
{
    // 简单的 XOR 混淆（不是安全加密，只是防止明文存储）
    QByteArray data = apiKey.toUtf8();
    QByteArray result;
    
    for (int i = 0; i < data.size(); ++i) {
        result.append(data[i] ^ ENCRYPTION_KEY[i % ENCRYPTION_KEY.size()]);
    }
    
    return result.toBase64();
}

QString AIAnalysisManager::decryptApiKey(const QString &encrypted) const
{
    QByteArray data = QByteArray::fromBase64(encrypted.toUtf8());
    QByteArray result;
    
    for (int i = 0; i < data.size(); ++i) {
        result.append(data[i] ^ ENCRYPTION_KEY[i % ENCRYPTION_KEY.size()]);
    }
    
    return QString::fromUtf8(result);
}

void AIAnalysisManager::saveSettings()
{
    QSettings settings;
    settings.beginGroup("AIAnalysis");
    
    // 保存当前服务
    settings.setValue("currentService", m_currentServiceId);
    
    // 保存 API Keys
    QJsonObject keys;
    for (auto it = m_apiKeys.begin(); it != m_apiKeys.end(); ++it) {
        keys[it.key()] = it.value();
    }
    settings.setValue("apiKeys", QJsonDocument(keys).toJson(QJsonDocument::Compact));
    
    // 保存自定义服务
    QJsonArray customServices;
    for (auto it = m_services.begin(); it != m_services.end(); ++it) {
        if (it->isCustom) {
            QJsonObject obj;
            obj["id"] = it->id;
            obj["name"] = it->name;
            obj["endpoint"] = it->endpoint;
            obj["model"] = it->model;
            obj["apiKeyHeader"] = it->apiKeyHeader;
            obj["apiKeyPrefix"] = it->apiKeyPrefix;
            customServices.append(obj);
        }
    }
    settings.setValue("customServices", QJsonDocument(customServices).toJson(QJsonDocument::Compact));
    
    // 保存代理配置
    settings.setValue("proxyType", static_cast<int>(m_proxyConfig.type));
    settings.setValue("proxyHost", m_proxyConfig.host);
    settings.setValue("proxyPort", m_proxyConfig.port);
    settings.setValue("proxyIsSocks5", m_proxyConfig.isSocks5);
    settings.setValue("proxyUsername", m_proxyConfig.username);
    // 密码也应该加密，这里简化处理
    settings.setValue("proxyPassword", encryptApiKey(m_proxyConfig.password));
    
    // 保存各服务选中的模型
    QJsonObject selectedModels;
    for (auto it = m_services.begin(); it != m_services.end(); ++it) {
        if (!it->model.isEmpty()) {
            selectedModels[it.key()] = it->model;
        }
    }
    settings.setValue("selectedModels", QJsonDocument(selectedModels).toJson(QJsonDocument::Compact));
    
    settings.endGroup();
}

void AIAnalysisManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("AIAnalysis");
    
    // 加载当前服务
    m_currentServiceId = settings.value("currentService", "openai").toString();
    
    // 加载 API Keys
    QString keysJson = settings.value("apiKeys").toString();
    if (!keysJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(keysJson.toUtf8());
        if (doc.isObject()) {
            QJsonObject keys = doc.object();
            for (auto it = keys.begin(); it != keys.end(); ++it) {
                m_apiKeys[it.key()] = it.value().toString();
            }
        }
    }
    
    // 加载自定义服务
    QString customJson = settings.value("customServices").toString();
    if (!customJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(customJson.toUtf8());
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const QJsonValue &val : arr) {
                QJsonObject obj = val.toObject();
                AIServiceConfig config;
                config.id = obj["id"].toString();
                config.name = obj["name"].toString();
                config.endpoint = obj["endpoint"].toString();
                config.model = obj["model"].toString();
                config.apiKeyHeader = obj["apiKeyHeader"].toString();
                config.apiKeyPrefix = obj["apiKeyPrefix"].toString();
                config.isCustom = true;
                m_services[config.id] = config;
            }
        }
    }
    
    // 加载代理配置
    m_proxyConfig.type = static_cast<ProxyConfig::Type>(settings.value("proxyType", 1).toInt());
    m_proxyConfig.host = settings.value("proxyHost").toString();
    m_proxyConfig.port = settings.value("proxyPort", 0).toInt();
    m_proxyConfig.isSocks5 = settings.value("proxyIsSocks5", false).toBool();
    m_proxyConfig.username = settings.value("proxyUsername").toString();
    m_proxyConfig.password = decryptApiKey(settings.value("proxyPassword").toString());
    
    // 加载各服务选中的模型
    QString modelsJson = settings.value("selectedModels").toString();
    if (!modelsJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(modelsJson.toUtf8());
        if (doc.isObject()) {
            QJsonObject models = doc.object();
            for (auto it = models.begin(); it != models.end(); ++it) {
                if (m_services.contains(it.key())) {
                    m_services[it.key()].model = it.value().toString();
                }
            }
        }
    }
    
    settings.endGroup();
}

QStringList AIAnalysisManager::getModelsForService(const QString &serviceId) const
{
    if (m_services.contains(serviceId)) {
        return m_services[serviceId].models;
    }
    return QStringList();
}

QString AIAnalysisManager::getCurrentModel(const QString &serviceId) const
{
    if (m_services.contains(serviceId)) {
        return m_services[serviceId].model;
    }
    return QString();
}

void AIAnalysisManager::setModel(const QString &serviceId, const QString &model)
{
    if (m_services.contains(serviceId) && m_services[serviceId].model != model) {
        m_services[serviceId].model = model;
        saveSettings();
        emit currentServiceChanged();  // 复用现有信号通知模型变化
    }
}
