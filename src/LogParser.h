/**
 * @file LogParser.h
 * @brief Log Line Parser Engine - Extracts structured fields from raw log lines
 * @description Supports configurable regex patterns and JSON parsing for structured logs
 */

#ifndef LOGPARSER_H
#define LOGPARSER_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QJsonObject>
#include <QCache>
#include <QMutex>

/**
 * @class LogParser
 * @brief Parses raw log lines into structured columns using regex or JSON
 * 
 * Features:
 * - Configurable regex patterns for different log formats
 * - JSON log line support
 * - LRU cache for parsed results (performance optimization)
 * - Thread-safe with mutex protection for cache
 */
class LogParser
{
public:
    /**
     * @brief Singleton instance accessor
     */
    static LogParser& instance();

    // Disable copy
    LogParser(const LogParser&) = delete;
    LogParser& operator=(const LogParser&) = delete;

    // ===================== Configuration =====================

    /**
     * @brief Set the regex pattern for parsing log lines
     * @param pattern Regex pattern with capture groups for each column
     * @return true if pattern is valid
     * 
     * Example pattern:
     *   ^(\[.*?\])\s+(\w+)\s+(\d+)\s+---\s+\[(.*?)\]\s+:\s+(.*)$
     *   Groups: [1]=Time, [2]=Level, [3]=Thread, [4]=Tag, [5]=Message
     */
    bool setPattern(const QString &pattern);

    /**
     * @brief Get the current regex pattern
     */
    QString pattern() const { return m_patternString; }

    /**
     * @brief Set column headers corresponding to capture groups
     * @param headers List of column names
     */
    void setColumnHeaders(const QStringList &headers);

    /**
     * @brief Get column headers
     */
    QStringList columnHeaders() const { return m_columnHeaders; }

    /**
     * @brief Get number of columns
     */
    int columnCount() const { return m_columnHeaders.size(); }

    /**
     * @brief Set JSON keys to extract (for JSON log lines)
     * @param keys List of JSON keys to map to columns
     */
    void setJsonKeys(const QStringList &keys);

    /**
     * @brief Get JSON extraction keys
     */
    QStringList jsonKeys() const { return m_jsonKeys; }

    /**
     * @brief Enable/disable JSON parsing mode
     */
    void setJsonParsingEnabled(bool enabled) { m_jsonParsingEnabled = enabled; }
    bool isJsonParsingEnabled() const { return m_jsonParsingEnabled; }
    
    // ===================== XML Configuration =====================
    
    /**
     * @brief Set XML element paths to extract
     * @param paths List of element paths (e.g., "message", "data/value")
     */
    void setXmlElementPaths(const QStringList &paths);
    QStringList xmlElementPaths() const { return m_xmlElementPaths; }
    
    /**
     * @brief Set XML attribute paths to extract
     * @param paths List of attribute paths (e.g., "event@timestamp", "log@level")
     */
    void setXmlAttributePaths(const QStringList &paths);
    QStringList xmlAttributePaths() const { return m_xmlAttributePaths; }
    
    /**
     * @brief Enable/disable XML parsing mode
     */
    void setXmlParsingEnabled(bool enabled) { m_xmlParsingEnabled = enabled; }
    bool isXmlParsingEnabled() const { return m_xmlParsingEnabled; }
    
    /**
     * @brief Enable Log4j XML specific parsing
     */
    void setLog4jXmlMode(bool enabled) { m_log4jXmlMode = enabled; }
    bool isLog4jXmlMode() const { return m_log4jXmlMode; }
    
    // ===================== DSV Configuration =====================
    
    /**
     * @brief DSV (Delimiter-Separated Values) configuration
     */
    struct DsvConfig {
        QChar delimiter = QLatin1Char(',');     // Field delimiter
        QChar quoteChar = QLatin1Char('"');     // Quote character
        QChar escapeChar = QLatin1Char('\\');   // Escape character
        bool hasHeader = true;                   // First line is header
        int columnCount = 0;                     // Auto-detect if 0
        bool trimFields = true;                  // Trim whitespace from fields
    };
    
    /**
     * @brief Set DSV parsing configuration
     */
    void setDsvConfig(const DsvConfig &config);
    DsvConfig dsvConfig() const { return m_dsvConfig; }
    
    /**
     * @brief Enable/disable DSV parsing mode
     */
    void setDsvParsingEnabled(bool enabled) { m_dsvParsingEnabled = enabled; }
    bool isDsvParsingEnabled() const { return m_dsvParsingEnabled; }
    
    // ===================== Multiline Configuration =====================
    
    /**
     * @brief Multiline merge mode
     */
    enum class MultilineMode {
        None,       // No multiline merging
        Indent,     // Lines starting with whitespace are continuations
        Regex,      // Lines NOT matching start pattern are continuations
        Both        // Either condition marks a new entry
    };
    
    /**
     * @brief Multiline configuration
     */
    struct MultilineConfig {
        MultilineMode mode = MultilineMode::None;
        QString startPattern;           // Regex for line start (Regex/Both mode)
        int minIndent = 1;              // Minimum indent for continuation (Indent mode)
        int maxLinesToMerge = 100;      // Safety limit
        QString lineSeparator = "\\n";  // Separator when displaying merged lines
    };
    
    /**
     * @brief Set multiline configuration
     */
    void setMultilineConfig(const MultilineConfig &config);
    MultilineConfig multilineConfig() const { return m_multilineConfig; }
    
    /**
     * @brief Enable/disable multiline merging
     */
    void setMultilineEnabled(bool enabled);
    bool isMultilineEnabled() const { return m_multilineEnabled; }
    
    /**
     * @brief Check if a line is a continuation of the previous line
     * @param line The line to check
     * @return true if this line should be merged with the previous
     */
    bool isContinuationLine(const QString &line) const;

    // ===================== Parsing =====================

    /**
     * @brief Parse a raw log line into structured fields
     * @param rawLine The raw log line text
     * @return List of field values corresponding to columns
     * @note Lazy parsing - called on-demand from Model::data()
     */
    QStringList parseLine(const QString &rawLine) const;

    /**
     * @brief Parse a log line and cache the result
     * @param lineIndex Unique line identifier for caching
     * @param rawLine The raw log line text
     * @return List of field values corresponding to columns
     */
    QStringList parseLineWithCache(qint64 lineIndex, const QString &rawLine) const;

    /**
     * @brief Get a specific field from a parsed line
     * @param rawLine The raw log line text
     * @param column Column index (0-based)
     * @return The field value, or empty string if not found
     */
    QString getField(const QString &rawLine, int column) const;

    /**
     * @brief Get a specific field using cache
     * @param lineIndex Unique line identifier for caching
     * @param rawLine The raw log line text
     * @param column Column index (0-based)
     * @return The field value, or empty string if not found
     */
    QString getFieldWithCache(qint64 lineIndex, const QString &rawLine, int column) const;

    /**
     * @brief Clear the parse cache
     */
    void clearCache();

    /**
     * @brief Set cache size (number of lines to cache)
     * @param size Maximum number of parsed lines to cache
     */
    void setCacheSize(int size);

    // ===================== Log Level Detection =====================

    /**
     * @brief Log level enumeration for filtering
     */
    enum class LogLevel {
        Unknown = 0,
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Fatal
    };

    /**
     * @brief Detect log level from a raw line
     * @param rawLine The raw log line text
     * @return Detected log level
     */
    LogLevel detectLevel(const QString &rawLine) const;

    /**
     * @brief Convert log level to string
     */
    static QString levelToString(LogLevel level);

    /**
     * @brief Parse log level from string (case-insensitive)
     */
    static LogLevel stringToLevel(const QString &str);

    /**
     * @brief Set which column contains the log level (for fast filtering)
     * @param columnIndex Column index, or -1 to auto-detect
     */
    void setLevelColumn(int columnIndex) { m_levelColumn = columnIndex; }
    int levelColumn() const { return m_levelColumn; }

    // ===================== Presets =====================

    /**
     * @brief Apply a preset log format configuration
     */
    enum class Preset {
        Generic,        // Generic auto-detect format (default)
        SpringBoot,     // Spring Boot default format
        Logback,        // Logback pattern
        Log4j,          // Log4j format
        Syslog,         // Syslog RFC 5424
        ApacheAccess,   // Apache access log
        XmlGeneric,     // Generic XML log format
        Log4jXml,       // Log4j XML layout
        Dsv,            // Delimiter-separated values
        Custom          // User-defined
    };
    
    // ===================== Auto Format Detection =====================
    
    /**
     * @brief Detection confidence level
     */
    struct FormatDetectionResult {
        Preset preset = Preset::Generic;
        int confidence = 0;         // 0-100
        QString formatName;
        QString description;
        QChar dsvDelimiter = QLatin1Char(',');  // For DSV detection
        bool hasHeader = true;                   // For DSV detection
    };
    
    /**
     * @brief Detect log format from sample content
     * @param sampleContent First N bytes/lines of the file
     * @return Detection result with preset and confidence
     * 
     * Detection priority:
     * 1. JSON (starts with { or [)
     * 2. XML (starts with <?xml or <log4j: or <event)
     * 3. Syslog (<\d+> PRI header)
     * 4. DSV (delimiter frequency + column consistency)
     * 5. Known regex patterns (SpringBoot/Logback/Log4j/Apache)
     * 6. Generic text
     */
    static FormatDetectionResult detectFormat(const QString &sampleContent);
    
    /**
     * @brief Auto-configure parser based on sample content
     * @param sampleContent First N bytes/lines of the file
     * @return true if a format was detected and applied
     */
    bool autoDetectAndConfigure(const QString &sampleContent);

    /**
     * @brief Apply a preset log format
     * @param preset The preset to apply
     */
    void applyPreset(Preset preset);

    /**
     * @brief Get current preset name
     */
    QString presetName() const { return m_presetName; }

private:
    LogParser();
    ~LogParser() = default;

    /**
     * @brief Parse JSON log line
     * @param rawLine The raw JSON log line
     * @return Parsed fields, or empty list if not valid JSON
     */
    QStringList parseJsonLine(const QString &rawLine) const;
    
    /**
     * @brief Parse XML log line or event
     * @param rawLine The raw XML log line/event
     * @return Parsed fields from XML elements/attributes
     */
    QStringList parseXmlLine(const QString &rawLine) const;
    
    /**
     * @brief Parse Log4j XML format specifically
     * @param rawLine The raw Log4j XML event
     * @return Parsed fields
     */
    QStringList parseLog4jXmlLine(const QString &rawLine) const;
    
    /**
     * @brief Parse DSV (Delimiter-Separated Values) line
     * @param rawLine The raw DSV line
     * @return Parsed fields
     * @note First 1000 lines use full RFC4180 parsing, rest use fast split
     */
    QStringList parseDsvLine(const QString &rawLine) const;

    /**
     * @brief Parse regex log line
     * @param rawLine The raw log line
     * @return Parsed fields from regex capture groups
     */
    QStringList parseRegexLine(const QString &rawLine) const;

    // Configuration
    QString m_patternString;
    QRegularExpression m_regex;
    QStringList m_columnHeaders;
    QStringList m_jsonKeys;
    bool m_jsonParsingEnabled = true;
    int m_levelColumn = -1;  // Auto-detect
    QString m_presetName = "Custom";
    
    // XML Configuration
    QStringList m_xmlElementPaths;      // Element paths to extract (e.g., "event/message")
    QStringList m_xmlAttributePaths;    // Attribute paths (e.g., "event@timestamp")
    bool m_xmlParsingEnabled = false;
    bool m_log4jXmlMode = false;
    
    // DSV Configuration
    DsvConfig m_dsvConfig;
    bool m_dsvParsingEnabled = false;
    mutable int m_dsvLinesParsed = 0;   // Track lines parsed for fast split optimization
    
    // Multiline Configuration
    MultilineConfig m_multilineConfig;
    bool m_multilineEnabled = false;
    QRegularExpression m_multilineStartRegex;

    // Cache (LRU cache for parsed lines)
    mutable QCache<qint64, QStringList> m_cache;
    mutable QMutex m_cacheMutex;
    int m_cacheSize = 2000;

    // Default column count when no pattern matches
    static constexpr int DEFAULT_COLUMN_COUNT = 1;
};

#endif // LOGPARSER_H
