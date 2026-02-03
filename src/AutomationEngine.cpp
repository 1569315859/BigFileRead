/**
 * @file AutomationEngine.cpp
 * @brief 自动化引擎实现
 */

#include "AutomationEngine.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDebug>
#include <QRegularExpression>
#include <QUuid>

// ============================================================================
// AutoAction 实现
// ============================================================================

QJsonObject AutoAction::toJson() const
{
    QJsonObject obj;
    obj["type"] = type;
    obj["params"] = QJsonObject::fromVariantMap(params);
    obj["timestamp"] = timestamp;
    obj["delayMs"] = delayMs;
    return obj;
}

AutoAction AutoAction::fromJson(const QJsonObject &json)
{
    AutoAction action;
    action.type = json["type"].toString();
    action.params = json["params"].toObject().toVariantMap();
    action.timestamp = json["timestamp"].toVariant().toLongLong();
    action.delayMs = json["delayMs"].toVariant().toLongLong();
    return action;
}

// ============================================================================
// AutoScript 实现
// ============================================================================

QJsonObject AutoScript::toJson() const
{
    QJsonObject obj;
    obj["name"] = name;
    obj["description"] = description;
    obj["version"] = version;
    obj["author"] = author;
    obj["createdAt"] = createdAt.toString(Qt::ISODate);
    obj["modifiedAt"] = modifiedAt.toString(Qt::ISODate);
    obj["variables"] = QJsonObject::fromVariantMap(variables);
    
    QJsonArray actionsArray;
    for (const auto &action : actions) {
        actionsArray.append(action.toJson());
    }
    obj["actions"] = actionsArray;
    
    return obj;
}

AutoScript AutoScript::fromJson(const QJsonObject &json)
{
    AutoScript script;
    script.name = json["name"].toString();
    script.description = json["description"].toString();
    script.version = json["version"].toString("1.0");
    script.author = json["author"].toString();
    script.createdAt = QDateTime::fromString(json["createdAt"].toString(), Qt::ISODate);
    script.modifiedAt = QDateTime::fromString(json["modifiedAt"].toString(), Qt::ISODate);
    script.variables = json["variables"].toObject().toVariantMap();
    
    QJsonArray actionsArray = json["actions"].toArray();
    for (const auto &actionVal : actionsArray) {
        script.actions.append(AutoAction::fromJson(actionVal.toObject()));
    }
    
    return script;
}

// ============================================================================
// AutomationEngine 实现
// ============================================================================

AutomationEngine::AutomationEngine(QObject *parent)
    : QObject(parent)
    , m_playbackTimer(new QTimer(this))
    , m_scheduleTimer(new QTimer(this))
{
    connect(m_playbackTimer, &QTimer::timeout, this, &AutomationEngine::onPlaybackTimer);
    connect(m_scheduleTimer, &QTimer::timeout, this, &AutomationEngine::onScheduleCheck);
    
    // 每分钟检查一次定时任务
    m_scheduleTimer->start(60000);
    
    // 加载最近脚本列表
    // TODO: 从设置中加载
}

AutomationEngine::~AutomationEngine()
{
    if (m_isRecording) {
        stopRecording();
    }
    if (m_isPlaying) {
        stopPlayback();
    }
}

void AutomationEngine::setPlaybackSpeed(double speed)
{
    if (speed > 0 && speed <= 10.0 && m_playbackSpeed != speed) {
        m_playbackSpeed = speed;
        emit playbackSpeedChanged();
    }
}

QVariantList AutomationEngine::recordedActions() const
{
    QVariantList list;
    for (const auto &action : m_recordingScript.actions) {
        QVariantMap map;
        map["type"] = action.type;
        map["params"] = action.params;
        map["delayMs"] = action.delayMs;
        list.append(map);
    }
    return list;
}

// ============================================================================
// 录制功能
// ============================================================================

void AutomationEngine::startRecording(const QString &scriptName)
{
    if (m_isRecording) {
        return;
    }
    
    m_recordingScript = AutoScript();
    m_recordingScript.name = scriptName.isEmpty() ? 
        tr("Recording %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")) : 
        scriptName;
    m_recordingScript.createdAt = QDateTime::currentDateTime();
    m_recordingScript.version = "1.0";
    
    m_isRecording = true;
    m_recordingPaused = false;
    m_lastActionTime = QDateTime::currentMSecsSinceEpoch();
    
    emit isRecordingChanged();
    emit recordingStarted();
    emit totalActionsChanged();
    
    qDebug() << "[AutomationEngine] Recording started:" << m_recordingScript.name;
}

void AutomationEngine::stopRecording()
{
    if (!m_isRecording) {
        return;
    }
    
    m_isRecording = false;
    m_recordingPaused = false;
    m_recordingScript.modifiedAt = QDateTime::currentDateTime();
    
    // 将录制的脚本复制到当前脚本，以便回放
    m_currentScript = m_recordingScript;
    
    emit isRecordingChanged();
    emit recordingStopped();
    emit totalActionsChanged();  // 通知QML更新Play按钮状态
    
    qDebug() << "[AutomationEngine] Recording stopped. Actions:" << m_recordingScript.actions.size();
}

void AutomationEngine::pauseRecording()
{
    if (m_isRecording && !m_recordingPaused) {
        m_recordingPaused = true;
        qDebug() << "[AutomationEngine] Recording paused";
    }
}

void AutomationEngine::resumeRecording()
{
    if (m_isRecording && m_recordingPaused) {
        m_recordingPaused = false;
        m_lastActionTime = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "[AutomationEngine] Recording resumed";
    }
}

void AutomationEngine::recordAction(const QString &type, const QVariantMap &params)
{
    if (!m_isRecording || m_recordingPaused) {
        return;
    }
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    AutoAction action;
    action.type = type;
    action.params = params;
    action.timestamp = now;
    action.delayMs = m_recordingScript.actions.isEmpty() ? 0 : (now - m_lastActionTime);
    
    m_recordingScript.actions.append(action);
    m_lastActionTime = now;
    
    emit actionRecorded(type, params);
    emit recordedActionsChanged();
    emit totalActionsChanged();
    
    qDebug() << "[AutomationEngine] Action recorded:" << type << params;
}

void AutomationEngine::undoLastAction()
{
    if (!m_recordingScript.actions.isEmpty()) {
        m_recordingScript.actions.removeLast();
        emit recordedActionsChanged();
        emit totalActionsChanged();
    }
}

void AutomationEngine::clearRecording()
{
    m_recordingScript.actions.clear();
    emit recordedActionsChanged();
    emit totalActionsChanged();
}

// ============================================================================
// 回放功能
// ============================================================================

void AutomationEngine::playScript(const QString &scriptPath)
{
    if (m_isPlaying) {
        stopPlayback();
    }
    
    // 如果没有指定脚本路径，使用当前录制的脚本
    if (scriptPath.isEmpty()) {
        if (m_recordingScript.actions.isEmpty()) {
            emit playbackError(tr("No script to play"));
            return;
        }
        m_currentScript = m_recordingScript;
    } else {
        if (!loadScript(scriptPath)) {
            emit playbackError(tr("Failed to load script: %1").arg(scriptPath));
            return;
        }
    }
    
    if (m_currentScript.actions.isEmpty()) {
        emit playbackError(tr("Script has no actions"));
        return;
    }
    
    m_isPlaying = true;
    m_isPaused = false;
    m_currentActionIndex = 0;
    
    emit isPlayingChanged();
    emit currentActionIndexChanged();
    emit playbackStarted();
    
    // 开始执行第一个动作
    executeCurrentAction();
}

void AutomationEngine::playFromIndex(int index)
{
    if (index < 0 || index >= m_currentScript.actions.size()) {
        return;
    }
    
    m_currentActionIndex = index;
    emit currentActionIndexChanged();
    
    if (!m_isPlaying) {
        m_isPlaying = true;
        m_isPaused = false;
        emit isPlayingChanged();
        emit playbackStarted();
    }
    
    executeCurrentAction();
}

void AutomationEngine::stopPlayback()
{
    if (!m_isPlaying) {
        return;
    }
    
    m_playbackTimer->stop();
    m_isPlaying = false;
    m_isPaused = false;
    
    emit isPlayingChanged();
    emit isPausedChanged();
    emit playbackFinished();
    
    qDebug() << "[AutomationEngine] Playback stopped";
}

void AutomationEngine::pausePlayback()
{
    if (m_isPlaying && !m_isPaused) {
        m_playbackTimer->stop();
        m_isPaused = true;
        emit isPausedChanged();
        qDebug() << "[AutomationEngine] Playback paused";
    }
}

void AutomationEngine::resumePlayback()
{
    if (m_isPlaying && m_isPaused) {
        m_isPaused = false;
        emit isPausedChanged();
        executeCurrentAction();
        qDebug() << "[AutomationEngine] Playback resumed";
    }
}

void AutomationEngine::stepForward()
{
    if (!m_isPlaying || m_currentActionIndex >= m_currentScript.actions.size()) {
        return;
    }
    
    // 暂停并执行当前动作
    m_isPaused = true;
    emit isPausedChanged();
    executeCurrentAction();
}

void AutomationEngine::stepBackward()
{
    if (m_currentActionIndex > 0) {
        m_currentActionIndex--;
        emit currentActionIndexChanged();
    }
}

void AutomationEngine::executeCurrentAction()
{
    if (m_currentActionIndex >= m_currentScript.actions.size()) {
        stopPlayback();
        return;
    }
    
    const AutoAction &action = m_currentScript.actions[m_currentActionIndex];
    
    // 展开变量
    QVariantMap expandedParams = action.params;
    for (auto it = expandedParams.begin(); it != expandedParams.end(); ++it) {
        if (it.value().typeId() == QMetaType::QString) {
            it.value() = expandVariables(it.value().toString());
        }
    }
    
    emit actionExecuting(m_currentActionIndex, action.type, expandedParams);
    emit executeAction(action.type, expandedParams);
    
    // 标记动作完成（实际应该等待AppController返回结果）
    emit actionCompleted(m_currentActionIndex, true, QString());
    
    // 计算下一个动作的延迟
    advanceToNextAction();
}

void AutomationEngine::advanceToNextAction()
{
    m_currentActionIndex++;
    emit currentActionIndexChanged();
    
    if (m_currentActionIndex >= m_currentScript.actions.size()) {
        stopPlayback();
        return;
    }
    
    if (m_isPaused) {
        return; // 单步执行模式
    }
    
    // 计算延迟
    const AutoAction &nextAction = m_currentScript.actions[m_currentActionIndex];
    int delay = static_cast<int>(nextAction.delayMs / m_playbackSpeed);
    
    // 最小延迟10ms，最大延迟5秒
    delay = qBound(10, delay, 5000);
    
    m_playbackTimer->start(delay);
}

void AutomationEngine::onPlaybackTimer()
{
    m_playbackTimer->stop();
    if (m_isPlaying && !m_isPaused) {
        executeCurrentAction();
    }
}

// ============================================================================
// 脚本管理
// ============================================================================

bool AutomationEngine::saveScript(const QString &filePath, const QString &name, 
                                   const QString &description)
{
    AutoScript scriptToSave = m_recordingScript;
    
    if (!name.isEmpty()) {
        scriptToSave.name = name;
    }
    if (!description.isEmpty()) {
        scriptToSave.description = description;
    }
    scriptToSave.modifiedAt = QDateTime::currentDateTime();
    
    QJsonDocument doc(scriptToSave.toJson());
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[AutomationEngine] Failed to save script:" << filePath;
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    updateRecentScripts(filePath);
    
    qDebug() << "[AutomationEngine] Script saved:" << filePath;
    return true;
}

bool AutomationEngine::loadScript(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[AutomationEngine] Failed to load script:" << filePath;
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "[AutomationEngine] JSON parse error:" << error.errorString();
        return false;
    }
    
    m_currentScript = AutoScript::fromJson(doc.object());
    emit totalActionsChanged();
    
    updateRecentScripts(filePath);
    
    qDebug() << "[AutomationEngine] Script loaded:" << filePath 
             << "Actions:" << m_currentScript.actions.size();
    return true;
}

QVariantMap AutomationEngine::getScriptInfo(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QVariantMap();
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    QJsonObject obj = doc.object();
    QVariantMap info;
    info["name"] = obj["name"].toString();
    info["description"] = obj["description"].toString();
    info["version"] = obj["version"].toString();
    info["author"] = obj["author"].toString();
    info["createdAt"] = obj["createdAt"].toString();
    info["modifiedAt"] = obj["modifiedAt"].toString();
    info["actionCount"] = obj["actions"].toArray().size();
    info["filePath"] = filePath;
    
    return info;
}

QVariantList AutomationEngine::listScripts(const QString &directory)
{
    QString dir = directory.isEmpty() ? getScriptsDirectory() : directory;
    QDir scriptsDir(dir);
    
    QVariantList scripts;
    QStringList filters;
    filters << "*.bfvscript" << "*.json";
    
    for (const QFileInfo &fileInfo : scriptsDir.entryInfoList(filters, QDir::Files)) {
        QVariantMap info = getScriptInfo(fileInfo.absoluteFilePath());
        if (!info.isEmpty()) {
            scripts.append(info);
        }
    }
    
    return scripts;
}

bool AutomationEngine::deleteScript(const QString &filePath)
{
    return QFile::remove(filePath);
}

bool AutomationEngine::duplicateScript(const QString &srcPath, const QString &dstPath)
{
    return QFile::copy(srcPath, dstPath);
}

// ============================================================================
// 脚本编辑
// ============================================================================

void AutomationEngine::insertAction(int index, const QString &type, const QVariantMap &params)
{
    if (index < 0 || index > m_recordingScript.actions.size()) {
        return;
    }
    
    AutoAction action;
    action.type = type;
    action.params = params;
    action.timestamp = QDateTime::currentMSecsSinceEpoch();
    action.delayMs = 100; // 默认延迟
    
    m_recordingScript.actions.insert(index, action);
    emit recordedActionsChanged();
    emit totalActionsChanged();
}

void AutomationEngine::updateAction(int index, const QString &type, const QVariantMap &params)
{
    if (index < 0 || index >= m_recordingScript.actions.size()) {
        return;
    }
    
    m_recordingScript.actions[index].type = type;
    m_recordingScript.actions[index].params = params;
    emit recordedActionsChanged();
}

void AutomationEngine::removeAction(int index)
{
    if (index < 0 || index >= m_recordingScript.actions.size()) {
        return;
    }
    
    m_recordingScript.actions.removeAt(index);
    emit recordedActionsChanged();
    emit totalActionsChanged();
}

void AutomationEngine::moveAction(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_recordingScript.actions.size() ||
        toIndex < 0 || toIndex >= m_recordingScript.actions.size()) {
        return;
    }
    
    AutoAction action = m_recordingScript.actions.takeAt(fromIndex);
    m_recordingScript.actions.insert(toIndex, action);
    emit recordedActionsChanged();
}

QVariantMap AutomationEngine::getAction(int index)
{
    if (index < 0 || index >= m_recordingScript.actions.size()) {
        return QVariantMap();
    }
    
    const AutoAction &action = m_recordingScript.actions[index];
    QVariantMap map;
    map["type"] = action.type;
    map["params"] = action.params;
    map["delayMs"] = action.delayMs;
    return map;
}

// ============================================================================
// 变量系统
// ============================================================================

void AutomationEngine::setVariable(const QString &name, const QVariant &value)
{
    m_variables[name] = value;
}

QVariant AutomationEngine::getVariable(const QString &name)
{
    return m_variables.value(name);
}

void AutomationEngine::clearVariables()
{
    m_variables.clear();
}

QString AutomationEngine::expandVariables(const QString &text)
{
    QString result = text;
    
    // 替换 ${variableName} 格式的变量
    QRegularExpression re("\\$\\{(\\w+)\\}");
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        if (m_variables.contains(varName)) {
            result.replace(match.captured(0), m_variables[varName].toString());
        }
    }
    
    // 内置变量
    result.replace("${DATE}", QDate::currentDate().toString("yyyy-MM-dd"));
    result.replace("${TIME}", QTime::currentTime().toString("HH:mm:ss"));
    result.replace("${DATETIME}", QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));
    result.replace("${TIMESTAMP}", QString::number(QDateTime::currentSecsSinceEpoch()));
    
    return result;
}

// ============================================================================
// 批量处理
// ============================================================================

void AutomationEngine::batchProcess(const QStringList &files, const QString &scriptPath)
{
    if (files.isEmpty()) {
        return;
    }
    
    if (!loadScript(scriptPath)) {
        emit batchError(QString(), tr("Failed to load script"));
        return;
    }
    
    m_batchFiles = files;
    m_batchIndex = 0;
    m_batchSuccessCount = 0;
    m_batchFailCount = 0;
    m_batchCancelled = false;
    
    emit batchStarted(files.size());
    
    // 设置当前文件变量并开始处理
    setVariable("CURRENT_FILE", m_batchFiles[0]);
    setVariable("BATCH_INDEX", 0);
    setVariable("BATCH_TOTAL", m_batchFiles.size());
    
    // 连接回放完成信号以处理下一个文件
    connect(this, &AutomationEngine::playbackFinished, this, [this]() {
        if (m_batchCancelled) {
            return;
        }
        
        m_batchSuccessCount++;
        m_batchIndex++;
        m_batchProgress = (m_batchIndex * 100) / m_batchFiles.size();
        
        emit batchProgress(m_batchIndex, m_batchFiles.size(), 
                          m_batchIndex < m_batchFiles.size() ? m_batchFiles[m_batchIndex] : QString());
        
        if (m_batchIndex < m_batchFiles.size()) {
            // 处理下一个文件
            setVariable("CURRENT_FILE", m_batchFiles[m_batchIndex]);
            setVariable("BATCH_INDEX", m_batchIndex);
            playScript();
        } else {
            // 批量处理完成
            emit batchFinished(m_batchSuccessCount, m_batchFailCount);
        }
    }, Qt::SingleShotConnection);
    
    playScript();
}

void AutomationEngine::cancelBatch()
{
    m_batchCancelled = true;
    stopPlayback();
    emit batchFinished(m_batchSuccessCount, m_batchFiles.size() - m_batchIndex);
}

// ============================================================================
// 定时执行
// ============================================================================

void AutomationEngine::scheduleScript(const QString &scriptPath, const QDateTime &runAt)
{
    ScheduledTask task;
    task.id = generateScheduleId();
    task.scriptPath = scriptPath;
    task.nextRun = runAt;
    task.recurring = false;
    
    m_scheduledTasks.append(task);
    
    qDebug() << "[AutomationEngine] Script scheduled:" << scriptPath << "at" << runAt;
}

void AutomationEngine::scheduleRecurring(const QString &scriptPath, const QString &cronExpression)
{
    // 简化的 cron 支持（仅支持基本格式）
    ScheduledTask task;
    task.id = generateScheduleId();
    task.scriptPath = scriptPath;
    task.cronExpression = cronExpression;
    task.recurring = true;
    
    // TODO: 解析 cron 表达式计算下次运行时间
    task.nextRun = QDateTime::currentDateTime().addSecs(3600); // 临时：1小时后
    
    m_scheduledTasks.append(task);
}

void AutomationEngine::cancelSchedule(const QString &scheduleId)
{
    for (int i = 0; i < m_scheduledTasks.size(); ++i) {
        if (m_scheduledTasks[i].id == scheduleId) {
            m_scheduledTasks.removeAt(i);
            break;
        }
    }
}

QVariantList AutomationEngine::getScheduledTasks()
{
    QVariantList list;
    for (const auto &task : m_scheduledTasks) {
        QVariantMap map;
        map["id"] = task.id;
        map["scriptPath"] = task.scriptPath;
        map["nextRun"] = task.nextRun.toString(Qt::ISODate);
        map["recurring"] = task.recurring;
        map["cronExpression"] = task.cronExpression;
        list.append(map);
    }
    return list;
}

void AutomationEngine::onScheduleCheck()
{
    QDateTime now = QDateTime::currentDateTime();
    
    for (int i = m_scheduledTasks.size() - 1; i >= 0; --i) {
        ScheduledTask &task = m_scheduledTasks[i];
        
        if (task.nextRun <= now) {
            qDebug() << "[AutomationEngine] Running scheduled script:" << task.scriptPath;
            playScript(task.scriptPath);
            
            if (task.recurring) {
                // TODO: 根据 cron 表达式计算下次运行时间
                task.nextRun = now.addSecs(3600);
            } else {
                m_scheduledTasks.removeAt(i);
            }
        }
    }
}

// ============================================================================
// 导入/导出
// ============================================================================

QString AutomationEngine::exportToLua(const QString &scriptPath)
{
    if (!loadScript(scriptPath)) {
        return QString();
    }
    
    QString lua;
    lua += "-- BigFileViewer Automation Script\n";
    lua += QString("-- Generated from: %1\n").arg(m_currentScript.name);
    lua += QString("-- Date: %1\n\n").arg(QDateTime::currentDateTime().toString());
    
    for (const auto &action : m_currentScript.actions) {
        lua += QString("bfv.%1(").arg(action.type.toLower());
        
        QStringList params;
        for (auto it = action.params.begin(); it != action.params.end(); ++it) {
            QString value = it.value().toString();
            if (it.value().typeId() == QMetaType::QString) {
                value = QString("\"%1\"").arg(value.replace("\"", "\\\""));
            }
            params.append(QString("%1 = %2").arg(it.key(), value));
        }
        
        lua += params.join(", ");
        lua += ")\n";
        
        if (action.delayMs > 100) {
            lua += QString("bfv.wait(%1)\n").arg(action.delayMs);
        }
    }
    
    return lua;
}

bool AutomationEngine::importFromLua(const QString &luaPath, const QString &outputPath)
{
    // TODO: 实现 Lua 脚本解析
    Q_UNUSED(luaPath)
    Q_UNUSED(outputPath)
    return false;
}

// ============================================================================
// 辅助方法
// ============================================================================

void AutomationEngine::updateRecentScripts(const QString &scriptPath)
{
    // 移除已存在的
    for (int i = m_recentScripts.size() - 1; i >= 0; --i) {
        if (m_recentScripts[i].toMap()["path"].toString() == scriptPath) {
            m_recentScripts.removeAt(i);
        }
    }
    
    // 添加到开头
    QVariantMap entry;
    entry["path"] = scriptPath;
    entry["name"] = QFileInfo(scriptPath).baseName();
    entry["accessedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_recentScripts.prepend(entry);
    
    // 限制数量
    while (m_recentScripts.size() > MAX_RECENT_SCRIPTS) {
        m_recentScripts.removeLast();
    }
    
    emit recentScriptsChanged();
}

QString AutomationEngine::getScriptsDirectory() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/scripts";
    QDir().mkpath(dir);
    return dir;
}

QString AutomationEngine::generateScheduleId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}
