#ifndef TEXTITEM_H
#define TEXTITEM_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QPen>
#include <QBrush>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QDebug>
#include <QSizeF>
#include <QColor>
#include <QFont>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QTextOption>
#include <QFontMetrics>
#include <QGraphicsProxyWidget>
#include "alignableitem.h"

/**
 * @brief 文本框图形项
 * 
 * 用于在 SimpleLabel 中显示和编辑文本的图形项。
 * 支持字体设置、颜色设置、边框样式、背景色等属性。
 */
class TextItem : public QGraphicsItem, public AlignableItem
{
public:
    // 调整手柄类型枚举 (仿照二维码和条形码)
    enum HandleType {
        NoHandle = -1,
        TopLeft = 0,
        TopCenter = 1,
        TopRight = 2,
        MiddleLeft = 3,
        MiddleRight = 4,
        BottomLeft = 5,
        BottomCenter = 6,
        BottomRight = 7
    };

    explicit TextItem(const QString &text = "", QGraphicsItem *parent = nullptr);
    ~TextItem() override = default;

    // 重写QGraphicsItem的纯虚函数
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // 文本内容相关
    void setText(const QString &text);
    QString text() const { return m_text; }

    // 字体相关
    void setFont(const QFont &font);
    QFont font() const { return m_font; }

    // 颜色相关
    void setTextColor(const QColor &color);
    QColor textColor() const { return m_textColor; }

    void setBackgroundColor(const QColor &color);
    QColor backgroundColor() const { return m_backgroundColor; }    // 边框相关
    void setBorderPen(const QPen &pen);
    QPen borderPen() const { return m_borderPen; }

    void setBorderEnabled(bool enabled);
    bool isBorderEnabled() const { return m_borderEnabled; }
    
    void setBorderWidth(int width);
    int borderWidth() const;
    
    void setBorderColor(const QColor &color);
    QColor borderColor() const;

    // 尺寸相关
    void setSize(const QSizeF &size);
    QSizeF size() const { return m_size; }

    // 文本对齐
    void setAlignment(Qt::Alignment alignment);
    Qt::Alignment alignment() const { return m_alignment; }

    // 字距（字母间距，绝对像素）
    void setLetterSpacing(qreal pixels);
    qreal letterSpacing() const { return m_letterSpacing; }

    // 文本换行
    void setWordWrap(bool wrap);
    bool wordWrap() const { return m_wordWrap; }

    // 自动调整大小
    void setAutoResize(bool autoResize);
    bool autoResize() const { return m_autoResize; }

    // 背景透明度
    void setBackgroundEnabled(bool enabled);
    bool isBackgroundEnabled() const { return m_backgroundEnabled; }

    // 计算文本所需的最小尺寸
    QSizeF calculateMinimumSize() const;

    // 实现 AlignableItem 接口
    QGraphicsItem* asGraphicsItem() override { return this; }
    const QGraphicsItem* asGraphicsItem() const override { return this; }

    // 静态成员变量，用于复制粘贴功能
    static bool s_hasCopiedItem;
    static QString s_copiedText;
    static QFont s_copiedFont;
    static QColor s_copiedTextColor;
    static QColor s_copiedBackgroundColor;
    static QPen s_copiedBorderPen;
    static bool s_copiedBorderEnabled;
    static QSizeF s_copiedSize;
    static Qt::Alignment s_copiedAlignment;
    static bool s_copiedWordWrap;
    static bool s_copiedAutoResize;
    static bool s_copiedBackgroundEnabled;
    static qreal s_copiedLetterSpacing;

    // 复制方法
    void copyItem(TextItem *item);

    // 编辑状态
    void setEditing(bool editing);
    bool isEditing() const { return m_isEditing; }

    // 内联编辑：开始/提交/取消
    void beginInlineEdit();
    void commitInlineEdit();
    void cancelInlineEdit();

protected:
    // 重写鼠标事件处理函数
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverEnterEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    
    // 添加场景事件重写用于调试
    bool sceneEvent(QEvent *event) override;

    // 对齐使用的内容矩形（局部坐标）
    QRectF alignmentRectLocal() const override { return QRectF(0, 0, m_size.width(), m_size.height()); }

private:
    // 私有成员函数
    void updateBoundingRect();
    void drawResizeHandles(QPainter *painter);
    
    // 调整大小相关方法 (仿照二维码和条形码)
    HandleType getHandleAt(const QPointF &pos) const;
    QRectF getHandleRect(HandleType handle) const;
    void updateSizeFromHandle(HandleType handle, const QPointF &delta);
    void setCursorForHandle(HandleType handle);

    // 文本绘制相关
    void drawText(QPainter *painter, const QRectF &rect);
    void drawBackground(QPainter *painter, const QRectF &rect);
    void drawBorder(QPainter *painter, const QRectF &rect);
    
    // 内部调整大小方法（不触发几何变化通知）
    void setSizeInternal(const QSizeF &size);

    // 成员变量
    QString m_text;                 // 文本内容
    QFont m_font;                   // 字体
    QColor m_textColor;             // 文本颜色
    QColor m_backgroundColor;       // 背景颜色
    QPen m_borderPen;               // 边框画笔
    bool m_borderEnabled;           // 是否启用边框
    bool m_backgroundEnabled;       // 是否启用背景
    QSizeF m_size;                  // 文本框尺寸
    Qt::Alignment m_alignment;      // 文本对齐方式
    qreal m_letterSpacing;          // 字距（像素，绝对）
    bool m_wordWrap;                // 是否自动换行
    bool m_autoResize;              // 是否自动调整大小
    bool m_isEditing;               // 是否处于编辑状态

    // 调整大小相关 (仿照二维码和条形码)
    bool m_isResizing;              // 是否正在调整大小
    QPointF m_resizeStartPos;       // 调整开始位置
    QSizeF m_resizeStartSize;       // 调整开始尺寸
    QPointF m_resizeStartItemPos;   // 调整开始时的文本框位置
    HandleType m_resizeHandle;      // 当前调整手柄

    // 内联编辑相关
    QGraphicsProxyWidget* m_editorProxy = nullptr;
    class CustomTextEdit* m_editor = nullptr; // 前向声明在cpp中定义
    QString m_originalTextForEdit;
};

#endif // TEXTITEM_H
