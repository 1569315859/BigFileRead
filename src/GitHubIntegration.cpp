/**
 * @file GitHubIntegration.cpp
 * @brief GitHub Issue Integration implementation
 */

#include "GitHubIntegration.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDateTime>
#include <QDebug>

// ============================================================================
// Singleton
// ============================================================================

GitHubIntegration& GitHubIntegration::instance()
{
    static GitHubIntegration instance;
    return instance;
}

GitHubIntegration::GitHubIntegration(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_baseUrl("https://api.github.com")
    , m_loading(false)
{
    loadConfiguration();
}

// ============================================================================
// Configuration
// ============================================================================

bool GitHubIntegration::isConfigured() const
{
    return !m_accessToken.isEmpty();
}

void GitHubIntegration::setBaseUrl(const QString &url)
{
    QString cleanUrl = url.trimmed();
    // Remove trailing slash
    while (cleanUrl.endsWith('/')) {
        cleanUrl.chop(1);
    }
    
    // For GitHub Enterprise, ensure /api/v3 suffix
    if (!cleanUrl.contains("api.github.com") && !cleanUrl.endsWith("/api/v3")) {
        cleanUrl += "/api/v3";
    }
    
    if (m_baseUrl != cleanUrl) {
        m_baseUrl = cleanUrl;
        emit configurationChanged();
    }
}

void GitHubIntegration::setAccessToken(const QString &token)
{
    QString cleanToken = token.trimmed();
    if (m_accessToken != cleanToken) {
        m_accessToken = cleanToken;
        m_username.clear();  // Clear username until re-verified
        emit configurationChanged();
    }
}

void GitHubIntegration::saveConfiguration()
{
    QSettings settings;
    settings.beginGroup("GitHubIntegration");
    settings.setValue("baseUrl", m_baseUrl);
    // Note: Access token should be stored securely in production
    // For simplicity, using QSettings with obfuscation
    settings.setValue("accessToken", QString(m_accessToken.toUtf8().toBase64()));
    settings.setValue("username", m_username);
    settings.endGroup();
    settings.sync();
    
    qDebug() << "GitHubIntegration: Configuration saved";
}

void GitHubIntegration::loadConfiguration()
{
    QSettings settings;
    settings.beginGroup("GitHubIntegration");
    m_baseUrl = settings.value("baseUrl", "https://api.github.com").toString();
    QString encodedToken = settings.value("accessToken").toString();
    if (!encodedToken.isEmpty()) {
        m_accessToken = QString::fromUtf8(QByteArray::fromBase64(encodedToken.toUtf8()));
    }
    m_username = settings.value("username").toString();
    settings.endGroup();
    
    emit configurationChanged();
    qDebug() << "GitHubIntegration: Configuration loaded, configured:" << isConfigured();
}

// ============================================================================
// Network Helpers
// ============================================================================

QNetworkRequest GitHubIntegration::createRequest(const QString &endpoint) const
{
    QString url = m_baseUrl;
    if (!endpoint.startsWith('/')) {
        url += '/';
    }
    url += endpoint;
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    
    if (!m_accessToken.isEmpty()) {
        request.setRawHeader("Authorization", 
            QString("Bearer %1").arg(m_accessToken).toUtf8());
    }
    
    return request;
}

void GitHubIntegration::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}

void GitHubIntegration::setError(const QString &error)
{
    m_lastError = error;
    emit errorOccurred(error);
}

// ============================================================================
// API Methods
// ============================================================================

void GitHubIntegration::testConnection()
{
    if (m_accessToken.isEmpty()) {
        setError(tr("Access token not configured"));
        emit connectionTested(false, tr("Access token not configured"));
        return;
    }
    
    setLoading(true);
    
    QNetworkRequest request = createRequest("/user");
    QNetworkReply *reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = RequestType::TestConnection;
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleNetworkReply(reply);
    });
}

void GitHubIntegration::fetchRepositories(bool includePrivate)
{
    if (!isConfigured()) {
        setError(tr("GitHub not configured"));
        return;
    }
    
    setLoading(true);
    
    // Fetch user's repositories
    QString endpoint = "/user/repos?sort=updated&per_page=100";
    if (!includePrivate) {
        endpoint += "&visibility=public";
    }
    
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = RequestType::FetchRepositories;
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleNetworkReply(reply);
    });
}

void GitHubIntegration::fetchLabels(const QString &owner, const QString &repo)
{
    if (!isConfigured()) {
        setError(tr("GitHub not configured"));
        return;
    }
    
    setLoading(true);
    
    QString endpoint = QString("/repos/%1/%2/labels?per_page=100").arg(owner, repo);
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = RequestType::FetchLabels;
    m_requestContext[reply] = {{"owner", owner}, {"repo", repo}};
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleNetworkReply(reply);
    });
}

void GitHubIntegration::fetchCollaborators(const QString &owner, const QString &repo)
{
    if (!isConfigured()) {
        setError(tr("GitHub not configured"));
        return;
    }
    
    setLoading(true);
    
    QString endpoint = QString("/repos/%1/%2/collaborators?per_page=100").arg(owner, repo);
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = RequestType::FetchCollaborators;
    m_requestContext[reply] = {{"owner", owner}, {"repo", repo}};
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleNetworkReply(reply);
    });
}

void GitHubIntegration::createIssue(const QString &owner,
                                     const QString &repo,
                                     const QString &title,
                                     const QString &body,
                                     const QStringList &labels,
                                     const QStringList &assignees)
{
    if (!isConfigured()) {
        setError(tr("GitHub not configured"));
        emit issueCreateFailed(tr("GitHub not configured"));
        return;
    }
    
    setLoading(true);
    
    // Build JSON payload
    QJsonObject payload;
    payload["title"] = title;
    payload["body"] = body;
    
    if (!labels.isEmpty()) {
        QJsonArray labelsArray;
        for (const QString &label : labels) {
            labelsArray.append(label);
        }
        payload["labels"] = labelsArray;
    }
    
    if (!assignees.isEmpty()) {
        QJsonArray assigneesArray;
        for (const QString &assignee : assignees) {
            assigneesArray.append(assignee);
        }
        payload["assignees"] = assigneesArray;
    }
    
    QString endpoint = QString("/repos/%1/%2/issues").arg(owner, repo);
    QNetworkRequest request = createRequest(endpoint);
    
    QNetworkReply *reply = m_networkManager->post(request, 
        QJsonDocument(payload).toJson(QJsonDocument::Compact));
    
    m_pendingRequests[reply] = RequestType::CreateIssue;
    m_requestContext[reply] = {{"owner", owner}, {"repo", repo}};
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleNetworkReply(reply);
    });
    
    qDebug() << "GitHubIntegration: Creating issue in" << owner << "/" << repo;
}

// ============================================================================
// Network Reply Handler
// ============================================================================

void GitHubIntegration::handleNetworkReply(QNetworkReply *reply)
{
    setLoading(false);
    
    RequestType requestType = m_pendingRequests.take(reply);
    QVariantMap context = m_requestContext.take(reply);
    
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        
        // Try to parse error from response body
        QByteArray responseData = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(responseData);
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("message")) {
                errorMsg = obj["message"].toString();
            }
        }
        
        qWarning() << "GitHubIntegration: Network error:" << errorMsg;
        setError(errorMsg);
        
        switch (requestType) {
            case RequestType::TestConnection:
                emit connectionTested(false, errorMsg);
                break;
            case RequestType::CreateIssue:
                emit issueCreateFailed(errorMsg);
                break;
            default:
                break;
        }
        
        reply->deleteLater();
        return;
    }
    
    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    
    switch (requestType) {
        case RequestType::TestConnection: {
            if (doc.isObject()) {
                QJsonObject user = doc.object();
                m_username = user["login"].toString();
                saveConfiguration();
                emit configurationChanged();
                emit connectionTested(true, tr("Connected as: %1").arg(m_username));
                qDebug() << "GitHubIntegration: Connected as" << m_username;
            }
            break;
        }
        
        case RequestType::FetchRepositories: {
            if (doc.isArray()) {
                QVariantList repos;
                QJsonArray array = doc.array();
                for (const QJsonValue &val : array) {
                    QJsonObject repo = val.toObject();
                    QVariantMap repoMap;
                    repoMap["owner"] = repo["owner"].toObject()["login"].toString();
                    repoMap["name"] = repo["name"].toString();
                    repoMap["fullName"] = repo["full_name"].toString();
                    repoMap["private"] = repo["private"].toBool();
                    repoMap["description"] = repo["description"].toString();
                    repos.append(repoMap);
                }
                emit repositoriesFetched(repos);
                qDebug() << "GitHubIntegration: Fetched" << repos.size() << "repositories";
            }
            break;
        }
        
        case RequestType::FetchLabels: {
            if (doc.isArray()) {
                QVariantList labels;
                QJsonArray array = doc.array();
                for (const QJsonValue &val : array) {
                    QJsonObject label = val.toObject();
                    QVariantMap labelMap;
                    labelMap["name"] = label["name"].toString();
                    labelMap["color"] = label["color"].toString();
                    labelMap["description"] = label["description"].toString();
                    labels.append(labelMap);
                }
                emit labelsFetched(context["owner"].toString(), 
                                   context["repo"].toString(), labels);
            }
            break;
        }
        
        case RequestType::FetchCollaborators: {
            if (doc.isArray()) {
                QVariantList collaborators;
                QJsonArray array = doc.array();
                for (const QJsonValue &val : array) {
                    QJsonObject user = val.toObject();
                    QVariantMap userMap;
                    userMap["login"] = user["login"].toString();
                    userMap["avatarUrl"] = user["avatar_url"].toString();
                    collaborators.append(userMap);
                }
                emit collaboratorsFetched(context["owner"].toString(),
                                          context["repo"].toString(), collaborators);
            }
            break;
        }
        
        case RequestType::CreateIssue: {
            if (doc.isObject()) {
                QJsonObject issue = doc.object();
                int issueNumber = issue["number"].toInt();
                QString issueUrl = issue["html_url"].toString();
                emit issueCreated(issueNumber, issueUrl);
                qDebug() << "GitHubIntegration: Created issue #" << issueNumber;
            }
            break;
        }
    }
    
    reply->deleteLater();
}

// ============================================================================
// Log Formatting
// ============================================================================

QString GitHubIntegration::formatLogForGitHub(const QStringList &logLines,
                                               const QString &filePath,
                                               const QVariantList &lineNumbers) const
{
    QString markdown;
    
    // Add file info header
    if (!filePath.isEmpty()) {
        // Extract just filename
        QString fileName = filePath;
        int lastSlash = qMax(filePath.lastIndexOf('/'), filePath.lastIndexOf('\\'));
        if (lastSlash >= 0) {
            fileName = filePath.mid(lastSlash + 1);
        }
        markdown += QString("**File:** `%1`\n").arg(fileName);
    }
    
    // Add line range info
    if (!lineNumbers.isEmpty()) {
        if (lineNumbers.size() == 1) {
            markdown += QString("**Line:** %1\n").arg(lineNumbers[0].toInt());
        } else {
            markdown += QString("**Lines:** %1-%2\n")
                .arg(lineNumbers.first().toInt())
                .arg(lineNumbers.last().toInt());
        }
    }
    
    // Add timestamp
    markdown += QString("**Captured:** %1\n\n")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
    
    // Add log content in code block
    markdown += "```log\n";
    for (int i = 0; i < logLines.size(); ++i) {
        // Add line number prefix
        int lineNum = (i < lineNumbers.size()) ? lineNumbers[i].toInt() : (i + 1);
        markdown += QString("%1: %2\n").arg(lineNum, 6).arg(logLines[i]);
    }
    markdown += "```\n";
    
    // Add footer with app info
    markdown += "\n---\n*Created by BigFileViewer*";
    
    return markdown;
}
