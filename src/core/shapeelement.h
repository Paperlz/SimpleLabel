#ifndef SHAPEELEMENT_H
#define SHAPEELEMENT_H

#include "labelelement.h"
#include <QPen>
#include <QBrush>
#include <QSizeF>

class AbstractShapeItem;

class ShapeElement : public labelelement
{
public:
    enum ShapeType {
        Line,
        Rectangle,
        Circle,
        Ellipse
    };

    explicit ShapeElement(ShapeType type = Rectangle);
    explicit ShapeElement(AbstractShapeItem *item, ShapeType type = Rectangle);
    virtual ~ShapeElement() = default;

    // 重写基类方法
    QString getType() const override;
    QJsonObject getData() const override;
    void setData(const QJsonObject &data) override;
    void setPos(const QPointF &pos) override;
    QPointF getPos() const override;
    QGraphicsItem* getItem() const override;
    void addToScene(QGraphicsScene* scene) override;

    // 形状类型
    ShapeType shapeType() const { return m_shapeType; }
    virtual void setShapeType(ShapeType type);

    // 画笔属性
    QPen pen() const { return m_pen; }
    void setPen(const QPen &pen);

    // 画刷属性
    QBrush brush() const { return m_brush; }
    void setBrush(const QBrush &brush);

    // 填充和边框开关
    bool fillEnabled() const { return m_fillEnabled; }
    void setFillEnabled(bool enabled);

    bool borderEnabled() const { return m_borderEnabled; }
    void setBorderEnabled(bool enabled);

    // 尺寸
    QSizeF size() const { return m_size; }
    void setSize(const QSizeF &size);

    // 线条特有属性（用于线条类型）
    QPointF startPoint() const { return m_startPoint; }
    void setStartPoint(const QPointF &point) { m_startPoint = point; }

    QPointF endPoint() const { return m_endPoint; }
    void setEndPoint(const QPointF &point) { m_endPoint = point; }

    // 便利方法
    QString shapeTypeToString() const;
    static ShapeType stringToShapeType(const QString &str);

protected:
    ShapeType m_shapeType;
    QPen m_pen;
    QBrush m_brush;
    bool m_fillEnabled;
    bool m_borderEnabled;
    QSizeF m_size;

    // 线条特有属性
    QPointF m_startPoint;
    QPointF m_endPoint;

    // 图形项指针
    mutable AbstractShapeItem* m_graphicsItem;

private:
    void initializeDefaults();
    void syncFromItem();
    void syncToItem();
protected:
    virtual AbstractShapeItem* createGraphicsItem() const;
};

#endif // SHAPEELEMENT_H
