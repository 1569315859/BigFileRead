/**
 * @file TextTransformer.cpp
 * @brief Text Transformation Implementation
 */

#include "TextTransformer.h"

#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QDateTime>
#include <QRandomGenerator>
#include <algorithm>

TextTransformer& TextTransformer::instance()
{
    static TextTransformer instance;
    return instance;
}

TextTransformer::TextTransformer(QObject* parent)
    : QObject(parent)
{
}

// === Case Conversion ===

QString TextTransformer::toUpperCase(const QString& text)
{
    return text.toUpper();
}

QString TextTransformer::toLowerCase(const QString& text)
{
    return text.toLower();
}

QString TextTransformer::toTitleCase(const QString& text)
{
    QString result = text.toLower();
    bool capitalizeNext = true;
    
    for (int i = 0; i < result.length(); ++i) {
        if (result[i].isSpace() || result[i] == '-' || result[i] == '_') {
            capitalizeNext = true;
        } else if (capitalizeNext && result[i].isLetter()) {
            result[i] = result[i].toUpper();
            capitalizeNext = false;
        }
    }
    
    return result;
}

QString TextTransformer::toCamelCase(const QString& text)
{
    QString result;
    bool capitalizeNext = false;
    
    for (const QChar& ch : text) {
        if (ch.isSpace() || ch == '-' || ch == '_') {
            capitalizeNext = true;
        } else if (capitalizeNext) {
            result += ch.toUpper();
            capitalizeNext = false;
        } else {
            result += ch.toLower();
        }
    }
    
    return result;
}

QString TextTransformer::toSnakeCase(const QString& text)
{
    QString result;
    
    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];
        
        if (ch.isUpper()) {
            if (i > 0 && !text[i-1].isUpper()) {
                result += '_';
            }
            result += ch.toLower();
        } else if (ch.isSpace() || ch == '-') {
            result += '_';
        } else {
            result += ch;
        }
    }
    
    return result;
}

QString TextTransformer::toKebabCase(const QString& text)
{
    QString result;
    
    for (int i = 0; i < text.length(); ++i) {
        QChar ch = text[i];
        
        if (ch.isUpper()) {
            if (i > 0 && !text[i-1].isUpper()) {
                result += '-';
            }
            result += ch.toLower();
        } else if (ch.isSpace() || ch == '_') {
            result += '-';
        } else {
            result += ch;
        }
    }
    
    return result;
}

// === Encoding/Decoding ===

QString TextTransformer::base64Encode(const QString& text)
{
    return QString::fromUtf8(text.toUtf8().toBase64());
}

QString TextTransformer::base64Decode(const QString& text)
{
    QByteArray decoded = QByteArray::fromBase64(text.toUtf8());
    return QString::fromUtf8(decoded);
}

QString TextTransformer::urlEncode(const QString& text)
{
    return QString::fromUtf8(QUrl::toPercentEncoding(text));
}

QString TextTransformer::urlDecode(const QString& text)
{
    return QUrl::fromPercentEncoding(text.toUtf8());
}

QString TextTransformer::htmlEncode(const QString& text)
{
    QString result = text;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    result.replace("'", "&#39;");
    return result;
}

QString TextTransformer::htmlDecode(const QString& text)
{
    QString result = text;
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&#39;", "'");
    result.replace("&amp;", "&");
    return result;
}

QString TextTransformer::hexEncode(const QString& text)
{
    return QString::fromUtf8(text.toUtf8().toHex());
}

QString TextTransformer::hexDecode(const QString& hexText)
{
    QByteArray decoded = QByteArray::fromHex(hexText.toUtf8());
    return QString::fromUtf8(decoded);
}

// === Format Conversion ===

QString TextTransformer::formatJson(const QString& text, int indentation)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit transformationError(tr("Invalid JSON: %1").arg(error.errorString()));
        return text;
    }
    
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString TextTransformer::minifyJson(const QString& text)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit transformationError(tr("Invalid JSON: %1").arg(error.errorString()));
        return text;
    }
    
    return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
}

QString TextTransformer::formatXml(const QString& text, int indentation)
{
    QXmlStreamReader reader(text);
    QString result;
    QXmlStreamWriter writer(&result);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(indentation);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.hasError()) {
            emit transformationError(tr("Invalid XML: %1").arg(reader.errorString()));
            return text;
        }
        
        if (reader.isStartDocument()) {
            writer.writeStartDocument();
        } else if (reader.isEndDocument()) {
            writer.writeEndDocument();
        } else if (reader.isStartElement()) {
            writer.writeStartElement(reader.namespaceUri().toString(), reader.name().toString());
            writer.writeAttributes(reader.attributes());
        } else if (reader.isEndElement()) {
            writer.writeEndElement();
        } else if (reader.isCharacters() && !reader.isWhitespace()) {
            writer.writeCharacters(reader.text().toString());
        } else if (reader.isCDATA()) {
            writer.writeCDATA(reader.text().toString());
        } else if (reader.isComment()) {
            writer.writeComment(reader.text().toString());
        }
    }
    
    return result;
}

QString TextTransformer::minifyXml(const QString& text)
{
    QString result;
    QXmlStreamReader reader(text);
    QXmlStreamWriter writer(&result);
    writer.setAutoFormatting(false);
    
    while (!reader.atEnd()) {
        reader.readNext();
        
        if (reader.hasError()) {
            emit transformationError(tr("Invalid XML: %1").arg(reader.errorString()));
            return text;
        }
        
        if (reader.isStartDocument()) {
            writer.writeStartDocument();
        } else if (reader.isEndDocument()) {
            writer.writeEndDocument();
        } else if (reader.isStartElement()) {
            writer.writeStartElement(reader.namespaceUri().toString(), reader.name().toString());
            writer.writeAttributes(reader.attributes());
        } else if (reader.isEndElement()) {
            writer.writeEndElement();
        } else if (reader.isCharacters() && !reader.isWhitespace()) {
            writer.writeCharacters(reader.text().toString());
        }
    }
    
    return result;
}

// === Regex Operations ===

QString TextTransformer::regexReplace(const QString& text, 
                                       const QString& pattern, 
                                       const QString& replacement)
{
    QRegularExpression regex(pattern);
    if (!regex.isValid()) {
        emit transformationError(tr("Invalid regex: %1").arg(regex.errorString()));
        return text;
    }
    
    return QString(text).replace(regex, replacement);
}

QStringList TextTransformer::regexExtract(const QString& text, 
                                           const QString& pattern,
                                           int captureGroup)
{
    QStringList results;
    QRegularExpression regex(pattern);
    
    if (!regex.isValid()) {
        emit transformationError(tr("Invalid regex: %1").arg(regex.errorString()));
        return results;
    }
    
    auto it = regex.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        if (captureGroup >= 0 && captureGroup <= match.lastCapturedIndex()) {
            results << match.captured(captureGroup);
        }
    }
    
    return results;
}

QStringList TextTransformer::regexSplit(const QString& text, const QString& pattern)
{
    QRegularExpression regex(pattern);
    if (!regex.isValid()) {
        emit transformationError(tr("Invalid regex: %1").arg(regex.errorString()));
        return QStringList() << text;
    }
    
    return text.split(regex);
}

// === Timestamp Conversion ===

QString TextTransformer::timestampToDateTime(qint64 timestamp, const QString& format)
{
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
    return dt.toString(format);
}

qint64 TextTransformer::dateTimeToTimestamp(const QString& dateTime, const QString& format)
{
    QDateTime dt = QDateTime::fromString(dateTime, format);
    return dt.toMSecsSinceEpoch();
}

QString TextTransformer::convertTimestampFormat(const QString& text,
                                                 const QString& fromFormat,
                                                 const QString& toFormat)
{
    QString result = text;
    
    // Build regex to match timestamps in fromFormat
    QString regexPattern = QRegularExpression::escape(fromFormat);
    regexPattern.replace("yyyy", R"(\d{4})");
    regexPattern.replace("MM", R"(\d{2})");
    regexPattern.replace("dd", R"(\d{2})");
    regexPattern.replace("HH", R"(\d{2})");
    regexPattern.replace("mm", R"(\d{2})");
    regexPattern.replace("ss", R"(\d{2})");
    regexPattern.replace("zzz", R"(\d{3})");
    
    QRegularExpression regex(regexPattern);
    auto it = regex.globalMatch(text);
    
    QList<QPair<int, QString>> replacements;
    while (it.hasNext()) {
        auto match = it.next();
        QDateTime dt = QDateTime::fromString(match.captured(), fromFormat);
        if (dt.isValid()) {
            replacements.append({match.capturedStart(), dt.toString(toFormat)});
        }
    }
    
    // Apply replacements in reverse order to preserve positions
    for (int i = replacements.size() - 1; i >= 0; --i) {
        auto match = regex.match(result, replacements[i].first);
        if (match.hasMatch()) {
            result.replace(match.capturedStart(), match.capturedLength(), replacements[i].second);
        }
    }
    
    return result;
}

// === Line Operations ===

QString TextTransformer::sortLines(const QString& text, bool ascending, bool caseSensitive)
{
    QStringList lines = text.split('\n');
    
    if (caseSensitive) {
        std::sort(lines.begin(), lines.end());
    } else {
        std::sort(lines.begin(), lines.end(), [](const QString& a, const QString& b) {
            return a.toLower() < b.toLower();
        });
    }
    
    if (!ascending) {
        std::reverse(lines.begin(), lines.end());
    }
    
    return lines.join('\n');
}

QString TextTransformer::uniqueLines(const QString& text, bool caseSensitive)
{
    QStringList lines = text.split('\n');
    QStringList result;
    QSet<QString> seen;
    
    for (const QString& line : lines) {
        QString key = caseSensitive ? line : line.toLower();
        if (!seen.contains(key)) {
            seen.insert(key);
            result << line;
        }
    }
    
    return result.join('\n');
}

QString TextTransformer::reverseLines(const QString& text)
{
    QStringList lines = text.split('\n');
    std::reverse(lines.begin(), lines.end());
    return lines.join('\n');
}

QString TextTransformer::shuffleLines(const QString& text)
{
    QStringList lines = text.split('\n');
    
    for (int i = lines.size() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        lines.swapItemsAt(i, j);
    }
    
    return lines.join('\n');
}

QString TextTransformer::numberLines(const QString& text, int startFrom)
{
    QStringList lines = text.split('\n');
    QStringList result;
    
    int width = QString::number(startFrom + lines.size() - 1).length();
    
    for (int i = 0; i < lines.size(); ++i) {
        result << QString("%1: %2").arg(startFrom + i, width).arg(lines[i]);
    }
    
    return result.join('\n');
}

QString TextTransformer::trimLines(const QString& text)
{
    QStringList lines = text.split('\n');
    QStringList result;
    
    for (const QString& line : lines) {
        result << line.trimmed();
    }
    
    return result.join('\n');
}

QString TextTransformer::removeEmptyLines(const QString& text)
{
    QStringList lines = text.split('\n');
    QStringList result;
    
    for (const QString& line : lines) {
        if (!line.trimmed().isEmpty()) {
            result << line;
        }
    }
    
    return result.join('\n');
}

QString TextTransformer::removeDuplicateLines(const QString& text)
{
    return uniqueLines(text, true);
}

// === Text Statistics ===

QVariantMap TextTransformer::getStatistics(const QString& text)
{
    QVariantMap stats;
    
    stats["characters"] = text.length();
    stats["charactersNoSpaces"] = text.count(QRegularExpression(R"(\S)"));
    
    QStringList words = text.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    stats["words"] = words.size();
    
    QStringList lines = text.split('\n');
    stats["lines"] = lines.size();
    stats["nonEmptyLines"] = std::count_if(lines.begin(), lines.end(), 
                                            [](const QString& line) { return !line.trimmed().isEmpty(); });
    
    stats["paragraphs"] = text.split(QRegularExpression(R"(\n\s*\n)"), Qt::SkipEmptyParts).size();
    
    // Character frequency
    QMap<QChar, int> charFreq;
    for (const QChar& ch : text) {
        charFreq[ch]++;
    }
    
    // Top 10 characters
    QList<QPair<QChar, int>> sortedChars;
    for (auto it = charFreq.begin(); it != charFreq.end(); ++it) {
        sortedChars.append({it.key(), it.value()});
    }
    std::sort(sortedChars.begin(), sortedChars.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    QVariantList topChars;
    for (int i = 0; i < std::min(10, static_cast<int>(sortedChars.size())); ++i) {
        QVariantMap charInfo;
        charInfo["char"] = QString(sortedChars[i].first);
        charInfo["count"] = sortedChars[i].second;
        topChars.append(charInfo);
    }
    stats["topCharacters"] = topChars;
    
    return stats;
}

// === Escape/Unescape ===

QString TextTransformer::escapeJson(const QString& text)
{
    QString result;
    for (const QChar& ch : text) {
        switch (ch.unicode()) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (ch.unicode() < 0x20) {
                    result += QString("\\u%1").arg(static_cast<int>(ch.unicode()), 4, 16, QChar('0'));
                } else {
                    result += ch;
                }
        }
    }
    return result;
}

QString TextTransformer::unescapeJson(const QString& text)
{
    QString result;
    int i = 0;
    
    while (i < text.length()) {
        if (text[i] == '\\' && i + 1 < text.length()) {
            QChar next = text[i + 1];
            switch (next.unicode()) {
                case '"': result += '"'; i += 2; break;
                case '\\': result += '\\'; i += 2; break;
                case '/': result += '/'; i += 2; break;
                case 'b': result += '\b'; i += 2; break;
                case 'f': result += '\f'; i += 2; break;
                case 'n': result += '\n'; i += 2; break;
                case 'r': result += '\r'; i += 2; break;
                case 't': result += '\t'; i += 2; break;
                case 'u':
                    if (i + 5 < text.length()) {
                        bool ok;
                        int codePoint = text.mid(i + 2, 4).toInt(&ok, 16);
                        if (ok) {
                            result += QChar(codePoint);
                            i += 6;
                            break;
                        }
                    }
                    result += text[i++];
                    break;
                default:
                    result += text[i++];
            }
        } else {
            result += text[i++];
        }
    }
    
    return result;
}

QString TextTransformer::escapeRegex(const QString& text)
{
    return QRegularExpression::escape(text);
}

QString TextTransformer::escapeSql(const QString& text)
{
    QString result = text;
    result.replace("'", "''");
    return result;
}

// === Batch Transformation ===

QString TextTransformer::applyTransformations(const QString& text, 
                                               const QStringList& transformations)
{
    QString result = text;
    
    for (const QString& transform : transformations) {
        if (transform == "uppercase") result = toUpperCase(result);
        else if (transform == "lowercase") result = toLowerCase(result);
        else if (transform == "titlecase") result = toTitleCase(result);
        else if (transform == "camelcase") result = toCamelCase(result);
        else if (transform == "snakecase") result = toSnakeCase(result);
        else if (transform == "kebabcase") result = toKebabCase(result);
        else if (transform == "base64encode") result = base64Encode(result);
        else if (transform == "base64decode") result = base64Decode(result);
        else if (transform == "urlencode") result = urlEncode(result);
        else if (transform == "urldecode") result = urlDecode(result);
        else if (transform == "htmlencode") result = htmlEncode(result);
        else if (transform == "htmldecode") result = htmlDecode(result);
        else if (transform == "hexencode") result = hexEncode(result);
        else if (transform == "hexdecode") result = hexDecode(result);
        else if (transform == "formatjson") result = formatJson(result);
        else if (transform == "minifyjson") result = minifyJson(result);
        else if (transform == "formatxml") result = formatXml(result);
        else if (transform == "minifyxml") result = minifyXml(result);
        else if (transform == "sortlines") result = sortLines(result);
        else if (transform == "uniquelines") result = uniqueLines(result);
        else if (transform == "reverselines") result = reverseLines(result);
        else if (transform == "trimlines") result = trimLines(result);
        else if (transform == "removeemptylines") result = removeEmptyLines(result);
        else if (transform == "escapejson") result = escapeJson(result);
        else if (transform == "unescapejson") result = unescapeJson(result);
        else if (transform == "escaperegex") result = escapeRegex(result);
        else if (transform == "escapesql") result = escapeSql(result);
    }
    
    return result;
}

QStringList TextTransformer::getAvailableTransformations()
{
    return {
        "uppercase", "lowercase", "titlecase", "camelcase", "snakecase", "kebabcase",
        "base64encode", "base64decode", "urlencode", "urldecode",
        "htmlencode", "htmldecode", "hexencode", "hexdecode",
        "formatjson", "minifyjson", "formatxml", "minifyxml",
        "sortlines", "uniquelines", "reverselines", "trimlines", "removeemptylines",
        "escapejson", "unescapejson", "escaperegex", "escapesql"
    };
}

QVariantMap TextTransformer::getTransformationCategories()
{
    QVariantMap categories;
    
    categories["Case Conversion"] = QStringList{
        "uppercase", "lowercase", "titlecase", "camelcase", "snakecase", "kebabcase"
    };
    
    categories["Encoding"] = QStringList{
        "base64encode", "base64decode", "urlencode", "urldecode",
        "htmlencode", "htmldecode", "hexencode", "hexdecode"
    };
    
    categories["Format"] = QStringList{
        "formatjson", "minifyjson", "formatxml", "minifyxml"
    };
    
    categories["Lines"] = QStringList{
        "sortlines", "uniquelines", "reverselines", "trimlines", "removeemptylines"
    };
    
    categories["Escape"] = QStringList{
        "escapejson", "unescapejson", "escaperegex", "escapesql"
    };
    
    return categories;
}
