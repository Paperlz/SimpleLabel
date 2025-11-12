#include "polygonelement.h"
#include "../graphics/polygonitem.h"
#include <QJsonArray>

PolygonElement::PolygonElement()
	: ShapeElement(Rectangle)
{
	initializeDefaults();
}

PolygonElement::PolygonElement(const QSizeF &size)
	: ShapeElement(Rectangle)
{
	setSize(size);
	initializeDefaults();
}

PolygonElement::PolygonElement(PolygonItem *item)
	: ShapeElement(item, Rectangle)
{
	if (item) {
		m_points = item->points();
	}
}

void PolygonElement::initializeDefaults()
{
	setFillEnabled(true);
	setBrush(QBrush(QColor(135, 206, 235), Qt::SolidPattern));
}

QString PolygonElement::getType() const
{
	return "Polygon";
}

QJsonObject PolygonElement::getData() const
{
	QJsonObject obj = ShapeElement::getData();
	QJsonArray arr;
	for (const QPointF &p : m_points) {
		QJsonObject o; o["x"] = p.x(); o["y"] = p.y();
		arr.append(o);
	}
	obj["points"] = arr;
	return obj;
}

void PolygonElement::setData(const QJsonObject &data)
{
	ShapeElement::setData(data);
	m_points.clear();
	const QJsonArray arr = data.value("points").toArray();
	for (const QJsonValue &v : arr) {
		QJsonObject o = v.toObject();
		m_points.append(QPointF(o.value("x").toDouble(), o.value("y").toDouble()));
	}
	if (auto pi = dynamic_cast<PolygonItem*>(m_graphicsItem)) {
		pi->setPoints(m_points);
	}
}

AbstractShapeItem* PolygonElement::createGraphicsItem() const
{
	auto *poly = new PolygonItem(size());
	poly->setPoints(m_points);
	return poly;
}

