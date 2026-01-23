/**
 * @file DataSanitizer.cpp
 * @brief 数据脱敏处理器实现
 */

#include "DataSanitizer.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QSet>

// 内置规则名称常量
const QString DataSanitizer::RULE_IP = "ip_address";
const QString DataSanitizer::RULE_IPV6 = "ipv6_address";
const QString DataSanitizer::RULE_EMAIL = "email";
const QString DataSanitizer::RULE_PHONE_CN = "phone_cn";
const QString DataSanitizer::RULE_PHONE_INTL = "phone_intl";
const QString DataSanitizer::RULE_IDCARD_CN = "idcard_cn";
const QString DataSanitizer::RULE_DOMAIN = "domain";
const QString DataSanitizer::RULE_PORT = "port";
const QString DataSanitizer::RULE_API_KEY = "api_key";
const QString DataSanitizer::RULE_TOKEN = "token";
const QString DataSanitizer::RULE_PASSWORD = "password";
const QString DataSanitizer::RULE_USERNAME = "username";
const QString DataSanitizer::RULE_PATH_WIN = "path_windows";
const QString DataSanitizer::RULE_PATH_UNIX = "path_unix";
const QString DataSanitizer::RULE_MAC_ADDRESS = "mac_address";
const QString DataSanitizer::RULE_CREDIT_CARD = "credit_card";
const QString DataSanitizer::RULE_SSN = "ssn";

DataSanitizer& DataSanitizer::instance()
{
    static DataSanitizer instance;
    return instance;
}

DataSanitizer::DataSanitizer(QObject *parent)
    : QObject(parent)
{
    initBuiltinRules();
    loadSettings();
    compilePatterns();
}

void DataSanitizer::initBuiltinRules()
{
    // IPv4 地址: 192.168.1.1 -> [IP:***.***.*.***]
    m_rules[RULE_IP] = SanitizeRule(
        tr("IP Address (IPv4)"),
        R"(\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b)",
        "[IP:***.***.***.***]",
        true, true
    );
    
    // IPv6 地址
    m_rules[RULE_IPV6] = SanitizeRule(
        tr("IP Address (IPv6)"),
        R"(\b(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}\b|\b(?:[0-9a-fA-F]{1,4}:){1,7}:\b|\b(?:[0-9a-fA-F]{1,4}:){1,6}:[0-9a-fA-F]{1,4}\b)",
        "[IPv6:****:****:****:****]",
        true, true
    );
    
    // 邮箱地址: user@example.com -> [EMAIL:***@***.***]
    m_rules[RULE_EMAIL] = SanitizeRule(
        tr("Email Address"),
        R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)",
        "[EMAIL:***@***.***]",
        true, true
    );
    
    // 中国手机号: 13812345678 -> [PHONE:138****5678]
    m_rules[RULE_PHONE_CN] = SanitizeRule(
        tr("Phone Number (China)"),
        R"(\b1[3-9]\d{9}\b)",
        "[PHONE:***********]",
        true, true
    );
    
    // 国际电话号码: +1-234-567-8900
    m_rules[RULE_PHONE_INTL] = SanitizeRule(
        tr("Phone Number (International)"),
        R"(\+?\d{1,3}[-.\s]?\(?\d{1,4}\)?[-.\s]?\d{1,4}[-.\s]?\d{1,9})",
        "[PHONE:+*-***-***-****]",
        false, true  // 默认关闭，可能误匹配
    );
    
    // 中国身份证号: 110101199001011234 -> [IDCARD:1101**********1234]
    m_rules[RULE_IDCARD_CN] = SanitizeRule(
        tr("ID Card (China)"),
        R"(\b[1-9]\d{5}(?:18|19|20)\d{2}(?:0[1-9]|1[0-2])(?:0[1-9]|[12]\d|3[01])\d{3}[\dXx]\b)",
        "[IDCARD:****************]",
        true, true
    );
    
    // 域名: www.example.com -> [DOMAIN:***]
    m_rules[RULE_DOMAIN] = SanitizeRule(
        tr("Domain Name"),
        R"(\b(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)+[a-zA-Z]{2,}\b)",
        "[DOMAIN:***]",
        false, true  // 默认关闭，日志中域名通常需要保留
    );
    
    // 端口号: :8080 -> :[PORT]
    m_rules[RULE_PORT] = SanitizeRule(
        tr("Port Number"),
        R"(:([0-9]{2,5})\b)",
        ":[PORT]",
        false, true  // 默认关闭
    );
    
    // API Key: api_key=xxx, apikey:xxx, api-key=xxx
    m_rules[RULE_API_KEY] = SanitizeRule(
        tr("API Key"),
        R"((?:api[_-]?key|apikey|api[_-]?secret|secret[_-]?key)[=:]\s*['"]?([a-zA-Z0-9_\-]{16,})['"]?)",
        "[API_KEY:***]",
        true, true
    );
    
    // Token: token=xxx, bearer xxx, authorization: bearer xxx
    m_rules[RULE_TOKEN] = SanitizeRule(
        tr("Token/Bearer"),
        R"((?:token|bearer|authorization)[=:\s]+['"]?([a-zA-Z0-9_\-\.]{20,})['"]?)",
        "[TOKEN:***]",
        true, true
    );
    
    // 密码字段: password=xxx, passwd:xxx, pwd=xxx
    m_rules[RULE_PASSWORD] = SanitizeRule(
        tr("Password Field"),
        R"((?:password|passwd|pwd|secret)[=:]\s*['"]?([^\s'"]{1,})['"]?)",
        "[PASSWORD:***]",
        true, true
    );
    
    // 用户名字段: username=xxx, user:xxx, login=xxx
    m_rules[RULE_USERNAME] = SanitizeRule(
        tr("Username Field"),
        R"((?:username|user|login|account)[=:]\s*['"]?([a-zA-Z0-9_\-\.@]{1,64})['"]?)",
        "[USERNAME:***]",
        false, true  // 默认关闭
    );
    
    // Windows 文件路径: C:\Users\xxx\Documents
    m_rules[RULE_PATH_WIN] = SanitizeRule(
        tr("File Path (Windows)"),
        R"([A-Za-z]:\\(?:Users|Documents and Settings)\\[^\\/:*?"<>|\r\n]+)",
        "[PATH:***]",
        false, true  // 默认关闭
    );
    
    // Unix 文件路径: /home/user/xxx
    m_rules[RULE_PATH_UNIX] = SanitizeRule(
        tr("File Path (Unix)"),
        R"(/(?:home|Users)/[a-zA-Z0-9_\-\.]+)",
        "[PATH:***]",
        false, true  // 默认关闭
    );
    
    // MAC 地址: 00:1A:2B:3C:4D:5E
    m_rules[RULE_MAC_ADDRESS] = SanitizeRule(
        tr("MAC Address"),
        R"(\b(?:[0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}\b)",
        "[MAC:**:**:**:**:**:**]",
        true, true
    );
    
    // 信用卡号: 4111111111111111 (简化模式)
    m_rules[RULE_CREDIT_CARD] = SanitizeRule(
        tr("Credit Card Number"),
        R"(\b(?:4[0-9]{12}(?:[0-9]{3})?|5[1-5][0-9]{14}|3[47][0-9]{13}|6(?:011|5[0-9]{2})[0-9]{12})\b)",
        "[CARD:****-****-****-****]",
        true, true
    );
    
    // 美国社会安全号: 123-45-6789
    m_rules[RULE_SSN] = SanitizeRule(
        tr("Social Security Number"),
        R"(\b\d{3}-\d{2}-\d{4}\b)",
        "[SSN:***-**-****]",
        true, true
    );
}

void DataSanitizer::compilePatterns()
{
    m_compiledPatterns.clear();
    
    for (auto it = m_rules.begin(); it != m_rules.end(); ++it) {
        if (it->enabled) {
            QRegularExpression regex(it->pattern, QRegularExpression::CaseInsensitiveOption);
            if (regex.isValid()) {
                m_compiledPatterns[it.key()] = regex;
            } else {
                qWarning() << "Invalid regex pattern for rule:" << it.key() << regex.errorString();
            }
        }
    }
}

QString DataSanitizer::sanitize(const QString &text) const
{
    if (text.isEmpty()) return text;
    
    QString result = text;
    
    for (auto it = m_compiledPatterns.begin(); it != m_compiledPatterns.end(); ++it) {
        const QString &ruleName = it.key();
        const QRegularExpression &regex = it.value();
        const SanitizeRule &rule = m_rules[ruleName];
        
        if (rule.enabled) {
            result = result.replace(regex, rule.replacement);
        }
    }
    
    return result;
}

QString DataSanitizer::sanitize(const QString &text, int level) const
{
    if (text.isEmpty()) return text;
    
    QString result = text;
    
    // 根据级别定义要应用的规则集
    // Level 0 (Minimal): 只脱敏密码、Token、API Key
    // Level 1 (Standard): 加上 IP、邮箱、手机号、用户名
    // Level 2 (Strict): 全部规则
    
    QSet<QString> applicableRules;
    
    // Level 0: 最基本的敏感数据
    applicableRules.insert(RULE_PASSWORD);
    applicableRules.insert(RULE_TOKEN);
    applicableRules.insert(RULE_API_KEY);
    
    if (level >= 1) {
        // Level 1: 常见个人信息
        applicableRules.insert(RULE_IP);
        applicableRules.insert(RULE_IPV6);
        applicableRules.insert(RULE_EMAIL);
        applicableRules.insert(RULE_PHONE_CN);
        applicableRules.insert(RULE_PHONE_INTL);
        applicableRules.insert(RULE_USERNAME);
    }
    
    if (level >= 2) {
        // Level 2: 全部规则
        applicableRules.insert(RULE_IDCARD_CN);
        applicableRules.insert(RULE_DOMAIN);
        applicableRules.insert(RULE_PORT);
        applicableRules.insert(RULE_PATH_WIN);
        applicableRules.insert(RULE_PATH_UNIX);
        applicableRules.insert(RULE_MAC_ADDRESS);
        applicableRules.insert(RULE_CREDIT_CARD);
        applicableRules.insert(RULE_SSN);
    }
    
    for (auto it = m_compiledPatterns.begin(); it != m_compiledPatterns.end(); ++it) {
        const QString &ruleName = it.key();
        const QRegularExpression &regex = it.value();
        const SanitizeRule &rule = m_rules[ruleName];
        
        // 检查规则是否在当前级别应该应用
        if (applicableRules.contains(ruleName) && rule.enabled) {
            result = result.replace(regex, rule.replacement);
        }
    }
    
    return result;
}

QStringList DataSanitizer::sanitizeLines(const QStringList &lines) const
{
    QStringList result;
    result.reserve(lines.size());
    
    for (const QString &line : lines) {
        result.append(sanitize(line));
    }
    
    return result;
}

QVariantList DataSanitizer::getSanitizePreview(const QString &text) const
{
    QVariantList previews;
    
    if (text.isEmpty()) return previews;
    
    // 收集所有匹配
    QList<QPair<int, SanitizePreviewItem>> allMatches;
    
    for (auto it = m_compiledPatterns.begin(); it != m_compiledPatterns.end(); ++it) {
        const QString &ruleName = it.key();
        const QRegularExpression &regex = it.value();
        const SanitizeRule &rule = m_rules[ruleName];
        
        if (!rule.enabled) continue;
        
        QRegularExpressionMatchIterator matchIt = regex.globalMatch(text);
        while (matchIt.hasNext()) {
            QRegularExpressionMatch match = matchIt.next();
            
            SanitizePreviewItem item;
            item.start = match.capturedStart();
            item.end = match.capturedEnd();
            item.original = match.captured();
            item.replacement = rule.replacement;
            item.ruleName = rule.name;
            
            allMatches.append(qMakePair(item.start, item));
        }
    }
    
    // 按位置排序
    std::sort(allMatches.begin(), allMatches.end(), 
              [](const auto &a, const auto &b) { return a.first < b.first; });
    
    // 移除重叠匹配（保留第一个）
    int lastEnd = -1;
    for (const auto &pair : allMatches) {
        const SanitizePreviewItem &item = pair.second;
        if (item.start >= lastEnd) {
            QVariantMap map;
            map["start"] = item.start;
            map["end"] = item.end;
            map["original"] = item.original;
            map["replacement"] = item.replacement;
            map["ruleName"] = item.ruleName;
            previews.append(map);
            lastEnd = item.end;
        }
    }
    
    return previews;
}

QString DataSanitizer::sanitizeWithOptions(const QString &text, const QVariantMap &options) const
{
    if (text.isEmpty()) return text;
    
    QString result = text;
    
    for (auto it = m_compiledPatterns.begin(); it != m_compiledPatterns.end(); ++it) {
        const QString &ruleName = it.key();
        const QRegularExpression &regex = it.value();
        const SanitizeRule &rule = m_rules[ruleName];
        
        // 检查是否在选项中启用
        bool enabled = rule.enabled;
        if (options.contains(ruleName)) {
            enabled = options[ruleName].toBool();
        }
        
        if (enabled) {
            result = result.replace(regex, rule.replacement);
        }
    }
    
    return result;
}

bool DataSanitizer::addCustomRule(const QString &name, const QString &pattern, const QString &replacement)
{
    if (name.isEmpty() || pattern.isEmpty()) return false;
    
    // 验证正则表达式
    QRegularExpression regex(pattern);
    if (!regex.isValid()) {
        qWarning() << "Invalid regex pattern:" << regex.errorString();
        return false;
    }
    
    // 生成唯一 ID
    QString id = "custom_" + name.toLower().replace(QRegularExpression("[^a-z0-9]"), "_");
    
    m_rules[id] = SanitizeRule(name, pattern, replacement, true, false);
    compilePatterns();
    
    emit rulesChanged();
    return true;
}

bool DataSanitizer::removeCustomRule(const QString &name)
{
    // 查找自定义规则
    for (auto it = m_rules.begin(); it != m_rules.end(); ++it) {
        if (!it->isBuiltin && (it.key() == name || it->name == name)) {
            m_rules.erase(it);
            compilePatterns();
            emit rulesChanged();
            return true;
        }
    }
    return false;
}

void DataSanitizer::setRuleEnabled(const QString &name, bool enabled)
{
    if (m_rules.contains(name)) {
        m_rules[name].enabled = enabled;
        compilePatterns();
        emit rulesChanged();
    }
}

bool DataSanitizer::isRuleEnabled(const QString &name) const
{
    return m_rules.contains(name) && m_rules[name].enabled;
}

QVariantList DataSanitizer::getRulesVariant() const
{
    QVariantList result;
    
    for (auto it = m_rules.begin(); it != m_rules.end(); ++it) {
        QVariantMap map;
        map["id"] = it.key();
        map["name"] = it->name;
        map["pattern"] = it->pattern;
        map["replacement"] = it->replacement;
        map["enabled"] = it->enabled;
        map["isBuiltin"] = it->isBuiltin;
        result.append(map);
    }
    
    return result;
}

QVariantList DataSanitizer::getCustomRules() const
{
    QVariantList result;
    
    for (auto it = m_rules.begin(); it != m_rules.end(); ++it) {
        if (!it->isBuiltin) {
            QVariantMap map;
            map["id"] = it.key();
            map["name"] = it->name;
            map["pattern"] = it->pattern;
            map["replacement"] = it->replacement;
            map["enabled"] = it->enabled;
            result.append(map);
        }
    }
    
    return result;
}

void DataSanitizer::resetToDefaults()
{
    m_rules.clear();
    initBuiltinRules();
    compilePatterns();
    emit rulesChanged();
}

bool DataSanitizer::hasEnabledRules() const
{
    for (const auto &rule : m_rules) {
        if (rule.enabled) {
            return true;
        }
    }
    return false;
}

// 便捷开关实现
bool DataSanitizer::isIpEnabled() const { return isRuleEnabled(RULE_IP); }
void DataSanitizer::setIpEnabled(bool enabled) { setRuleEnabled(RULE_IP, enabled); setRuleEnabled(RULE_IPV6, enabled); }

bool DataSanitizer::isEmailEnabled() const { return isRuleEnabled(RULE_EMAIL); }
void DataSanitizer::setEmailEnabled(bool enabled) { setRuleEnabled(RULE_EMAIL, enabled); }

bool DataSanitizer::isPhoneEnabled() const { return isRuleEnabled(RULE_PHONE_CN); }
void DataSanitizer::setPhoneEnabled(bool enabled) { setRuleEnabled(RULE_PHONE_CN, enabled); }

bool DataSanitizer::isIdCardEnabled() const { return isRuleEnabled(RULE_IDCARD_CN); }
void DataSanitizer::setIdCardEnabled(bool enabled) { setRuleEnabled(RULE_IDCARD_CN, enabled); }

bool DataSanitizer::isDomainEnabled() const { return isRuleEnabled(RULE_DOMAIN); }
void DataSanitizer::setDomainEnabled(bool enabled) { setRuleEnabled(RULE_DOMAIN, enabled); }

bool DataSanitizer::isPortEnabled() const { return isRuleEnabled(RULE_PORT); }
void DataSanitizer::setPortEnabled(bool enabled) { setRuleEnabled(RULE_PORT, enabled); }

bool DataSanitizer::isApiKeyEnabled() const { return isRuleEnabled(RULE_API_KEY); }
void DataSanitizer::setApiKeyEnabled(bool enabled) { setRuleEnabled(RULE_API_KEY, enabled); setRuleEnabled(RULE_TOKEN, enabled); }

bool DataSanitizer::isUsernameEnabled() const { return isRuleEnabled(RULE_USERNAME); }
void DataSanitizer::setUsernameEnabled(bool enabled) { setRuleEnabled(RULE_USERNAME, enabled); }

bool DataSanitizer::isPathEnabled() const { return isRuleEnabled(RULE_PATH_WIN); }
void DataSanitizer::setPathEnabled(bool enabled) { setRuleEnabled(RULE_PATH_WIN, enabled); setRuleEnabled(RULE_PATH_UNIX, enabled); }

void DataSanitizer::saveSettings()
{
    QSettings settings;
    settings.beginGroup("DataSanitizer");
    
    // 保存启用状态
    QJsonObject enabledRules;
    for (auto it = m_rules.begin(); it != m_rules.end(); ++it) {
        enabledRules[it.key()] = it->enabled;
    }
    settings.setValue("enabledRules", QJsonDocument(enabledRules).toJson(QJsonDocument::Compact));
    
    // 保存自定义规则
    QJsonArray customRules;
    for (auto it = m_rules.begin(); it != m_rules.end(); ++it) {
        if (!it->isBuiltin) {
            QJsonObject rule;
            rule["id"] = it.key();
            rule["name"] = it->name;
            rule["pattern"] = it->pattern;
            rule["replacement"] = it->replacement;
            rule["enabled"] = it->enabled;
            customRules.append(rule);
        }
    }
    settings.setValue("customRules", QJsonDocument(customRules).toJson(QJsonDocument::Compact));
    
    settings.endGroup();
}

void DataSanitizer::loadSettings()
{
    QSettings settings;
    settings.beginGroup("DataSanitizer");
    
    // 加载启用状态
    QString enabledJson = settings.value("enabledRules").toString();
    if (!enabledJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(enabledJson.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (m_rules.contains(it.key())) {
                    m_rules[it.key()].enabled = it.value().toBool();
                }
            }
        }
    }
    
    // 加载自定义规则
    QString customJson = settings.value("customRules").toString();
    if (!customJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(customJson.toUtf8());
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const QJsonValue &val : arr) {
                QJsonObject obj = val.toObject();
                QString id = obj["id"].toString();
                m_rules[id] = SanitizeRule(
                    obj["name"].toString(),
                    obj["pattern"].toString(),
                    obj["replacement"].toString(),
                    obj["enabled"].toBool(true),
                    false
                );
            }
        }
    }
    
    settings.endGroup();
    compilePatterns();
}
