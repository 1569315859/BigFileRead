/**
 * @file NotificationManager.cpp
 * @brief Unified Notification Manager implementation
 */

#include "NotificationManager.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#ifdef HAVE_QT_MULTIMEDIA
#include <QMediaPlayer>
#include <QAudioOutput>
#endif
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QApplication>
#include <QRegularExpression>

NotificationManager& NotificationManager::instance()
{
    static NotificationManager instance;
    return instance;
}

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_trayIcon(nullptr)
    , m_commandProcess(new QProcess(this))
    , m_webhookEnabled(true)
    , m_popupEnabled(true)
    , m_commandEnabled(true)
    , m_soundEnabled(true)
    , m_maxNotificationsPerMinute(60)
    , m_notificationCount(0)
{
    // Setup system tray
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setIcon(QIcon(":/icons/app.png"));
        m_trayIcon->setToolTip(tr("BigFileViewer"));
        m_trayIcon->show();
        
        connect(m_trayIcon, &QSystemTrayIcon::messageClicked,
                this, &NotificationManager::popupClicked);
    }
    
    // Setup process signals
    connect(m_commandProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus status) {
        Q_UNUSED(status)
        QString stdOut = QString::fromUtf8(m_commandProcess->readAllStandardOutput());
        QString stdErr = QString::fromUtf8(m_commandProcess->readAllStandardError());
        emit commandOutput(stdOut, stdErr, exitCode);
    });
    
    loadSettings();
}

NotificationManager::~NotificationManager()
{
    saveSettings();
}

void NotificationManager::setWebhookEnabled(bool enabled)
{
    if (m_webhookEnabled != enabled) {
        m_webhookEnabled = enabled;
        saveSettings();
        emit settingsChanged();
    }
}

void NotificationManager::setPopupEnabled(bool enabled)
{
    if (m_popupEnabled != enabled) {
        m_popupEnabled = enabled;
        saveSettings();
        emit settingsChanged();
    }
}

void NotificationManager::setCommandEnabled(bool enabled)
{
    if (m_commandEnabled != enabled) {
        m_commandEnabled = enabled;
        saveSettings();
        emit settingsChanged();
    }
}

void NotificationManager::setSoundEnabled(bool enabled)
{
    if (m_soundEnabled != enabled) {
        m_soundEnabled = enabled;
        saveSettings();
        emit settingsChanged();
    }
}

void NotificationManager::sendNotification(int type, const QVariantMap &config,
                                            const QString &ruleName, int lineNumber,
                                            const QString &lineContent)
{
    if (!checkRateLimit()) {
        emit notificationError(type, tr("Rate limit exceeded"));
        return;
    }
    
    QVariantMap data;
    data["ruleName"] = ruleName;
    data["lineNumber"] = lineNumber;
    data["lineContent"] = lineContent;
    data["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    data["hostname"] = QSysInfo::machineHostName();
    
    switch (static_cast<NotificationType>(type)) {
    case Webhook:
        if (m_webhookEnabled) {
            QString url = config.value("url").toString();
            int method = config.value("method", POST).toInt();
            QVariantMap headers = config.value("headers").toMap();
            QString bodyTemplate = config.value("bodyTemplate").toString();
            
            QString body = applyTemplate(bodyTemplate, data);
            doSendWebhook(url, static_cast<HttpMethod>(method), headers, body);
        }
        break;
        
    case Popup:
        if (m_popupEnabled) {
            QString title = config.value("title", tr("Alert: %1").arg(ruleName)).toString();
            QString message = config.value("message", 
                tr("Line %1: %2").arg(lineNumber).arg(lineContent)).toString();
            int icon = config.value("icon", 1).toInt();  // Warning by default
            showSystemTrayMessage(title, applyTemplate(message, data), icon);
        }
        break;
        
    case Command:
        if (m_commandEnabled) {
            QString command = config.value("command").toString();
            QVariantMap envVars = config.value("envVars").toMap();
            envVars["ALERT_RULE"] = ruleName;
            envVars["ALERT_LINE_NUMBER"] = QString::number(lineNumber);
            envVars["ALERT_LINE_CONTENT"] = lineContent;
            envVars["ALERT_TIMESTAMP"] = data["timestamp"].toString();
            doExecuteCommand(applyTemplate(command, data), envVars);
        }
        break;
        
    case Sound:
        if (m_soundEnabled) {
            QString soundFile = config.value("soundFile").toString();
            if (soundFile.isEmpty()) {
                playDefaultAlert();
            } else {
                playSound(soundFile);
            }
        }
        break;
        
    case Log: {
        QString logFile = config.value("logFile").toString();
        if (!logFile.isEmpty()) {
            QFile file(logFile);
            if (file.open(QIODevice::Append | QIODevice::Text)) {
                QString logLine = QString("[%1] %2 - Line %3: %4\n")
                    .arg(data["timestamp"].toString())
                    .arg(ruleName)
                    .arg(lineNumber)
                    .arg(lineContent);
                file.write(logLine.toUtf8());
                file.close();
                addToHistory(type, tr("Logged to %1").arg(logFile), true);
            }
        }
        break;
    }
        
    default:
        break;
    }
    
    m_notificationCount++;
    emit notificationCountChanged();
}

void NotificationManager::sendWebhook(const QString &url, int method,
                                       const QVariantMap &headers,
                                       const QString &bodyTemplate,
                                       const QVariantMap &data)
{
    QString body = applyTemplate(bodyTemplate, data);
    doSendWebhook(url, static_cast<HttpMethod>(method), headers, body);
}

void NotificationManager::testWebhook(const QString &url, int method,
                                       const QVariantMap &headers,
                                       const QString &body)
{
    doSendWebhook(url, static_cast<HttpMethod>(method), headers, body);
}

void NotificationManager::doSendWebhook(const QString &url, HttpMethod method,
                                         const QVariantMap &headers, const QString &body)
{
    QNetworkRequest request;
    request.setUrl(QUrl(url));
    
    // Set headers
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
    }
    
    // Default content type if not set
    if (!headers.contains("Content-Type")) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }
    
    QNetworkReply *reply = nullptr;
    
    switch (method) {
    case POST:
        reply = m_networkManager->post(request, body.toUtf8());
        break;
    case PUT:
        reply = m_networkManager->put(request, body.toUtf8());
        break;
    case PATCH:
        reply = m_networkManager->sendCustomRequest(request, "PATCH", body.toUtf8());
        break;
    }
    
    if (reply) {
        connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString response = QString::fromUtf8(reply->readAll());
            
            bool success = (statusCode >= 200 && statusCode < 300);
            addToHistory(Webhook, tr("Webhook to %1 - Status %2").arg(url).arg(statusCode), success);
            
            emit webhookResponse(statusCode, response);
            
            if (!success) {
                emit notificationError(Webhook, tr("Webhook failed: %1 - %2")
                    .arg(statusCode).arg(reply->errorString()));
            } else {
                emit notificationSent(Webhook, tr("Webhook sent to %1").arg(url));
            }
            
            reply->deleteLater();
        });
    }
}

void NotificationManager::showPopup(const QString &title, const QString &message, int durationMs)
{
    if (m_trayIcon) {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, durationMs);
        addToHistory(Popup, tr("Popup: %1").arg(title), true);
        emit notificationSent(Popup, title);
    }
}

void NotificationManager::showSystemTrayMessage(const QString &title, const QString &message, int icon)
{
    if (m_trayIcon) {
        QSystemTrayIcon::MessageIcon msgIcon;
        switch (icon) {
        case 0: msgIcon = QSystemTrayIcon::Information; break;
        case 1: msgIcon = QSystemTrayIcon::Warning; break;
        case 2: msgIcon = QSystemTrayIcon::Critical; break;
        default: msgIcon = QSystemTrayIcon::Information; break;
        }
        
        m_trayIcon->showMessage(title, message, msgIcon, 5000);
        addToHistory(Popup, tr("Tray message: %1").arg(title), true);
        emit notificationSent(Popup, title);
    }
}

void NotificationManager::executeCommand(const QString &command, const QVariantMap &envVars)
{
    doExecuteCommand(command, envVars);
}

void NotificationManager::testCommand(const QString &command)
{
    doExecuteCommand(command, QVariantMap());
}

void NotificationManager::doExecuteCommand(const QString &command, const QVariantMap &envVars)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    for (auto it = envVars.begin(); it != envVars.end(); ++it) {
        env.insert(it.key(), it.value().toString());
    }
    
    m_commandProcess->setProcessEnvironment(env);
    
#ifdef Q_OS_WIN
    m_commandProcess->start("cmd.exe", QStringList() << "/c" << command);
#else
    m_commandProcess->start("/bin/sh", QStringList() << "-c" << command);
#endif
    
    addToHistory(Command, tr("Command: %1").arg(command), true);
    emit notificationSent(Command, command);
}

void NotificationManager::playSound(const QString &soundFile)
{
#ifdef HAVE_QT_MULTIMEDIA
    static QMediaPlayer *player = nullptr;
    static QAudioOutput *audioOutput = nullptr;
    
    if (!player) {
        player = new QMediaPlayer(this);
        audioOutput = new QAudioOutput(this);
        player->setAudioOutput(audioOutput);
    }
    
    player->setSource(QUrl::fromLocalFile(soundFile));
    audioOutput->setVolume(0.5);
    player->play();
    
    addToHistory(Sound, tr("Sound: %1").arg(soundFile), true);
    emit notificationSent(Sound, soundFile);
#else
    Q_UNUSED(soundFile)
    QApplication::beep();
    emit notificationSent(Sound, tr("Beep (Multimedia not available)"));
#endif
}

void NotificationManager::playDefaultAlert()
{
    // Try to play a system sound
#ifdef Q_OS_WIN
    QApplication::beep();
#else
    // On Linux/macOS, try to use a default sound
    QString defaultSound = "/usr/share/sounds/freedesktop/stereo/bell.oga";
    if (QFile::exists(defaultSound)) {
        playSound(defaultSound);
    } else {
        QApplication::beep();
    }
#endif
    
    emit notificationSent(Sound, tr("Default alert"));
}

QStringList NotificationManager::getAvailableSounds() const
{
    QStringList sounds;
    
    // Check common sound directories
    QStringList searchPaths;
#ifdef Q_OS_WIN
    searchPaths << QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation).first() + "/Sounds";
    searchPaths << "C:/Windows/Media";
#else
    searchPaths << "/usr/share/sounds";
    searchPaths << QDir::homePath() + "/.local/share/sounds";
#endif
    
    for (const QString &path : searchPaths) {
        QDir dir(path);
        if (dir.exists()) {
            QStringList filters;
            filters << "*.wav" << "*.mp3" << "*.ogg" << "*.oga";
            QStringList files = dir.entryList(filters, QDir::Files);
            for (const QString &file : files) {
                sounds << dir.absoluteFilePath(file);
            }
        }
    }
    
    return sounds;
}

void NotificationManager::saveWebhookConfig(const QString &name, const QVariantMap &config)
{
    m_webhookConfigs[name] = config;
    saveSettings();
}

void NotificationManager::deleteWebhookConfig(const QString &name)
{
    m_webhookConfigs.remove(name);
    saveSettings();
}

QVariantList NotificationManager::getWebhookConfigs() const
{
    QVariantList result;
    for (auto it = m_webhookConfigs.begin(); it != m_webhookConfigs.end(); ++it) {
        QVariantMap item = it.value();
        item["name"] = it.key();
        result.append(item);
    }
    return result;
}

QVariantMap NotificationManager::getWebhookConfig(const QString &name) const
{
    return m_webhookConfigs.value(name, QVariantMap());
}

void NotificationManager::saveCommandConfig(const QString &name, const QVariantMap &config)
{
    m_commandConfigs[name] = config;
    saveSettings();
}

void NotificationManager::deleteCommandConfig(const QString &name)
{
    m_commandConfigs.remove(name);
    saveSettings();
}

QVariantList NotificationManager::getCommandConfigs() const
{
    QVariantList result;
    for (auto it = m_commandConfigs.begin(); it != m_commandConfigs.end(); ++it) {
        QVariantMap item = it.value();
        item["name"] = it.key();
        result.append(item);
    }
    return result;
}

QString NotificationManager::applyTemplate(const QString &templateStr, const QVariantMap &data)
{
    QString result = templateStr;
    
    // Replace {{variable}} patterns
    QRegularExpression re(R"(\{\{(\w+)\}\})");
    QRegularExpressionMatchIterator it = re.globalMatch(templateStr);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        QString value = data.value(varName).toString();
        result.replace(match.captured(0), value);
    }
    
    // Also support $variable and ${variable} patterns
    QRegularExpression re2(R"(\$\{?(\w+)\}?)");
    it = re2.globalMatch(result);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString varName = match.captured(1);
        QString value = data.value(varName).toString();
        result.replace(match.captured(0), value);
    }
    
    return result;
}

QVariantList NotificationManager::getAvailableTemplateVariables() const
{
    QVariantList vars;
    
    auto addVar = [&](const QString &name, const QString &desc) {
        QVariantMap item;
        item["name"] = name;
        item["description"] = desc;
        vars.append(item);
    };
    
    addVar("ruleName", tr("The name of the triggered rule"));
    addVar("lineNumber", tr("The line number that triggered the rule"));
    addVar("lineContent", tr("The content of the line"));
    addVar("timestamp", tr("The current timestamp (ISO format)"));
    addVar("hostname", tr("The hostname of the machine"));
    addVar("level", tr("The log level (if parsed)"));
    addVar("message", tr("The log message (if parsed)"));
    
    return vars;
}

QVariantList NotificationManager::getNotificationHistory(int limit) const
{
    QVariantList result;
    int start = qMax(0, m_history.size() - limit);
    
    for (int i = m_history.size() - 1; i >= start; --i) {
        const auto &record = m_history[i];
        QVariantMap item;
        item["timestamp"] = record.timestamp.toString(Qt::ISODate);
        item["type"] = record.type;
        item["details"] = record.details;
        item["success"] = record.success;
        result.append(item);
    }
    
    return result;
}

void NotificationManager::clearNotificationHistory()
{
    m_history.clear();
}

void NotificationManager::setRateLimit(int maxPerMinute)
{
    m_maxNotificationsPerMinute = maxPerMinute;
    saveSettings();
}

void NotificationManager::addToHistory(int type, const QString &details, bool success)
{
    NotificationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.type = type;
    record.details = details;
    record.success = success;
    m_history.append(record);
    
    // Keep history limited
    while (m_history.size() > 1000) {
        m_history.removeFirst();
    }
}

bool NotificationManager::checkRateLimit()
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime oneMinuteAgo = now.addSecs(-60);
    
    // Remove old timestamps
    while (!m_recentNotifications.isEmpty() && 
           m_recentNotifications.head() < oneMinuteAgo) {
        m_recentNotifications.dequeue();
    }
    
    if (m_recentNotifications.size() >= m_maxNotificationsPerMinute) {
        return false;
    }
    
    m_recentNotifications.enqueue(now);
    return true;
}

void NotificationManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("NotificationManager");
    
    m_webhookEnabled = settings.value("webhookEnabled", true).toBool();
    m_popupEnabled = settings.value("popupEnabled", true).toBool();
    m_commandEnabled = settings.value("commandEnabled", true).toBool();
    m_soundEnabled = settings.value("soundEnabled", true).toBool();
    m_maxNotificationsPerMinute = settings.value("maxNotificationsPerMinute", 60).toInt();
    
    // Load webhook configs
    QString webhookJson = settings.value("webhookConfigs").toString();
    if (!webhookJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(webhookJson.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                m_webhookConfigs[it.key()] = it.value().toObject().toVariantMap();
            }
        }
    }
    
    // Load command configs
    QString commandJson = settings.value("commandConfigs").toString();
    if (!commandJson.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(commandJson.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                m_commandConfigs[it.key()] = it.value().toObject().toVariantMap();
            }
        }
    }
    
    settings.endGroup();
}

void NotificationManager::saveSettings()
{
    QSettings settings;
    settings.beginGroup("NotificationManager");
    
    settings.setValue("webhookEnabled", m_webhookEnabled);
    settings.setValue("popupEnabled", m_popupEnabled);
    settings.setValue("commandEnabled", m_commandEnabled);
    settings.setValue("soundEnabled", m_soundEnabled);
    settings.setValue("maxNotificationsPerMinute", m_maxNotificationsPerMinute);
    
    // Save webhook configs
    QJsonObject webhookObj;
    for (auto it = m_webhookConfigs.begin(); it != m_webhookConfigs.end(); ++it) {
        webhookObj[it.key()] = QJsonObject::fromVariantMap(it.value());
    }
    settings.setValue("webhookConfigs", QString::fromUtf8(QJsonDocument(webhookObj).toJson(QJsonDocument::Compact)));
    
    // Save command configs
    QJsonObject commandObj;
    for (auto it = m_commandConfigs.begin(); it != m_commandConfigs.end(); ++it) {
        commandObj[it.key()] = QJsonObject::fromVariantMap(it.value());
    }
    settings.setValue("commandConfigs", QString::fromUtf8(QJsonDocument(commandObj).toJson(QJsonDocument::Compact)));
    
    settings.endGroup();
}
