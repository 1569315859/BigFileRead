/**
 * @file DatabaseConnector.h
 * @brief Database Connection Manager for BigFileViewer
 * 
 * Supports connecting to various databases and querying logs:
 * - SQLite (embedded)
 * - MySQL/MariaDB
 * - PostgreSQL
 * - Microsoft SQL Server (ODBC)
 * - Oracle (ODBC)
 */

#ifndef DATABASECONNECTOR_H
#define DATABASECONNECTOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QTimer>
#include <QThread>
#include <QMutex>

/**
 * @brief Database connection profile
 */
struct DatabaseProfile {
    QString name;           ///< Profile name
    QString driver;         ///< Qt SQL driver: QSQLITE, QMYSQL, QPSQL, QODBC
    QString host;           ///< Database host
    int port;               ///< Database port
    QString database;       ///< Database name or file path (SQLite)
    QString username;       ///< Username
    QString password;       ///< Password (stored encrypted)
    QString connectionOptions; ///< Additional connection options
    QString defaultQuery;   ///< Default query to run on connect
    bool autoConnect;       ///< Auto-connect on startup
};

/**
 * @brief Query result with metadata
 */
struct QueryResult {
    bool success;
    QString errorMessage;
    QStringList columns;
    QVariantList rows;      ///< List of QVariantMap for each row
    int rowCount;
    qint64 executionTimeMs;
};

/**
 * @class DatabaseConnector
 * @brief Manages database connections and log queries
 * 
 * Features:
 * - Multiple database driver support
 * - Connection pooling
 * - Async query execution
 * - Query history
 * - Result export to log format
 */
class DatabaseConnector : public QObject
{
    Q_OBJECT
    
    // ===== QML Properties =====
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY connectionChanged)
    Q_PROPERTY(QString currentProfile READ currentProfile NOTIFY connectionChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(bool isQuerying READ isQuerying NOTIFY queryingChanged)
    Q_PROPERTY(QStringList availableDrivers READ availableDrivers CONSTANT)
    Q_PROPERTY(QVariantList queryHistory READ queryHistory NOTIFY queryHistoryChanged)

public:
    /**
     * @brief Singleton instance accessor
     */
    static DatabaseConnector& instance();
    
    // ===== State Properties =====
    
    bool isConnected() const { return m_isConnected; }
    QString currentProfile() const { return m_currentProfile; }
    QString lastError() const { return m_lastError; }
    bool isQuerying() const { return m_isQuerying; }
    QStringList availableDrivers() const;
    QVariantList queryHistory() const { return m_queryHistory; }
    
    // ===== Q_INVOKABLE Methods for QML =====
    
    /**
     * @brief Get available database drivers
     * @return List of driver info {name, description, available}
     */
    Q_INVOKABLE QVariantList getDriverInfo() const;
    
    /**
     * @brief Test database connection
     * @param profile Connection profile as QVariantMap
     */
    Q_INVOKABLE void testConnection(const QVariantMap &profile);
    
    /**
     * @brief Connect to database
     * @param profileName Name of saved profile to use
     */
    Q_INVOKABLE void connectToDatabase(const QString &profileName);
    
    /**
     * @brief Connect with profile data directly
     * @param profile Connection profile as QVariantMap
     */
    Q_INVOKABLE void connectWithProfile(const QVariantMap &profile);
    
    /**
     * @brief Disconnect from current database
     */
    Q_INVOKABLE void disconnect();
    
    /**
     * @brief Execute a SQL query
     * @param sql SQL query string
     * @param maxRows Maximum rows to return (0 = unlimited)
     */
    Q_INVOKABLE void executeQuery(const QString &sql, int maxRows = 10000);
    
    /**
     * @brief Cancel running query
     */
    Q_INVOKABLE void cancelQuery();
    
    /**
     * @brief Get list of tables in current database
     * @return List of table names
     */
    Q_INVOKABLE QStringList getTables();
    
    /**
     * @brief Get columns of a table
     * @param tableName Table name
     * @return List of column info {name, type, nullable}
     */
    Q_INVOKABLE QVariantList getTableColumns(const QString &tableName);
    
    /**
     * @brief Preview table data
     * @param tableName Table name
     * @param limit Max rows to return
     */
    Q_INVOKABLE void previewTable(const QString &tableName, int limit = 100);
    
    /**
     * @brief Convert query results to log lines
     * @param timestampColumn Column to use as timestamp
     * @param messageColumn Column to use as message
     * @param levelColumn Column to use as log level (optional)
     * @return List of formatted log lines
     */
    Q_INVOKABLE QStringList resultToLogLines(const QString &timestampColumn,
                                              const QString &messageColumn,
                                              const QString &levelColumn = QString());
    
    /**
     * @brief Export query results to temporary file and load
     * @param timestampColumn Column for timestamp
     * @param messageColumn Column for message
     * @param levelColumn Column for level (optional)
     * @return Path to temporary file
     */
    Q_INVOKABLE QString exportResultsToFile(const QString &timestampColumn,
                                             const QString &messageColumn,
                                             const QString &levelColumn = QString());
    
    // ===== Profile Management =====
    
    /**
     * @brief Save a connection profile
     */
    Q_INVOKABLE void saveProfile(const QVariantMap &profile);
    
    /**
     * @brief Delete a connection profile
     */
    Q_INVOKABLE void deleteProfile(const QString &name);
    
    /**
     * @brief Get all saved profiles
     * @return List of profile objects
     */
    Q_INVOKABLE QVariantList getProfiles() const;
    
    /**
     * @brief Get a specific profile
     */
    Q_INVOKABLE QVariantMap getProfile(const QString &name) const;
    
    /**
     * @brief Clear query history
     */
    Q_INVOKABLE void clearQueryHistory();

signals:
    void connectionChanged();
    void errorOccurred(const QString &error);
    void queryingChanged();
    void queryHistoryChanged();
    
    /**
     * @brief Emitted when connection test completes
     */
    void connectionTested(bool success, const QString &message);
    
    /**
     * @brief Emitted when query completes
     * @param result Query result as QVariantMap
     */
    void queryCompleted(const QVariantMap &result);
    
    /**
     * @brief Emitted during long query with progress
     * @param rowsProcessed Number of rows processed so far
     */
    void queryProgress(int rowsProcessed);
    
    /**
     * @brief Emitted when profiles list changes
     */
    void profilesChanged();

private:
    explicit DatabaseConnector(QObject *parent = nullptr);
    ~DatabaseConnector();
    
    // Disable copy
    DatabaseConnector(const DatabaseConnector&) = delete;
    DatabaseConnector& operator=(const DatabaseConnector&) = delete;
    
    /**
     * @brief Set error message
     */
    void setError(const QString &error);
    
    /**
     * @brief Load profiles from settings
     */
    void loadProfiles();
    
    /**
     * @brief Save profiles to settings
     */
    void saveProfiles();
    
    /**
     * @brief Add query to history
     */
    void addToHistory(const QString &sql, bool success, qint64 executionTimeMs);
    
    /**
     * @brief Create connection string for driver
     */
    QString buildConnectionString(const QVariantMap &profile) const;
    
    /**
     * @brief Get default port for driver
     */
    int getDefaultPort(const QString &driver) const;
    
    // Connection state
    bool m_isConnected;
    QString m_currentProfile;
    QString m_lastError;
    bool m_isQuerying;
    
    // Database connection
    QSqlDatabase m_database;
    QString m_connectionName;
    
    // Query state
    QThread *m_queryThread;
    bool m_cancelRequested;
    QMutex m_queryMutex;
    
    // Last query result (for export)
    QueryResult m_lastResult;
    
    // Saved profiles
    QMap<QString, QVariantMap> m_profiles;
    
    // Query history (last 100 queries)
    QVariantList m_queryHistory;
    static const int MAX_HISTORY_SIZE = 100;
};

#endif // DATABASECONNECTOR_H
