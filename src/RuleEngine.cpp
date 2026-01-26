/**
 * @file RuleEngine.cpp
 * @brief Rule Engine implementation for BigFileViewer
 */

#include "RuleEngine.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QTime>
#include <QDate>

RuleEngine& RuleEngine::instance()
{
    static RuleEngine instance;
    return instance;
}

RuleEngine::RuleEngine(QObject *parent)
    : QObject(parent)
{
    loadRules();
}

RuleEngine::~RuleEngine()
{
    saveRules();
}

QVariantList RuleEngine::rules() const
{
    QVariantList result;
    for (const auto &rule : m_rules) {
        result.append(ruleToVariant(rule));
    }
    return result;
}

void RuleEngine::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged();
    }
}

int RuleEngine::activeRuleCount() const
{
    int count = 0;
    for (const auto &rule : m_rules) {
        if (rule.enabled) count++;
    }
    return count;
}

void RuleEngine::addRule(const QVariantMap &ruleData)
{
    Rule rule = ruleFromVariant(ruleData);
    if (rule.id.isEmpty()) {
        rule.id = generateRuleId();
    }
    m_rules.append(rule);
    saveRules();
    emit rulesChanged();
}

void RuleEngine::updateRule(const QString &ruleId, const QVariantMap &ruleData)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == ruleId) {
            Rule rule = ruleFromVariant(ruleData);
            rule.id = ruleId;  // Preserve ID
            rule.triggerCount = m_rules[i].triggerCount;  // Preserve stats
            rule.lastTriggered = m_rules[i].lastTriggered;
            m_rules[i] = rule;
            saveRules();
            emit rulesChanged();
            return;
        }
    }
}

void RuleEngine::deleteRule(const QString &ruleId)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == ruleId) {
            m_rules.removeAt(i);
            saveRules();
            emit rulesChanged();
            return;
        }
    }
}

QVariantMap RuleEngine::getRule(const QString &ruleId) const
{
    for (const auto &rule : m_rules) {
        if (rule.id == ruleId) {
            return ruleToVariant(rule);
        }
    }
    return QVariantMap();
}

void RuleEngine::setRuleEnabled(const QString &ruleId, bool enabled)
{
    for (auto &rule : m_rules) {
        if (rule.id == ruleId) {
            rule.enabled = enabled;
            saveRules();
            emit rulesChanged();
            return;
        }
    }
}

QVariantMap RuleEngine::createCondition(int field, int op, const QString &value,
                                         bool caseSensitive, bool negated)
{
    QVariantMap condition;
    condition["field"] = field;
    condition["operator"] = op;
    condition["value"] = value;
    condition["caseSensitive"] = caseSensitive;
    condition["negated"] = negated;
    return condition;
}

QVariantMap RuleEngine::createAction(int actionType, const QVariantMap &config)
{
    QVariantMap action;
    action["type"] = actionType;
    action["config"] = config;
    return action;
}

QString RuleEngine::generateRuleId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QVariantList RuleEngine::testRule(const QVariantMap &ruleData, const QStringList &sampleLines)
{
    QVariantList results;
    Rule rule = ruleFromVariant(ruleData);
    
    for (int i = 0; i < sampleLines.size(); ++i) {
        const QString &line = sampleLines[i];
        bool matched = false;
        
        if (rule.logic == And) {
            matched = true;
            for (const auto &cond : rule.conditions) {
                if (!evaluateCondition(cond, line, QVariantMap())) {
                    matched = false;
                    break;
                }
            }
        } else {  // Or
            for (const auto &cond : rule.conditions) {
                if (evaluateCondition(cond, line, QVariantMap())) {
                    matched = true;
                    break;
                }
            }
        }
        
        QVariantMap result;
        result["lineNumber"] = i + 1;
        result["line"] = line;
        result["matched"] = matched;
        results.append(result);
    }
    
    emit testCompleted(results);
    return results;
}

bool RuleEngine::evaluateConditions(const QString &line, const QVariantMap &parsedData)
{
    // Quick check - no rules
    if (m_rules.isEmpty()) return false;
    
    for (const auto &rule : m_rules) {
        if (!rule.enabled) continue;
        if (!isWithinSchedule(rule)) continue;
        
        bool matched = false;
        
        if (rule.logic == And) {
            matched = true;
            for (const auto &cond : rule.conditions) {
                if (!evaluateCondition(cond, line, parsedData)) {
                    matched = false;
                    break;
                }
            }
        } else {
            for (const auto &cond : rule.conditions) {
                if (evaluateCondition(cond, line, parsedData)) {
                    matched = true;
                    break;
                }
            }
        }
        
        if (matched) return true;
    }
    
    return false;
}

void RuleEngine::evaluateLine(const QString &line, int lineNumber, const QVariantMap &parsedData)
{
    if (!m_enabled) return;
    
    for (auto &rule : m_rules) {
        if (!rule.enabled) continue;
        if (!isWithinSchedule(rule)) continue;
        
        // Check cooldown
        if (rule.lastTriggered.isValid()) {
            int secsSinceLastTrigger = rule.lastTriggered.secsTo(QDateTime::currentDateTime());
            if (secsSinceLastTrigger < rule.cooldownSeconds) {
                continue;
            }
        }
        
        bool matched = false;
        
        if (rule.logic == And) {
            matched = true;
            for (const auto &cond : rule.conditions) {
                if (!evaluateCondition(cond, line, parsedData)) {
                    matched = false;
                    break;
                }
            }
        } else {
            for (const auto &cond : rule.conditions) {
                if (evaluateCondition(cond, line, parsedData)) {
                    matched = true;
                    break;
                }
            }
        }
        
        if (matched) {
            rule.lastTriggered = QDateTime::currentDateTime();
            rule.triggerCount++;
            m_triggeredCount++;
            
            emit ruleTriggered(rule.id, rule.name, lineNumber, line);
            
            // Execute actions
            for (const auto &action : rule.actions) {
                emit executeAction(static_cast<int>(action.type), action.config,
                                   rule.name, lineNumber, line);
            }
            
            emit triggeredCountChanged();
        }
    }
}

void RuleEngine::resetTriggerCounts()
{
    for (auto &rule : m_rules) {
        rule.triggerCount = 0;
        rule.lastTriggered = QDateTime();
    }
    m_triggeredCount = 0;
    emit triggeredCountChanged();
    emit rulesChanged();
}

QString RuleEngine::exportRules() const
{
    QJsonArray rulesArray;
    for (const auto &rule : m_rules) {
        rulesArray.append(QJsonObject::fromVariantMap(ruleToVariant(rule)));
    }
    
    QJsonDocument doc(rulesArray);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

bool RuleEngine::importRules(const QString &jsonData)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        return false;
    }
    
    if (!doc.isArray()) {
        return false;
    }
    
    QJsonArray rulesArray = doc.array();
    for (const auto &ruleVal : rulesArray) {
        if (ruleVal.isObject()) {
            QVariantMap ruleData = ruleVal.toObject().toVariantMap();
            ruleData["id"] = generateRuleId();  // Generate new IDs
            addRule(ruleData);
        }
    }
    
    return true;
}

QVariantList RuleEngine::getAvailableActions() const
{
    QVariantList actions;
    
    QVariantMap email;
    email["type"] = EmailAction;
    email["name"] = tr("Email");
    email["description"] = tr("Send email notification");
    email["icon"] = "mail";
    actions.append(email);
    
    QVariantMap webhook;
    webhook["type"] = WebhookAction;
    webhook["name"] = tr("Webhook");
    webhook["description"] = tr("Send HTTP POST to URL");
    webhook["icon"] = "link";
    actions.append(webhook);
    
    QVariantMap popup;
    popup["type"] = PopupAction;
    popup["name"] = tr("Popup");
    popup["description"] = tr("Show desktop notification");
    popup["icon"] = "dialog-information";
    actions.append(popup);
    
    QVariantMap sound;
    sound["type"] = SoundAction;
    sound["name"] = tr("Sound");
    sound["description"] = tr("Play alert sound");
    sound["icon"] = "audio-volume-high";
    actions.append(sound);
    
    QVariantMap command;
    command["type"] = CommandAction;
    command["name"] = tr("Command");
    command["description"] = tr("Run command/script");
    command["icon"] = "utilities-terminal";
    actions.append(command);
    
    QVariantMap highlight;
    highlight["type"] = HighlightAction;
    highlight["name"] = tr("Highlight");
    highlight["description"] = tr("Highlight matching lines");
    highlight["icon"] = "edit-select-all";
    actions.append(highlight);
    
    QVariantMap log;
    log["type"] = LogAction;
    log["name"] = tr("Log");
    log["description"] = tr("Write to log file");
    log["icon"] = "text-x-generic";
    actions.append(log);
    
    return actions;
}

QVariantList RuleEngine::getAvailableOperators() const
{
    QVariantList ops;
    
    auto addOp = [&](ConditionOperator op, const QString &name, const QString &desc) {
        QVariantMap item;
        item["operator"] = op;
        item["name"] = name;
        item["description"] = desc;
        ops.append(item);
    };
    
    addOp(Contains, tr("Contains"), tr("Text contains value"));
    addOp(NotContains, tr("Does not contain"), tr("Text does not contain value"));
    addOp(Equals, tr("Equals"), tr("Text equals value"));
    addOp(NotEquals, tr("Does not equal"), tr("Text does not equal value"));
    addOp(StartsWith, tr("Starts with"), tr("Text starts with value"));
    addOp(EndsWith, tr("Ends with"), tr("Text ends with value"));
    addOp(Matches, tr("Matches regex"), tr("Text matches regular expression"));
    addOp(GreaterThan, tr("Greater than"), tr("Value is greater than"));
    addOp(LessThan, tr("Less than"), tr("Value is less than"));
    addOp(Between, tr("Between"), tr("Value is between two values"));
    
    return ops;
}

QVariantList RuleEngine::getAvailableFields() const
{
    QVariantList fields;
    
    auto addField = [&](ConditionField field, const QString &name, const QString &desc) {
        QVariantMap item;
        item["field"] = field;
        item["name"] = name;
        item["description"] = desc;
        fields.append(item);
    };
    
    addField(RawLine, tr("Raw Line"), tr("The entire log line"));
    addField(LogLevel, tr("Log Level"), tr("ERROR, WARN, INFO, DEBUG, etc."));
    addField(Timestamp, tr("Timestamp"), tr("The log entry timestamp"));
    addField(Message, tr("Message"), tr("The log message content"));
    addField(Source, tr("Source"), tr("The log source/logger name"));
    addField(Thread, tr("Thread"), tr("The thread name/ID"));
    addField(CustomField, tr("Custom Field"), tr("A custom parsed field"));
    
    return fields;
}

bool RuleEngine::evaluateCondition(const Condition &condition, const QString &line,
                                    const QVariantMap &parsedData) const
{
    QString fieldValue;
    
    switch (condition.field) {
    case RawLine:
        fieldValue = line;
        break;
    case LogLevel:
        fieldValue = parsedData.value("level", "").toString();
        break;
    case Timestamp:
        fieldValue = parsedData.value("timestamp", "").toString();
        break;
    case Message:
        fieldValue = parsedData.value("message", line).toString();
        break;
    case Source:
        fieldValue = parsedData.value("source", "").toString();
        break;
    case Thread:
        fieldValue = parsedData.value("thread", "").toString();
        break;
    case CustomField:
        fieldValue = parsedData.value(condition.customFieldName, "").toString();
        break;
    }
    
    Qt::CaseSensitivity cs = condition.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    bool result = false;
    
    switch (condition.op) {
    case Contains:
        result = fieldValue.contains(condition.value, cs);
        break;
    case NotContains:
        result = !fieldValue.contains(condition.value, cs);
        break;
    case Equals:
        result = fieldValue.compare(condition.value, cs) == 0;
        break;
    case NotEquals:
        result = fieldValue.compare(condition.value, cs) != 0;
        break;
    case StartsWith:
        result = fieldValue.startsWith(condition.value, cs);
        break;
    case EndsWith:
        result = fieldValue.endsWith(condition.value, cs);
        break;
    case Matches: {
        QRegularExpression regex(condition.value);
        if (!condition.caseSensitive) {
            regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }
        result = regex.match(fieldValue).hasMatch();
        break;
    }
    case GreaterThan: {
        bool ok1, ok2;
        double v1 = fieldValue.toDouble(&ok1);
        double v2 = condition.value.toDouble(&ok2);
        if (ok1 && ok2) {
            result = v1 > v2;
        } else {
            result = fieldValue.compare(condition.value, cs) > 0;
        }
        break;
    }
    case LessThan: {
        bool ok1, ok2;
        double v1 = fieldValue.toDouble(&ok1);
        double v2 = condition.value.toDouble(&ok2);
        if (ok1 && ok2) {
            result = v1 < v2;
        } else {
            result = fieldValue.compare(condition.value, cs) < 0;
        }
        break;
    }
    case Between: {
        bool ok1, ok2, ok3;
        double v = fieldValue.toDouble(&ok1);
        double v1 = condition.value.toDouble(&ok2);
        double v2 = condition.value2.toDouble(&ok3);
        if (ok1 && ok2 && ok3) {
            result = v >= v1 && v <= v2;
        }
        break;
    }
    }
    
    return condition.negated ? !result : result;
}

bool RuleEngine::isWithinSchedule(const Rule &rule) const
{
    if (!rule.scheduleEnabled) return true;
    
    QTime currentTime = QTime::currentTime();
    QDate currentDate = QDate::currentDate();
    int currentDay = currentDate.dayOfWeek() % 7;  // Convert to 0=Sunday
    
    // Check day of week
    if (!rule.scheduleDays.isEmpty() && !rule.scheduleDays.contains(currentDay)) {
        return false;
    }
    
    // Check time range
    if (!rule.scheduleStart.isEmpty() && !rule.scheduleEnd.isEmpty()) {
        QTime startTime = QTime::fromString(rule.scheduleStart, "HH:mm");
        QTime endTime = QTime::fromString(rule.scheduleEnd, "HH:mm");
        
        if (startTime.isValid() && endTime.isValid()) {
            if (startTime <= endTime) {
                // Normal range (e.g., 09:00 - 17:00)
                if (currentTime < startTime || currentTime > endTime) {
                    return false;
                }
            } else {
                // Overnight range (e.g., 22:00 - 06:00)
                if (currentTime < startTime && currentTime > endTime) {
                    return false;
                }
            }
        }
    }
    
    return true;
}

void RuleEngine::loadRules()
{
    QSettings settings;
    settings.beginGroup("RuleEngine");
    
    QString jsonData = settings.value("rules").toString();
    if (!jsonData.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            for (const auto &val : arr) {
                if (val.isObject()) {
                    m_rules.append(ruleFromVariant(val.toObject().toVariantMap()));
                }
            }
        }
    }
    
    m_enabled = settings.value("enabled", true).toBool();
    settings.endGroup();
}

void RuleEngine::saveRules()
{
    QSettings settings;
    settings.beginGroup("RuleEngine");
    
    QJsonArray arr;
    for (const auto &rule : m_rules) {
        arr.append(QJsonObject::fromVariantMap(ruleToVariant(rule)));
    }
    
    QJsonDocument doc(arr);
    settings.setValue("rules", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    settings.setValue("enabled", m_enabled);
    settings.endGroup();
}

RuleEngine::Rule RuleEngine::ruleFromVariant(const QVariantMap &data) const
{
    Rule rule;
    rule.id = data.value("id").toString();
    rule.name = data.value("name").toString();
    rule.description = data.value("description").toString();
    rule.enabled = data.value("enabled", true).toBool();
    rule.logic = static_cast<LogicOperator>(data.value("logic", And).toInt());
    rule.cooldownSeconds = data.value("cooldownSeconds", 60).toInt();
    rule.scheduleEnabled = data.value("scheduleEnabled", false).toBool();
    rule.scheduleStart = data.value("scheduleStart").toString();
    rule.scheduleEnd = data.value("scheduleEnd").toString();
    
    // Parse conditions
    QVariantList conditions = data.value("conditions").toList();
    for (const auto &condVar : conditions) {
        QVariantMap condMap = condVar.toMap();
        Condition cond;
        cond.field = static_cast<ConditionField>(condMap.value("field", RawLine).toInt());
        cond.customFieldName = condMap.value("customFieldName").toString();
        cond.op = static_cast<ConditionOperator>(condMap.value("operator", Contains).toInt());
        cond.value = condMap.value("value").toString();
        cond.value2 = condMap.value("value2").toString();
        cond.caseSensitive = condMap.value("caseSensitive", false).toBool();
        cond.negated = condMap.value("negated", false).toBool();
        rule.conditions.append(cond);
    }
    
    // Parse actions
    QVariantList actions = data.value("actions").toList();
    for (const auto &actVar : actions) {
        QVariantMap actMap = actVar.toMap();
        Action act;
        act.type = static_cast<ActionType>(actMap.value("type", PopupAction).toInt());
        act.config = actMap.value("config").toMap();
        rule.actions.append(act);
    }
    
    // Parse schedule days
    QVariantList days = data.value("scheduleDays").toList();
    for (const auto &day : days) {
        rule.scheduleDays.append(day.toInt());
    }
    
    return rule;
}

QVariantMap RuleEngine::ruleToVariant(const Rule &rule) const
{
    QVariantMap result;
    result["id"] = rule.id;
    result["name"] = rule.name;
    result["description"] = rule.description;
    result["enabled"] = rule.enabled;
    result["logic"] = static_cast<int>(rule.logic);
    result["cooldownSeconds"] = rule.cooldownSeconds;
    result["scheduleEnabled"] = rule.scheduleEnabled;
    result["scheduleStart"] = rule.scheduleStart;
    result["scheduleEnd"] = rule.scheduleEnd;
    result["triggerCount"] = rule.triggerCount;
    
    QVariantList conditions;
    for (const auto &cond : rule.conditions) {
        QVariantMap condMap;
        condMap["field"] = static_cast<int>(cond.field);
        condMap["customFieldName"] = cond.customFieldName;
        condMap["operator"] = static_cast<int>(cond.op);
        condMap["value"] = cond.value;
        condMap["value2"] = cond.value2;
        condMap["caseSensitive"] = cond.caseSensitive;
        condMap["negated"] = cond.negated;
        conditions.append(condMap);
    }
    result["conditions"] = conditions;
    
    QVariantList actions;
    for (const auto &act : rule.actions) {
        QVariantMap actMap;
        actMap["type"] = static_cast<int>(act.type);
        actMap["config"] = act.config;
        actions.append(actMap);
    }
    result["actions"] = actions;
    
    QVariantList days;
    for (int day : rule.scheduleDays) {
        days.append(day);
    }
    result["scheduleDays"] = days;
    
    return result;
}
