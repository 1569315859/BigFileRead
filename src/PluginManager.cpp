/**
 * @file PluginManager.cpp
 * @brief Plugin System Manager Implementation
 */

#include "PluginManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSettings>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QDebug>
#include <QUuid>

// ============== PluginProcess Implementation ==============

PluginProcess::PluginProcess(const PluginInfo &info, QObject *parent)
    : QObject(parent)
    , m_info(info)
    , m_process(new QProcess(this))
    , m_socket(nullptr)
    , m_serverName(QString("bigfileviewer_plugin_%1_%2").arg(info.id).arg(QUuid::createUuid().toString(QUuid::Id128)))
{
    connect(m_process, &QProcess::readyReadStandardOutput, this, &PluginProcess::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &PluginProcess::onReadyReadStandardError);
    connect(m_process, &QProcess::errorOccurred, this, &PluginProcess::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &PluginProcess::onProcessFinished);
}

PluginProcess::~PluginProcess()
{
    stop();
}

bool PluginProcess::start()
{
    if (m_process->state() != QProcess::NotRunning) {
        return false;
    }
    
    QString entryPoint = QDir(m_info.path).filePath(m_info.entryPoint);
    if (!QFileInfo::exists(entryPoint)) {
        emit processError(tr("Entry point not found: %1").arg(entryPoint));
        return false;
    }
    
    QString program;
    QStringList args;
    QFileInfo fi(entryPoint);
    QString ext = fi.suffix().toLower();
    
    if (ext == "py") {
        program = "python";
        args << entryPoint;
    } else if (ext == "js") {
        program = "node";
        args << entryPoint;
    } else if (ext == "exe" || ext.isEmpty()) {
        program = entryPoint;
    } else {
        emit processError(tr("Unsupported plugin type: %1").arg(ext));
        return false;
    }
    
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("BFV_PLUGIN_ID", m_info.id);
    env.insert("BFV_IPC_SERVER", m_serverName);
    env.insert("BFV_PLUGIN_DATA_DIR", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins/" + m_info.id);
    m_process->setProcessEnvironment(env);
    
    m_process->setWorkingDirectory(m_info.path);
    m_process->start(program, args);
    
    if (!m_process->waitForStarted(5000)) {
        emit processError(tr("Failed to start plugin process"));
        return false;
    }
    
    return true;
}

void PluginProcess::stop()
{
    if (m_process->state() != QProcess::NotRunning) {
        sendMessage(PluginMessageType::Shutdown, QJsonObject());
        
        if (!m_process->waitForFinished(3000)) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
    }
    
    if (m_socket) {
        m_socket->disconnectFromServer();
        m_socket->deleteLater();
        m_socket = nullptr;
    }
}

bool PluginProcess::isRunning() const
{
    return m_process->state() == QProcess::Running;
}

void PluginProcess::sendMessage(PluginMessageType type, const QJsonObject &payload)
{
    QJsonObject msg;
    msg["type"] = static_cast<int>(type);
    msg["payload"] = payload;
    
    QByteArray data = QJsonDocument(msg).toJson(QJsonDocument::Compact) + "\n";
    
    if (m_socket && m_socket->state() == QLocalSocket::ConnectedState) {
        m_socket->write(data);
        m_socket->flush();
    } else {
        m_process->write(data);
    }
}

void PluginProcess::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    m_buffer.append(data);
    
    while (true) {
        int idx = m_buffer.indexOf('\n');
        if (idx == -1) break;
        
        QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        
        if (!line.isEmpty()) {
            processMessage(line);
        }
    }
}

void PluginProcess::onReadyReadStandardError()
{
    QString errText = QString::fromUtf8(m_process->readAllStandardError());
    emit outputReady(errText);
}

void PluginProcess::onProcessError(QProcess::ProcessError error)
{
    QString errorStr;
    switch (error) {
        case QProcess::FailedToStart: errorStr = tr("Failed to start"); break;
        case QProcess::Crashed: errorStr = tr("Process crashed"); break;
        case QProcess::Timedout: errorStr = tr("Process timed out"); break;
        case QProcess::WriteError: errorStr = tr("Write error"); break;
        case QProcess::ReadError: errorStr = tr("Read error"); break;
        default: errorStr = tr("Unknown error"); break;
    }
    emit processError(errorStr);
}

void PluginProcess::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    emit processFinished(exitCode);
}

void PluginProcess::onSocketReadyRead()
{
    if (!m_socket) return;
    
    QByteArray data = m_socket->readAll();
    m_buffer.append(data);
    
    while (true) {
        int idx = m_buffer.indexOf('\n');
        if (idx == -1) break;
        
        QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        
        if (!line.isEmpty()) {
            processMessage(line);
        }
    }
}

void PluginProcess::processMessage(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Plugin message parse error:" << error.errorString();
        emit outputReady(QString::fromUtf8(data));
        return;
    }
    
    QJsonObject msg = doc.object();
    int typeInt = msg["type"].toInt();
    PluginMessageType type = static_cast<PluginMessageType>(typeInt);
    QJsonObject payload = msg["payload"].toObject();
    
    emit messageReceived(type, payload);
}

// ============== PluginManager Implementation ==============

PluginManager& PluginManager::instance()
{
    static PluginManager instance;
    return instance;
}

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
    , m_ipcServer(new QLocalServer(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    QString appDir = QCoreApplication::applicationDirPath();
    m_pluginDirs << appDir + "/plugins";
    m_pluginDirs << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
    
    loadSettings();
    scanPluginDirectories();
    
    QString serverName = QString("bigfileviewer_ipc_%1").arg(QCoreApplication::applicationPid());
    m_ipcServer->listen(serverName);
    
    connect(m_ipcServer, &QLocalServer::newConnection, this, [this]() {
        QLocalSocket *socket = m_ipcServer->nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            QByteArray data = socket->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject msg = doc.object();
                QString pluginId = msg["plugin_id"].toString();
                Q_UNUSED(pluginId);
            }
        });
    });
}

PluginManager::~PluginManager()
{
    stopAllPlugins();
    saveSettings();
}

void PluginManager::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Plugins");
    
    QStringList customDirs = settings.value("customDirectories").toStringList();
    for (const QString &dir : customDirs) {
        if (!m_pluginDirs.contains(dir)) {
            m_pluginDirs << dir;
        }
    }
    
    settings.endGroup();
}

void PluginManager::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Plugins");
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QStringList customDirs;
    for (const QString &dir : m_pluginDirs) {
        if (!dir.startsWith(appDir) && !dir.startsWith(dataDir)) {
            customDirs << dir;
        }
    }
    settings.setValue("customDirectories", customDirs);
    
    QStringList enabledList;
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().enabled) {
            enabledList << it.key();
        }
    }
    settings.setValue("enabledPlugins", enabledList);
    
    settings.endGroup();
}

void PluginManager::scanPluginDirectories()
{
    m_plugins.clear();
    
    for (const QString &dirPath : m_pluginDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        
        for (const QString &subdir : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QString manifestPath = dir.filePath(subdir + "/manifest.json");
            if (QFileInfo::exists(manifestPath)) {
                PluginInfo info;
                info.path = dir.filePath(subdir);
                if (loadPluginManifest(manifestPath, info)) {
                    m_plugins[info.id] = info;
                }
            }
        }
    }
    
    emit pluginsChanged();
}

bool PluginManager::loadPluginManifest(const QString &manifestPath, PluginInfo &info)
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open manifest:" << manifestPath;
        return false;
    }
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Manifest parse error:" << error.errorString();
        return false;
    }
    
    QJsonObject obj = doc.object();
    
    if (!obj.contains("id") || !obj.contains("name") || !obj.contains("version") || !obj.contains("entry")) {
        qWarning() << "Manifest missing required fields:" << manifestPath;
        return false;
    }
    
    info.id = obj["id"].toString();
    info.name = obj["name"].toString();
    info.version = obj["version"].toString();
    info.description = obj["description"].toString();
    info.author = obj["author"].toString();
    info.homepage = obj["homepage"].toString();
    info.type = obj["type"].toString("analyzer");
    info.entryPoint = obj["entry"].toString();
    info.manifest = obj;
    
    QJsonArray perms = obj["permissions"].toArray();
    for (const QJsonValue &v : perms) {
        info.permissions << v.toString();
    }
    
    if (!validatePlugin(info)) {
        return false;
    }
    
    info.status = tr("Available");
    return true;
}

bool PluginManager::validatePlugin(const PluginInfo &info) const
{
    QString entryPath = QDir(info.path).filePath(info.entryPoint);
    if (!QFileInfo::exists(entryPath)) {
        qWarning() << "Plugin entry point not found:" << entryPath;
        return false;
    }
    
    QStringList validTypes = {"parser", "analyzer", "exporter", "ui", "mixed"};
    if (!validTypes.contains(info.type)) {
        qWarning() << "Invalid plugin type:" << info.type;
        return false;
    }
    
    return true;
}

bool PluginManager::checkPermissions(const PluginInfo &info) const
{
    QStringList dangerousPerms = {"filesystem_full", "network_unrestricted", "execute_commands"};
    
    for (const QString &perm : info.permissions) {
        if (dangerousPerms.contains(perm)) {
            qWarning() << "Plugin requests dangerous permission:" << perm;
        }
    }
    
    return true;
}

QVariantList PluginManager::availablePlugins() const
{
    QVariantList list;
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        list.append(it.value().toVariantMap());
    }
    return list;
}

QVariantList PluginManager::enabledPlugins() const
{
    QVariantList list;
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().enabled) {
            list.append(it.value().toVariantMap());
        }
    }
    return list;
}

QVariantList PluginManager::runningPlugins() const
{
    QVariantList list;
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().running) {
            list.append(it.value().toVariantMap());
        }
    }
    return list;
}

bool PluginManager::hasParserPlugins() const
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().enabled && (it.value().type == "parser" || it.value().type == "mixed")) {
            return true;
        }
    }
    return false;
}

bool PluginManager::hasAnalyzerPlugins() const
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().enabled && (it.value().type == "analyzer" || it.value().type == "mixed")) {
            return true;
        }
    }
    return false;
}

bool PluginManager::hasExporterPlugins() const
{
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().enabled && (it.value().type == "exporter" || it.value().type == "mixed")) {
            return true;
        }
    }
    return false;
}

void PluginManager::refreshPlugins()
{
    scanPluginDirectories();
}

QVariantMap PluginManager::getPluginInfo(const QString &pluginId) const
{
    if (m_plugins.contains(pluginId)) {
        return m_plugins[pluginId].toVariantMap();
    }
    return QVariantMap();
}

bool PluginManager::enablePlugin(const QString &pluginId, bool enable)
{
    if (!m_plugins.contains(pluginId)) {
        return false;
    }
    
    PluginInfo &info = m_plugins[pluginId];
    
    if (enable && !checkPermissions(info)) {
        return false;
    }
    
    info.enabled = enable;
    if (!enable && info.running) {
        stopPlugin(pluginId);
    }
    
    saveSettings();
    emit pluginsChanged();
    return true;
}

bool PluginManager::startPlugin(const QString &pluginId)
{
    if (!m_plugins.contains(pluginId)) {
        return false;
    }
    
    PluginInfo &info = m_plugins[pluginId];
    if (!info.enabled) {
        return false;
    }
    
    if (info.running) {
        return true;
    }
    
    auto process = std::make_unique<PluginProcess>(info);
    
    connect(process.get(), &PluginProcess::messageReceived, this,
            [this, pluginId](PluginMessageType type, const QJsonObject &payload) {
                onPluginMessage(pluginId, type, payload);
            });
    
    connect(process.get(), &PluginProcess::processError, this,
            [this, pluginId](const QString &errMsg) {
                emit pluginError(pluginId, errMsg);
            });
    
    connect(process.get(), &PluginProcess::processFinished, this,
            [this, pluginId](int exitCode) {
                Q_UNUSED(exitCode);
                if (m_plugins.contains(pluginId)) {
                    m_plugins[pluginId].running = false;
                    m_plugins[pluginId].status = tr("Stopped");
                }
                delete m_processes.take(pluginId);
                emit pluginStopped(pluginId);
                emit pluginsChanged();
            });
    
    connect(process.get(), &PluginProcess::outputReady, this,
            [this, pluginId](const QString &output) {
                emit pluginOutput(pluginId, output);
            });
    
    if (!process->start()) {
        return false;
    }
    
    QJsonObject initPayload;
    initPayload["plugin_id"] = pluginId;
    initPayload["version"] = info.version;
    process->sendMessage(PluginMessageType::Initialize, initPayload);
    
    m_processes[pluginId] = process.release();
    info.running = true;
    info.status = tr("Running");
    
    emit pluginStarted(pluginId);
    emit pluginsChanged();
    return true;
}

bool PluginManager::stopPlugin(const QString &pluginId)
{
    if (!m_processes.contains(pluginId)) {
        return false;
    }
    
    PluginProcess* proc = m_processes.take(pluginId);
    delete proc;
    
    if (m_plugins.contains(pluginId)) {
        m_plugins[pluginId].running = false;
        m_plugins[pluginId].status = tr("Stopped");
    }
    
    emit pluginStopped(pluginId);
    emit pluginsChanged();
    return true;
}

void PluginManager::stopAllPlugins()
{
    QStringList pluginIds = m_processes.keys();
    for (const QString &id : pluginIds) {
        stopPlugin(id);
    }
}

QStringList PluginManager::pluginDirectories() const
{
    return m_pluginDirs;
}

void PluginManager::addPluginDirectory(const QString &path)
{
    if (!m_pluginDirs.contains(path)) {
        m_pluginDirs << path;
        scanPluginDirectories();
        saveSettings();
    }
}

void PluginManager::removePluginDirectory(const QString &path)
{
    m_pluginDirs.removeAll(path);
    scanPluginDirectories();
    saveSettings();
}

bool PluginManager::installPlugin(const QString &archivePath)
{
    Q_UNUSED(archivePath);
    emit pluginError("", tr("Plugin installation not yet implemented"));
    return false;
}

bool PluginManager::uninstallPlugin(const QString &pluginId)
{
    if (!m_plugins.contains(pluginId)) {
        return false;
    }
    
    if (m_plugins[pluginId].running) {
        stopPlugin(pluginId);
    }
    
    QString path = m_plugins[pluginId].path;
    QDir dir(path);
    if (dir.exists()) {
        dir.removeRecursively();
    }
    
    m_plugins.remove(pluginId);
    emit pluginsChanged();
    return true;
}

bool PluginManager::updatePlugin(const QString &pluginId, const QString &archivePath)
{
    Q_UNUSED(archivePath);
    if (m_plugins.contains(pluginId) && m_plugins[pluginId].running) {
        stopPlugin(pluginId);
    }
    
    emit pluginError(pluginId, tr("Plugin update not yet implemented"));
    return false;
}

void PluginManager::searchOnlinePlugins(const QString &query)
{
    Q_UNUSED(query);
    emit onlinePluginsFound(QVariantList());
}

void PluginManager::downloadPlugin(const QString &pluginId, const QString &url)
{
    Q_UNUSED(url);
    emit pluginError(pluginId, tr("Plugin download not yet implemented"));
}

void PluginManager::invokePluginCommand(const QString &pluginId, const QString &command, const QVariantMap &args)
{
    if (!m_processes.contains(pluginId)) {
        if (!startPlugin(pluginId)) {
            emit pluginError(pluginId, tr("Failed to start plugin"));
            return;
        }
    }
    
    QJsonObject payload;
    payload["command"] = command;
    payload["args"] = QJsonObject::fromVariantMap(args);
    
    m_processes[pluginId]->sendMessage(PluginMessageType::Configure, payload);
}

QVariantList PluginManager::getPluginCommands(const QString &pluginId) const
{
    if (!m_plugins.contains(pluginId)) {
        return QVariantList();
    }
    
    QJsonArray commands = m_plugins[pluginId].manifest["commands"].toArray();
    return commands.toVariantList();
}

QVariantList PluginManager::getPluginMenus(const QString &pluginId) const
{
    if (!m_plugins.contains(pluginId)) {
        return QVariantList();
    }
    
    QJsonArray menus = m_plugins[pluginId].manifest["menus"].toArray();
    return menus.toVariantList();
}

QStringList PluginManager::getParserPluginFormats() const
{
    QStringList formats;
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().enabled && (it.value().type == "parser" || it.value().type == "mixed")) {
            QJsonArray fmts = it.value().manifest["formats"].toArray();
            for (const QJsonValue &v : fmts) {
                formats << v.toString();
            }
        }
    }
    return formats;
}

QVariantMap PluginManager::parseWithPlugin(const QString &pluginId, const QString &line)
{
    if (!m_processes.contains(pluginId)) {
        return QVariantMap();
    }
    
    QJsonObject payload;
    payload["line"] = line;
    
    m_processes[pluginId]->sendMessage(PluginMessageType::ProcessLine, payload);
    
    return QVariantMap();
}

void PluginManager::processLinesWithPlugin(const QString &pluginId, const QStringList &lines)
{
    if (!m_processes.contains(pluginId)) {
        if (!startPlugin(pluginId)) {
            return;
        }
    }
    
    QJsonObject payload;
    QJsonArray arr;
    for (const QString &line : lines) {
        arr.append(line);
    }
    payload["lines"] = arr;
    
    m_processes[pluginId]->sendMessage(PluginMessageType::ProcessBatch, payload);
}

void PluginManager::analyzeWithPlugin(const QString &pluginId, const QVariantList &logData)
{
    if (!m_processes.contains(pluginId)) {
        if (!startPlugin(pluginId)) {
            return;
        }
    }
    
    QJsonObject payload;
    payload["data"] = QJsonArray::fromVariantList(logData);
    
    m_processes[pluginId]->sendMessage(PluginMessageType::Analyze, payload);
}

QStringList PluginManager::getExportFormats() const
{
    QStringList formats;
    for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it) {
        if (it.value().enabled && (it.value().type == "exporter" || it.value().type == "mixed")) {
            QJsonArray fmts = it.value().manifest["exportFormats"].toArray();
            for (const QJsonValue &v : fmts) {
                formats << v.toString();
            }
        }
    }
    return formats;
}

void PluginManager::exportWithPlugin(const QString &pluginId, const QVariantList &logData, const QString &outputPath)
{
    if (!m_processes.contains(pluginId)) {
        if (!startPlugin(pluginId)) {
            return;
        }
    }
    
    QJsonObject payload;
    payload["data"] = QJsonArray::fromVariantList(logData);
    payload["outputPath"] = outputPath;
    
    m_processes[pluginId]->sendMessage(PluginMessageType::Export, payload);
}

void PluginManager::onPluginMessage(const QString &pluginId, PluginMessageType type, const QJsonObject &payload)
{
    switch (type) {
        case PluginMessageType::Result:
            emit parseResult(pluginId, payload.toVariantMap());
            break;
            
        case PluginMessageType::Error:
            emit pluginError(pluginId, payload["message"].toString());
            break;
            
        case PluginMessageType::Progress:
            emit pluginProgress(pluginId, payload["current"].toInt(), payload["total"].toInt());
            break;
            
        case PluginMessageType::Log:
            emit pluginOutput(pluginId, payload["message"].toString());
            break;
            
        default:
            break;
    }
}

QString PluginManager::getPluginsDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/plugins";
}

QString PluginManager::getPluginDataDir(const QString &pluginId) const
{
    return getPluginsDir() + "/" + pluginId + "/data";
}
