/**
 * @file JiraIntegration.cpp
 * @brief Jira Integration Implementation
 */

#include "JiraIntegration.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <QDebug>

// ============================================================================
// Singleton
// ============================================================================

JiraIntegration& JiraIntegration::instance()
{
    static JiraIntegration instance;
    return instance;
}

JiraIntegration::JiraIntegration(QObject *parent)
    : QObject(parent)
    , m_loading(false)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &JiraIntegration::handleNetworkReply);
    
    loadConfiguration();
}

// ============================================================================
// Configuration
// ============================================================================

void JiraIntegration::loadConfiguration()
{
    QSettings settings("BigFileViewer", "BigFileViewer");
    settings.beginGroup("JiraIntegration");
    
    m_baseUrl = settings.value("baseUrl", "").toString();
    m_username = settings.value("username", "").toString();
    m_apiToken = settings.value("apiToken", "").toString();
    
    // Load templates
    QString templatesJson = settings.value("templates", "").toString();
    if (!templatesJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(templatesJson.toUtf8());
        if (doc.isArray()) {
            m_templates = doc.array().toVariantList();
        }
    }
    
    settings.endGroup();
}

void JiraIntegration::saveConfiguration()
{
    QSettings settings("BigFileViewer", "BigFileViewer");
    settings.beginGroup("JiraIntegration");
    
    settings.setValue("baseUrl", m_baseUrl);
    settings.setValue("username", m_username);
    settings.setValue("apiToken", m_apiToken);
    
    // Save templates
    QJsonArray templatesArray = QJsonArray::fromVariantList(m_templates);
    settings.setValue("templates", QString::fromUtf8(
        QJsonDocument(templatesArray).toJson(QJsonDocument::Compact)));
    
    settings.endGroup();
    settings.sync();
    
    emit configurationChanged();
}

bool JiraIntegration::isConfigured() const
{
    return !m_baseUrl.isEmpty() && !m_username.isEmpty() && !m_apiToken.isEmpty();
}

QVariantMap JiraIntegration::getConfiguration() const
{
    QVariantMap config;
    config["baseUrl"] = m_baseUrl;
    config["username"] = m_username;
    config["hasToken"] = !m_apiToken.isEmpty();
    config["isConfigured"] = isConfigured();
    return config;
}

void JiraIntegration::setBaseUrl(const QString &url)
{
    QString cleanUrl = url.trimmed();
    // Remove trailing slash
    while (cleanUrl.endsWith('/')) {
        cleanUrl.chop(1);
    }
    
    if (m_baseUrl != cleanUrl) {
        m_baseUrl = cleanUrl;
        emit configurationChanged();
    }
}

void JiraIntegration::setUsername(const QString &username)
{
    if (m_username != username) {
        m_username = username;
        emit configurationChanged();
    }
}

void JiraIntegration::setApiToken(const QString &token)
{
    m_apiToken = token;
    // Don't emit for security
}

// ============================================================================
// Network Helpers
// ============================================================================

QNetworkRequest JiraIntegration::createRequest(const QString &endpoint) const
{
    QUrl url(m_baseUrl + endpoint);
    QNetworkRequest request(url);
    
    // Set headers
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Accept", "application/json");
    
    // Basic authentication (Base64 encoded username:token)
    QString credentials = m_username + ":" + m_apiToken;
    QByteArray authData = "Basic " + credentials.toUtf8().toBase64();
    request.setRawHeader("Authorization", authData);
    
    return request;
}

void JiraIntegration::setLoading(bool loading)
{
    if (m_loading != loading) {
        m_loading = loading;
        emit loadingChanged();
    }
}

void JiraIntegration::setError(const QString &error)
{
    m_lastError = error;
    if (!error.isEmpty()) {
        emit errorOccurred(error);
    }
}

// ============================================================================
// Connection Test
// ============================================================================

void JiraIntegration::testConnection()
{
    if (!isConfigured()) {
        emit connectionTested(false, tr("Jira is not configured"));
        return;
    }
    
    setLoading(true);
    setError("");
    
    // Test by fetching current user
    QNetworkRequest request = createRequest("/rest/api/2/myself");
    QNetworkReply *reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = TestConnection;
}

// ============================================================================
// Projects
// ============================================================================

void JiraIntegration::fetchProjects()
{
    if (!isConfigured()) {
        setError(tr("Jira is not configured"));
        return;
    }
    
    setLoading(true);
    setError("");
    
    QNetworkRequest request = createRequest("/rest/api/2/project");
    QNetworkReply *reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = FetchProjects;
}

QVariantList JiraIntegration::getProjects() const
{
    return m_projects;
}

void JiraIntegration::fetchIssueTypes(const QString &projectKey)
{
    if (!isConfigured() || projectKey.isEmpty()) {
        return;
    }
    
    setLoading(true);
    setError("");
    
    QString endpoint = QString("/rest/api/2/project/%1").arg(projectKey);
    QNetworkRequest request = createRequest(endpoint);
    QNetworkReply *reply = m_networkManager->get(request);
    
    m_pendingRequests[reply] = FetchIssueTypes;
    m_requestContext[reply] = projectKey;
}

QVariantList JiraIntegration::getIssueTypes(const QString &projectKey) const
{
    return m_issueTypes.value(projectKey);
}

// ============================================================================
// Issue Creation
// ============================================================================

QString JiraIntegration::createIssue(const QString &projectKey,
                                      const QString &issueType,
                                      const QString &summary,
                                      const QString &description,
                                      const QStringList &labels,
                                      const QString &priority)
{
    if (!isConfigured()) {
        emit issueCreateFailed(tr("Jira is not configured"));
        return QString();
    }
    
    if (projectKey.isEmpty() || issueType.isEmpty() || summary.isEmpty()) {
        emit issueCreateFailed(tr("Project, issue type, and summary are required"));
        return QString();
    }
    
    setLoading(true);
    setError("");
    
    // Build issue JSON
    QJsonObject fields;
    
    // Project
    QJsonObject project;
    project["key"] = projectKey;
    fields["project"] = project;
    
    // Issue type
    QJsonObject issueTypeObj;
    issueTypeObj["name"] = issueType;
    fields["issuetype"] = issueTypeObj;
    
    // Summary
    fields["summary"] = summary;
    
    // Description
    if (!description.isEmpty()) {
        fields["description"] = description;
    }
    
    // Labels
    if (!labels.isEmpty()) {
        QJsonArray labelsArray;
        for (const QString &label : labels) {
            if (!label.trimmed().isEmpty()) {
                labelsArray.append(label.trimmed());
            }
        }
        if (!labelsArray.isEmpty()) {
            fields["labels"] = labelsArray;
        }
    }
    
    // Priority
    if (!priority.isEmpty()) {
        QJsonObject priorityObj;
        priorityObj["name"] = priority;
        fields["priority"] = priorityObj;
    }
    
    QJsonObject issue;
    issue["fields"] = fields;
    
    QJsonDocument doc(issue);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    
    qDebug() << "Creating Jira issue:" << data;
    
    QNetworkRequest request = createRequest("/rest/api/2/issue");
    QNetworkReply *reply = m_networkManager->post(request, data);
    
    m_pendingRequests[reply] = CreateIssue;
    
    return QString::number(reinterpret_cast<quintptr>(reply));  // Request ID
}

QString JiraIntegration::formatLogForJira(const QStringList &logLines,
                                           const QString &filePath,
                                           const QVariantList &lineNumbers) const
{
    QString description;
    
    // Header
    description += "h3. Log Details\n\n";
    
    // File info
    if (!filePath.isEmpty()) {
        description += QString("*File:* %1\n").arg(filePath);
    }
    
    // Line numbers
    if (!lineNumbers.isEmpty()) {
        QStringList lineStrs;
        for (const QVariant &ln : lineNumbers) {
            lineStrs.append(QString::number(ln.toInt()));
        }
        if (lineStrs.size() == 1) {
            description += QString("*Line:* %1\n").arg(lineStrs.first());
        } else {
            description += QString("*Lines:* %1 - %2\n")
                .arg(lineStrs.first())
                .arg(lineStrs.last());
        }
    }
    
    description += "\n";
    
    // Log content in code block
    description += "{code:title=Log Content}\n";
    description += logLines.join("\n");
    description += "\n{code}\n";
    
    // Timestamp
    description += QString("\n_Created from BigFileViewer at %1_\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    
    return description;
}

// ============================================================================
// Templates
// ============================================================================

QVariantList JiraIntegration::getTemplates() const
{
    return m_templates;
}

void JiraIntegration::saveTemplate(const QString &name,
                                    const QString &projectKey,
                                    const QString &issueType,
                                    const QString &summaryTemplate,
                                    const QStringList &labels)
{
    // Remove existing template with same name
    for (int i = 0; i < m_templates.size(); ++i) {
        QVariantMap tmpl = m_templates[i].toMap();
        if (tmpl["name"].toString() == name) {
            m_templates.removeAt(i);
            break;
        }
    }
    
    // Add new template
    QVariantMap tmpl;
    tmpl["name"] = name;
    tmpl["projectKey"] = projectKey;
    tmpl["issueType"] = issueType;
    tmpl["summaryTemplate"] = summaryTemplate;
    tmpl["labels"] = labels;
    
    m_templates.append(tmpl);
    saveConfiguration();
}

void JiraIntegration::removeTemplate(const QString &name)
{
    for (int i = 0; i < m_templates.size(); ++i) {
        QVariantMap tmpl = m_templates[i].toMap();
        if (tmpl["name"].toString() == name) {
            m_templates.removeAt(i);
            saveConfiguration();
            return;
        }
    }
}

// ============================================================================
// Network Reply Handler
// ============================================================================

void JiraIntegration::handleNetworkReply(QNetworkReply *reply)
{
    reply->deleteLater();
    
    if (!m_pendingRequests.contains(reply)) {
        return;
    }
    
    RequestType type = m_pendingRequests.take(reply);
    QString context = m_requestContext.take(reply);
    
    setLoading(false);
    
    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        
        // Try to get more details from response
        QByteArray data = reply->readAll();
        if (!data.isEmpty()) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                if (obj.contains("errorMessages")) {
                    QJsonArray errors = obj["errorMessages"].toArray();
                    if (!errors.isEmpty()) {
                        errorMsg = errors.first().toString();
                    }
                } else if (obj.contains("message")) {
                    errorMsg = obj["message"].toString();
                }
            }
        }
        
        setError(errorMsg);
        
        switch (type) {
            case TestConnection:
                emit connectionTested(false, errorMsg);
                break;
            case CreateIssue:
                emit issueCreateFailed(errorMsg);
                break;
            default:
                break;
        }
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    switch (type) {
        case TestConnection: {
            if (doc.isObject()) {
                QJsonObject user = doc.object();
                QString displayName = user["displayName"].toString();
                emit connectionTested(true, tr("Connected as %1").arg(displayName));
            } else {
                emit connectionTested(false, tr("Invalid response from server"));
            }
            break;
        }
        
        case FetchProjects: {
            if (doc.isArray()) {
                m_projects.clear();
                QJsonArray arr = doc.array();
                for (const QJsonValue &val : arr) {
                    QJsonObject proj = val.toObject();
                    QVariantMap item;
                    item["key"] = proj["key"].toString();
                    item["name"] = proj["name"].toString();
                    item["id"] = proj["id"].toString();
                    m_projects.append(item);
                }
                emit projectsFetched(m_projects);
            }
            break;
        }
        
        case FetchIssueTypes: {
            if (doc.isObject()) {
                QJsonObject proj = doc.object();
                QJsonArray types = proj["issueTypes"].toArray();
                
                QVariantList typesList;
                for (const QJsonValue &val : types) {
                    QJsonObject typeObj = val.toObject();
                    QVariantMap item;
                    item["id"] = typeObj["id"].toString();
                    item["name"] = typeObj["name"].toString();
                    item["description"] = typeObj["description"].toString();
                    item["subtask"] = typeObj["subtask"].toBool();
                    typesList.append(item);
                }
                
                m_issueTypes[context] = typesList;
                emit issueTypesFetched(context, typesList);
            }
            break;
        }
        
        case CreateIssue: {
            if (doc.isObject()) {
                QJsonObject result = doc.object();
                QString key = result["key"].toString();
                QString id = result["id"].toString();
                QString url = m_baseUrl + "/browse/" + key;
                
                qDebug() << "Jira issue created:" << key;
                emit issueCreated(key, url);
            } else {
                emit issueCreateFailed(tr("Invalid response from server"));
            }
            break;
        }
    }
}
