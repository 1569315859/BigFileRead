/**
 * @file AutomationEngine.h
 * @brief 自动化引擎 - 操作录制与回放
 * 
 * 功能：
 * - 录制用户操作（打开文件、过滤、搜索、导出等）
 * - 回放录制的操作序列
 * - 保存/加载脚本（JSON格式）
 * - 定时执行脚本
 * - 批量处理文件
 */

#ifndef AUTOMATIONENGINE_H
#define AUTOMATIONENGINE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QQueue>

/**
 * @brief 单个操作动作
 */
struct AutoAction {
    QString type;           // 动作类型
    QVariantMap params;     // 动作参数
    qint64 timestamp;       // 录制时的时间戳
    qint64 delayMs;         // 与上一个动作的延迟（毫秒）
    
    QJsonObject toJson() const;
    static AutoAction fromJson(const QJsonObject &json);
};

/**
 * @brief 自动化脚本
 */
struct AutoScript {
    QString name;           // 脚本名称
    QString description;    // 脚本描述
    QString version;        // 版本号
    QString author;         // 作者
    QDateTime createdAt;    // 创建时间
    QDateTime modifiedAt;   // 修改时间
    QList<AutoAction> actions;  // 动作列表
    QVariantMap variables;  // 脚本变量（可用于参数化）
    
    QJsonObject toJson() const;
    static AutoScript fromJson(const QJsonObject &json);
};

/**
 * @brief 自动化引擎类
 */
class AutomationEngine : public QObject
{
    Q_OBJECT
    
    // QML 属性
    Q_PROPERTY(bool isRecording READ isRecording NOTIFY isRecordingChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool isPaused READ isPaused NOTIFY isPausedChanged)
    Q_PROPERTY(int currentActionIndex READ currentActionIndex NOTIFY currentActionIndexChanged)
    Q_PROPERTY(int totalActions READ totalActions NOTIFY totalActionsChanged)
    Q_PROPERTY(double playbackSpeed READ playbackSpeed WRITE setPlaybackSpeed NOTIFY playbackSpeedChanged)
    Q_PROPERTY(QVariantList recentScripts READ recentScripts NOTIFY recentScriptsChanged)
    Q_PROPERTY(QVariantList recordedActions READ recordedActions NOTIFY recordedActionsChanged)
    
public:
    /**
     * @brief 支持的动作类型
     */
    enum ActionType {
        OpenFile,           // 打开文件
        CloseFile,          // 关闭文件
        ApplyFilter,        // 应用过滤器
        ClearFilter,        // 清除过滤器
        Search,             // 搜索
        GoToLine,           // 跳转到行
        AddBookmark,        // 添加书签
        RemoveBookmark,     // 移除书签
        ExportData,         // 导出数据
        ApplyHighlight,     // 应用高亮
        SetColumn,          // 设置列配置
        RunQuery,           // 执行查询
        ShowStatistics,     // 显示统计
        CreateChart,        // 创建图表
        SwitchView,         // 切换视图
        SetVariable,        // 设置变量
        WaitForCondition,   // 等待条件
        Delay,              // 延迟
        Log,                // 记录日志
        Custom              // 自定义动作
    };
    Q_ENUM(ActionType)
    
    explicit AutomationEngine(QObject *parent = nullptr);
    ~AutomationEngine();
    
    // 单例访问
    static AutomationEngine& instance() {
        static AutomationEngine inst;
        return inst;
    }
    
    // 属性访问器
    bool isRecording() const { return m_isRecording; }
    bool isPlaying() const { return m_isPlaying; }
    bool isPaused() const { return m_isPaused; }
    int currentActionIndex() const { return m_currentActionIndex; }
    int totalActions() const { return m_currentScript.actions.size(); }
    double playbackSpeed() const { return m_playbackSpeed; }
    void setPlaybackSpeed(double speed);
    QVariantList recentScripts() const { return m_recentScripts; }
    QVariantList recordedActions() const;
    
    // 录制功能
    Q_INVOKABLE void startRecording(const QString &scriptName = QString());
    Q_INVOKABLE void stopRecording();
    Q_INVOKABLE void pauseRecording();
    Q_INVOKABLE void resumeRecording();
    Q_INVOKABLE void recordAction(const QString &type, const QVariantMap &params = QVariantMap());
    Q_INVOKABLE void undoLastAction();
    Q_INVOKABLE void clearRecording();
    
    // 回放功能
    Q_INVOKABLE void playScript(const QString &scriptPath = QString());
    Q_INVOKABLE void playFromIndex(int index);
    Q_INVOKABLE void stopPlayback();
    Q_INVOKABLE void pausePlayback();
    Q_INVOKABLE void resumePlayback();
    Q_INVOKABLE void stepForward();
    Q_INVOKABLE void stepBackward();
    
    // 脚本管理
    Q_INVOKABLE bool saveScript(const QString &filePath, const QString &name = QString(), 
                                 const QString &description = QString());
    Q_INVOKABLE bool loadScript(const QString &filePath);
    Q_INVOKABLE QVariantMap getScriptInfo(const QString &filePath);
    Q_INVOKABLE QVariantList listScripts(const QString &directory = QString());
    Q_INVOKABLE bool deleteScript(const QString &filePath);
    Q_INVOKABLE bool duplicateScript(const QString &srcPath, const QString &dstPath);
    
    // 脚本编辑
    Q_INVOKABLE void insertAction(int index, const QString &type, const QVariantMap &params);
    Q_INVOKABLE void updateAction(int index, const QString &type, const QVariantMap &params);
    Q_INVOKABLE void removeAction(int index);
    Q_INVOKABLE void moveAction(int fromIndex, int toIndex);
    Q_INVOKABLE QVariantMap getAction(int index);
    
    // 变量系统
    Q_INVOKABLE void setVariable(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant getVariable(const QString &name);
    Q_INVOKABLE void clearVariables();
    Q_INVOKABLE QString expandVariables(const QString &text);
    
    // 批量处理
    Q_INVOKABLE void batchProcess(const QStringList &files, const QString &scriptPath);
    Q_INVOKABLE void cancelBatch();
    Q_INVOKABLE int batchProgress() const { return m_batchProgress; }
    
    // 定时执行
    Q_INVOKABLE void scheduleScript(const QString &scriptPath, const QDateTime &runAt);
    Q_INVOKABLE void scheduleRecurring(const QString &scriptPath, const QString &cronExpression);
    Q_INVOKABLE void cancelSchedule(const QString &scheduleId);
    Q_INVOKABLE QVariantList getScheduledTasks();
    
    // 导入/导出
    Q_INVOKABLE QString exportToLua(const QString &scriptPath);
    Q_INVOKABLE bool importFromLua(const QString &luaPath, const QString &outputPath);
    
signals:
    void isRecordingChanged();
    void isPlayingChanged();
    void isPausedChanged();
    void currentActionIndexChanged();
    void totalActionsChanged();
    void playbackSpeedChanged();
    void recentScriptsChanged();
    void recordedActionsChanged();
    
    // 录制信号
    void recordingStarted();
    void recordingStopped();
    void actionRecorded(const QString &type, const QVariantMap &params);
    
    // 回放信号
    void playbackStarted();
    void playbackFinished();
    void playbackError(const QString &error);
    void actionExecuting(int index, const QString &type, const QVariantMap &params);
    void actionCompleted(int index, bool success, const QString &result);
    
    // 批量处理信号
    void batchStarted(int totalFiles);
    void batchProgress(int current, int total, const QString &currentFile);
    void batchFinished(int successCount, int failCount);
    void batchError(const QString &file, const QString &error);
    
    // 执行动作请求（由AppController处理）
    void executeAction(const QString &type, const QVariantMap &params);
    
private slots:
    void onPlaybackTimer();
    void onScheduleCheck();
    
private:
    void executeCurrentAction();
    void advanceToNextAction();
    void updateRecentScripts(const QString &scriptPath);
    QString getScriptsDirectory() const;
    QString generateScheduleId() const;
    
    // 录制状态
    bool m_isRecording = false;
    bool m_recordingPaused = false;
    qint64 m_lastActionTime = 0;
    AutoScript m_recordingScript;
    
    // 回放状态
    bool m_isPlaying = false;
    bool m_isPaused = false;
    int m_currentActionIndex = 0;
    double m_playbackSpeed = 1.0;
    AutoScript m_currentScript;
    QTimer *m_playbackTimer = nullptr;
    
    // 批量处理
    QStringList m_batchFiles;
    int m_batchIndex = 0;
    int m_batchProgress = 0;
    int m_batchSuccessCount = 0;
    int m_batchFailCount = 0;
    bool m_batchCancelled = false;
    
    // 变量系统
    QVariantMap m_variables;
    
    // 定时任务
    struct ScheduledTask {
        QString id;
        QString scriptPath;
        QDateTime nextRun;
        QString cronExpression;
        bool recurring;
    };
    QList<ScheduledTask> m_scheduledTasks;
    QTimer *m_scheduleTimer = nullptr;
    
    // 最近脚本
    QVariantList m_recentScripts;
    static const int MAX_RECENT_SCRIPTS = 10;
};

#endif // AUTOMATIONENGINE_H
