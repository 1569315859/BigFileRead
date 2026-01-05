/**
 * @file CompactLineDelegate.h
 * @brief 紧凑行委托 - 支持搜索高亮和语法高亮（正则表达式）
 * @version 2.3 - 增强正则高亮与视觉优先级
 */

#ifndef COMPACTLINEDELEGATE_H
#define COMPACTLINEDELEGATE_H

#include <QStyledItemDelegate>
#include <QFontMetrics>
#include <QPainter>
#include <QApplication>
#include <QRegularExpression>

/**
 * @brief 语法高亮规则（支持正则表达式）
 * @note 使用 QRegularExpression 进行模式匹配，默认大小写不敏感
 */
struct HighlightRule {
    QRegularExpression pattern;  ///< 正则表达式模式
    QColor color;                ///< 高亮颜色（前景色）
    bool isActive = true;        ///< 是否启用
    QString name;                ///< 规则名称（用于 UI 显示）
    
    HighlightRule() = default;
    
    /**
     * @brief 构造高亮规则
     * @param regexPattern 正则表达式字符串（自动添加 CaseInsensitiveOption）
     * @param foregroundColor 文本前景色
     * @param ruleName 规则名称
     */
    HighlightRule(const QString &regexPattern, const QColor &foregroundColor, const QString &ruleName = QString())
        : pattern(regexPattern, QRegularExpression::CaseInsensitiveOption)
        , color(foregroundColor)
        , isActive(true)
        , name(ruleName)
    {}
};

/**
 * @brief 紧凑行委托类
 * @description 固定行高和宽度，避免 Qt 为每行计算尺寸导致的性能问题
 *              同时确保水平滚动条正常显示，并支持搜索词高亮及语法高亮
 */
class CompactLineDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// 行的垂直内边距（上下各 4 像素，防止最后一行被裁剪）
    static constexpr int VERTICAL_PADDING = 4;
    /// 固定宽度（确保水平滚动条出现）
    static constexpr int FIXED_WIDTH = 8000;

    explicit CompactLineDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_rowHeight(0)
    {
        // 初始化默认语法高亮规则（日志关键词）
        initDefaultRules();
    }

    /**
     * @brief 初始化默认高亮规则（日志级别关键词）
     * @note 构造函数自动添加 CaseInsensitiveOption，无需使用 (?i)
     */
    void initDefaultRules()
    {
        m_highlightRules.clear();
        
        // 错误类（红色）- 匹配 error, Error, ERROR, fatal, fail 等
        // 注意：不需要 (?i)，因为 HighlightRule 构造函数自动添加 CaseInsensitiveOption
        m_highlightRules.append(HighlightRule(
            "error|fatal|fail|critical|exception|错误|失败|异常",
            QColor("#ff5555"),  // Bright Red (Dracula theme)
            "Error"
        ));
        
        // 警告类（橙黄色）- 匹配 warning, warn, alert 等
        m_highlightRules.append(HighlightRule(
            "warn|warning|alert|caution|警告|注意",
            QColor("#ffb86c"),  // Orange (Dracula theme)
            "Warning"
        ));
        
        // 信息类（浅绿色）- 匹配 info, notice, success 等
        m_highlightRules.append(HighlightRule(
            "info|notice|success|信息|成功",
            QColor("#50fa7b"),  // Green (Dracula theme)
            "Info"
        ));
        
        // 调试类（灰色）- 匹配 debug, trace, verbose 等
        m_highlightRules.append(HighlightRule(
            "debug|trace|verbose|调试",
            QColor("#6272a4"),  // Comment color (Dracula theme)
            "Debug"
        ));
    }

    /**
     * @brief 设置高亮规则列表
     * @param rules 规则列表
     */
    void setHighlightRules(const QList<HighlightRule> &rules)
    {
        m_highlightRules = rules;
    }

    /**
     * @brief 获取当前高亮规则
     */
    const QList<HighlightRule> &highlightRules() const
    {
        return m_highlightRules;
    }

    /**
     * @brief 添加语法高亮规则（字符串模式）
     * @param pattern 正则表达式模式字符串
     * @param color 高亮颜色
     * @param name 规则名称
     */
    void addHighlightRule(const QString &pattern, const QColor &color, const QString &name = QString())
    {
        m_highlightRules.append(HighlightRule(pattern, color, name));
    }

    /**
     * @brief 添加语法高亮规则（QRegularExpression 对象）
     * @param regex 已配置的正则表达式
     * @param color 高亮颜色
     * @param name 规则名称
     */
    void addRule(const QRegularExpression &regex, const QColor &color, const QString &name = QString())
    {
        HighlightRule rule;
        rule.pattern = regex;
        rule.color = color;
        rule.name = name;
        rule.isActive = true;
        m_highlightRules.append(rule);
    }

    /**
     * @brief 计算行号槽宽度
     * @param fm 字体度量
     * @param totalRows 总行数
     * @return 行号槽宽度（像素）
     */
    int calculateGutterWidth(const QFontMetrics &fm, int totalRows) const
    {
        // 计算需要的位数
        int digitCount = 1;
        int temp = totalRows;
        while (temp >= 10) {
            temp /= 10;
            digitCount++;
        }
        // 至少显示 4 位数
        digitCount = qMax(digitCount, 4);
        
        // 宽度 = 数字宽度 * 位数 + 左右内边距
        return fm.horizontalAdvance('9') * digitCount + GUTTER_PADDING * 2;
    }
    
    /// 行号槽内边距
    static constexpr int GUTTER_PADDING = 8;

    /**
     * @brief 设置固定行高
     * @param height 行高（像素）
     */
    void setRowHeight(int height)
    {
        m_rowHeight = height;
    }

    /**
     * @brief 根据字体自动计算行高
     * @param font 使用的字体
     */
    void setRowHeight(const QFont &font)
    {
        QFontMetrics fm(font);
        // 正确的行高：字体高度 + 上下内边距
        m_rowHeight = fm.height() + VERTICAL_PADDING * 2;
    }

    /**
     * @brief 返回固定的 sizeHint
     */
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(index)

        int height = m_rowHeight;
        if (height <= 0) {
            QFontMetrics fm(option.font);
            height = fm.height() + VERTICAL_PADDING * 2;
        }

        return QSize(FIXED_WIDTH, height);
    }

    /**
     * @brief 自定义绘制（含行号、搜索高亮和语法高亮）
     * 
     * 视觉优先级（从高到低）：
     * 1. 选中状态 - 蓝色背景 + 白色文本
     * 2. 搜索匹配 - 橙色/黄色背景高亮
     * 3. 语法高亮 - 根据规则着色文本
     */
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QString text = index.data(Qt::DisplayRole).toString();
        painter->save();

        // 获取总行数用于计算行号槽宽度
        int totalRows = index.model() ? index.model()->rowCount() : 1;
        QFontMetrics fm(option.font);
        int gutterWidth = calculateGutterWidth(fm, totalRows);

        // === 绘制行号槽背景 ===
        QRect gutterRect(option.rect.left(), option.rect.top(), gutterWidth, option.rect.height());
        QColor gutterBgColor("#252526");  // 比主背景稍亮
        painter->fillRect(gutterRect, gutterBgColor);
        
        // === 绘制书签图标 ===
        // 使用 Qt::UserRole + 5 (BookmarkRole) 获取书签状态
        bool isBookmarked = index.data(Qt::UserRole + 5).toBool();
        if (isBookmarked) {
            // 在行号槽左侧绘制蓝色圆形书签标记
            int iconSize = 10;  // 圆形直径
            int iconX = option.rect.left() + 4;  // 左边距
            int iconY = option.rect.top() + (option.rect.height() - iconSize) / 2;  // 垂直居中
            
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(QColor("#3794ff"));  // VS Code 蓝色
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(iconX, iconY, iconSize, iconSize);
            painter->restore();
        }
        
        // === 绘制行号 ===
        QString lineNumber = QString::number(index.row() + 1);
        QColor lineNumberColor("#858585");  // 暗灰色
        painter->setPen(lineNumberColor);
        painter->setFont(option.font);
        
        // 行号右对齐，留出右边距
        QRect lineNumTextRect = gutterRect.adjusted(0, 0, -GUTTER_PADDING, 0);
        
        // 使用基线绘制确保垂直居中
        int textY = lineNumTextRect.top() + (lineNumTextRect.height() - fm.height()) / 2 + fm.ascent();
        int textX = lineNumTextRect.right() - fm.horizontalAdvance(lineNumber);
        painter->drawText(textX, textY, lineNumber);

        // === 绘制内容区背景（选中/悬停状态）===
        QRect contentRect = option.rect;
        contentRect.setLeft(option.rect.left() + gutterWidth);
        
        // 创建修改后的 option 用于内容区域
        QStyleOptionViewItem contentOption = option;
        contentOption.rect = contentRect;
        
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &contentOption, painter, option.widget);

        // 检测是否处于选中状态
        bool isSelected = (option.state & QStyle::State_Selected);
        
        // 内容区域起始 X 坐标（行号槽之后）
        int contentLeft = option.rect.left() + gutterWidth + 4;

        // === 搜索高亮绘制（在文本之前，仅在非选中状态绘制）===
        // 存储搜索匹配区域，用于后续文本颜色处理
        QVector<QPair<int, int>> searchMatchRanges;  // (startPos, length)
        
        if (!m_highlightText.isEmpty() && !text.isEmpty()) {
            // 普通匹配颜色 - 半透明黄色
            QColor normalMatchColor(234, 92, 0, 100);
            // 活动匹配颜色 - 不透明橙色
            QColor activeMatchColor(255, 152, 0);
            
            int searchPos = 0;
            int matchCounter = 0;
            
            // 支持正则搜索高亮
            if (m_useRegexSearch && m_searchRegex.isValid()) {
                QRegularExpressionMatchIterator it = m_searchRegex.globalMatch(text);
                while (it.hasNext()) {
                    QRegularExpressionMatch match = it.next();
                    int matchStart = match.capturedStart();
                    int matchLen = match.capturedLength();
                    
                    searchMatchRanges.append({matchStart, matchLen});
                    
                    // 仅在非选中状态绘制背景
                    if (!isSelected) {
                        QString beforeMatch = text.left(matchStart);
                        QString matchText = text.mid(matchStart, matchLen);
                        
                        int startX = fm.horizontalAdvance(beforeMatch);
                        int matchWidth = fm.horizontalAdvance(matchText);
                        
                        QRect highlightRect(contentLeft + startX, option.rect.top(),
                                            matchWidth, option.rect.height());
                        
                        QColor color = normalMatchColor;
                        if (index.row() == m_activeRow && matchCounter == m_activeMatchIndex) {
                            color = activeMatchColor;
                        }
                        painter->fillRect(highlightRect, color);
                    }
                    matchCounter++;
                }
            } else {
                // 普通字符串搜索高亮
                const int matchLength = m_highlightText.length();
                
                while (searchPos < text.length()) {
                    int matchIndex = text.indexOf(m_highlightText, searchPos, m_caseSensitivity);
                    if (matchIndex == -1) break;
                    
                    searchMatchRanges.append({matchIndex, matchLength});
                    
                    if (!isSelected) {
                        QString beforeMatch = text.left(matchIndex);
                        QString matchText = text.mid(matchIndex, matchLength);
                        
                        int startX = fm.horizontalAdvance(beforeMatch);
                        int matchWidth = fm.horizontalAdvance(matchText);
                        
                        QRect highlightRect(contentLeft + startX, option.rect.top(),
                                            matchWidth, option.rect.height());
                        
                        QColor color = normalMatchColor;
                        if (index.row() == m_activeRow && matchCounter == m_activeMatchIndex) {
                            color = activeMatchColor;
                        }
                        painter->fillRect(highlightRect, color);
                    }
                    
                    searchPos = matchIndex + matchLength;
                    matchCounter++;
                }
            }
        }

        // === 确定文本颜色 ===
        // 优先级: 选中(白) > 搜索匹配内(白) > 语法高亮(规则色) > 默认(灰)
        QColor defaultTextColor = option.palette.color(QPalette::Text);
        QColor syntaxColor = defaultTextColor;  // 语法高亮色
        
        // 检查语法高亮规则（使用正则表达式）
        if (!text.isEmpty()) {
            for (const HighlightRule &rule : m_highlightRules) {
                if (!rule.isActive) continue;
                QRegularExpressionMatch match = rule.pattern.match(text);
                if (match.hasMatch()) {
                    syntaxColor = rule.color;
                    break;  // 使用第一个匹配的规则（优先级最高）
                }
            }
        }
        
        // 计算文本区域
        QRect textRect = option.rect;
        textRect.setLeft(contentLeft);
        textY = textRect.top() + (textRect.height() - fm.height()) / 2 + fm.ascent();
        
        painter->setFont(option.font);
        
        // === 分段绘制文本（处理搜索匹配内文本颜色）===
        if (isSelected) {
            // 选中状态：整行白色文本
            painter->setPen(QColor("#ffffff"));
            painter->drawText(textRect.left(), textY, text);
        } else if (searchMatchRanges.isEmpty()) {
            // 无搜索匹配：使用语法高亮色
            painter->setPen(syntaxColor);
            painter->drawText(textRect.left(), textY, text);
        } else {
            // 有搜索匹配：分段绘制
            // 搜索匹配区域内使用白色（确保在橙色背景上可读）
            // 搜索匹配区域外使用语法高亮色
            int currentX = textRect.left();
            int lastEnd = 0;
            
            for (const auto &range : searchMatchRanges) {
                int start = range.first;
                int len = range.second;
                
                // 绘制匹配前的文本（语法高亮色）
                if (start > lastEnd) {
                    QString beforeText = text.mid(lastEnd, start - lastEnd);
                    painter->setPen(syntaxColor);
                    painter->drawText(currentX, textY, beforeText);
                    currentX += fm.horizontalAdvance(beforeText);
                }
                
                // 绘制匹配文本（白色，确保可读性）
                QString matchText = text.mid(start, len);
                painter->setPen(QColor("#ffffff"));
                painter->drawText(currentX, textY, matchText);
                currentX += fm.horizontalAdvance(matchText);
                
                lastEnd = start + len;
            }
            
            // 绘制剩余文本（语法高亮色）
            if (lastEnd < text.length()) {
                QString remaining = text.mid(lastEnd);
                painter->setPen(syntaxColor);
                painter->drawText(currentX, textY, remaining);
            }
        }

        painter->restore();
    }

    /**
     * @brief 设置搜索高亮词（普通字符串模式）
     * @param text 搜索文本
     * @param cs 大小写敏感性
     */
    void setHighlightTerm(const QString &text, Qt::CaseSensitivity cs = Qt::CaseInsensitive)
    {
        m_highlightText = text;
        m_caseSensitivity = cs;
        m_useRegexSearch = false;
    }
    
    /**
     * @brief 设置搜索高亮词（正则表达式模式）
     * @param pattern 正则表达式模式
     * @param valid 正则是否有效（无效时回退到普通搜索）
     */
    void setHighlightRegex(const QString &pattern)
    {
        if (pattern.isEmpty()) {
            m_highlightText.clear();
            m_useRegexSearch = false;
            return;
        }
        
        m_searchRegex = QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
        if (m_searchRegex.isValid()) {
            m_useRegexSearch = true;
            m_highlightText = pattern;  // 保留原始文本用于显示
        } else {
            // 正则无效，回退到普通搜索
            m_highlightText = pattern;
            m_useRegexSearch = false;
        }
    }

    /**
     * @brief 设置当前激活的匹配
     * @param row 行号（-1 表示无激活匹配）
     * @param matchIndex 该行内的匹配索引（0-based）
     */
    void setActiveMatch(int row, int matchIndex)
    {
        m_activeRow = row;
        m_activeMatchIndex = matchIndex;
    }

private:
    int m_rowHeight;  ///< 固定行高
    QString m_highlightText;  ///< 搜索高亮文本
    Qt::CaseSensitivity m_caseSensitivity = Qt::CaseInsensitive;  ///< 大小写敏感性
    bool m_useRegexSearch = false;  ///< 是否使用正则搜索
    QRegularExpression m_searchRegex;  ///< 搜索用正则表达式
    int m_activeRow = -1;  ///< 当前激活匹配的行号
    int m_activeMatchIndex = -1;  ///< 当前激活匹配在该行内的索引
    QList<HighlightRule> m_highlightRules;  ///< 语法高亮规则列表（正则表达式）
};

#endif // COMPACTLINEDELEGATE_H
