/**
 * @file RuleEngine.h
 * @brief Rule Engine for BigFileViewer - Alert and Notification Rules
 * 
 * Features:
 * - Rule definition with conditions and actions
 * - Rule wizard for guided creation
 * - Multiple action types (email, webhook, popup, command)
 * - Rule testing and validation
 */

#ifndef RULEENGINE_H
#define RULEENGINE_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QRegularExpression>
#include <QTimer>
#include <QDateTime>

/**
 * @class RuleEngine
 * @brief Manages alert and notification rules
 */
class RuleEngine : public QObject
{
    Q_OBJECT
    
    // Properties
    Q_PROPERTY(QVariantList rules READ rules NOTIFY rulesChanged)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(int activeRuleCount READ activeRuleCount NOTIFY rulesChanged)
    Q_PROPERTY(int triggeredCount READ triggeredCount NOTIFY triggeredCountChanged)
    
public:
    // Condition operators
    enum ConditionOperator {
        Contains,
        NotContains,
        Equals,
        NotEquals,
        StartsWith,
        EndsWith,
        Matches,        // Regex
        GreaterThan,
        LessThan,
        Between
    };
    Q_ENUM(ConditionOperator)
    
    // Condition fields
    enum ConditionField {
        RawLine,
        LogLevel,
        Timestamp,
        Message,
        Source,
        Thread,
        CustomField
    };
    Q_ENUM(ConditionField)
    
    // Logic operators
    enum LogicOperator {
        And,
        Or
    };
    Q_ENUM(LogicOperator)
    
    // Action types
    enum ActionType {
        EmailAction,
        WebhookAction,
        PopupAction,
        SoundAction,
        CommandAction,
        HighlightAction,
        LogAction
    };
    Q_ENUM(ActionType)
    
    /**
     * @brief Condition structure
     */
    struct Condition {
        ConditionField field = RawLine;
        QString customFieldName;
        ConditionOperator op = Contains;
        QString value;
        QString value2;  // For Between operator
        bool caseSensitive = false;
        bool negated = false;
    };
    
    /**
     * @brief Action structure
     */
    struct Action {
        ActionType type = PopupAction;
        QVariantMap config;  // Action-specific configuration
    };
    
    /**
     * @brief Rule structure
     */
    struct Rule {
        QString id;
        QString name;
        QString description;
        bool enabled = true;
        
        QList<Condition> conditions;
        LogicOperator logic = And;
        
        QList<Action> actions;
        
        // Rate limiting
        int cooldownSeconds = 60;
        QDateTime lastTriggered;
        int triggerCount = 0;
        
        // Scheduling
        bool scheduleEnabled = false;
        QString scheduleStart;  // "HH:mm"
        QString scheduleEnd;    // "HH:mm"
        QList<int> scheduleDays;  // 0=Sunday, 6=Saturday
    };
    
    /**
     * @brief Singleton instance
     */
    static RuleEngine& instance();
    
    // Properties
    QVariantList rules() const;
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    int activeRuleCount() const;
    int triggeredCount() const { return m_triggeredCount; }
    
    // Rule management
    Q_INVOKABLE void addRule(const QVariantMap &ruleData);
    Q_INVOKABLE void updateRule(const QString &ruleId, const QVariantMap &ruleData);
    Q_INVOKABLE void deleteRule(const QString &ruleId);
    Q_INVOKABLE QVariantMap getRule(const QString &ruleId) const;
    Q_INVOKABLE void setRuleEnabled(const QString &ruleId, bool enabled);
    
    // Rule wizard helpers
    Q_INVOKABLE QVariantMap createCondition(int field, int op, const QString &value,
                                             bool caseSensitive = false, bool negated = false);
    Q_INVOKABLE QVariantMap createAction(int actionType, const QVariantMap &config);
    Q_INVOKABLE QString generateRuleId();
    
    // Testing
    Q_INVOKABLE QVariantList testRule(const QVariantMap &ruleData, const QStringList &sampleLines);
    Q_INVOKABLE bool evaluateConditions(const QString &line, const QVariantMap &parsedData = QVariantMap());
    
    // Execution
    Q_INVOKABLE void evaluateLine(const QString &line, int lineNumber, const QVariantMap &parsedData = QVariantMap());
    Q_INVOKABLE void resetTriggerCounts();
    
    // Import/Export
    Q_INVOKABLE QString exportRules() const;
    Q_INVOKABLE bool importRules(const QString &jsonData);
    
    // Available actions info
    Q_INVOKABLE QVariantList getAvailableActions() const;
    Q_INVOKABLE QVariantList getAvailableOperators() const;
    Q_INVOKABLE QVariantList getAvailableFields() const;

signals:
    void rulesChanged();
    void enabledChanged();
    void triggeredCountChanged();
    
    /**
     * @brief Emitted when a rule is triggered
     */
    void ruleTriggered(const QString &ruleId, const QString &ruleName,
                       int lineNumber, const QString &lineContent);
    
    /**
     * @brief Emitted when an action needs to be executed
     */
    void executeAction(int actionType, const QVariantMap &config,
                       const QString &ruleName, int lineNumber, const QString &lineContent);
    
    /**
     * @brief Emitted for rule testing results
     */
    void testCompleted(const QVariantList &results);

private:
    explicit RuleEngine(QObject *parent = nullptr);
    ~RuleEngine();
    
    // Disable copy
    RuleEngine(const RuleEngine&) = delete;
    RuleEngine& operator=(const RuleEngine&) = delete;
    
    // Helpers
    bool evaluateCondition(const Condition &condition, const QString &line,
                           const QVariantMap &parsedData) const;
    bool isWithinSchedule(const Rule &rule) const;
    void loadRules();
    void saveRules();
    
    Rule ruleFromVariant(const QVariantMap &data) const;
    QVariantMap ruleToVariant(const Rule &rule) const;
    
    // Members
    QList<Rule> m_rules;
    bool m_enabled = true;
    int m_triggeredCount = 0;
};

#endif // RULEENGINE_H
