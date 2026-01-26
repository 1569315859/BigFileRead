/**
 * @file SqlScratchpad.h
 * @brief SQL Query Analysis Tool for BigFileViewer
 * 
 * Allows users to query log data using SQL syntax.
 * Uses embedded SQLite database for query execution.
 */

#ifndef SQLSCRATCHPAD_H
#define SQLSCRATCHPAD_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QSqlDatabase>
#include <QThread>

class BigFileModel;

/**
 * @class SqlScratchpad
 * @brief SQL query interface for log analysis
 * 
 * Features:
 * - Import log data into SQLite memory database
 * - Execute SQL queries with syntax highlighting support
 * - Query history and saved queries
 * - Export query results
 */
class SqlScratchpad : public QObject
{
    Q_OBJECT
    
    Q_PROPERTY(bool isReady READ isReady NOTIFY readyChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY loadingChanged)
    Q_PROPERTY(int rowCount READ rowCount NOTIFY dataChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY errorOccurred)
    Q_PROPERTY(QStringList queryHistory READ queryHistory NOTIFY historyChanged)
    Q_PROPERTY(QStringList savedQueries READ savedQueries NOTIFY savedQueriesChanged)
    Q_PROPERTY(QStringList tableColumns READ tableColumns NOTIFY dataChanged)

public:
    static SqlScratchpad& instance();
    
    // Properties
    bool isReady() const { return m_isReady; }
    bool isLoading() const { return m_isLoading; }
    int rowCount() const { return m_rowCount; }
    QString lastError() const { return m_lastError; }
    QStringList queryHistory() const { return m_queryHistory; }
    QStringList savedQueries() const;
    QStringList tableColumns() const { return m_tableColumns; }
    
    /**
     * @brief Import data from BigFileModel into SQLite
     * @param model The log model to import
     * @param maxRows Maximum rows to import (0 = all)
     */
    Q_INVOKABLE void importFromModel(BigFileModel* model, int maxRows = 0);
    
    /**
     * @brief Import data from current filter results
     * @param model The log model
     */
    Q_INVOKABLE void importFilteredData(BigFileModel* model);
    
    /**
     * @brief Execute SQL query
     * @param sql SQL query string
     * @return Query ID for tracking
     */
    Q_INVOKABLE QString executeQuery(const QString& sql);
    
    /**
     * @brief Cancel running query
     */
    Q_INVOKABLE void cancelQuery();
    
    /**
     * @brief Get query results
     * @param offset Starting row
     * @param limit Maximum rows to return
     * @return List of row objects
     */
    Q_INVOKABLE QVariantList getResults(int offset = 0, int limit = 100);
    
    /**
     * @brief Get column names from last query
     */
    Q_INVOKABLE QStringList getResultColumns();
    
    /**
     * @brief Export results to file
     * @param filePath Output file path
     * @param format Export format (csv, json, html)
     */
    Q_INVOKABLE void exportResults(const QString& filePath, const QString& format);
    
    /**
     * @brief Save a query for later use
     * @param name Query name
     * @param sql SQL query string
     */
    Q_INVOKABLE void saveQuery(const QString& name, const QString& sql);
    
    /**
     * @brief Delete a saved query
     */
    Q_INVOKABLE void deleteSavedQuery(const QString& name);
    
    /**
     * @brief Get a saved query by name
     */
    Q_INVOKABLE QString getSavedQuery(const QString& name);
    
    /**
     * @brief Clear query history
     */
    Q_INVOKABLE void clearHistory();
    
    /**
     * @brief Get table schema information
     */
    Q_INVOKABLE QVariantList getTableInfo();
    
    /**
     * @brief Get SQL keywords for autocomplete
     */
    Q_INVOKABLE QStringList getSqlKeywords();
    
    /**
     * @brief Get SQL functions for autocomplete
     */
    Q_INVOKABLE QStringList getSqlFunctions();
    
    /**
     * @brief Reset database (clear all data)
     */
    Q_INVOKABLE void resetDatabase();

signals:
    void readyChanged();
    void loadingChanged();
    void dataChanged();
    void errorOccurred(const QString& error);
    void historyChanged();
    void savedQueriesChanged();
    
    /**
     * @brief Import progress
     * @param current Current row
     * @param total Total rows
     */
    void importProgress(int current, int total);
    
    /**
     * @brief Query completed
     * @param queryId Query identifier
     * @param rowCount Number of result rows
     * @param elapsed Execution time in milliseconds
     */
    void queryCompleted(const QString& queryId, int rowCount, qint64 elapsed);
    
    /**
     * @brief Query failed
     */
    void queryFailed(const QString& queryId, const QString& error);
    
    /**
     * @brief Export completed
     */
    void exportCompleted(const QString& filePath);

private:
    explicit SqlScratchpad(QObject* parent = nullptr);
    ~SqlScratchpad();
    
    // Disable copy
    SqlScratchpad(const SqlScratchpad&) = delete;
    SqlScratchpad& operator=(const SqlScratchpad&) = delete;
    
    void setLoading(bool loading);
    void setError(const QString& error);
    void initDatabase();
    void createLogsTable();
    void addToHistory(const QString& sql);
    void loadSavedQueries();
    void saveSavedQueries();
    
    // Database
    QSqlDatabase m_db;
    bool m_isReady;
    bool m_isLoading;
    int m_rowCount;
    QString m_lastError;
    
    // Query state
    QString m_currentQueryId;
    bool m_cancelRequested;
    QStringList m_resultColumns;
    
    // History and saved queries
    QStringList m_queryHistory;
    QMap<QString, QString> m_savedQueries;
    int m_maxHistorySize;
    
    // Table info
    QStringList m_tableColumns;
};

#endif // SQLSCRATCHPAD_H
