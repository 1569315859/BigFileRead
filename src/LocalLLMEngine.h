/**
 * @file LocalLLMEngine.h
 * @brief 本地 LLM 引擎 - 支持 GGUF 格式模型的本地推理
 * @description 集成 llama.cpp 实现本地 AI 推理，保护数据隐私
 */

#ifndef LOCALLLMENGINE_H
#define LOCALLLMENGINE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QThread>
#include <QMutex>
#include <QProcess>
#include <functional>
#include <memory>

/**
 * @brief 本地模型配置
 */
struct LocalModelConfig {
    QString modelPath;          ///< GGUF 模型文件路径
    QString modelName;          ///< 模型显示名称
    int contextLength = 4096;   ///< 上下文长度
    int gpuLayers = 0;          ///< GPU 加速层数 (0 = CPU only)
    int threads = 4;            ///< CPU 线程数
    float temperature = 0.7f;   ///< 温度参数
    float topP = 0.9f;          ///< Top-P 采样
    int maxTokens = 2048;       ///< 最大生成 token 数
    bool useMmap = true;        ///< 使用内存映射
    bool useMlock = false;      ///< 锁定内存
};

/**
 * @brief 推理状态
 */
enum class InferenceState {
    Idle,           ///< 空闲
    Loading,        ///< 加载模型中
    Ready,          ///< 模型就绪
    Generating,     ///< 生成中
    Error           ///< 错误
};

/**
 * @class LocalLLMEngine
 * @brief 本地 LLM 推理引擎
 * 
 * 使用外部 llama.cpp 可执行文件进行推理，避免复杂的库依赖。
 * 支持:
 * - GGUF 格式模型（Llama, Phi, Qwen, Gemma 等）
 * - 流式输出
 * - GPU 加速（CUDA/Metal/Vulkan）
 * - 多模型管理
 */
class LocalLLMEngine : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QString currentModel READ currentModel NOTIFY currentModelChanged)
    Q_PROPERTY(bool isReady READ isReady NOTIFY stateChanged)
    Q_PROPERTY(bool isGenerating READ isGenerating NOTIFY stateChanged)
    Q_PROPERTY(QString llamaCppPath READ llamaCppPath WRITE setLlamaCppPath NOTIFY llamaCppPathChanged)

public:
    /**
     * @brief 单例访问器
     */
    static LocalLLMEngine& instance();
    
    // 禁用拷贝
    LocalLLMEngine(const LocalLLMEngine&) = delete;
    LocalLLMEngine& operator=(const LocalLLMEngine&) = delete;
    
    // ===================== llama.cpp 配置 =====================
    
    /**
     * @brief 获取 llama.cpp 可执行文件路径
     */
    Q_INVOKABLE QString llamaCppPath() const { return m_llamaCppPath; }
    
    /**
     * @brief 设置 llama.cpp 可执行文件路径
     * @param path llama-cli 或 llama-server 路径
     */
    Q_INVOKABLE void setLlamaCppPath(const QString &path);
    
    /**
     * @brief 检测 llama.cpp 是否可用
     * @return 是否可用
     */
    Q_INVOKABLE bool isLlamaCppAvailable() const;
    
    /**
     * @brief 获取 llama.cpp 版本信息
     */
    Q_INVOKABLE QString getLlamaCppVersion() const;
    
    /**
     * @brief 自动检测系统中的 llama.cpp
     * @return 检测到的路径，未找到返回空
     */
    Q_INVOKABLE QString autoDetectLlamaCpp() const;
    
    // ===================== 模型管理 =====================
    
    /**
     * @brief 获取已配置的模型列表
     * @return [{name, path, size, quantization}, ...]
     */
    Q_INVOKABLE QVariantList getModels() const;
    
    /**
     * @brief 添加模型
     * @param modelPath GGUF 文件路径
     * @param name 显示名称（可选，自动从文件名提取）
     * @return 模型 ID
     */
    Q_INVOKABLE QString addModel(const QString &modelPath, const QString &name = QString());
    
    /**
     * @brief 移除模型
     * @param modelId 模型 ID
     */
    Q_INVOKABLE void removeModel(const QString &modelId);
    
    /**
     * @brief 获取当前模型 ID
     */
    Q_INVOKABLE QString currentModel() const { return m_currentModelId; }
    
    /**
     * @brief 设置当前模型
     * @param modelId 模型 ID
     */
    Q_INVOKABLE void setCurrentModel(const QString &modelId);
    
    /**
     * @brief 获取模型配置
     * @param modelId 模型 ID
     */
    Q_INVOKABLE QVariantMap getModelConfig(const QString &modelId) const;
    
    /**
     * @brief 更新模型配置
     * @param modelId 模型 ID
     * @param config 配置项
     */
    Q_INVOKABLE void updateModelConfig(const QString &modelId, const QVariantMap &config);
    
    /**
     * @brief 验证模型文件
     * @param modelPath GGUF 文件路径
     * @return {valid, name, size, quantization, error}
     */
    Q_INVOKABLE QVariantMap validateModel(const QString &modelPath) const;
    
    // ===================== 推理 =====================
    
    /**
     * @brief 状态检查
     */
    Q_INVOKABLE bool isReady() const { return m_state == InferenceState::Ready || m_state == InferenceState::Idle; }
    Q_INVOKABLE bool isGenerating() const { return m_state == InferenceState::Generating; }
    
    /**
     * @brief 生成回复
     * @param prompt 完整提示词
     * @param systemPrompt 系统提示词（可选）
     */
    Q_INVOKABLE void generate(const QString &prompt, const QString &systemPrompt = QString());
    
    /**
     * @brief 分析日志（与 AIAnalysisManager 接口一致）
     * @param logContent 日志内容
     * @param question 用户问题
     * @param systemPrompt 系统提示词
     */
    Q_INVOKABLE void analyzeLog(const QString &logContent, const QString &question,
                                 const QString &systemPrompt = QString());
    
    /**
     * @brief 取消当前生成
     */
    Q_INVOKABLE void cancelGeneration();
    
    /**
     * @brief 自然语言转查询条件
     * @param question 用户问题（如"找出所有错误"）
     * @param columns 可用列名列表
     */
    Q_INVOKABLE void naturalLanguageToQuery(const QString &question, const QStringList &columns);
    
    /**
     * @brief 检测数据异常
     * @param data 数据样本
     * @param columnName 列名
     */
    Q_INVOKABLE void detectAnomalies(const QStringList &data, const QString &columnName);
    
    /**
     * @brief 建议数据清洗规则
     * @param samples 脏数据样本
     */
    Q_INVOKABLE void suggestCleaningRules(const QStringList &samples);
    
    // ===================== 持久化 =====================
    
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE void loadSettings();

signals:
    /**
     * @brief 生成响应（流式）
     * @param token 生成的 token
     * @param isComplete 是否完成
     */
    void generationResponse(const QString &token, bool isComplete);
    
    /**
     * @brief 生成完成
     * @param fullResponse 完整响应
     */
    void generationCompleted(const QString &fullResponse);
    
    /**
     * @brief 生成错误
     * @param error 错误信息
     */
    void generationError(const QString &error);
    
    /**
     * @brief 查询条件生成完成
     * @param queryCondition 生成的查询条件 JSON
     */
    void queryConditionGenerated(const QVariantMap &queryCondition);
    
    /**
     * @brief 异常检测完成
     * @param anomalies 异常索引列表
     * @param explanations 解释列表
     */
    void anomaliesDetected(const QList<int> &anomalies, const QStringList &explanations);
    
    /**
     * @brief 清洗规则建议完成
     * @param rules 规则列表
     */
    void cleaningRulesSuggested(const QStringList &rules);
    
    void currentModelChanged();
    void stateChanged();
    void llamaCppPathChanged();
    void modelLoadProgress(int percent, const QString &message);

private slots:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);

private:
    LocalLLMEngine(QObject *parent = nullptr);
    ~LocalLLMEngine();
    
    /**
     * @brief 构建 llama.cpp 命令行参数
     */
    QStringList buildLlamaArgs(const QString &prompt, const QString &systemPrompt) const;
    
    /**
     * @brief 解析 GGUF 文件元数据
     */
    QVariantMap parseGGUFMetadata(const QString &modelPath) const;
    
    /**
     * @brief 估算模型内存需求
     */
    qint64 estimateMemoryRequirement(const QString &modelPath) const;
    
    // llama.cpp 配置
    QString m_llamaCppPath;
    
    // 模型管理
    QMap<QString, LocalModelConfig> m_models;
    QString m_currentModelId;
    
    // 推理状态
    InferenceState m_state = InferenceState::Idle;
    QProcess *m_process = nullptr;
    QString m_accumulatedOutput;
    
    // 线程安全
    mutable QMutex m_mutex;
    
    // 默认系统提示词
    static const QString DEFAULT_LOG_ANALYSIS_PROMPT;
    static const QString QUERY_GENERATION_PROMPT;
    static const QString ANOMALY_DETECTION_PROMPT;
    static const QString CLEANING_RULES_PROMPT;
};

#endif // LOCALLLMENGINE_H
