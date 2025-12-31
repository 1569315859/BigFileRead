/**
 * @file CompactLineDelegate.h
 * @brief 紧凑行委托 - 实现正确的行高度和宽度以优化大文件显示
 * @version 2.0 - 修复行高截断和水平滚动条问题
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
 *              同时确保水平滚动条正常显示
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
     * @description 关键优化：
     *   - 高度：fontMetrics.height() + 4 像素内边距
     *   - 宽度：返回大固定值强制水平滚动条出现
     */
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(index)

        int height = m_rowHeight;
        if (height <= 0) {
            // 如果未设置固定高度，使用字体计算
            QFontMetrics fm(option.font);
            height = fm.height() + VERTICAL_PADDING * 2;
        }

        // 返回固定大宽度，确保水平滚动条出现
        return QSize(FIXED_WIDTH, height);
    }

    /**
     * @brief 自定义绘制
     * @description 绘制紧凑的文本行，垂直居中
     */
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        // 获取数据
        QString text = index.data(Qt::DisplayRole).toString();

        // 设置画笔
        painter->save();

        // 绘制背景
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

        // 设置文本颜色
        if (option.state & QStyle::State_Selected) {
            painter->setPen(option.palette.color(QPalette::HighlightedText));
        } else {
            painter->setPen(option.palette.color(QPalette::Text));
        }

        // 计算文本区域（带左边距）
        QRect textRect = option.rect;
        textRect.setLeft(textRect.left() + 4);  // 左边距 4 像素

        // 绘制文本，垂直居中
        painter->setFont(option.font);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);

        painter->restore();
    }

private:
    int m_rowHeight;  ///< 固定行高
};

#endif // COMPACTLINEDELEGATE_H
