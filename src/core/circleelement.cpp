#include "circleelement.h"
#include "../graphics/circleitem.h"
#include <QtMath>

CircleElement::CircleElement()
    : ShapeElement(Circle)
    , m_isCircle(true)
{
    initializeDefaults();
}

CircleElement::CircleElement(const QSizeF &size)
    : ShapeElement(Circle)
    , m_isCircle(true)
{
    setSize(size);
    initializeDefaults();
}

CircleElement::CircleElement(CircleItem *item)
    : ShapeElement(item, item && item->isCircle() ? Circle : Ellipse)
    , m_isCircle(item ? item->isCircle() : true)
{
}

void CircleElement::initializeDefaults()
{
    // 圆形默认启用填充
    setFillEnabled(true);
    setBrush(QBrush(QColor(144, 238, 144), Qt::SolidPattern)); // 浅绿色
    
    // 如果是圆形，确保宽高相等
    if (m_isCircle) {
        qreal diameter = qMax(m_size.width(), m_size.height());
        setSize(QSizeF(diameter, diameter));
    }
}

void CircleElement::setIsCircle(bool isCircle)
{
    if (m_isCircle != isCircle) {
        m_isCircle = isCircle;
        setShapeType(m_isCircle ? Circle : Ellipse);

        if (m_isCircle) {
            // 转换为圆形，使用较大的尺寸作为直径
            qreal diameter = qMax(m_size.width(), m_size.height());
            setSize(QSizeF(diameter, diameter));
        }

        if (auto circleItem = dynamic_cast<CircleItem*>(getItem())) {
            circleItem->setIsCircle(m_isCircle);
        }
    }
}

QString CircleElement::getType() const
{
    return m_isCircle ? "Circle" : "Ellipse";
}

AbstractShapeItem* CircleElement::createGraphicsItem() const
{
    auto *item = new CircleItem(size());
    item->setIsCircle(m_isCircle);
    return item;
}

void CircleElement::setShapeType(ShapeType type)
{
    ShapeElement::setShapeType(type);
    const bool circle = (type == Circle);
    if (m_isCircle != circle) {
        m_isCircle = circle;
    }
    if (auto circleItem = dynamic_cast<CircleItem*>(getItem())) {
        circleItem->setIsCircle(m_isCircle);
    }
}
