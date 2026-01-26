/**
 * @file DatabaseConnector.cpp
 * @brief Implementation of DatabaseConnector
 */

#include "DatabaseConnector.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QCryptographicHash>
#include <QSqlDriver>
#include <QSqlField>
#include <QElapsedTimer>
#include <QDebug>

// ============ Singleton ============

DatabaseConnector& DatabaseConnector::instance()
{
    static DatabaseConnector instance;
    return instance;
}

// ============ Constructor/Destructor ============

DatabaseConnector::DatabaseConnector(QObject *parent)
    : QObject(parent)
    , m_isConnected(false)
    , m_isQuerying(false)
    , m_queryThread(nullptr)
    , m_cancelRequested(false)
{
    // Generate unique connection name
    m_connectionName = QString("BigFileViewer_%1").arg(reinterpret_cast<quintptr>(this));
    
    loadProfiles();
    
    qDebug() << "DatabaseConnector initialized. Available drivers:" << QSqlDatabase::drivers();
}

DatabaseConnector::~DatabaseConnector()
{
    disconnect();
    saveProfiles();
}

// ============ Driver Information ============

QStringList DatabaseConnector::availableDrivers() const
{
    return QSqlDatabase::drivers();
}

QVariantList DatabaseConnector::getDriverInfo() const
{
    QVariantList result;
    
    // Define supported drivers with descriptions
    struct DriverInfo {
        QString name;
        QString description;
        int defaultPort;
    };
    
    QList<DriverInfo> drivers = {
        {"QSQLITE", "SQLite (Embedded)", 0},
        {"QMYSQL", "MySQL / MariaDB", 3306},
        {"QPSQL", "PostgreSQL", 5432},
        {"QODBC", "ODBC (SQL Server, Oracle, etc.)", 1433}
    };
    
    QStringList available = QSqlDatabase::drivers();
    
    for (const auto &driver : drivers) {
        QVariantMap info;
        info["name"] = driver.name;
        info["description"] = driver.description;
        info["defaultPort"] = driver.defaultPort;
        info["available"] = available.contains(driver.name);
        result.append(info);
    }
    
    return result;
}

int DatabaseConnector::getDefaultPort(const QString &driver) const
{
    if (driver == "QMYSQL") return 3306;
    if (driver == "QPSQL") return 5432;
    if (driver == "QODBC") return 1433;
    return 0;
}

// ============ Connection Management ============

void DatabaseConnector::testConnection(const QVariantMap &profile)
{
    QString driver = profile.value("driver", "QSQLITE").toString();
    QString testConnName = m_connectionName + "_test";
    
    // Remove existing test connection
    if (QSqlDatabase::contains(testConnName)) {
        QSqlDatabase::removeDatabase(testConnName);
    }
    
    QSqlDatabase db = QSqlDatabase::addDatabase(driver, testConnName);
    
    if (driver == "QSQLITE") {
        db.setDatabaseName(profile.value("database").toString());
    } else {
        db.setHostName(profile.value("host", "localhost").toString());
        db.setPort(profile.value("port", getDefaultPort(driver)).toInt());
        db.setDatabaseName(profile.value("database").toString());
        db.setUserName(profile.value("username").toString());
        db.setPassword(profile.value("password").toString());
        
        QString options = profile.value("connectionOptions").toString();
        if (!options.isEmpty()) {
            db.setConnectOptions(options);
        }
    }
    
    bool success = db.open();
    QString message;
    
    if (success) {
        message = tr("Connection successful! Server version: %1")
                  .arg(db.driver()->handle().toString());
        db.close();
    } else {
        message = tr("Connection failed: %1").arg(db.lastError().text());
    }
    
    QSqlDatabase::removeDatabase(testConnName);
    
    emit connectionTested(success, message);
}

void DatabaseConnector::connectToDatabase(const QString &profileName)
{
    if (!m_profiles.contains(profileName)) {
        setError(tr("Profile not found: %1").arg(profileName));
        return;
    }
    
    connectWithProfile(m_profiles[profileName]);
}

void DatabaseConnector::connectWithProfile(const QVariantMap &profile)
{
    // Disconnect existing connection
    disconnect();
    
    QString driver = profile.value("driver", "QSQLITE").toString();
    
    // Check if driver is available
    if (!QSqlDatabase::isDriverAvailable(driver)) {
        setError(tr("Database driver not available: %1").arg(driver));
        return;
    }
    
    // Create connection
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    
    m_database = QSqlDatabase::addDatabase(driver, m_connectionName);
    
    if (driver == "QSQLITE") {
        QString dbPath = profile.value("database").toString();
        // Handle :memory: and relative paths
        if (dbPath != ":memory:" && !QDir::isAbsolutePath(dbPath)) {
            dbPath = QDir::currentPath() + "/" + dbPath;
        }
        m_database.setDatabaseName(dbPath);
    } else {
        m_database.setHostName(profile.value("host", "localhost").toString());
        m_database.setPort(profile.value("port", getDefaultPort(driver)).toInt());
        m_database.setDatabaseName(profile.value("database").toString());
        m_database.setUserName(profile.value("username").toString());
        m_database.setPassword(profile.value("password").toString());
        
        QString options = profile.value("connectionOptions").toString();
        if (!options.isEmpty()) {
            m_database.setConnectOptions(options);
        }
    }
    
    if (!m_database.open()) {
        setError(tr("Failed to connect: %1").arg(m_database.lastError().text()));
        return;
    }
    
    m_isConnected = true;
    m_currentProfile = profile.value("name", tr("Unnamed")).toString();
    
    qDebug() << "Connected to database:" << m_currentProfile;
    
    emit connectionChanged();
    
    // Execute default query if specified
    QString defaultQuery = profile.value("defaultQuery").toString();
    if (!defaultQuery.isEmpty()) {
        executeQuery(defaultQuery);
    }
}

void DatabaseConnector::disconnect()
{
    if (m_isQuerying) {
        cancelQuery();
    }
    
    if (m_database.isOpen()) {
        m_database.close();
    }
    
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    
    m_isConnected = false;
    m_currentProfile.clear();
    
    emit connectionChanged();
}

// ============ Query Execution ============

void DatabaseConnector::executeQuery(const QString &sql, int maxRows)
{
    if (!m_isConnected) {
        setError(tr("Not connected to database"));
        return;
    }
    
    if (m_isQuerying) {
        setError(tr("A query is already running"));
        return;
    }
    
    m_isQuerying = true;
    m_cancelRequested = false;
    emit queryingChanged();
    
    QElapsedTimer timer;
    timer.start();
    
    QSqlQuery query(m_database);
    
    // Set forward-only for better performance with large result sets
    query.setForwardOnly(true);
    
    bool success = query.exec(sql);
    
    QueryResult result;
    result.success = success;
    result.executionTimeMs = timer.elapsed();
    
    if (!success) {
        result.errorMessage = query.lastError().text();
        setError(result.errorMessage);
    } else {
        // Get column names
        QSqlRecord record = query.record();
        for (int i = 0; i < record.count(); ++i) {
            result.columns.append(record.fieldName(i));
        }
        
        // Fetch rows
        int rowCount = 0;
        while (query.next()) {
            if (m_cancelRequested) {
                break;
            }
            
            if (maxRows > 0 && rowCount >= maxRows) {
                break;
            }
            
            QVariantMap row;
            for (int i = 0; i < record.count(); ++i) {
                row[result.columns[i]] = query.value(i);
            }
            result.rows.append(row);
            rowCount++;
            
            // Emit progress every 1000 rows
            if (rowCount % 1000 == 0) {
                emit queryProgress(rowCount);
            }
        }
        
        result.rowCount = rowCount;
    }
    
    // Store for export
    m_lastResult = result;
    
    // Add to history
    addToHistory(sql, success, result.executionTimeMs);
    
    m_isQuerying = false;
    emit queryingChanged();
    
    // Convert to QVariantMap for QML
    QVariantMap resultMap;
    resultMap["success"] = result.success;
    resultMap["errorMessage"] = result.errorMessage;
    resultMap["columns"] = result.columns;
    resultMap["rows"] = result.rows;
    resultMap["rowCount"] = result.rowCount;
    resultMap["executionTimeMs"] = result.executionTimeMs;
    
    emit queryCompleted(resultMap);
    
    qDebug() << "Query completed in" << result.executionTimeMs << "ms,"
             << result.rowCount << "rows";
}

void DatabaseConnector::cancelQuery()
{
    QMutexLocker locker(&m_queryMutex);
    m_cancelRequested = true;
}

// ============ Schema Information ============

QStringList DatabaseConnector::getTables()
{
    if (!m_isConnected) {
        return QStringList();
    }
    
    return m_database.tables(QSql::Tables);
}

QVariantList DatabaseConnector::getTableColumns(const QString &tableName)
{
    QVariantList result;
    
    if (!m_isConnected) {
        return result;
    }
    
    QSqlRecord record = m_database.record(tableName);
    
    for (int i = 0; i < record.count(); ++i) {
        QVariantMap column;
        column["name"] = record.fieldName(i);
        column["type"] = QVariant::typeToName(record.field(i).metaType().id());
        column["nullable"] = !record.field(i).requiredStatus();
        result.append(column);
    }
    
    return result;
}

void DatabaseConnector::previewTable(const QString &tableName, int limit)
{
    if (!m_isConnected) {
        setError(tr("Not connected to database"));
        return;
    }
    
    // Sanitize table name to prevent SQL injection
    QString safeTableName = tableName;
    safeTableName.replace("\"", "");
    safeTableName.replace("'", "");
    safeTableName.replace(";", "");
    
    QString sql = QString("SELECT * FROM \"%1\" LIMIT %2").arg(safeTableName).arg(limit);
    
    // Adjust syntax for different databases
    QString driver = m_database.driverName();
    if (driver == "QODBC" || driver.contains("QOCI")) {
        // SQL Server / Oracle syntax
        sql = QString("SELECT TOP %2 * FROM \"%1\"").arg(safeTableName).arg(limit);
    }
    
    executeQuery(sql, limit);
}

// ============ Result Export ============

QStringList DatabaseConnector::resultToLogLines(const QString &timestampColumn,
                                                 const QString &messageColumn,
                                                 const QString &levelColumn)
{
    QStringList lines;
    
    for (const QVariant &rowVar : m_lastResult.rows) {
        QVariantMap row = rowVar.toMap();
        
        QString timestamp = row.value(timestampColumn).toString();
        QString message = row.value(messageColumn).toString();
        QString level = levelColumn.isEmpty() ? "INFO" : row.value(levelColumn, "INFO").toString();
        
        // Format: [TIMESTAMP] [LEVEL] MESSAGE
        QString line = QString("[%1] [%2] %3").arg(timestamp, level, message);
        lines.append(line);
    }
    
    return lines;
}

QString DatabaseConnector::exportResultsToFile(const QString &timestampColumn,
                                                const QString &messageColumn,
                                                const QString &levelColumn)
{
    QStringList lines = resultToLogLines(timestampColumn, messageColumn, levelColumn);
    
    if (lines.isEmpty()) {
        setError(tr("No data to export"));
        return QString();
    }
    
    // Create temp file
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString fileName = QString("db_query_%1.log")
                       .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
    QString filePath = tempDir + "/" + fileName;
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(tr("Failed to create temp file: %1").arg(file.errorString()));
        return QString();
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    for (const QString &line : lines) {
        stream << line << "\n";
    }
    
    file.close();
    
    qDebug() << "Exported" << lines.size() << "log lines to" << filePath;
    
    return filePath;
}

// ============ Profile Management ============

void DatabaseConnector::saveProfile(const QVariantMap &profile)
{
    QString name = profile.value("name").toString();
    if (name.isEmpty()) {
        setError(tr("Profile name cannot be empty"));
        return;
    }
    
    // Don't store plain password - encrypt it
    QVariantMap safeProfile = profile;
    QString password = profile.value("password").toString();
    if (!password.isEmpty()) {
        // Simple XOR encryption (not secure, but better than plain text)
        // In production, use proper encryption
        QByteArray key = "BigFileViewerKey";
        QByteArray encrypted;
        for (int i = 0; i < password.size(); ++i) {
            encrypted.append(password.at(i).unicode() ^ key.at(i % key.size()));
        }
        safeProfile["password"] = encrypted.toBase64();
        safeProfile["passwordEncrypted"] = true;
    }
    
    m_profiles[name] = safeProfile;
    saveProfiles();
    
    emit profilesChanged();
}

void DatabaseConnector::deleteProfile(const QString &name)
{
    if (m_profiles.remove(name) > 0) {
        saveProfiles();
        emit profilesChanged();
    }
}

QVariantList DatabaseConnector::getProfiles() const
{
    QVariantList result;
    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        QVariantMap profile = it.value();
        // Don't expose password
        profile.remove("password");
        profile.remove("passwordEncrypted");
        result.append(profile);
    }
    return result;
}

QVariantMap DatabaseConnector::getProfile(const QString &name) const
{
    return m_profiles.value(name);
}

void DatabaseConnector::loadProfiles()
{
    QSettings settings;
    settings.beginGroup("DatabaseProfiles");
    
    QStringList profiles = settings.childGroups();
    for (const QString &name : profiles) {
        settings.beginGroup(name);
        
        QVariantMap profile;
        profile["name"] = name;
        profile["driver"] = settings.value("driver", "QSQLITE").toString();
        profile["host"] = settings.value("host", "localhost").toString();
        profile["port"] = settings.value("port", 0).toInt();
        profile["database"] = settings.value("database").toString();
        profile["username"] = settings.value("username").toString();
        profile["password"] = settings.value("password").toString();
        profile["passwordEncrypted"] = settings.value("passwordEncrypted", false).toBool();
        profile["connectionOptions"] = settings.value("connectionOptions").toString();
        profile["defaultQuery"] = settings.value("defaultQuery").toString();
        profile["autoConnect"] = settings.value("autoConnect", false).toBool();
        
        m_profiles[name] = profile;
        
        settings.endGroup();
    }
    
    settings.endGroup();
    
    // Load query history
    settings.beginGroup("QueryHistory");
    int count = settings.beginReadArray("queries");
    for (int i = 0; i < count && i < MAX_HISTORY_SIZE; ++i) {
        settings.setArrayIndex(i);
        QVariantMap entry;
        entry["sql"] = settings.value("sql").toString();
        entry["success"] = settings.value("success").toBool();
        entry["executionTimeMs"] = settings.value("executionTimeMs").toLongLong();
        entry["timestamp"] = settings.value("timestamp").toString();
        m_queryHistory.append(entry);
    }
    settings.endArray();
    settings.endGroup();
}

void DatabaseConnector::saveProfiles()
{
    QSettings settings;
    
    // Clear existing profiles
    settings.remove("DatabaseProfiles");
    
    settings.beginGroup("DatabaseProfiles");
    
    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        settings.beginGroup(it.key());
        
        const QVariantMap &profile = it.value();
        settings.setValue("driver", profile.value("driver"));
        settings.setValue("host", profile.value("host"));
        settings.setValue("port", profile.value("port"));
        settings.setValue("database", profile.value("database"));
        settings.setValue("username", profile.value("username"));
        settings.setValue("password", profile.value("password"));
        settings.setValue("passwordEncrypted", profile.value("passwordEncrypted"));
        settings.setValue("connectionOptions", profile.value("connectionOptions"));
        settings.setValue("defaultQuery", profile.value("defaultQuery"));
        settings.setValue("autoConnect", profile.value("autoConnect"));
        
        settings.endGroup();
    }
    
    settings.endGroup();
    
    // Save query history
    settings.beginGroup("QueryHistory");
    settings.beginWriteArray("queries");
    for (int i = 0; i < m_queryHistory.size(); ++i) {
        settings.setArrayIndex(i);
        QVariantMap entry = m_queryHistory[i].toMap();
        settings.setValue("sql", entry["sql"]);
        settings.setValue("success", entry["success"]);
        settings.setValue("executionTimeMs", entry["executionTimeMs"]);
        settings.setValue("timestamp", entry["timestamp"]);
    }
    settings.endArray();
    settings.endGroup();
}

// ============ Query History ============

void DatabaseConnector::addToHistory(const QString &sql, bool success, qint64 executionTimeMs)
{
    QVariantMap entry;
    entry["sql"] = sql;
    entry["success"] = success;
    entry["executionTimeMs"] = executionTimeMs;
    entry["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    m_queryHistory.prepend(entry);
    
    // Trim history
    while (m_queryHistory.size() > MAX_HISTORY_SIZE) {
        m_queryHistory.removeLast();
    }
    
    emit queryHistoryChanged();
}

void DatabaseConnector::clearQueryHistory()
{
    m_queryHistory.clear();
    
    QSettings settings;
    settings.remove("QueryHistory");
    
    emit queryHistoryChanged();
}

// ============ Error Handling ============

void DatabaseConnector::setError(const QString &error)
{
    m_lastError = error;
    qWarning() << "DatabaseConnector error:" << error;
    emit errorOccurred(error);
}
