/**
 * @file GitHubIntegration.h
 * @brief GitHub Issue Integration for BigFileViewer
 * 
 * Enterprise feature: Create GitHub issues directly from log content
 * Supports both GitHub.com and GitHub Enterprise Server
 */

#ifndef GITHUBINTEGRATION_H
#define GITHUBINTEGRATION_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>

/**
 * @class GitHubIntegration
 * @brief Manages GitHub REST API integration for issue creation
 * 
 * Features:
 * - Personal Access Token (PAT) authentication
 * - Repository listing (user's repos)
 * - Issue creation with labels and assignees
 * - Log content formatting for GitHub Markdown
 * - Support for GitHub.com and GitHub Enterprise
 */
class GitHubIntegration : public QObject
{
    Q_OBJECT
    
    // ===== QML Properties =====
    Q_PROPERTY(bool isConfigured READ isConfigured NOTIFY configurationChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl WRITE setBaseUrl NOTIFY configurationChanged)
    Q_PROPERTY(QString username READ username NOTIFY configurationChanged)
    Q_PROPERTY(bool loading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)

public:
    /**
     * @brief Singleton instance accessor
     */
    static GitHubIntegration& instance();
    
    // ===== Configuration =====
    
    /**
     * @brief Check if GitHub is configured with valid credentials
     */
    bool isConfigured() const;
    
    /**
     * @brief Get the GitHub API base URL
     * @return API URL (https://api.github.com or enterprise URL)
     */
    QString baseUrl() const { return m_baseUrl; }
    
    /**
     * @brief Set the GitHub API base URL
     * @param url API base URL
     */
    void setBaseUrl(const QString &url);
    
    /**
     * @brief Get the authenticated username
     */
    QString username() const { return m_username; }
    
    /**
     * @brief Check if a request is in progress
     */
    bool isLoading() const { return m_loading; }
    
    /**
     * @brief Get the last error message
     */
    QString lastError() const { return m_lastError; }
    
    // ===== Q_INVOKABLE Methods for QML =====
    
    /**
     * @brief Set the Personal Access Token
     * @param token GitHub PAT with repo scope
     */
    Q_INVOKABLE void setAccessToken(const QString &token);
    
    /**
     * @brief Test connection and get authenticated user info
     */
    Q_INVOKABLE void testConnection();
    
    /**
     * @brief Fetch user's repositories
     * @param includePrivate Include private repositories
     */
    Q_INVOKABLE void fetchRepositories(bool includePrivate = true);
    
    /**
     * @brief Fetch labels for a repository
     * @param owner Repository owner
     * @param repo Repository name
     */
    Q_INVOKABLE void fetchLabels(const QString &owner, const QString &repo);
    
    /**
     * @brief Fetch collaborators for a repository (for assignees)
     * @param owner Repository owner
     * @param repo Repository name
     */
    Q_INVOKABLE void fetchCollaborators(const QString &owner, const QString &repo);
    
    /**
     * @brief Create a new issue
     * @param owner Repository owner
     * @param repo Repository name
     * @param title Issue title
     * @param body Issue body (Markdown)
     * @param labels List of label names
     * @param assignees List of usernames to assign
     */
    Q_INVOKABLE void createIssue(const QString &owner,
                                  const QString &repo,
                                  const QString &title,
                                  const QString &body,
                                  const QStringList &labels = QStringList(),
                                  const QStringList &assignees = QStringList());
    
    /**
     * @brief Format log lines for GitHub Markdown
     * @param logLines List of log lines
     * @param filePath Source file path
     * @param lineNumbers Corresponding line numbers
     * @return Formatted Markdown string
     */
    Q_INVOKABLE QString formatLogForGitHub(const QStringList &logLines,
                                            const QString &filePath,
                                            const QVariantList &lineNumbers) const;
    
    /**
     * @brief Save configuration to settings
     */
    Q_INVOKABLE void saveConfiguration();
    
    /**
     * @brief Load configuration from settings
     */
    Q_INVOKABLE void loadConfiguration();

signals:
    /**
     * @brief Emitted when configuration changes
     */
    void configurationChanged();
    
    /**
     * @brief Emitted when loading state changes
     */
    void loadingChanged();
    
    /**
     * @brief Emitted when an error occurs
     * @param error Error message
     */
    void errorOccurred(const QString &error);
    
    /**
     * @brief Emitted when connection test completes
     * @param success Whether connection was successful
     * @param message Result message (username on success, error on failure)
     */
    void connectionTested(bool success, const QString &message);
    
    /**
     * @brief Emitted when repositories are fetched
     * @param repos List of repository objects {owner, name, fullName, private, description}
     */
    void repositoriesFetched(const QVariantList &repos);
    
    /**
     * @brief Emitted when labels are fetched
     * @param owner Repository owner
     * @param repo Repository name
     * @param labels List of label objects {name, color, description}
     */
    void labelsFetched(const QString &owner, const QString &repo, const QVariantList &labels);
    
    /**
     * @brief Emitted when collaborators are fetched
     * @param owner Repository owner
     * @param repo Repository name
     * @param collaborators List of user objects {login, avatarUrl}
     */
    void collaboratorsFetched(const QString &owner, const QString &repo, const QVariantList &collaborators);
    
    /**
     * @brief Emitted when issue is created successfully
     * @param issueNumber The created issue number
     * @param issueUrl URL to view the issue
     */
    void issueCreated(int issueNumber, const QString &issueUrl);
    
    /**
     * @brief Emitted when issue creation fails
     * @param error Error message
     */
    void issueCreateFailed(const QString &error);

private:
    explicit GitHubIntegration(QObject *parent = nullptr);
    ~GitHubIntegration() = default;
    
    // Disable copy
    GitHubIntegration(const GitHubIntegration&) = delete;
    GitHubIntegration& operator=(const GitHubIntegration&) = delete;
    
    /**
     * @brief Create a network request with auth headers
     */
    QNetworkRequest createRequest(const QString &endpoint) const;
    
    /**
     * @brief Handle network reply
     */
    void handleNetworkReply(QNetworkReply *reply);
    
    /**
     * @brief Set loading state
     */
    void setLoading(bool loading);
    
    /**
     * @brief Set error message
     */
    void setError(const QString &error);
    
    // Network manager
    QNetworkAccessManager *m_networkManager;
    
    // Configuration
    QString m_baseUrl;          // API base URL
    QString m_accessToken;      // Personal Access Token
    QString m_username;         // Authenticated username
    
    // State
    bool m_loading;
    QString m_lastError;
    
    // Request tracking
    enum class RequestType {
        TestConnection,
        FetchRepositories,
        FetchLabels,
        FetchCollaborators,
        CreateIssue
    };
    QMap<QNetworkReply*, RequestType> m_pendingRequests;
    QMap<QNetworkReply*, QVariantMap> m_requestContext;
};

#endif // GITHUBINTEGRATION_H
