/**
 * @file PluginManager.h
 * @brief Plugin System Manager Header
 * 
 * Provides a robust plugin architecture for BigFileViewer:
 * - Plugin discovery and loading
 * - Sandboxed execution via subprocess isolation
 * - Inter-process communication (IPC)
 * - Plugin lifecycle management
 * - Security validation
 */

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QThread>
#include <QMap>
#include <QTimer>
#include <QNetworkAccessManager>
#include <memory>

/**
 * @brief Plugin information structure
 */
struct PluginInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString version MEMBER version)
    Q_PROPERTY(QString description MEMBER description)
    Q_PROPERTY(QString author MEMBER author)
    Q_PROPERTY(QString homepage MEMBER homepage)
    Q_PROPERTY(QString type MEMBER type)
    Q_PROPERTY(QString entryPoint MEMBER entryPoint)
    Q_PROPERTY(QStringList permissions MEMBER permissions)
    Q_PROPERTY(bool enabled MEMBER enabled)
    Q_PROPERTY(bool running MEMBER running)
    Q_PROPERTY(QString status MEMBER status)
    
public:
    QString id;                    // Unique plugin ID
    QString name;                  // Display name
    QString version;              // Semver version
    QString description;          // Plugin description
    QString author;               // Author name
    QString homepage;             // Plugin homepage/repo
    QString type;                 // "parser", "analyzer", "exporter", "ui"
    QString entryPoint;           // Entry script/executable
    QStringList permissions;      // Required permissions
    bool enabled = false;         // User enabled
    bool running = false;         // Currently running
    QString status;               // Status message
    QString path;                 // Plugin directory path
    QJsonObject manifest;         // Full manifest data
    
    QVariantMap toVariantMap() const {
        QVariantMap map;
        map["id"] = id;
        map["name"] = name;
        map["version"] = version;
        map["description"] = description;
        map["author"] = author;
        map["homepage"] = homepage;
        map["type"] = type;
        map["entryPoint"] = entryPoint;
        map["permissions"] = permissions;
        map["enabled"] = enabled;
        map["running"] = running;
        map["status"] = status;
        return map;
    }
};

/**
 * @brief Plugin IPC message types
 */
enum class PluginMessageType {
    Initialize,
    Shutdown,
    Configure,
    ProcessLine,
    ProcessBatch,
    Analyze,
    Export,
    RegisterCommand,
    RegisterMenu,
    ShowNotification,
    Result,
    Error,
    Progress,
    Log
};

/**
 * @brief Sandboxed plugin process wrapper
 */
class PluginProcess : public QObject
{
    Q_OBJECT
    
public:
    explicit PluginProcess(const PluginInfo &info, QObject *parent = nullptr);
    ~PluginProcess();
    
    bool start();
    void stop();
    bool isRunning() const;
    
    void sendMessage(PluginMessageType type, const QJsonObject &payload);
    
signals:
    void messageReceived(PluginMessageType type, const QJsonObject &payload);
    void processError(const QString &error);
    void processFinished(int exitCode);
    void outputReady(const QString &output);
    
private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onSocketReadyRead();
    
private:
    PluginInfo m_info;
    QProcess *m_process;
    QLocalSocket *m_socket;
    QString m_serverName;
    QByteArray m_buffer;
    
    void processMessage(const QByteArray &data);
};

/**
 * @brief Plugin Manager singleton
 */
class PluginManager : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(QVariantList availablePlugins READ availablePlugins NOTIFY pluginsChanged)
    Q_PROPERTY(QVariantList enabledPlugins READ enabledPlugins NOTIFY pluginsChanged)
    Q_PROPERTY(QVariantList runningPlugins READ runningPlugins NOTIFY pluginsChanged)
    Q_PROPERTY(bool hasParserPlugins READ hasParserPlugins NOTIFY pluginsChanged)
    Q_PROPERTY(bool hasAnalyzerPlugins READ hasAnalyzerPlugins NOTIFY pluginsChanged)
    Q_PROPERTY(bool hasExporterPlugins READ hasExporterPlugins NOTIFY pluginsChanged)
    
public:
    static PluginManager& instance();
    
    QVariantList availablePlugins() const;
    QVariantList enabledPlugins() const;
    QVariantList runningPlugins() const;
    
    bool hasParserPlugins() const;
    bool hasAnalyzerPlugins() const;
    bool hasExporterPlugins() const;
    
    Q_INVOKABLE void refreshPlugins();
    Q_INVOKABLE QVariantMap getPluginInfo(const QString &pluginId) const;
    Q_INVOKABLE bool enablePlugin(const QString &pluginId, bool enable);
    Q_INVOKABLE bool startPlugin(const QString &pluginId);
    Q_INVOKABLE bool stopPlugin(const QString &pluginId);
    Q_INVOKABLE void stopAllPlugins();
    
    Q_INVOKABLE QStringList pluginDirectories() const;
    Q_INVOKABLE void addPluginDirectory(const QString &path);
    Q_INVOKABLE void removePluginDirectory(const QString &path);
    
    Q_INVOKABLE bool installPlugin(const QString &archivePath);
    Q_INVOKABLE bool uninstallPlugin(const QString &pluginId);
    Q_INVOKABLE bool updatePlugin(const QString &pluginId, const QString &archivePath);
    
    Q_INVOKABLE void searchOnlinePlugins(const QString &query);
    Q_INVOKABLE void downloadPlugin(const QString &pluginId, const QString &url);
    
    Q_INVOKABLE void invokePluginCommand(const QString &pluginId, const QString &command, const QVariantMap &args);
    Q_INVOKABLE QVariantList getPluginCommands(const QString &pluginId) const;
    Q_INVOKABLE QVariantList getPluginMenus(const QString &pluginId) const;
    
    Q_INVOKABLE QStringList getParserPluginFormats() const;
    Q_INVOKABLE QVariantMap parseWithPlugin(const QString &pluginId, const QString &line);
    Q_INVOKABLE void processLinesWithPlugin(const QString &pluginId, const QStringList &lines);
    
    Q_INVOKABLE void analyzeWithPlugin(const QString &pluginId, const QVariantList &logData);
    
    Q_INVOKABLE QStringList getExportFormats() const;
    Q_INVOKABLE void exportWithPlugin(const QString &pluginId, const QVariantList &logData, const QString &outputPath);
    
signals:
    void pluginsChanged();
    void pluginStarted(const QString &pluginId);
    void pluginStopped(const QString &pluginId);
    void pluginError(const QString &pluginId, const QString &error);
    void pluginOutput(const QString &pluginId, const QString &output);
    void pluginProgress(const QString &pluginId, int current, int total);
    
    void parseResult(const QString &pluginId, const QVariantMap &result);
    void batchParseResult(const QString &pluginId, const QVariantList &results);
    void analyzeResult(const QString &pluginId, const QVariantMap &result);
    void exportComplete(const QString &pluginId, const QString &outputPath);
    
    void onlinePluginsFound(const QVariantList &plugins);
    void pluginDownloadProgress(const QString &pluginId, int percent);
    void pluginInstalled(const QString &pluginId);
    
private:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager();
    Q_DISABLE_COPY(PluginManager)
    
    void loadSettings();
    void saveSettings();
    void scanPluginDirectories();
    bool loadPluginManifest(const QString &manifestPath, PluginInfo &info);
    bool validatePlugin(const PluginInfo &info) const;
    bool checkPermissions(const PluginInfo &info) const;
    
    QString getPluginsDir() const;
    QString getPluginDataDir(const QString &pluginId) const;
    
    void onPluginMessage(const QString &pluginId, PluginMessageType type, const QJsonObject &payload);
    
    QMap<QString, PluginInfo> m_plugins;
    QHash<QString, PluginProcess*> m_processes;
    QStringList m_pluginDirs;
    QLocalServer *m_ipcServer;
    QNetworkAccessManager *m_networkManager;
};

#endif // PLUGINMANAGER_H
