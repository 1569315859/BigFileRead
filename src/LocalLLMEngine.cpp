/**
 * @file LocalLLMEngine.cpp
 * @brief 本地 LLM 引擎实现
 */

#include "LocalLLMEngine.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QUuid>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>
#include <QDebug>
#include <QCoreApplication>

// 默认提示词
const QString LocalLLMEngine::DEFAULT_LOG_ANALYSIS_PROMPT = 
    "You are a log analysis expert. Analyze the provided log content and answer the user's question. "
    "Focus on identifying patterns, errors, warnings, and anomalies. "
    "Provide clear and actionable insights.";

const QString LocalLLMEngine::QUERY_GENERATION_PROMPT = 
    "You are a data query assistant. Convert the user's natural language question into a structured query condition. "
    "Output a JSON object with the following format: "
    "{\"column\": \"column_name\", \"operator\": \"equals|contains|greater|less|regex\", \"value\": \"search_value\", \"logic\": \"and|or\"} "
    "For multiple conditions, output an array of such objects. Only output the JSON, no explanation.";

const QString LocalLLMEngine::ANOMALY_DETECTION_PROMPT = 
    "You are a data quality analyst. Examine the provided data samples and identify any anomalies, outliers, or suspicious values. "
    "For each anomaly found, provide the index (0-based) and a brief explanation. "
    "Output as JSON: {\"anomalies\": [{\"index\": 0, \"explanation\": \"reason\"}]}";

const QString LocalLLMEngine::CLEANING_RULES_PROMPT = 
    "You are a data cleaning expert. Examine the provided dirty data samples and suggest cleaning rules. "
    "Provide specific regex patterns or transformation rules that can be applied. "
    "Output as a numbered list of rules.";

LocalLLMEngine& LocalLLMEngine::instance()
{
    static LocalLLMEngine instance;
    return instance;
}

LocalLLMEngine::LocalLLMEngine(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
{
    loadSettings();
    
    // 尝试自动检测 llama.cpp
    if (m_llamaCppPath.isEmpty()) {
        m_llamaCppPath = autoDetectLlamaCpp();
    }
}

LocalLLMEngine::~LocalLLMEngine()
{
    cancelGeneration();
    saveSettings();
}

// ===================== llama.cpp 配置 =====================

void LocalLLMEngine::setLlamaCppPath(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    if (m_llamaCppPath != path) {
        m_llamaCppPath = path;
        emit llamaCppPathChanged();
        saveSettings();
    }
}

bool LocalLLMEngine::isLlamaCppAvailable() const
{
    if (m_llamaCppPath.isEmpty()) {
        return false;
    }
    QFileInfo fi(m_llamaCppPath);
    return fi.exists() && fi.isExecutable();
}

QString LocalLLMEngine::getLlamaCppVersion() const
{
    if (!isLlamaCppAvailable()) {
        return QString();
    }
    
    QProcess proc;
    proc.start(m_llamaCppPath, {"--version"});
    if (!proc.waitForFinished(5000)) {
        return QString();
    }
    
    QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (output.isEmpty()) {
        output = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    }
    return output;
}

QString LocalLLMEngine::autoDetectLlamaCpp() const
{
    QStringList searchPaths;
    
    // 应用程序目录
    searchPaths << QCoreApplication::applicationDirPath();
    searchPaths << QCoreApplication::applicationDirPath() + "/llama.cpp";
    searchPaths << QCoreApplication::applicationDirPath() + "/bin";
    
    // 用户目录
    searchPaths << QDir::homePath() + "/llama.cpp";
    searchPaths << QDir::homePath() + "/llama.cpp/build/bin";
    searchPaths << QDir::homePath() + "/.local/bin";
    
    // 常见安装位置
#ifdef Q_OS_WIN
    searchPaths << "C:/llama.cpp";
    searchPaths << "C:/llama.cpp/build/bin/Release";
    searchPaths << QDir::homePath() + "/AppData/Local/llama.cpp";
#else
    searchPaths << "/usr/local/bin";
    searchPaths << "/opt/llama.cpp";
#endif
    
    // 可执行文件名
    QStringList exeNames;
#ifdef Q_OS_WIN
    exeNames << "llama-cli.exe" << "llama.exe" << "main.exe" << "llama-server.exe";
#else
    exeNames << "llama-cli" << "llama" << "main" << "llama-server";
#endif
    
    for (const QString &path : searchPaths) {
        QDir dir(path);
        if (!dir.exists()) continue;
        
        for (const QString &exe : exeNames) {
            QString fullPath = dir.absoluteFilePath(exe);
            QFileInfo fi(fullPath);
            if (fi.exists() && fi.isExecutable()) {
                return fullPath;
            }
        }
    }
    
    return QString();
}

// ===================== 模型管理 =====================

QVariantList LocalLLMEngine::getModels() const
{
    QMutexLocker locker(&m_mutex);
    QVariantList result;
    
    for (auto it = m_models.constBegin(); it != m_models.constEnd(); ++it) {
        const LocalModelConfig &config = it.value();
        QFileInfo fi(config.modelPath);
        
        QVariantMap model;
        model["id"] = it.key();
        model["name"] = config.modelName;
        model["path"] = config.modelPath;
        model["size"] = fi.exists() ? fi.size() : 0;
        model["sizeStr"] = fi.exists() ? QString::number(fi.size() / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB" : "N/A";
        model["exists"] = fi.exists();
        model["contextLength"] = config.contextLength;
        model["gpuLayers"] = config.gpuLayers;
        result.append(model);
    }
    
    return result;
}

QString LocalLLMEngine::addModel(const QString &modelPath, const QString &name)
{
    QFileInfo fi(modelPath);
    if (!fi.exists() || !fi.isFile()) {
        emit generationError(tr("Model file not found: %1").arg(modelPath));
        return QString();
    }
    
    // 验证 GGUF 格式
    QVariantMap metadata = validateModel(modelPath);
    if (!metadata.value("valid").toBool()) {
        emit generationError(metadata.value("error").toString());
        return QString();
    }
    
    QMutexLocker locker(&m_mutex);
    
    QString modelId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    
    LocalModelConfig config;
    config.modelPath = fi.absoluteFilePath();
    config.modelName = name.isEmpty() ? metadata.value("name").toString() : name;
    config.contextLength = metadata.value("contextLength", 4096).toInt();
    
    m_models.insert(modelId, config);
    
    // 如果是第一个模型，设为当前
    if (m_currentModelId.isEmpty()) {
        m_currentModelId = modelId;
        emit currentModelChanged();
    }
    
    saveSettings();
    return modelId;
}

void LocalLLMEngine::removeModel(const QString &modelId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_models.remove(modelId) > 0) {
        if (m_currentModelId == modelId) {
            m_currentModelId = m_models.isEmpty() ? QString() : m_models.firstKey();
            emit currentModelChanged();
        }
        saveSettings();
    }
}

void LocalLLMEngine::setCurrentModel(const QString &modelId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_models.contains(modelId) && m_currentModelId != modelId) {
        m_currentModelId = modelId;
        emit currentModelChanged();
        saveSettings();
    }
}

QVariantMap LocalLLMEngine::getModelConfig(const QString &modelId) const
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_models.contains(modelId)) {
        return QVariantMap();
    }
    
    const LocalModelConfig &config = m_models[modelId];
    QVariantMap result;
    result["modelPath"] = config.modelPath;
    result["modelName"] = config.modelName;
    result["contextLength"] = config.contextLength;
    result["gpuLayers"] = config.gpuLayers;
    result["threads"] = config.threads;
    result["temperature"] = config.temperature;
    result["topP"] = config.topP;
    result["maxTokens"] = config.maxTokens;
    result["useMmap"] = config.useMmap;
    result["useMlock"] = config.useMlock;
    
    return result;
}

void LocalLLMEngine::updateModelConfig(const QString &modelId, const QVariantMap &config)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_models.contains(modelId)) {
        return;
    }
    
    LocalModelConfig &modelConfig = m_models[modelId];
    
    if (config.contains("modelName")) modelConfig.modelName = config["modelName"].toString();
    if (config.contains("contextLength")) modelConfig.contextLength = config["contextLength"].toInt();
    if (config.contains("gpuLayers")) modelConfig.gpuLayers = config["gpuLayers"].toInt();
    if (config.contains("threads")) modelConfig.threads = config["threads"].toInt();
    if (config.contains("temperature")) modelConfig.temperature = config["temperature"].toFloat();
    if (config.contains("topP")) modelConfig.topP = config["topP"].toFloat();
    if (config.contains("maxTokens")) modelConfig.maxTokens = config["maxTokens"].toInt();
    if (config.contains("useMmap")) modelConfig.useMmap = config["useMmap"].toBool();
    if (config.contains("useMlock")) modelConfig.useMlock = config["useMlock"].toBool();
    
    saveSettings();
}

QVariantMap LocalLLMEngine::validateModel(const QString &modelPath) const
{
    QVariantMap result;
    result["valid"] = false;
    
    QFile file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        result["error"] = tr("Cannot open file: %1").arg(modelPath);
        return result;
    }
    
    // GGUF 魔数检查
    QByteArray magic = file.read(4);
    if (magic != "GGUF") {
        result["error"] = tr("Not a valid GGUF file (invalid magic number)");
        return result;
    }
    
    // 读取版本
    quint32 version;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    file.seek(4);
    stream >> version;
    
    if (version < 2 || version > 3) {
        result["error"] = tr("Unsupported GGUF version: %1").arg(version);
        return result;
    }
    
    // 从文件名提取模型信息
    QFileInfo fi(modelPath);
    QString baseName = fi.baseName();
    
    result["valid"] = true;
    result["name"] = baseName;
    result["size"] = fi.size();
    result["version"] = version;
    
    // 尝试从文件名提取量化类型
    QStringList quantTypes = {"Q2_K", "Q3_K_S", "Q3_K_M", "Q3_K_L", "Q4_0", "Q4_1", 
                              "Q4_K_S", "Q4_K_M", "Q5_0", "Q5_1", "Q5_K_S", "Q5_K_M",
                              "Q6_K", "Q8_0", "F16", "F32"};
    for (const QString &qt : quantTypes) {
        if (baseName.toUpper().contains(qt)) {
            result["quantization"] = qt;
            break;
        }
    }
    
    // 估算上下文长度（默认）
    result["contextLength"] = 4096;
    
    return result;
}

// ===================== 推理 =====================

void LocalLLMEngine::generate(const QString &prompt, const QString &systemPrompt)
{
    if (!isLlamaCppAvailable()) {
        emit generationError(tr("llama.cpp not found. Please configure the path in settings."));
        return;
    }
    
    if (m_currentModelId.isEmpty() || !m_models.contains(m_currentModelId)) {
        emit generationError(tr("No model selected. Please add and select a model first."));
        return;
    }
    
    if (m_state == InferenceState::Generating) {
        emit generationError(tr("Generation already in progress."));
        return;
    }
    
    // 验证模型文件存在
    const LocalModelConfig &config = m_models[m_currentModelId];
    if (!QFile::exists(config.modelPath)) {
        emit generationError(tr("Model file not found: %1").arg(config.modelPath));
        return;
    }
    
    m_state = InferenceState::Generating;
    emit stateChanged();
    m_accumulatedOutput.clear();
    
    // 创建进程
    if (m_process) {
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    
    connect(m_process, &QProcess::readyReadStandardOutput, this, &LocalLLMEngine::onProcessReadyRead);
    connect(m_process, &QProcess::readyReadStandardError, this, &LocalLLMEngine::onProcessReadyRead);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &LocalLLMEngine::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &LocalLLMEngine::onProcessError);
    
    QStringList args = buildLlamaArgs(prompt, systemPrompt);
    
    qDebug() << "Starting llama.cpp:" << m_llamaCppPath << args;
    m_process->start(m_llamaCppPath, args);
}

void LocalLLMEngine::analyzeLog(const QString &logContent, const QString &question,
                                 const QString &systemPrompt)
{
    QString prompt = QString("Log Content:\n```\n%1\n```\n\nQuestion: %2")
                        .arg(logContent.left(16000))  // 限制长度
                        .arg(question);
    
    QString sysPrompt = systemPrompt.isEmpty() ? DEFAULT_LOG_ANALYSIS_PROMPT : systemPrompt;
    generate(prompt, sysPrompt);
}

void LocalLLMEngine::cancelGeneration()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    
    if (m_state == InferenceState::Generating) {
        m_state = InferenceState::Idle;
        emit stateChanged();
    }
}

void LocalLLMEngine::naturalLanguageToQuery(const QString &question, const QStringList &columns)
{
    QString prompt = QString("Available columns: %1\n\nUser question: %2\n\nGenerate query condition JSON:")
                        .arg(columns.join(", "))
                        .arg(question);
    
    // 使用特殊标记表示这是查询生成请求
    m_accumulatedOutput = "###QUERY_MODE###";
    generate(prompt, QUERY_GENERATION_PROMPT);
}

void LocalLLMEngine::detectAnomalies(const QStringList &data, const QString &columnName)
{
    QString samples = data.mid(0, 100).join("\n");  // 取前100个样本
    QString prompt = QString("Column: %1\n\nData samples:\n%2\n\nIdentify anomalies:")
                        .arg(columnName)
                        .arg(samples);
    
    m_accumulatedOutput = "###ANOMALY_MODE###";
    generate(prompt, ANOMALY_DETECTION_PROMPT);
}

void LocalLLMEngine::suggestCleaningRules(const QStringList &samples)
{
    QString prompt = QString("Dirty data samples:\n%1\n\nSuggest cleaning rules:")
                        .arg(samples.mid(0, 50).join("\n"));
    
    m_accumulatedOutput = "###CLEANING_MODE###";
    generate(prompt, CLEANING_RULES_PROMPT);
}

// ===================== 进程回调 =====================

void LocalLLMEngine::onProcessReadyRead()
{
    if (!m_process) return;
    
    QByteArray output = m_process->readAllStandardOutput();
    if (!output.isEmpty()) {
        QString text = QString::fromUtf8(output);
        
        // 检查是否是特殊模式
        bool isSpecialMode = m_accumulatedOutput.startsWith("###");
        QString modeMarker = isSpecialMode ? m_accumulatedOutput.left(m_accumulatedOutput.indexOf("###", 3) + 3) : "";
        
        if (isSpecialMode) {
            m_accumulatedOutput = modeMarker + m_accumulatedOutput.mid(modeMarker.length()) + text;
        } else {
            m_accumulatedOutput += text;
            emit generationResponse(text, false);
        }
    }
}

void LocalLLMEngine::onProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    m_state = InferenceState::Idle;
    emit stateChanged();
    
    if (status == QProcess::CrashExit) {
        emit generationError(tr("llama.cpp crashed unexpectedly"));
        return;
    }
    
    if (exitCode != 0) {
        QString errorOutput = QString::fromUtf8(m_process->readAllStandardError());
        emit generationError(tr("llama.cpp exited with code %1: %2").arg(exitCode).arg(errorOutput));
        return;
    }
    
    // 处理特殊模式
    if (m_accumulatedOutput.startsWith("###QUERY_MODE###")) {
        QString response = m_accumulatedOutput.mid(QString("###QUERY_MODE###").length());
        // 尝试解析 JSON
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        if (!doc.isNull()) {
            emit queryConditionGenerated(doc.object().toVariantMap());
        } else {
            emit generationCompleted(response);
        }
    } else if (m_accumulatedOutput.startsWith("###ANOMALY_MODE###")) {
        QString response = m_accumulatedOutput.mid(QString("###ANOMALY_MODE###").length());
        QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
        if (!doc.isNull() && doc.object().contains("anomalies")) {
            QList<int> indices;
            QStringList explanations;
            for (const QJsonValue &v : doc.object()["anomalies"].toArray()) {
                QJsonObject obj = v.toObject();
                indices.append(obj["index"].toInt());
                explanations.append(obj["explanation"].toString());
            }
            emit anomaliesDetected(indices, explanations);
        } else {
            emit generationCompleted(response);
        }
    } else if (m_accumulatedOutput.startsWith("###CLEANING_MODE###")) {
        QString response = m_accumulatedOutput.mid(QString("###CLEANING_MODE###").length());
        QStringList rules = response.split('\n', Qt::SkipEmptyParts);
        emit cleaningRulesSuggested(rules);
    } else {
        emit generationResponse(QString(), true);
        emit generationCompleted(m_accumulatedOutput);
    }
}

void LocalLLMEngine::onProcessError(QProcess::ProcessError error)
{
    m_state = InferenceState::Error;
    emit stateChanged();
    
    QString errorMsg;
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = tr("Failed to start llama.cpp. Check if the path is correct and the file is executable.");
            break;
        case QProcess::Crashed:
            errorMsg = tr("llama.cpp crashed during execution.");
            break;
        case QProcess::Timedout:
            errorMsg = tr("llama.cpp timed out.");
            break;
        default:
            errorMsg = tr("Unknown error occurred while running llama.cpp.");
    }
    
    emit generationError(errorMsg);
}

// ===================== 辅助方法 =====================

QStringList LocalLLMEngine::buildLlamaArgs(const QString &prompt, const QString &systemPrompt) const
{
    QMutexLocker locker(&m_mutex);
    
    const LocalModelConfig &config = m_models[m_currentModelId];
    
    QStringList args;
    args << "-m" << config.modelPath;
    args << "-c" << QString::number(config.contextLength);
    args << "-n" << QString::number(config.maxTokens);
    args << "-t" << QString::number(config.threads);
    args << "--temp" << QString::number(config.temperature, 'f', 2);
    args << "--top-p" << QString::number(config.topP, 'f', 2);
    
    if (config.gpuLayers > 0) {
        args << "-ngl" << QString::number(config.gpuLayers);
    }
    
    if (config.useMmap) {
        args << "--mmap";
    }
    
    if (config.useMlock) {
        args << "--mlock";
    }
    
    // 构建完整提示
    QString fullPrompt;
    if (!systemPrompt.isEmpty()) {
        // ChatML 格式
        fullPrompt = QString("<|im_start|>system\n%1<|im_end|>\n<|im_start|>user\n%2<|im_end|>\n<|im_start|>assistant\n")
                        .arg(systemPrompt)
                        .arg(prompt);
    } else {
        fullPrompt = prompt;
    }
    
    args << "-p" << fullPrompt;
    args << "--no-display-prompt";  // 不显示提示词
    
    return args;
}

QVariantMap LocalLLMEngine::parseGGUFMetadata(const QString &modelPath) const
{
    // 简化实现，完整解析需要更复杂的 GGUF 解析器
    return validateModel(modelPath);
}

qint64 LocalLLMEngine::estimateMemoryRequirement(const QString &modelPath) const
{
    QFileInfo fi(modelPath);
    // 粗略估算：模型大小 + 上下文缓存
    return fi.size() + (4096 * 1024 * 4);  // 文件大小 + ~16MB 上下文
}

// ===================== 持久化 =====================

void LocalLLMEngine::saveSettings()
{
    QSettings settings;
    settings.beginGroup("LocalLLM");
    
    settings.setValue("llamaCppPath", m_llamaCppPath);
    settings.setValue("currentModel", m_currentModelId);
    
    // 保存模型列表
    settings.beginWriteArray("models");
    int i = 0;
    for (auto it = m_models.constBegin(); it != m_models.constEnd(); ++it, ++i) {
        settings.setArrayIndex(i);
        settings.setValue("id", it.key());
        settings.setValue("path", it.value().modelPath);
        settings.setValue("name", it.value().modelName);
        settings.setValue("contextLength", it.value().contextLength);
        settings.setValue("gpuLayers", it.value().gpuLayers);
        settings.setValue("threads", it.value().threads);
        settings.setValue("temperature", it.value().temperature);
        settings.setValue("topP", it.value().topP);
        settings.setValue("maxTokens", it.value().maxTokens);
        settings.setValue("useMmap", it.value().useMmap);
        settings.setValue("useMlock", it.value().useMlock);
    }
    settings.endArray();
    
    settings.endGroup();
}

void LocalLLMEngine::loadSettings()
{
    QSettings settings;
    settings.beginGroup("LocalLLM");
    
    m_llamaCppPath = settings.value("llamaCppPath").toString();
    m_currentModelId = settings.value("currentModel").toString();
    
    // 加载模型列表
    int count = settings.beginReadArray("models");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        
        QString id = settings.value("id").toString();
        LocalModelConfig config;
        config.modelPath = settings.value("path").toString();
        config.modelName = settings.value("name").toString();
        config.contextLength = settings.value("contextLength", 4096).toInt();
        config.gpuLayers = settings.value("gpuLayers", 0).toInt();
        config.threads = settings.value("threads", 4).toInt();
        config.temperature = settings.value("temperature", 0.7).toFloat();
        config.topP = settings.value("topP", 0.9).toFloat();
        config.maxTokens = settings.value("maxTokens", 2048).toInt();
        config.useMmap = settings.value("useMmap", true).toBool();
        config.useMlock = settings.value("useMlock", false).toBool();
        
        m_models.insert(id, config);
    }
    settings.endArray();
    
    settings.endGroup();
    
    // 验证当前模型是否存在
    if (!m_currentModelId.isEmpty() && !m_models.contains(m_currentModelId)) {
        m_currentModelId = m_models.isEmpty() ? QString() : m_models.firstKey();
    }
}
