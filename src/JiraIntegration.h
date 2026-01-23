/**
 * @file JiraIntegration.h
 * @brief Jira Integration for creating issues from log snippets
 * @details Enterprise feature: Create Jira issues directly from selected log content
 */

#ifndef JIRAINTEGRATION_H
#define JIRAINTEGRATION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * @brief Jira Integration Manager
 * 
 * Enterprise feature that allows creating Jira issues directly from
 * selected log snippets. Supports Jira Cloud and Jira Server.
 * 
 * Features:
 * - Jira Cloud and Server support
 * - API token / Personal access token authentication
 * - Project and issue type selection
 * - Custom fields support
 * - Issue template management
 */
class JiraIntegration : public QObject
{
    Q_OBJECT
    
    // ========================================================================
    // QML Properties
    // ========================================================================
    
    /** @brief Whether Jira is configured */
    Q_PROPERTY(bool isConfigured READ isConfigured NOTIFY configurationChanged)
    
    /** @brief Jira base URL */
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY configurationChanged)
    
    /** @brief Jira username/email */
    Q_PROPERTY(QString username READ username WRITE setUsername NOTIFY configurationChanged)
    
    /** @brief Whether currently loading */
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    
    /** @brief Last error message */
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    
public:
    // ========================================================================
    // Singleton
    // ========================================================================
    
    static JiraIntegration& instance();
    
    // ========================================================================
    // Property Getters
    // ========================================================================
    
    bool isConfigured() const;
    QString baseUrl() const { return m_baseUrl; }
    QString username() const { return m_username; }
    bool isLoading() const { return m_loading; }
    QString lastError() const { return m_lastError; }
    
    // ========================================================================
    // Property Setters
    // ========================================================================
    
    void setBaseUrl(const QString &url);
    void setUsername(const QString &username);
    
    /**
     * @brief Set API token (stored securely)
     */
    Q_INVOKABLE void setApiToken(const QString &token);
    
    // ========================================================================
    // Configuration
    // ========================================================================
    
    /**
     * @brief Get current configuration
     */
    Q_INVOKABLE QVariantMap getConfiguration() const;
    
    /**
     * @brief Save configuration
     */
    Q_INVOKABLE void saveConfiguration();
    
    /**
     * @brief Test connection to Jira
     */
    Q_INVOKABLE void testConnection();
    
    // ========================================================================
    // Project Management
    // ========================================================================
    
    /**
     * @brief Fetch available projects
     */
    Q_INVOKABLE void fetchProjects();
    
    /**
     * @brief Get cached projects list
     */
    Q_INVOKABLE QVariantList getProjects() const;
    
    /**
     * @brief Fetch issue types for a project
     * @param projectKey Project key (e.g., "PROJ")
     */
    Q_INVOKABLE void fetchIssueTypes(const QString &projectKey);
    
    /**
     * @brief Get cached issue types for a project
     */
    Q_INVOKABLE QVariantList getIssueTypes(const QString &projectKey) const;
    
    // ========================================================================
    // Issue Creation
    // ========================================================================
    
    /**
     * @brief Create a new Jira issue
     * @param projectKey Project key
     * @param issueType Issue type name (e.g., "Bug", "Task")
     * @param summary Issue summary/title
     * @param description Issue description (can include log content)
     * @param labels Optional labels
     * @param priority Optional priority name
     * @return Request ID for tracking
     */
    Q_INVOKABLE QString createIssue(const QString &projectKey,
                                     const QString &issueType,
                                     const QString &summary,
                                     const QString &description,
                                     const QStringList &labels = QStringList(),
                                     const QString &priority = QString());
    
    /**
     * @brief Format log snippet for Jira description
     * @param logLines Selected log lines
     * @param filePath Source file path
     * @param lineNumbers Line numbers
     * @return Formatted description with code block
     */
    Q_INVOKABLE QString formatLogForJira(const QStringList &logLines,
                                          const QString &filePath,
                                          const QVariantList &lineNumbers) const;
    
    // ========================================================================
    // Templates
    // ========================================================================
    
    /**
     * @brief Get saved issue templates
     */
    Q_INVOKABLE QVariantList getTemplates() const;
    
    /**
     * @brief Save an issue template
     */
    Q_INVOKABLE void saveTemplate(const QString &name,
                                   const QString &projectKey,
                                   const QString &issueType,
                                   const QString &summaryTemplate,
                                   const QStringList &labels);
    
    /**
     * @brief Remove a template
     */
    Q_INVOKABLE void removeTemplate(const QString &name);
    
signals:
    void configurationChanged();
    void loadingChanged();
    void errorOccurred(const QString &error);
    
    /** @brief Connection test result */
    void connectionTested(bool success, const QString &message);
    
    /** @brief Projects list fetched */
    void projectsFetched(const QVariantList &projects);
    
    /** @brief Issue types fetched */
    void issueTypesFetched(const QString &projectKey, const QVariantList &types);
    
    /** @brief Issue created successfully */
    void issueCreated(const QString &issueKey, const QString &issueUrl);
    
    /** @brief Issue creation failed */
    void issueCreateFailed(const QString &error);
    
private slots:
    void handleNetworkReply(QNetworkReply *reply);
    
private:
    explicit JiraIntegration(QObject *parent = nullptr);
    ~JiraIntegration() = default;
    
    // Disable copy
    JiraIntegration(const JiraIntegration&) = delete;
    JiraIntegration& operator=(const JiraIntegration&) = delete;
    
    void loadConfiguration();
    QNetworkRequest createRequest(const QString &endpoint) const;
    void setLoading(bool loading);
    void setError(const QString &error);
    
    // Configuration
    QString m_baseUrl;
    QString m_username;
    QString m_apiToken;
    
    // State
    bool m_loading;
    QString m_lastError;
    
    // Cached data
    QVariantList m_projects;
    QMap<QString, QVariantList> m_issueTypes;  // projectKey -> issue types
    QVariantList m_templates;
    
    // Network
    QNetworkAccessManager *m_networkManager;
    
    // Track pending requests
    enum RequestType {
        TestConnection,
        FetchProjects,
        FetchIssueTypes,
        CreateIssue
    };
    QMap<QNetworkReply*, RequestType> m_pendingRequests;
    QMap<QNetworkReply*, QString> m_requestContext;  // For storing project key, etc.
};

#endif // JIRAINTEGRATION_H
