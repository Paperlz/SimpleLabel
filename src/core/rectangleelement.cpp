#include "rectangleelement.h"
#include "../graphics/rectangleitem.h"

RectangleElement::RectangleElement()
    : ShapeElement(Rectangle)
{
    initializeDefaults();
}

RectangleElement::RectangleElement(const QSizeF &size)
    : ShapeElement(Rectangle)
{
    setSize(size);
    initializeDefaults();
}

RectangleElement::RectangleElement(RectangleItem *item)
    : ShapeElement(item, Rectangle)
{
    if (item) {
        m_cornerRadius = item->cornerRadius();
    }
}

void RectangleElement::initializeDefaults()
{
    // 矩形默认启用填充
    setFillEnabled(true);
    setBrush(QBrush(QColor(173, 216, 230), Qt::SolidPattern)); // 浅蓝色
}

QString RectangleElement::getType() const
{
    return "Rectangle";
}

AbstractShapeItem* RectangleElement::createGraphicsItem() const
{
    auto *ri = new RectangleItem(size());
    ri->setCornerRadius(m_cornerRadius);
    return ri;
}

QJsonObject RectangleElement::getData() const
{
    QJsonObject obj = ShapeElement::getData();
    if (m_cornerRadius > 0.0) {
        obj["cornerRadius"] = m_cornerRadius;
    } else {
        obj["cornerRadius"] = 0.0;
    }
    return obj;
}

void RectangleElement::setData(const QJsonObject &data)
{
    ShapeElement::setData(data);
    m_cornerRadius = data.value("cornerRadius").toDouble(0.0);
    if (auto *ri = dynamic_cast<RectangleItem*>(m_graphicsItem)) {
        ri->setCornerRadius(m_cornerRadius);
    }
}
