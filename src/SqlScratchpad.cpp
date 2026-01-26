/**
 * @file SqlScratchpad.cpp
 * @brief SQL Query Analysis Tool Implementation
 */

#include "SqlScratchpad.h"
#include "BigFileModel.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QSqlField>
#include <QSettings>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QElapsedTimer>
#include <QUuid>
#include <QCoreApplication>

SqlScratchpad& SqlScratchpad::instance()
{
    static SqlScratchpad instance;
    return instance;
}

SqlScratchpad::SqlScratchpad(QObject* parent)
    : QObject(parent)
    , m_isReady(false)
    , m_isLoading(false)
    , m_rowCount(0)
    , m_cancelRequested(false)
    , m_maxHistorySize(100)
{
    initDatabase();
    loadSavedQueries();
}

SqlScratchpad::~SqlScratchpad()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void SqlScratchpad::initDatabase()
{
    // Create unique connection name
    QString connectionName = QString("SqlScratchpad_%1").arg(reinterpret_cast<quintptr>(this));
    
    m_db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    m_db.setDatabaseName(":memory:");
    
    if (!m_db.open()) {
        setError(tr("Failed to create in-memory database: %1").arg(m_db.lastError().text()));
        return;
    }
    
    createLogsTable();
    m_isReady = true;
    emit readyChanged();
}

void SqlScratchpad::createLogsTable()
{
    QSqlQuery query(m_db);
    
    // Create main logs table with common columns
    query.exec(R"(
        CREATE TABLE IF NOT EXISTS logs (
            id INTEGER PRIMARY KEY,
            line_number INTEGER,
            timestamp TEXT,
            level TEXT,
            source TEXT,
            thread TEXT,
            message TEXT,
            raw_content TEXT
        )
    )");
    
    // Create indexes for common queries
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_timestamp ON logs(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_level ON logs(level)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_source ON logs(source)");
    
    m_tableColumns = {"id", "line_number", "timestamp", "level", "source", "thread", "message", "raw_content"};
    emit dataChanged();
}

void SqlScratchpad::setLoading(bool loading)
{
    if (m_isLoading != loading) {
        m_isLoading = loading;
        emit loadingChanged();
    }
}

void SqlScratchpad::setError(const QString& error)
{
    m_lastError = error;
    emit errorOccurred(error);
}

void SqlScratchpad::importFromModel(BigFileModel* model, int maxRows)
{
    if (!model || !m_isReady) {
        setError(tr("Invalid model or database not ready"));
        return;
    }
    
    setLoading(true);
    m_cancelRequested = false;
    
    // Clear existing data
    QSqlQuery clearQuery(m_db);
    clearQuery.exec("DELETE FROM logs");
    
    // Begin transaction for faster inserts
    m_db.transaction();
    
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(R"(
        INSERT INTO logs (line_number, timestamp, level, source, thread, message, raw_content)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    
    int totalRows = model->totalLineCount();
    if (maxRows > 0 && maxRows < totalRows) {
        totalRows = maxRows;
    }
    
    int imported = 0;
    const int batchSize = 1000;
    
    for (int i = 0; i < totalRows && !m_cancelRequested; ++i) {
        QString content = model->getRawLine(i);
        
        // Parse log entry (simplified - in production, use LogParser)
        QString timestamp, level, source, thread, message;
        
        // Try to extract timestamp (common patterns)
        QRegularExpression tsRegex(R"((\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:?\d{2})?))");
        auto tsMatch = tsRegex.match(content);
        if (tsMatch.hasMatch()) {
            timestamp = tsMatch.captured(1);
        }
        
        // Try to extract level
        QRegularExpression levelRegex(R"(\b(TRACE|DEBUG|INFO|WARN(?:ING)?|ERROR|FATAL|CRITICAL)\b)", 
                                       QRegularExpression::CaseInsensitiveOption);
        auto levelMatch = levelRegex.match(content);
        if (levelMatch.hasMatch()) {
            level = levelMatch.captured(1).toUpper();
        }
        
        // Try to extract thread
        QRegularExpression threadRegex(R"(\[([^\]]+)\]|Thread-(\d+)|tid[=:](\d+))");
        auto threadMatch = threadRegex.match(content);
        if (threadMatch.hasMatch()) {
            thread = threadMatch.captured(1).isEmpty() ? 
                     (threadMatch.captured(2).isEmpty() ? threadMatch.captured(3) : threadMatch.captured(2)) :
                     threadMatch.captured(1);
        }
        
        // Message is the full content for now
        message = content;
        
        insertQuery.addBindValue(i + 1);  // line_number
        insertQuery.addBindValue(timestamp);
        insertQuery.addBindValue(level);
        insertQuery.addBindValue(source);
        insertQuery.addBindValue(thread);
        insertQuery.addBindValue(message);
        insertQuery.addBindValue(content);
        
        if (!insertQuery.exec()) {
            qWarning() << "Failed to insert row:" << insertQuery.lastError().text();
        }
        
        imported++;
        
        // Progress update every batch
        if (imported % batchSize == 0) {
            emit importProgress(imported, totalRows);
            QCoreApplication::processEvents();  // Keep UI responsive
        }
    }
    
    m_db.commit();
    
    m_rowCount = imported;
    setLoading(false);
    emit dataChanged();
    emit importProgress(imported, totalRows);
}

void SqlScratchpad::importFilteredData(BigFileModel* model)
{
    if (!model || !m_isReady) {
        setError(tr("Invalid model or database not ready"));
        return;
    }
    
    setLoading(true);
    m_cancelRequested = false;
    
    // Clear existing data
    QSqlQuery clearQuery(m_db);
    clearQuery.exec("DELETE FROM logs");
    
    m_db.transaction();
    
    QSqlQuery insertQuery(m_db);
    insertQuery.prepare(R"(
        INSERT INTO logs (line_number, timestamp, level, source, thread, message, raw_content)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    
    int totalRows = model->lineCount();
    int imported = 0;
    const int batchSize = 1000;
    
    for (int i = 0; i < totalRows && !m_cancelRequested; ++i) {
        int actualLine = model->toRealRow(i);
        QString content = model->getRawLine(actualLine);
        
        // Simplified parsing (same as above)
        QString timestamp, level, source, thread, message;
        
        QRegularExpression tsRegex(R"((\d{4}-\d{2}-\d{2}[T ]\d{2}:\d{2}:\d{2}(?:\.\d+)?))");
        auto tsMatch = tsRegex.match(content);
        if (tsMatch.hasMatch()) {
            timestamp = tsMatch.captured(1);
        }
        
        QRegularExpression levelRegex(R"(\b(TRACE|DEBUG|INFO|WARN(?:ING)?|ERROR|FATAL|CRITICAL)\b)", 
                                       QRegularExpression::CaseInsensitiveOption);
        auto levelMatch = levelRegex.match(content);
        if (levelMatch.hasMatch()) {
            level = levelMatch.captured(1).toUpper();
        }
        
        message = content;
        
        insertQuery.addBindValue(actualLine + 1);
        insertQuery.addBindValue(timestamp);
        insertQuery.addBindValue(level);
        insertQuery.addBindValue(source);
        insertQuery.addBindValue(thread);
        insertQuery.addBindValue(message);
        insertQuery.addBindValue(content);
        
        insertQuery.exec();
        imported++;
        
        if (imported % batchSize == 0) {
            emit importProgress(imported, totalRows);
            QCoreApplication::processEvents();
        }
    }
    
    m_db.commit();
    
    m_rowCount = imported;
    setLoading(false);
    emit dataChanged();
}

QString SqlScratchpad::executeQuery(const QString& sql)
{
    if (!m_isReady) {
        setError(tr("Database not ready"));
        return QString();
    }
    
    QString queryId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    m_currentQueryId = queryId;
    m_cancelRequested = false;
    
    setLoading(true);
    
    QElapsedTimer timer;
    timer.start();
    
    QSqlQuery query(m_db);
    
    if (!query.exec(sql)) {
        QString error = query.lastError().text();
        setError(error);
        setLoading(false);
        emit queryFailed(queryId, error);
        return queryId;
    }
    
    // Get column names
    m_resultColumns.clear();
    QSqlRecord record = query.record();
    for (int i = 0; i < record.count(); ++i) {
        m_resultColumns.append(record.fieldName(i));
    }
    
    // Count results (for SELECT queries)
    int resultCount = 0;
    if (query.isSelect()) {
        // Move to last to get count
        if (query.last()) {
            resultCount = query.at() + 1;
        }
        query.first();
        query.previous();  // Reset to before first
    } else {
        resultCount = query.numRowsAffected();
    }
    
    qint64 elapsed = timer.elapsed();
    
    addToHistory(sql);
    setLoading(false);
    
    emit queryCompleted(queryId, resultCount, elapsed);
    
    return queryId;
}

void SqlScratchpad::cancelQuery()
{
    m_cancelRequested = true;
}

QVariantList SqlScratchpad::getResults(int offset, int limit)
{
    QVariantList results;
    
    if (!m_isReady || m_resultColumns.isEmpty()) {
        return results;
    }
    
    // Re-execute with LIMIT and OFFSET
    // This is a simplified approach - in production, cache the query
    QString lastQuery;
    if (!m_queryHistory.isEmpty()) {
        lastQuery = m_queryHistory.first();
    }
    
    if (lastQuery.isEmpty()) {
        return results;
    }
    
    // Add LIMIT/OFFSET if not already present
    QString paginatedQuery = lastQuery;
    if (!paginatedQuery.contains("LIMIT", Qt::CaseInsensitive)) {
        paginatedQuery += QString(" LIMIT %1 OFFSET %2").arg(limit).arg(offset);
    }
    
    QSqlQuery query(m_db);
    if (!query.exec(paginatedQuery)) {
        return results;
    }
    
    while (query.next()) {
        QVariantMap row;
        for (int i = 0; i < m_resultColumns.size(); ++i) {
            row[m_resultColumns[i]] = query.value(i);
        }
        results.append(row);
    }
    
    return results;
}

QStringList SqlScratchpad::getResultColumns()
{
    return m_resultColumns;
}

void SqlScratchpad::exportResults(const QString& filePath, const QString& format)
{
    if (!m_isReady || m_queryHistory.isEmpty()) {
        setError(tr("No results to export"));
        return;
    }
    
    QString lastQuery = m_queryHistory.first();
    QSqlQuery query(m_db);
    
    if (!query.exec(lastQuery)) {
        setError(query.lastError().text());
        return;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(tr("Cannot open file for writing: %1").arg(filePath));
        return;
    }
    
    QTextStream out(&file);
    
    if (format.toLower() == "csv") {
        // CSV header
        out << m_resultColumns.join(",") << "\n";
        
        while (query.next()) {
            QStringList values;
            for (int i = 0; i < m_resultColumns.size(); ++i) {
                QString value = query.value(i).toString();
                // Escape quotes and wrap in quotes if contains comma
                if (value.contains(',') || value.contains('"') || value.contains('\n')) {
                    value.replace("\"", "\"\"");
                    value = "\"" + value + "\"";
                }
                values.append(value);
            }
            out << values.join(",") << "\n";
        }
    }
    else if (format.toLower() == "json") {
        QJsonArray rows;
        
        while (query.next()) {
            QJsonObject row;
            for (int i = 0; i < m_resultColumns.size(); ++i) {
                row[m_resultColumns[i]] = QJsonValue::fromVariant(query.value(i));
            }
            rows.append(row);
        }
        
        QJsonDocument doc(rows);
        out << doc.toJson(QJsonDocument::Indented);
    }
    else if (format.toLower() == "html") {
        out << "<!DOCTYPE html>\n<html>\n<head>\n";
        out << "<style>table { border-collapse: collapse; } th, td { border: 1px solid #ddd; padding: 8px; text-align: left; } th { background-color: #4CAF50; color: white; } tr:nth-child(even) { background-color: #f2f2f2; }</style>\n";
        out << "</head>\n<body>\n<table>\n<tr>\n";
        
        for (const QString& col : m_resultColumns) {
            out << "<th>" << col.toHtmlEscaped() << "</th>\n";
        }
        out << "</tr>\n";
        
        while (query.next()) {
            out << "<tr>\n";
            for (int i = 0; i < m_resultColumns.size(); ++i) {
                out << "<td>" << query.value(i).toString().toHtmlEscaped() << "</td>\n";
            }
            out << "</tr>\n";
        }
        
        out << "</table>\n</body>\n</html>\n";
    }
    
    file.close();
    emit exportCompleted(filePath);
}

void SqlScratchpad::saveQuery(const QString& name, const QString& sql)
{
    m_savedQueries[name] = sql;
    saveSavedQueries();
    emit savedQueriesChanged();
}

void SqlScratchpad::deleteSavedQuery(const QString& name)
{
    m_savedQueries.remove(name);
    saveSavedQueries();
    emit savedQueriesChanged();
}

QString SqlScratchpad::getSavedQuery(const QString& name)
{
    return m_savedQueries.value(name);
}

QStringList SqlScratchpad::savedQueries() const
{
    return m_savedQueries.keys();
}

void SqlScratchpad::clearHistory()
{
    m_queryHistory.clear();
    emit historyChanged();
}

void SqlScratchpad::addToHistory(const QString& sql)
{
    // Remove if already exists
    m_queryHistory.removeAll(sql);
    
    // Add to front
    m_queryHistory.prepend(sql);
    
    // Limit size
    while (m_queryHistory.size() > m_maxHistorySize) {
        m_queryHistory.removeLast();
    }
    
    emit historyChanged();
}

void SqlScratchpad::loadSavedQueries()
{
    QSettings settings;
    settings.beginGroup("SqlScratchpad");
    
    int size = settings.beginReadArray("savedQueries");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString name = settings.value("name").toString();
        QString sql = settings.value("sql").toString();
        m_savedQueries[name] = sql;
    }
    settings.endArray();
    
    // Load history
    m_queryHistory = settings.value("history").toStringList();
    
    settings.endGroup();
}

void SqlScratchpad::saveSavedQueries()
{
    QSettings settings;
    settings.beginGroup("SqlScratchpad");
    
    settings.beginWriteArray("savedQueries");
    int i = 0;
    for (auto it = m_savedQueries.begin(); it != m_savedQueries.end(); ++it, ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", it.key());
        settings.setValue("sql", it.value());
    }
    settings.endArray();
    
    // Save history
    settings.setValue("history", m_queryHistory);
    
    settings.endGroup();
}

QVariantList SqlScratchpad::getTableInfo()
{
    QVariantList info;
    
    QSqlQuery query(m_db);
    query.exec("PRAGMA table_info(logs)");
    
    while (query.next()) {
        QVariantMap col;
        col["cid"] = query.value(0);
        col["name"] = query.value(1);
        col["type"] = query.value(2);
        col["notnull"] = query.value(3);
        col["dflt_value"] = query.value(4);
        col["pk"] = query.value(5);
        info.append(col);
    }
    
    return info;
}

QStringList SqlScratchpad::getSqlKeywords()
{
    return {
        "SELECT", "FROM", "WHERE", "AND", "OR", "NOT", "IN", "LIKE", "BETWEEN",
        "IS", "NULL", "ORDER", "BY", "ASC", "DESC", "LIMIT", "OFFSET",
        "GROUP", "HAVING", "DISTINCT", "AS", "JOIN", "LEFT", "RIGHT", "INNER", "OUTER",
        "ON", "UNION", "ALL", "INSERT", "INTO", "VALUES", "UPDATE", "SET", "DELETE",
        "CREATE", "TABLE", "INDEX", "DROP", "ALTER", "ADD", "COLUMN",
        "CASE", "WHEN", "THEN", "ELSE", "END", "CAST", "COALESCE", "IFNULL"
    };
}

QStringList SqlScratchpad::getSqlFunctions()
{
    return {
        // Aggregate
        "COUNT", "SUM", "AVG", "MIN", "MAX", "GROUP_CONCAT", "TOTAL",
        // String
        "LENGTH", "LOWER", "UPPER", "SUBSTR", "TRIM", "LTRIM", "RTRIM",
        "REPLACE", "INSTR", "PRINTF", "UNICODE", "CHAR", "HEX", "ZEROBLOB",
        // Numeric
        "ABS", "ROUND", "RANDOM", "MAX", "MIN",
        // Date/Time
        "DATE", "TIME", "DATETIME", "JULIANDAY", "STRFTIME",
        // Other
        "TYPEOF", "COALESCE", "IFNULL", "NULLIF", "IIF", "GLOB", "LIKE"
    };
}

void SqlScratchpad::resetDatabase()
{
    QSqlQuery query(m_db);
    query.exec("DELETE FROM logs");
    m_rowCount = 0;
    m_resultColumns.clear();
    emit dataChanged();
}
