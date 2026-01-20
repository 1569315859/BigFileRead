#include "KeywordConfigManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

KeywordConfigManager::KeywordConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings("BigFileViewer", "BigFileViewer")
{
    loadFromSettings();
    qDebug() << "[KeywordConfigManager] Loaded" << m_keywords.size() << "keywords";
}

QVariantMap KeywordConfigManager::keywords() const
{
    return m_keywords;
}

QVariantList KeywordConfigManager::keywordList() const
{
    QVariantList list;
    for (auto it = m_keywords.constBegin(); it != m_keywords.constEnd(); ++it) {
        QVariantMap item;
        item["keyword"] = it.key();
        item["color"] = it.value().toString();
        list.append(item);
    }
    // 按优先级排序（FATAL > ERROR > WARN > INFO > DEBUG > TRACE > 其他）
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        static const QStringList priority = {
            "FATAL", "CRITICAL", "ERROR", "FAIL", "FAILED", "EXCEPTION",
            "WARN", "WARNING", "ALERT",
            "INFO", "NOTICE", "SUCCESS",
            "DEBUG", "VERBOSE",
            "TRACE"
        };
        QString keyA = a.toMap()["keyword"].toString().toUpper();
        QString keyB = b.toMap()["keyword"].toString().toUpper();
        int indexA = priority.indexOf(keyA);
        int indexB = priority.indexOf(keyB);
        if (indexA < 0) indexA = 999;
        if (indexB < 0) indexB = 999;
        return indexA < indexB;
    });
    return list;
}

void KeywordConfigManager::setKeyword(const QString &keyword, const QString &color)
{
    if (keyword.isEmpty()) return;
    
    QString upperKeyword = keyword.toUpper();
    if (m_keywords.value(upperKeyword).toString() != color) {
        m_keywords[upperKeyword] = color;
        saveToSettings();
        emit keywordsChanged();
    }
}

void KeywordConfigManager::removeKeyword(const QString &keyword)
{
    QString upperKeyword = keyword.toUpper();
    if (m_keywords.contains(upperKeyword)) {
        m_keywords.remove(upperKeyword);
        saveToSettings();
        emit keywordsChanged();
    }
}

QString KeywordConfigManager::getColor(const QString &keyword) const
{
    return m_keywords.value(keyword.toUpper()).toString();
}

bool KeywordConfigManager::hasKeyword(const QString &keyword) const
{
    return m_keywords.contains(keyword.toUpper());
}

void KeywordConfigManager::resetToDefaults()
{
    m_keywords.clear();
    initDefaults();
    saveToSettings();
    emit keywordsChanged();
}

QString KeywordConfigManager::exportConfig() const
{
    QJsonObject obj;
    for (auto it = m_keywords.constBegin(); it != m_keywords.constEnd(); ++it) {
        obj[it.key()] = it.value().toString();
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool KeywordConfigManager::importConfig(const QString &json)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    
    QJsonObject obj = doc.object();
    m_keywords.clear();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        m_keywords[it.key().toUpper()] = it.value().toString();
    }
    saveToSettings();
    emit keywordsChanged();
    return true;
}

void KeywordConfigManager::loadFromSettings()
{
    m_keywords.clear();
    
    int size = m_settings.beginReadArray("keywords");
    if (size == 0) {
        m_settings.endArray();
        initDefaults();
        saveToSettings();
        return;
    }
    
    for (int i = 0; i < size; ++i) {
        m_settings.setArrayIndex(i);
        QString keyword = m_settings.value("keyword").toString();
        QString color = m_settings.value("color").toString();
        if (!keyword.isEmpty() && !color.isEmpty()) {
            m_keywords[keyword.toUpper()] = color;
        }
    }
    m_settings.endArray();
}

void KeywordConfigManager::saveToSettings()
{
    m_settings.beginWriteArray("keywords");
    int index = 0;
    for (auto it = m_keywords.constBegin(); it != m_keywords.constEnd(); ++it) {
        m_settings.setArrayIndex(index++);
        m_settings.setValue("keyword", it.key());
        m_settings.setValue("color", it.value().toString());
    }
    m_settings.endArray();
    m_settings.sync();
}

void KeywordConfigManager::initDefaults()
{
    // ========== 错误级别（红色系）==========
    m_keywords["FATAL"]     = "#FF0000";  // 纯红色 - 致命错误
    m_keywords["CRITICAL"]  = "#FF0000";  // 纯红色 - 严重错误
    m_keywords["ERROR"]     = "#FF6B6B";  // 浅红色 - 错误
    m_keywords["FAIL"]      = "#FF6B6B";  // 浅红色 - 失败
    m_keywords["FAILED"]    = "#FF6B6B";  // 浅红色 - 失败
    m_keywords["EXCEPTION"] = "#FF4444";  // 红色 - 异常
    m_keywords["PANIC"]     = "#FF0000";  // 纯红色 - 恐慌
    m_keywords["ABORT"]     = "#FF0000";  // 纯红色 - 中止
    
    // ========== 警告级别（橙黄色系）==========
    m_keywords["WARN"]      = "#FFA500";  // 橙色 - 警告
    m_keywords["WARNING"]   = "#FFA500";  // 橙色 - 警告
    m_keywords["ALERT"]     = "#FFD700";  // 金色 - 警报
    m_keywords["CAUTION"]   = "#FFD700";  // 金色 - 注意
    m_keywords["TIMEOUT"]   = "#FF8C00";  // 深橙色 - 超时
    m_keywords["SLOW"]      = "#FF8C00";  // 深橙色 - 慢
    m_keywords["DEPRECATED"]= "#FFA500";  // 橙色 - 已弃用
    
    // ========== 信息级别（蓝色系）==========
    m_keywords["INFO"]      = "#00BFFF";  // 深天蓝色 - 信息
    m_keywords["NOTICE"]    = "#87CEEB";  // 天蓝色 - 通知
    m_keywords["LOG"]       = "#87CEEB";  // 天蓝色 - 日志
    
    // ========== 成功/确认（绿色系）==========
    m_keywords["SUCCESS"]   = "#32CD32";  // 酸橙绿 - 成功
    m_keywords["OK"]        = "#32CD32";  // 酸橙绿 - 确定
    m_keywords["DONE"]      = "#32CD32";  // 酸橙绿 - 完成
    m_keywords["PASS"]      = "#32CD32";  // 酸橙绿 - 通过
    m_keywords["PASSED"]    = "#32CD32";  // 酸橙绿 - 通过
    m_keywords["COMPLETE"]  = "#32CD32";  // 酸橙绿 - 完成
    m_keywords["CONNECTED"] = "#00FF7F";  // 春绿色 - 已连接
    m_keywords["STARTED"]   = "#00FF7F";  // 春绿色 - 已启动
    
    // ========== 调试级别（紫色系 - 比灰色更明显）==========
    m_keywords["DEBUG"]     = "#DA70D6";  // 兰花紫 - 调试（更明显）
    m_keywords["VERBOSE"]   = "#BA55D3";  // 中兰紫 - 详细
    m_keywords["DETAIL"]    = "#BA55D3";  // 中兰紫 - 详细
    
    // ========== 跟踪级别（青色系）==========
    m_keywords["TRACE"]     = "#20B2AA";  // 浅海绿色 - 跟踪
    m_keywords["ENTER"]     = "#20B2AA";  // 浅海绿色 - 进入
    m_keywords["EXIT"]      = "#20B2AA";  // 浅海绿色 - 退出
    m_keywords["BEGIN"]     = "#48D1CC";  // 中绿松石色 - 开始
    m_keywords["END"]       = "#48D1CC";  // 中绿松石色 - 结束
    
    // ========== 状态关键字 ==========
    m_keywords["DISCONNECTED"] = "#DC143C"; // 深红色 - 断开连接
    m_keywords["STOPPED"]   = "#DC143C";    // 深红色 - 已停止
    m_keywords["OFFLINE"]   = "#DC143C";    // 深红色 - 离线
    m_keywords["ONLINE"]    = "#00FF7F";    // 春绿色 - 在线
    m_keywords["PENDING"]   = "#FFD700";    // 金色 - 等待中
    m_keywords["WAITING"]   = "#FFD700";    // 金色 - 等待
    m_keywords["RUNNING"]   = "#00BFFF";    // 深天蓝色 - 运行中
    m_keywords["LOADING"]   = "#00BFFF";    // 深天蓝色 - 加载中
}
