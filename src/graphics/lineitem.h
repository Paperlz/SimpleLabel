#ifndef LINEITEM_H
#define LINEITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QMenu>
#include <QAction>
#include <QDebug>
#include "alignableitem.h"

class LineItem : public QGraphicsItem, public AlignableItem
{
public:
    // 调整手柄类型
    enum HandleType {
        NoHandle = 0,
        StartHandle,
        EndHandle
    };

    explicit LineItem(QGraphicsItem *parent = nullptr);
    explicit LineItem(const QPointF &startPoint, const QPointF &endPoint, QGraphicsItem *parent = nullptr);
    virtual ~LineItem() = default;

    // 重写QGraphicsItem的纯虚函数
    QRectF boundingRect() const override;
    QPainterPath shape() const override;  // 精确命中测试：细描边 + 手柄
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // 线条属性
    void setPen(const QPen &pen);
    QPen pen() const { return m_pen; }

    void setStartPoint(const QPointF &point);
    QPointF startPoint() const { return m_startPoint; }

    void setEndPoint(const QPointF &point);
    QPointF endPoint() const { return m_endPoint; }

    void setLine(const QPointF &startPoint, const QPointF &endPoint);
    // 虚线支持：设置/查询是否使用虚线绘制
    void setDashed(bool dashed);
    bool isDashed() const { return m_pen.style() == Qt::DashLine; }
    
    // 计算线条长度
    qreal length() const;
    
    // 计算线条角度（弧度）
    qreal angle() const;

    // 与竖直方向(向上)的夹角 [0,360)
    double angleFromVertical() const;
    void setAngleFromVertical(double degrees);

    // 实现 AlignableItem 接口
    QGraphicsItem* asGraphicsItem() override { return this; }
    const QGraphicsItem* asGraphicsItem() const override { return this; }

    // 静态成员变量，用于复制粘贴功能
    static bool s_hasCopiedItem;
    static QPen s_copiedPen;
    static QPointF s_copiedStartPoint;
    static QPointF s_copiedEndPoint;

    // 当前正在被调整的 LineItem（若无则为 nullptr），供外部如 LabelScene 查询
    static LineItem* s_adjustingItem;

    
    // 复制方法
    void copyItem(LineItem *item);

protected:
    // 鼠标事件处理
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    // 右键菜单
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    // 对齐使用的内容矩形（局部坐标）：线段包围盒（含笔宽），不含手柄
    QRectF alignmentRectLocal() const override;

private:
    // 辅助方法
    HandleType getHandleAt(const QPointF &pos) const;
    QRectF getHandleRect(HandleType handle) const;
    void updateLineFromHandle(HandleType handle, const QPointF &scenePos);
    void setCursorForHandle(HandleType handle);
    void drawResizeHandles(QPainter *painter);
    QPointF getHandlePosition(HandleType handle) const;
    void initializeDefaults();

    // 成员变量
    QPen m_pen;                     // 线条画笔
    QPointF m_startPoint;           // 起点（相对于item坐标系）
    QPointF m_endPoint;             // 终点（相对于item坐标系）

    // 调整大小相关
    bool m_isAdjusting;             // 是否正在调整（实例级）
    HandleType m_adjustHandle;      // 当前调整手柄
    QPointF m_adjustStartPos;       // 调整开始位置（场景坐标）
    QPointF m_adjustStartPoint;     // 调整开始时的起点
    QPointF m_adjustEndPoint;       // 调整开始时的终点
};

#endif // LINEITEM_H
