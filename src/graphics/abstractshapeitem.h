#ifndef ABSTRACTSHAPEITEM_H
#define ABSTRACTSHAPEITEM_H

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
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include "alignableitem.h"

class AbstractShapeItem : public QGraphicsItem, public AlignableItem
{
public:
    // 调整手柄类型枚举
    enum HandleType {
        NoHandle = 0,
        TopLeft,
        TopCenter,
        TopRight,
        MiddleLeft,
        MiddleRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    explicit AbstractShapeItem(QGraphicsItem *parent = nullptr);
    virtual ~AbstractShapeItem() = default;

    // 重写QGraphicsItem的纯虚函数
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // 形状属性
    void setPen(const QPen &pen);
    QPen pen() const { return m_pen; }
    
    void setBrush(const QBrush &brush);
    QBrush brush() const { return m_brush; }
    
    void setFillEnabled(bool enabled);
    bool fillEnabled() const { return m_fillEnabled; }
    
    void setBorderEnabled(bool enabled);
    bool borderEnabled() const { return m_borderEnabled; }

    // 尺寸相关
    virtual void setSize(const QSizeF &size);
    virtual QSizeF size() const { return m_size; }

    // 实现 AlignableItem 接口
    QGraphicsItem* asGraphicsItem() override { return this; }
    const QGraphicsItem* asGraphicsItem() const override { return this; }

    // 静态成员变量，用于复制粘贴功能
    static bool s_hasCopiedItem;
    static QPen s_copiedPen;
    static QBrush s_copiedBrush;
    static bool s_copiedFillEnabled;
    static bool s_copiedBorderEnabled;
    static QSizeF s_copiedSize;

    // 复制方法
    virtual void copyItem(AbstractShapeItem *item);

protected:
    // 鼠标事件处理
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    // 右键菜单
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    // 纯虚函数，由子类实现具体的绘制逻辑
    virtual void paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) = 0;
    
    // 虚函数，子类可以重写以提供特定的调整手柄行为
    virtual HandleType getHandleAt(const QPointF &pos) const;
    virtual QRectF getHandleRect(HandleType handle) const;
    virtual void updateSizeFromHandle(HandleType handle, const QPointF &delta);
    virtual void setCursorForHandle(HandleType handle);
    virtual void drawResizeHandles(QPainter *painter);

    // 对齐使用的内容矩形（局部坐标）
    QRectF alignmentRectLocal() const override { return QRectF(0, 0, m_size.width(), m_size.height()); }

    // 成员变量
    QPen m_pen;                     // 边框画笔
    QBrush m_brush;                 // 填充画刷
    bool m_fillEnabled;             // 是否启用填充
    bool m_borderEnabled;           // 是否启用边框
    QSizeF m_size;                  // 形状尺寸

    // 调整大小相关
    bool m_isResizing;              // 是否正在调整大小
    HandleType m_resizeHandle;      // 当前调整手柄
    QPointF m_resizeStartPos;       // 调整开始位置
    QSizeF m_resizeStartSize;       // 调整开始尺寸
    QPointF m_resizeStartItemPos;   // 调整开始时的项目位置

private:
    void initializeDefaults();
};

#endif // ABSTRACTSHAPEITEM_H
