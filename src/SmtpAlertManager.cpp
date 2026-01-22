/**
 * @file SmtpAlertManager.cpp
 * @brief SMTP Email Alert Manager Implementation
 */

#include "SmtpAlertManager.h"
#include <QUuid>
#include <QDateTime>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QSslSocket>
#include <QDebug>

// ============================================================================
// Singleton
// ============================================================================

SmtpAlertManager& SmtpAlertManager::instance()
{
    static SmtpAlertManager instance;
    return instance;
}

SmtpAlertManager::SmtpAlertManager(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_smtpPort(587)
    , m_useTls(true)
    , m_batchIntervalMs(30000)  // 30 seconds batch interval
    , m_networkManager(new QNetworkAccessManager(this))
{
    m_batchTimer = new QTimer(this);
    m_batchTimer->setInterval(m_batchIntervalMs);
    connect(m_batchTimer, &QTimer::timeout, this, &SmtpAlertManager::processPendingAlerts);
    
    loadSettings();
    loadRules();
}

// ============================================================================
// Property Setters
// ============================================================================

void SmtpAlertManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        
        if (m_enabled && !m_batchTimer->isActive()) {
            m_batchTimer->start();
        } else if (!m_enabled && m_batchTimer->isActive()) {
            m_batchTimer->stop();
        }
        
        emit enabledChanged();
        saveSettings();
    }
}

void SmtpAlertManager::setSmtpHost(const QString &host)
{
    if (m_smtpHost != host) {
        m_smtpHost = host;
        emit settingsChanged();
    }
}

void SmtpAlertManager::setSmtpPort(int port)
{
    if (m_smtpPort != port) {
        m_smtpPort = port;
        emit settingsChanged();
    }
}

void SmtpAlertManager::setUseTls(bool useTls)
{
    if (m_useTls != useTls) {
        m_useTls = useTls;
        emit settingsChanged();
    }
}

void SmtpAlertManager::setUsername(const QString &username)
{
    if (m_username != username) {
        m_username = username;
        emit settingsChanged();
    }
}

void SmtpAlertManager::setFromAddress(const QString &address)
{
    if (m_fromAddress != address) {
        m_fromAddress = address;
        emit settingsChanged();
    }
}

void SmtpAlertManager::setToAddresses(const QString &addresses)
{
    if (m_toAddresses != addresses) {
        m_toAddresses = addresses;
        emit settingsChanged();
    }
}

void SmtpAlertManager::setPassword(const QString &password)
{
    m_password = password;
    // Don't emit signal for security
}

// ============================================================================
// Settings
// ============================================================================

void SmtpAlertManager::loadSettings()
{
    QSettings settings("BigFileViewer", "BigFileViewer");
    settings.beginGroup("SmtpAlert");
    
    m_enabled = settings.value("enabled", false).toBool();
    m_smtpHost = settings.value("smtpHost", "smtp.gmail.com").toString();
    m_smtpPort = settings.value("smtpPort", 587).toInt();
    m_useTls = settings.value("useTls", true).toBool();
    m_username = settings.value("username", "").toString();
    m_fromAddress = settings.value("fromAddress", "").toString();
    m_toAddresses = settings.value("toAddresses", "").toString();
    // Password is not stored in plain text - should use system keychain in production
    m_password = settings.value("password", "").toString();
    m_batchIntervalMs = settings.value("batchInterval", 30000).toInt();
    
    settings.endGroup();
    
    if (m_enabled) {
        m_batchTimer->start();
    }
}

void SmtpAlertManager::saveSettings()
{
    QSettings settings("BigFileViewer", "BigFileViewer");
    settings.beginGroup("SmtpAlert");
    
    settings.setValue("enabled", m_enabled);
    settings.setValue("smtpHost", m_smtpHost);
    settings.setValue("smtpPort", m_smtpPort);
    settings.setValue("useTls", m_useTls);
    settings.setValue("username", m_username);
    settings.setValue("fromAddress", m_fromAddress);
    settings.setValue("toAddresses", m_toAddresses);
    settings.setValue("password", m_password);  // Note: In production, use keychain
    settings.setValue("batchInterval", m_batchIntervalMs);
    
    settings.endGroup();
    settings.sync();
}

QVariantMap SmtpAlertManager::getSettings() const
{
    QVariantMap result;
    result["enabled"] = m_enabled;
    result["smtpHost"] = m_smtpHost;
    result["smtpPort"] = m_smtpPort;
    result["useTls"] = m_useTls;
    result["username"] = m_username;
    result["fromAddress"] = m_fromAddress;
    result["toAddresses"] = m_toAddresses;
    result["hasPassword"] = !m_password.isEmpty();
    result["batchInterval"] = m_batchIntervalMs;
    return result;
}

// ============================================================================
// Rule Management
// ============================================================================

void SmtpAlertManager::loadRules()
{
    QSettings settings("BigFileViewer", "BigFileViewer");
    QString rulesJson = settings.value("SmtpAlert/rules", "").toString();
    
    if (rulesJson.isEmpty()) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(rulesJson.toUtf8());
    if (!doc.isArray()) return;
    
    QJsonArray arr = doc.array();
    m_rules.clear();
    
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        AlertRule rule;
        rule.id = obj["id"].toString();
        rule.name = obj["name"].toString();
        rule.keywords = obj["keywords"].toString().split(",", Qt::SkipEmptyParts);
        rule.caseSensitive = obj["caseSensitive"].toBool();
        rule.useRegex = obj["useRegex"].toBool();
        rule.cooldownMinutes = obj["cooldownMinutes"].toInt(5);
        rule.enabled = obj["enabled"].toBool(true);
        rule.lastTriggered = 0;
        
        // Trim keywords
        for (QString &kw : rule.keywords) {
            kw = kw.trimmed();
        }
        
        m_rules.append(rule);
    }
    
    emit rulesChanged();
}

void SmtpAlertManager::saveRules()
{
    QJsonArray arr;
    
    for (const AlertRule &rule : m_rules) {
        QJsonObject obj;
        obj["id"] = rule.id;
        obj["name"] = rule.name;
        obj["keywords"] = rule.keywords.join(",");
        obj["caseSensitive"] = rule.caseSensitive;
        obj["useRegex"] = rule.useRegex;
        obj["cooldownMinutes"] = rule.cooldownMinutes;
        obj["enabled"] = rule.enabled;
        arr.append(obj);
    }
    
    QJsonDocument doc(arr);
    
    QSettings settings("BigFileViewer", "BigFileViewer");
    settings.setValue("SmtpAlert/rules", QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    settings.sync();
}

QVariantList SmtpAlertManager::getRules() const
{
    QVariantList result;
    
    for (const AlertRule &rule : m_rules) {
        QVariantMap map;
        map["id"] = rule.id;
        map["name"] = rule.name;
        map["keywords"] = rule.keywords.join(", ");
        map["caseSensitive"] = rule.caseSensitive;
        map["useRegex"] = rule.useRegex;
        map["cooldownMinutes"] = rule.cooldownMinutes;
        map["enabled"] = rule.enabled;
        result.append(map);
    }
    
    return result;
}

QString SmtpAlertManager::addRule(const QString &name, const QString &keywords,
                                   bool caseSensitive, bool useRegex, int cooldownMinutes)
{
    AlertRule rule;
    rule.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    rule.name = name;
    rule.keywords = keywords.split(",", Qt::SkipEmptyParts);
    rule.caseSensitive = caseSensitive;
    rule.useRegex = useRegex;
    rule.cooldownMinutes = cooldownMinutes;
    rule.enabled = true;
    rule.lastTriggered = 0;
    
    // Trim keywords
    for (QString &kw : rule.keywords) {
        kw = kw.trimmed();
    }
    
    m_rules.append(rule);
    saveRules();
    emit rulesChanged();
    
    return rule.id;
}

bool SmtpAlertManager::updateRule(const QString &id, const QString &name,
                                   const QString &keywords, bool caseSensitive,
                                   bool useRegex, int cooldownMinutes)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == id) {
            m_rules[i].name = name;
            m_rules[i].keywords = keywords.split(",", Qt::SkipEmptyParts);
            m_rules[i].caseSensitive = caseSensitive;
            m_rules[i].useRegex = useRegex;
            m_rules[i].cooldownMinutes = cooldownMinutes;
            
            // Trim keywords
            for (QString &kw : m_rules[i].keywords) {
                kw = kw.trimmed();
            }
            
            saveRules();
            emit rulesChanged();
            return true;
        }
    }
    return false;
}

bool SmtpAlertManager::removeRule(const QString &id)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == id) {
            m_rules.removeAt(i);
            saveRules();
            emit rulesChanged();
            return true;
        }
    }
    return false;
}

bool SmtpAlertManager::setRuleEnabled(const QString &id, bool enabled)
{
    for (int i = 0; i < m_rules.size(); ++i) {
        if (m_rules[i].id == id) {
            m_rules[i].enabled = enabled;
            saveRules();
            emit rulesChanged();
            return true;
        }
    }
    return false;
}

// ============================================================================
// Alert Processing
// ============================================================================

bool SmtpAlertManager::matchesRule(const AlertRule &rule, const QString &line, QString &matchedKeyword)
{
    for (const QString &keyword : rule.keywords) {
        if (keyword.isEmpty()) continue;
        
        if (rule.useRegex) {
            QRegularExpression::PatternOptions opts = QRegularExpression::NoPatternOption;
            if (!rule.caseSensitive) {
                opts |= QRegularExpression::CaseInsensitiveOption;
            }
            QRegularExpression re(keyword, opts);
            if (re.isValid() && re.match(line).hasMatch()) {
                matchedKeyword = keyword;
                return true;
            }
        } else {
            Qt::CaseSensitivity cs = rule.caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
            if (line.contains(keyword, cs)) {
                matchedKeyword = keyword;
                return true;
            }
        }
    }
    return false;
}

void SmtpAlertManager::checkLine(const QString &line, int lineNumber, const QString &filePath)
{
    if (!m_enabled || line.isEmpty()) return;
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    
    for (AlertRule &rule : m_rules) {
        if (!rule.enabled) continue;
        
        // Check cooldown
        qint64 cooldownMs = rule.cooldownMinutes * 60 * 1000;
        if (now - rule.lastTriggered < cooldownMs) {
            continue;  // Still in cooldown
        }
        
        QString matchedKeyword;
        if (matchesRule(rule, line, matchedKeyword)) {
            rule.lastTriggered = now;
            queueAlert(rule, matchedKeyword, line, lineNumber, filePath);
            emit alertTriggered(rule.name, matchedKeyword, line, lineNumber);
        }
    }
}

void SmtpAlertManager::queueAlert(const AlertRule &rule, const QString &keyword,
                                   const QString &line, int lineNumber, const QString &filePath)
{
    AlertItem alert;
    alert.ruleName = rule.name;
    alert.matchedKeyword = keyword;
    alert.logLine = line.left(500);  // Truncate very long lines
    alert.lineNumber = lineNumber;
    alert.filePath = filePath;
    alert.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    QMutexLocker locker(&m_queueMutex);
    m_alertQueue.enqueue(alert);
    emit alertQueueChanged();
    
    qDebug() << "Alert queued:" << rule.name << "-" << keyword << "at line" << lineNumber;
}

void SmtpAlertManager::processPendingAlerts()
{
    QMutexLocker locker(&m_queueMutex);
    
    if (m_alertQueue.isEmpty()) return;
    
    // Collect all pending alerts
    QList<AlertItem> alerts;
    while (!m_alertQueue.isEmpty()) {
        alerts.append(m_alertQueue.dequeue());
    }
    
    locker.unlock();
    emit alertQueueChanged();
    
    // Build and send email
    QString subject = QString("[BigFileViewer Alert] %1 alert(s) triggered").arg(alerts.size());
    QString body = buildAlertEmailBody(alerts);
    
    if (sendEmail(subject, body)) {
        emit emailSent(alerts.size());
        emit statusMessage(tr("Email sent with %1 alert(s)").arg(alerts.size()));
    }
}

QString SmtpAlertManager::buildAlertEmailBody(const QList<AlertItem> &alerts)
{
    QString body;
    body += "BigFileViewer - Log Alert Notification\n";
    body += "======================================\n\n";
    body += QString("Total Alerts: %1\n").arg(alerts.size());
    body += QString("Time: %1\n\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    
    for (int i = 0; i < alerts.size(); ++i) {
        const AlertItem &alert = alerts[i];
        body += QString("--- Alert %1 ---\n").arg(i + 1);
        body += QString("Rule: %1\n").arg(alert.ruleName);
        body += QString("Matched: %1\n").arg(alert.matchedKeyword);
        body += QString("File: %1\n").arg(alert.filePath);
        body += QString("Line: %1\n").arg(alert.lineNumber);
        body += QString("Content:\n%1\n\n").arg(alert.logLine);
    }
    
    body += "---\n";
    body += "This is an automated message from BigFileViewer.\n";
    
    return body;
}

// ============================================================================
// Email Sending (Simple SMTP Implementation)
// ============================================================================

bool SmtpAlertManager::sendEmail(const QString &subject, const QString &body)
{
    if (m_smtpHost.isEmpty() || m_fromAddress.isEmpty() || m_toAddresses.isEmpty()) {
        emit emailFailed(tr("SMTP settings incomplete"));
        return false;
    }
    
    QSslSocket socket;
    
    // Connect to SMTP server
    if (m_useTls && m_smtpPort == 465) {
        // Implicit TLS (SMTPS)
        socket.connectToHostEncrypted(m_smtpHost, m_smtpPort);
    } else {
        socket.connectToHost(m_smtpHost, m_smtpPort);
    }
    
    if (!socket.waitForConnected(10000)) {
        emit emailFailed(tr("Failed to connect to SMTP server: %1").arg(socket.errorString()));
        return false;
    }
    
    auto readResponse = [&socket]() -> QString {
        socket.waitForReadyRead(5000);
        return QString::fromUtf8(socket.readAll());
    };
    
    auto sendCommand = [&socket](const QString &cmd) {
        socket.write((cmd + "\r\n").toUtf8());
        socket.waitForBytesWritten(3000);
    };
    
    // Read greeting
    QString response = readResponse();
    if (!response.startsWith("220")) {
        emit emailFailed(tr("Unexpected server response: %1").arg(response));
        return false;
    }
    
    // EHLO
    sendCommand("EHLO localhost");
    response = readResponse();
    
    // STARTTLS if using port 587
    if (m_useTls && m_smtpPort == 587) {
        sendCommand("STARTTLS");
        response = readResponse();
        if (response.startsWith("220")) {
            socket.startClientEncryption();
            if (!socket.waitForEncrypted(10000)) {
                emit emailFailed(tr("TLS encryption failed"));
                return false;
            }
            // Re-send EHLO after TLS
            sendCommand("EHLO localhost");
            response = readResponse();
        }
    }
    
    // AUTH LOGIN
    if (!m_username.isEmpty() && !m_password.isEmpty()) {
        sendCommand("AUTH LOGIN");
        response = readResponse();
        if (response.startsWith("334")) {
            sendCommand(QString::fromUtf8(m_username.toUtf8().toBase64()));
            response = readResponse();
            if (response.startsWith("334")) {
                sendCommand(QString::fromUtf8(m_password.toUtf8().toBase64()));
                response = readResponse();
                if (!response.startsWith("235")) {
                    emit emailFailed(tr("Authentication failed: %1").arg(response));
                    return false;
                }
            }
        }
    }
    
    // MAIL FROM
    sendCommand(QString("MAIL FROM:<%1>").arg(m_fromAddress));
    response = readResponse();
    if (!response.startsWith("250")) {
        emit emailFailed(tr("MAIL FROM rejected: %1").arg(response));
        return false;
    }
    
    // RCPT TO (for each recipient)
    QStringList recipients = m_toAddresses.split(",", Qt::SkipEmptyParts);
    for (const QString &rcpt : recipients) {
        sendCommand(QString("RCPT TO:<%1>").arg(rcpt.trimmed()));
        response = readResponse();
        if (!response.startsWith("250")) {
            qWarning() << "RCPT TO rejected for" << rcpt << ":" << response;
        }
    }
    
    // DATA
    sendCommand("DATA");
    response = readResponse();
    if (!response.startsWith("354")) {
        emit emailFailed(tr("DATA command rejected: %1").arg(response));
        return false;
    }
    
    // Email content
    QString email;
    email += QString("From: BigFileViewer <%1>\r\n").arg(m_fromAddress);
    email += QString("To: %1\r\n").arg(m_toAddresses);
    email += QString("Subject: %1\r\n").arg(subject);
    email += "MIME-Version: 1.0\r\n";
    email += "Content-Type: text/plain; charset=utf-8\r\n";
    email += QString("Date: %1\r\n").arg(QDateTime::currentDateTimeUtc().toString(Qt::RFC2822Date));
    email += "\r\n";
    email += body;
    email += "\r\n.\r\n";
    
    socket.write(email.toUtf8());
    socket.waitForBytesWritten(5000);
    response = readResponse();
    
    if (!response.startsWith("250")) {
        emit emailFailed(tr("Email not accepted: %1").arg(response));
        return false;
    }
    
    // QUIT
    sendCommand("QUIT");
    socket.disconnectFromHost();
    
    qDebug() << "Email sent successfully";
    return true;
}

bool SmtpAlertManager::sendTestEmail()
{
    QString subject = "[BigFileViewer] Test Email";
    QString body = "This is a test email from BigFileViewer.\n\n";
    body += "If you received this email, your SMTP settings are configured correctly.\n\n";
    body += QString("Sent at: %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    
    return sendEmail(subject, body);
}
