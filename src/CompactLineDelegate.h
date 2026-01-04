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
 *              同时确保水平滚动条正常显示，并支持搜索词高亮及活动匹配区分
 */
class CompactLineDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /// 行的垂直内边距（上下各 2 像素）
    static constexpr int VERTICAL_PADDING = 2;
    /// 固定宽度（确保水平滚动条出现）
    static constexpr int FIXED_WIDTH = 8000;

    explicit CompactLineDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_rowHeight(0)
    {
    }

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
     * @brief 自定义绘制（含搜索高亮和活动匹配区分）
     */
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QString text = index.data(Qt::DisplayRole).toString();
        painter->save();

        // 绘制背景
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

        // === 搜索高亮绘制（在文本之前）===
        if (!m_highlightText.isEmpty() && !text.isEmpty()) {
            QFontMetrics fm(option.font);
            
            // 普通匹配颜色 - 半透明黄色
            QColor normalMatchColor(234, 92, 0, 100);
            // 活动匹配颜色 - 不透明橙色
            QColor activeMatchColor(255, 152, 0);
            
            int searchPos = 0;
            const int matchLength = m_highlightText.length();
            int matchCounter = 0;  // 该行内的匹配计数器
            
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
                
                // 绘制高亮矩形
                QRect highlightRect(
                    option.rect.left() + 4 + startX,
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

        // 设置文本颜色
        if (option.state & QStyle::State_Selected) {
            painter->setPen(option.palette.color(QPalette::HighlightedText));
        } else {
            painter->setPen(option.palette.color(QPalette::Text));
        }

        // 计算文本区域（带左边距）
        QRect textRect = option.rect;
        textRect.setLeft(textRect.left() + 4);

        // 绘制文本，垂直居中
        painter->setFont(option.font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

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
};

#endif // COMPACTLINEDELEGATE_H
