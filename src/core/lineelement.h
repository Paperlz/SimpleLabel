#ifndef LINEELEMENT_H
#define LINEELEMENT_H

#include "labelelement.h"
#include <QPen>
#include <QPointF>

class LineItem;

class LineElement : public labelelement
{
public:
    explicit LineElement();
    explicit LineElement(const QPointF &startPoint, const QPointF &endPoint);
    explicit LineElement(LineItem *item);
    virtual ~LineElement() = default;

    // 重写基类方法
    QString getType() const override;
    QJsonObject getData() const override;
    void setData(const QJsonObject &data) override;
    void setPos(const QPointF &pos) override;
    QPointF getPos() const override;
    QGraphicsItem* getItem() const override;
    void addToScene(QGraphicsScene* scene) override;

    // 画笔属性
    QPen pen() const { return m_pen; }
    void setPen(const QPen &pen);

    // 线条端点
    QPointF startPoint() const { return m_startPoint; }
    void setStartPoint(const QPointF &point);

    QPointF endPoint() const { return m_endPoint; }
    void setEndPoint(const QPointF &point);

    void setLine(const QPointF &startPoint, const QPointF &endPoint);

    // 计算线条长度
    qreal length() const;
    
    // 计算线条角度（弧度）
    qreal angle() const;

    // 虚线属性（序列化）
    bool isDashed() const { return m_pen.style() == Qt::DashLine; }
    void setDashed(bool dashed);

private:
    QPen m_pen;
    QPointF m_startPoint;
    QPointF m_endPoint;
    mutable LineItem* m_graphicsItem;

    void initializeDefaults();
    void syncFromItem();
    void syncToItem();
};

#endif // LINEELEMENT_H
