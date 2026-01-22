/**
 * @file SmtpAlertManager.h
 * @brief SMTP Email Alert Manager
 * @details Enterprise feature: Send email notifications when specific keywords are detected
 */

#ifndef SMTPALERTMANAGER_H
#define SMTPALERTMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QSettings>
#include <QTimer>
#include <QQueue>
#include <QMutex>
#include <QNetworkAccessManager>

/**
 * @brief Alert rule structure
 */
struct AlertRule {
    QString id;              ///< Unique rule ID
    QString name;            ///< Rule display name
    QStringList keywords;    ///< Keywords to trigger alert
    bool caseSensitive;      ///< Case sensitive matching
    bool useRegex;           ///< Use regex matching
    int cooldownMinutes;     ///< Minimum time between alerts (prevent spam)
    bool enabled;            ///< Rule enabled state
    qint64 lastTriggered;    ///< Last trigger timestamp (ms)
    
    AlertRule() : caseSensitive(false), useRegex(false), 
                  cooldownMinutes(5), enabled(true), lastTriggered(0) {}
};

/**
 * @brief Email alert item in queue
 */
struct AlertItem {
    QString ruleName;        ///< Rule that triggered the alert
    QString matchedKeyword;  ///< The keyword that matched
    QString logLine;         ///< The log line content
    int lineNumber;          ///< Line number in file
    QString filePath;        ///< Source file path
    qint64 timestamp;        ///< When the alert was triggered
};

/**
 * @brief SMTP Email Alert Manager
 * 
 * Enterprise feature that monitors log content and sends email alerts
 * when specified keywords are detected.
 * 
 * Features:
 * - Multiple alert rules with different keywords
 * - Cooldown period to prevent alert spam
 * - Email batching to group multiple alerts
 * - SMTP/TLS support
 * - Test email functionality
 */
class SmtpAlertManager : public QObject
{
    Q_OBJECT
    
    // ========================================================================
    // QML Properties
    // ========================================================================
    
    /** @brief Whether email alerts are enabled globally */
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled NOTIFY enabledChanged)
    
    /** @brief SMTP server host */
    Q_PROPERTY(QString smtpHost READ smtpHost WRITE setSmtpHost NOTIFY settingsChanged)
    
    /** @brief SMTP server port */
    Q_PROPERTY(int smtpPort READ smtpPort WRITE setSmtpPort NOTIFY settingsChanged)
    
    /** @brief Use SSL/TLS */
    Q_PROPERTY(bool useTls READ useTls WRITE setUseTls NOTIFY settingsChanged)
    
    /** @brief SMTP username */
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY settingsChanged)
    
    /** @brief Sender email address */
    Q_PROPERTY(QString fromAddress READ fromAddress WRITE setFromAddress NOTIFY settingsChanged)
    
    /** @brief Recipient email addresses (comma separated) */
    Q_PROPERTY(QString toAddresses READ toAddresses WRITE setToAddresses NOTIFY settingsChanged)
    
    /** @brief Number of active rules */
    Q_PROPERTY(int ruleCount READ ruleCount NOTIFY rulesChanged)
    
    /** @brief Pending alerts in queue */
    Q_PROPERTY(int pendingAlerts READ pendingAlerts NOTIFY alertQueueChanged)
    
public:
    // ========================================================================
    // Singleton
    // ========================================================================
    
    static SmtpAlertManager& instance();
    
    // ========================================================================
    // Property Getters
    // ========================================================================
    
    bool isEnabled() const { return m_enabled; }
    QString smtpHost() const { return m_smtpHost; }
    int smtpPort() const { return m_smtpPort; }
    bool useTls() const { return m_useTls; }
    QString username() const { return m_username; }
    QString fromAddress() const { return m_fromAddress; }
    QString toAddresses() const { return m_toAddresses; }
    int ruleCount() const { return m_rules.size(); }
    int pendingAlerts() const { return m_alertQueue.size(); }
    
    // ========================================================================
    // Property Setters
    // ========================================================================
    
    void setEnabled(bool enabled);
    void setSmtpHost(const QString &host);
    void setSmtpPort(int port);
    void setUseTls(bool useTls);
    void setUsername(const QString &username);
    void setFromAddress(const QString &address);
    void setToAddresses(const QString &addresses);
    
    /**
     * @brief Set SMTP password (stored securely)
     */
    Q_INVOKABLE void setPassword(const QString &password);
    
    // ========================================================================
    // Rule Management
    // ========================================================================
    
    /**
     * @brief Get all alert rules
     * @return List of rules as QVariantList for QML
     */
    Q_INVOKABLE QVariantList getRules() const;
    
    /**
     * @brief Add a new alert rule
     * @param name Rule display name
     * @param keywords Comma-separated keywords
     * @param caseSensitive Case sensitive matching
     * @param useRegex Use regex matching
     * @param cooldownMinutes Cooldown period
     * @return Rule ID
     */
    Q_INVOKABLE QString addRule(const QString &name, const QString &keywords,
                                 bool caseSensitive = false, bool useRegex = false,
                                 int cooldownMinutes = 5);
    
    /**
     * @brief Update an existing rule
     */
    Q_INVOKABLE bool updateRule(const QString &id, const QString &name,
                                 const QString &keywords, bool caseSensitive,
                                 bool useRegex, int cooldownMinutes);
    
    /**
     * @brief Remove a rule
     */
    Q_INVOKABLE bool removeRule(const QString &id);
    
    /**
     * @brief Enable/disable a rule
     */
    Q_INVOKABLE bool setRuleEnabled(const QString &id, bool enabled);
    
    // ========================================================================
    // Alert Processing
    // ========================================================================
    
    /**
     * @brief Check a log line against all rules
     * @param line The log line content
     * @param lineNumber Line number in file
     * @param filePath Source file path
     */
    Q_INVOKABLE void checkLine(const QString &line, int lineNumber, const QString &filePath);
    
    /**
     * @brief Send a test email
     * @return true if email was queued successfully
     */
    Q_INVOKABLE bool sendTestEmail();
    
    /**
     * @brief Get SMTP settings as QVariantMap for QML
     */
    Q_INVOKABLE QVariantMap getSettings() const;
    
    /**
     * @brief Save all settings
     */
    Q_INVOKABLE void saveSettings();
    
signals:
    void enabledChanged();
    void settingsChanged();
    void rulesChanged();
    void alertQueueChanged();
    
    /** @brief Emitted when an alert is triggered */
    void alertTriggered(const QString &ruleName, const QString &keyword, 
                        const QString &line, int lineNumber);
    
    /** @brief Emitted when email is sent successfully */
    void emailSent(int alertCount);
    
    /** @brief Emitted when email sending fails */
    void emailFailed(const QString &error);
    
    /** @brief Emitted for status updates */
    void statusMessage(const QString &message);

private slots:
    void processPendingAlerts();
    
private:
    explicit SmtpAlertManager(QObject *parent = nullptr);
    ~SmtpAlertManager() = default;
    
    // Disable copy
    SmtpAlertManager(const SmtpAlertManager&) = delete;
    SmtpAlertManager& operator=(const SmtpAlertManager&) = delete;
    
    void loadSettings();
    void loadRules();
    void saveRules();
    
    bool matchesRule(const AlertRule &rule, const QString &line, QString &matchedKeyword);
    void queueAlert(const AlertRule &rule, const QString &keyword, 
                    const QString &line, int lineNumber, const QString &filePath);
    
    bool sendEmail(const QString &subject, const QString &body);
    QString buildAlertEmailBody(const QList<AlertItem> &alerts);
    
    // Settings
    bool m_enabled;
    QString m_smtpHost;
    int m_smtpPort;
    bool m_useTls;
    QString m_username;
    QString m_password;
    QString m_fromAddress;
    QString m_toAddresses;
    
    // Rules and alerts
    QList<AlertRule> m_rules;
    QQueue<AlertItem> m_alertQueue;
    QMutex m_queueMutex;
    
    // Timer for batching alerts
    QTimer *m_batchTimer;
    int m_batchIntervalMs;
    
    // Network
    QNetworkAccessManager *m_networkManager;
};

#endif // SMTPALERTMANAGER_H
