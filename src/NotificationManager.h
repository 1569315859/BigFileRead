/**
 * @file NotificationManager.h
 * @brief Unified Notification Manager for BigFileViewer
 * 
 * Supports multiple notification channels:
 * - Webhook (HTTP POST/PUT)
 * - Desktop popup (system tray, toast)
 * - Command execution
 * - Sound alerts
 * - Email (via SmtpAlertManager)
 */

#ifndef NOTIFICATIONMANAGER_H
#define NOTIFICATIONMANAGER_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QQueue>
#include <QTimer>

class NotificationManager : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool webhookEnabled READ isWebhookEnabled WRITE setWebhookEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool popupEnabled READ isPopupEnabled WRITE setPopupEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool commandEnabled READ isCommandEnabled WRITE setCommandEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool soundEnabled READ isSoundEnabled WRITE setSoundEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int notificationCount READ notificationCount NOTIFY notificationCountChanged)

public:
    static NotificationManager& instance();
    
    // Notification types
    enum NotificationType {
        Webhook = 0,
        Popup,
        Command,
        Sound,
        Email,
        Log
    };
    Q_ENUM(NotificationType)
    
    // HTTP methods for webhook
    enum HttpMethod {
        POST = 0,
        PUT,
        PATCH
    };
    Q_ENUM(HttpMethod)
    
    // Settings getters
    bool isWebhookEnabled() const { return m_webhookEnabled; }
    bool isPopupEnabled() const { return m_popupEnabled; }
    bool isCommandEnabled() const { return m_commandEnabled; }
    bool isSoundEnabled() const { return m_soundEnabled; }
    int notificationCount() const { return m_notificationCount; }
    
    // Settings setters
    void setWebhookEnabled(bool enabled);
    void setPopupEnabled(bool enabled);
    void setCommandEnabled(bool enabled);
    void setSoundEnabled(bool enabled);
    
    // ===== Q_INVOKABLE Methods for QML =====
    
    // Generic send notification
    Q_INVOKABLE void sendNotification(int type, const QVariantMap &config,
                                       const QString &ruleName, int lineNumber,
                                       const QString &lineContent);
    
    // Webhook methods
    Q_INVOKABLE void sendWebhook(const QString &url, int method,
                                  const QVariantMap &headers,
                                  const QString &bodyTemplate,
                                  const QVariantMap &data);
    
    Q_INVOKABLE void testWebhook(const QString &url, int method,
                                  const QVariantMap &headers,
                                  const QString &body);
    
    // Popup methods
    Q_INVOKABLE void showPopup(const QString &title, const QString &message,
                               int durationMs = 5000);
    
    Q_INVOKABLE void showSystemTrayMessage(const QString &title, const QString &message,
                                            int icon = 0);  // 0=Info, 1=Warning, 2=Critical
    
    // Command methods
    Q_INVOKABLE void executeCommand(const QString &command, const QVariantMap &envVars);
    
    Q_INVOKABLE void testCommand(const QString &command);
    
    // Sound methods
    Q_INVOKABLE void playSound(const QString &soundFile);
    
    Q_INVOKABLE void playDefaultAlert();
    
    Q_INVOKABLE QStringList getAvailableSounds() const;
    
    // Configuration
    Q_INVOKABLE void saveWebhookConfig(const QString &name, const QVariantMap &config);
    Q_INVOKABLE void deleteWebhookConfig(const QString &name);
    Q_INVOKABLE QVariantList getWebhookConfigs() const;
    Q_INVOKABLE QVariantMap getWebhookConfig(const QString &name) const;
    
    Q_INVOKABLE void saveCommandConfig(const QString &name, const QVariantMap &config);
    Q_INVOKABLE void deleteCommandConfig(const QString &name);
    Q_INVOKABLE QVariantList getCommandConfigs() const;
    
    // Template helpers
    Q_INVOKABLE QString applyTemplate(const QString &templateStr, const QVariantMap &data);
    Q_INVOKABLE QVariantList getAvailableTemplateVariables() const;
    
    // Notification history
    Q_INVOKABLE QVariantList getNotificationHistory(int limit = 100) const;
    Q_INVOKABLE void clearNotificationHistory();
    
    // Rate limiting
    Q_INVOKABLE void setRateLimit(int maxPerMinute);
    Q_INVOKABLE int getRateLimit() const { return m_maxNotificationsPerMinute; }

signals:
    void settingsChanged();
    void notificationCountChanged();
    void notificationSent(int type, const QString &details);
    void notificationError(int type, const QString &error);
    void webhookResponse(int statusCode, const QString &response);
    void commandOutput(const QString &stdOut, const QString &stdErr, int exitCode);
    void popupClicked();

private:
    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager();
    
    NotificationManager(const NotificationManager&) = delete;
    NotificationManager& operator=(const NotificationManager&) = delete;
    
    void loadSettings();
    void saveSettings();
    
    void addToHistory(int type, const QString &details, bool success);
    bool checkRateLimit();
    
    void doSendWebhook(const QString &url, HttpMethod method,
                       const QVariantMap &headers, const QString &body);
    
    void doExecuteCommand(const QString &command, const QVariantMap &envVars);
    
    // Network
    QNetworkAccessManager *m_networkManager;
    
    // System tray
    QSystemTrayIcon *m_trayIcon;
    
    // Process for commands
    QProcess *m_commandProcess;
    
    // Settings
    bool m_webhookEnabled;
    bool m_popupEnabled;
    bool m_commandEnabled;
    bool m_soundEnabled;
    
    // Saved configurations
    QMap<QString, QVariantMap> m_webhookConfigs;
    QMap<QString, QVariantMap> m_commandConfigs;
    
    // Notification history
    struct NotificationRecord {
        QDateTime timestamp;
        int type;
        QString details;
        bool success;
    };
    QList<NotificationRecord> m_history;
    
    // Rate limiting
    int m_maxNotificationsPerMinute;
    QQueue<QDateTime> m_recentNotifications;
    
    // Stats
    int m_notificationCount;
};

#endif // NOTIFICATIONMANAGER_H
