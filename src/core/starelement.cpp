#include "starelement.h"
#include "shapeelement.h"
#include "../graphics/staritem.h"

StarElement::StarElement()
	: ShapeElement(Rectangle) // 基类需要类型，占位后不使用其工厂
	, m_points(5)
	, m_innerRatio(0.5)
{
	initializeDefaults();
}

StarElement::StarElement(const QSizeF &size)
	: ShapeElement(Rectangle)
	, m_points(5)
	, m_innerRatio(0.5)
{
	setSize(size);
	initializeDefaults();
}

StarElement::StarElement(StarItem *item)
	: ShapeElement(item, Rectangle)
	, m_points(item ? item->pointCount() : 5)
	, m_innerRatio(item ? item->innerRatio() : 0.5)
{
}

void StarElement::initializeDefaults()
{
	setFillEnabled(true);
	setBrush(QBrush(QColor(255, 215, 0), Qt::SolidPattern));
}

QString StarElement::getType() const
{
	return "Star";
}

QJsonObject StarElement::getData() const
{
	QJsonObject obj = ShapeElement::getData();
	obj["points"] = m_points;
	obj["innerRatio"] = m_innerRatio;
	return obj;
}

void StarElement::setData(const QJsonObject &data)
{
	ShapeElement::setData(data);
	m_points = data.value("points").toInt(m_points);
	m_innerRatio = data.value("innerRatio").toDouble(m_innerRatio);
	if (auto si = dynamic_cast<StarItem*>(m_graphicsItem)) {
		si->setPointCount(m_points);
		si->setInnerRatio(m_innerRatio);
	}
}

AbstractShapeItem* StarElement::createGraphicsItem() const
{
	auto *star = new StarItem(size());
	star->setPointCount(m_points);
	star->setInnerRatio(m_innerRatio);
	return star;
}

