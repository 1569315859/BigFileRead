/**
 * @file TextTransformer.h
 * @brief Text Transformation Utilities for BigFileViewer
 * 
 * Provides various text transformation operations:
 * - Case conversion
 * - Encoding/Decoding (Base64, URL, HTML)
 * - Format conversion (JSON, XML formatting)
 * - Regex-based transformations
 * - Timestamp conversion
 */

#ifndef TEXTTRANSFORMER_H
#define TEXTTRANSFORMER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

/**
 * @class TextTransformer
 * @brief Singleton class for text transformation operations
 */
class TextTransformer : public QObject
{
    Q_OBJECT
    
public:
    static TextTransformer& instance();
    
    // === Case Conversion ===
    
    Q_INVOKABLE QString toUpperCase(const QString& text);
    Q_INVOKABLE QString toLowerCase(const QString& text);
    Q_INVOKABLE QString toTitleCase(const QString& text);
    Q_INVOKABLE QString toCamelCase(const QString& text);
    Q_INVOKABLE QString toSnakeCase(const QString& text);
    Q_INVOKABLE QString toKebabCase(const QString& text);
    
    // === Encoding/Decoding ===
    
    Q_INVOKABLE QString base64Encode(const QString& text);
    Q_INVOKABLE QString base64Decode(const QString& text);
    
    Q_INVOKABLE QString urlEncode(const QString& text);
    Q_INVOKABLE QString urlDecode(const QString& text);
    
    Q_INVOKABLE QString htmlEncode(const QString& text);
    Q_INVOKABLE QString htmlDecode(const QString& text);
    
    Q_INVOKABLE QString hexEncode(const QString& text);
    Q_INVOKABLE QString hexDecode(const QString& hexText);
    
    // === Format Conversion ===
    
    Q_INVOKABLE QString formatJson(const QString& text, int indentation = 4);
    Q_INVOKABLE QString minifyJson(const QString& text);
    
    Q_INVOKABLE QString formatXml(const QString& text, int indentation = 2);
    Q_INVOKABLE QString minifyXml(const QString& text);
    
    // === Regex Operations ===
    
    Q_INVOKABLE QString regexReplace(const QString& text, 
                                      const QString& pattern, 
                                      const QString& replacement);
    
    Q_INVOKABLE QStringList regexExtract(const QString& text, 
                                          const QString& pattern,
                                          int captureGroup = 0);
    
    Q_INVOKABLE QStringList regexSplit(const QString& text, 
                                         const QString& pattern);
    
    // === Timestamp Conversion ===
    
    Q_INVOKABLE QString timestampToDateTime(qint64 timestamp, const QString& format = "yyyy-MM-dd HH:mm:ss");
    Q_INVOKABLE qint64 dateTimeToTimestamp(const QString& dateTime, const QString& format = "yyyy-MM-dd HH:mm:ss");
    
    Q_INVOKABLE QString convertTimestampFormat(const QString& text,
                                                const QString& fromFormat,
                                                const QString& toFormat);
    
    // === Line Operations ===
    
    Q_INVOKABLE QString sortLines(const QString& text, bool ascending = true, bool caseSensitive = true);
    Q_INVOKABLE QString uniqueLines(const QString& text, bool caseSensitive = true);
    Q_INVOKABLE QString reverseLines(const QString& text);
    Q_INVOKABLE QString shuffleLines(const QString& text);
    Q_INVOKABLE QString numberLines(const QString& text, int startFrom = 1);
    Q_INVOKABLE QString trimLines(const QString& text);
    Q_INVOKABLE QString removeEmptyLines(const QString& text);
    Q_INVOKABLE QString removeDuplicateLines(const QString& text);
    
    // === Text Statistics ===
    
    Q_INVOKABLE QVariantMap getStatistics(const QString& text);
    
    // === Escape/Unescape ===
    
    Q_INVOKABLE QString escapeJson(const QString& text);
    Q_INVOKABLE QString unescapeJson(const QString& text);
    
    Q_INVOKABLE QString escapeRegex(const QString& text);
    
    Q_INVOKABLE QString escapeSql(const QString& text);
    
    // === Batch Transformation ===
    
    /**
     * @brief Apply multiple transformations in sequence
     * @param text Input text
     * @param transformations List of transformation names
     * @return Transformed text
     */
    Q_INVOKABLE QString applyTransformations(const QString& text, 
                                              const QStringList& transformations);
    
    /**
     * @brief Get list of available transformations
     */
    Q_INVOKABLE QStringList getAvailableTransformations();
    
    /**
     * @brief Get transformation categories
     */
    Q_INVOKABLE QVariantMap getTransformationCategories();

signals:
    void transformationError(const QString& error);

private:
    explicit TextTransformer(QObject* parent = nullptr);
    ~TextTransformer() = default;
    
    TextTransformer(const TextTransformer&) = delete;
    TextTransformer& operator=(const TextTransformer&) = delete;
};

#endif // TEXTTRANSFORMER_H
