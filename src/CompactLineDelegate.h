/**
 * @file CompactLineDelegate.h
 * @brief 紧凑行委托 - 支持搜索高亮和活动匹配区分
 * @version 2.2 - 添加活动匹配高亮功能
 */

#ifndef COMPACTLINEDELEGATE_H
#define COMPACTLINEDELEGATE_H

#include <QStyledItemDelegate>
#include <QFontMetrics>
#include <QPainter>
#include <QApplication>

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

    /// 语法高亮规则
    struct KeywordRule {
        QString text;   ///< 关键词
        QColor color;   ///< 高亮颜色
    };

    explicit CompactLineDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_rowHeight(0)
    {
        // 初始化默认语法高亮规则（日志关键词）
        // 错误类（红色）
        m_syntaxRules.append({"Error", QColor("#ff5252")});
        m_syntaxRules.append({"Fatal", QColor("#ff5252")});
        m_syntaxRules.append({"FATAL", QColor("#ff5252")});
        m_syntaxRules.append({"ERROR", QColor("#ff5252")});
        m_syntaxRules.append({"错误", QColor("#ff5252")});
        m_syntaxRules.append({"失败", QColor("#ff5252")});
        
        // 警告类（橙黄色）
        m_syntaxRules.append({"Warning", QColor("#ffb74d")});
        m_syntaxRules.append({"Warn", QColor("#ffb74d")});
        m_syntaxRules.append({"WARNING", QColor("#ffb74d")});
        m_syntaxRules.append({"WARN", QColor("#ffb74d")});
        m_syntaxRules.append({"警告", QColor("#ffb74d")});
        
        // 信息类（浅绿色，可选）
        m_syntaxRules.append({"Info", QColor("#81c784")});
        m_syntaxRules.append({"INFO", QColor("#81c784")});
        m_syntaxRules.append({"信息", QColor("#81c784")});
    }

    /**
     * @brief 添加语法高亮规则
     */
    void addSyntaxRule(const QString &text, const QColor &color)
    {
        m_syntaxRules.append({text, color});
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

        // === 搜索高亮绘制（在文本之前）===
        if (!m_highlightText.isEmpty() && !text.isEmpty()) {
            // 使用已计算的 fm
            
            // 普通匹配颜色 - 半透明黄色
            QColor normalMatchColor(234, 92, 0, 100);
            // 活动匹配颜色 - 不透明橙色
            QColor activeMatchColor(255, 152, 0);
            
            int searchPos = 0;
            const int matchLength = m_highlightText.length();
            int matchCounter = 0;  // 该行内的匹配计数器
            
            // 内容区域起始 X 坐标（行号槽之后）
            int contentLeft = option.rect.left() + gutterWidth + 4;
            
            // 查找所有匹配并绘制高亮背景
            while (searchPos < text.length()) {
                int matchIndex = text.indexOf(m_highlightText, searchPos, m_caseSensitivity);
                if (matchIndex == -1) {
                    break;
                }
                
                // 计算匹配位置的几何信息
                QString beforeMatch = text.left(matchIndex);
                QString matchText = text.mid(matchIndex, matchLength);
                
                int startX = fm.horizontalAdvance(beforeMatch);
                int matchWidth = fm.horizontalAdvance(matchText);
                
                // 绘制高亮矩形（考虑行号槽偏移）
                QRect highlightRect(
                    contentLeft + startX,
                    option.rect.top(),
                    matchWidth,
                    option.rect.height()
                );
                
                // 区分活动匹配和普通匹配
                QColor color = normalMatchColor;
                if (index.row() == m_activeRow && matchCounter == m_activeMatchIndex) {
                    color = activeMatchColor;  // 活动匹配使用橙色
                }
                
                painter->fillRect(highlightRect, color);
                
                // 移动到下一个搜索位置
                searchPos = matchIndex + matchLength;
                matchCounter++;
            }
        }

        // === 确定文本颜色（语法高亮）===
        QColor penColor = option.palette.color(QPalette::Text);  // 默认文本色
        
        // 检查语法高亮规则
        if (!text.isEmpty()) {
            for (const KeywordRule &rule : m_syntaxRules) {
                if (text.contains(rule.text, Qt::CaseInsensitive)) {
                    penColor = rule.color;
                    break;  // 使用第一个匹配的规则
                }
            }
        }
        
        // 选中状态时强制使用白色文本（确保对比度）
        if (option.state & QStyle::State_Selected) {
            penColor = QColor("#ffffff");
        }
        
        painter->setPen(penColor);

        // 计算文本区域（行号槽之后 + 左边距）
        QRect textRect = option.rect;
        textRect.setLeft(option.rect.left() + gutterWidth + 4);
        
        // 使用已计算的 fm 计算正确的基线位置
        textY = textRect.top() + (textRect.height() - fm.height()) / 2 + fm.ascent();
        
        // 绘制文本（使用计算的基线位置，避免 Qt 对齐问题）
        painter->setFont(option.font);
        painter->drawText(textRect.left(), textY, text);

        painter->restore();
    }

    /**
     * @brief 设置搜索高亮词
     */
    void setHighlightTerm(const QString &text, Qt::CaseSensitivity cs = Qt::CaseInsensitive)
    {
        m_highlightText = text;
        m_caseSensitivity = cs;
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
    int m_activeRow = -1;  ///< 当前激活匹配的行号
    int m_activeMatchIndex = -1;  ///< 当前激活匹配在该行内的索引
    QList<KeywordRule> m_syntaxRules;  ///< 语法高亮规则列表
};

#endif // COMPACTLINEDELEGATE_H
