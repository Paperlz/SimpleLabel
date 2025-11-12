#include "arrowelement.h"
#include "../graphics/arrowitem.h"
#include <QtMath>
#include <QGraphicsScene>

ArrowElement::ArrowElement()
	: m_startPoint(0,0)
	, m_endPoint(100,0)
	, m_headLength(15.0)
	, m_headAngle(30.0)
	, m_graphicsItem(nullptr)
{
	initializeDefaults();
}

ArrowElement::ArrowElement(const QPointF &startPoint, const QPointF &endPoint)
	: m_startPoint(startPoint)
	, m_endPoint(endPoint)
	, m_headLength(15.0)
	, m_headAngle(30.0)
	, m_graphicsItem(nullptr)
{
	initializeDefaults();
}

ArrowElement::ArrowElement(ArrowItem *item)
	: m_graphicsItem(item)
{
	initializeDefaults();
	syncFromItem();
}

void ArrowElement::initializeDefaults()
{
	m_pen = QPen(Qt::black, 2, Qt::SolidLine);
}

QString ArrowElement::getType() const
{
	return "Arrow";
}

QJsonObject ArrowElement::getData() const
{
	QJsonObject obj;
	obj["penColor"] = m_pen.color().name();
	obj["penWidth"] = m_pen.widthF();
	obj["penStyle"] = static_cast<int>(m_pen.style());
	QJsonObject s; s["x"] = m_startPoint.x(); s["y"] = m_startPoint.y(); obj["startPoint"] = s;
	QJsonObject e; e["x"] = m_endPoint.x(); e["y"] = m_endPoint.y(); obj["endPoint"] = e;
	obj["headLength"] = m_headLength;
	obj["headAngle"] = m_headAngle;
	return obj;
}

void ArrowElement::setData(const QJsonObject &data)
{
	QColor c(data.value("penColor").toString("#000000"));
	m_pen.setColor(c);
	m_pen.setWidthF(data.value("penWidth").toDouble(2.0));
	m_pen.setStyle(static_cast<Qt::PenStyle>(data.value("penStyle").toInt(int(Qt::SolidLine))));
	QJsonObject s = data.value("startPoint").toObject();
	QJsonObject e = data.value("endPoint").toObject();
	m_startPoint = QPointF(s.value("x").toDouble(), s.value("y").toDouble());
	m_endPoint = QPointF(e.value("x").toDouble(), e.value("y").toDouble());
	m_headLength = data.value("headLength").toDouble(m_headLength);
	m_headAngle = data.value("headAngle").toDouble(m_headAngle);
	syncToItem();
}

void ArrowElement::setPos(const QPointF &pos)
{
	if (m_graphicsItem) {
		m_graphicsItem->setPos(pos);
	}
}

QPointF ArrowElement::getPos() const
{
	return m_graphicsItem ? m_graphicsItem->pos() : QPointF();
}

QGraphicsItem* ArrowElement::getItem() const
{
	return m_graphicsItem;
}

void ArrowElement::addToScene(QGraphicsScene *scene)
{
	if (!scene) return;
	if (!m_graphicsItem) {
		m_graphicsItem = createGraphicsItem();
		syncToItem();
	}
	scene->addItem(m_graphicsItem);
}

void ArrowElement::setPen(const QPen &pen)
{
	m_pen = pen;
	syncToItem();
}

void ArrowElement::setStartPoint(const QPointF &p)
{
	m_startPoint = p;
	syncToItem();
}

void ArrowElement::setEndPoint(const QPointF &p)
{
	m_endPoint = p;
	syncToItem();
}

void ArrowElement::setLine(const QPointF &s, const QPointF &e)
{
	m_startPoint = s; m_endPoint = e; syncToItem();
}

qreal ArrowElement::length() const
{
	return QLineF(m_startPoint, m_endPoint).length();
}

qreal ArrowElement::angle() const
{
	return std::atan2(m_endPoint.y() - m_startPoint.y(), m_endPoint.x() - m_startPoint.x());
}

void ArrowElement::syncFromItem()
{
	if (!m_graphicsItem) return;
	m_pen = m_graphicsItem->pen();
	m_startPoint = m_graphicsItem->startPoint();
	m_endPoint = m_graphicsItem->endPoint();
	m_headLength = m_graphicsItem->headLength();
	m_headAngle = m_graphicsItem->headAngle();
}

void ArrowElement::syncToItem()
{
	if (!m_graphicsItem) return;
	m_graphicsItem->setPen(m_pen);
	m_graphicsItem->setLine(m_startPoint, m_endPoint);
	m_graphicsItem->setHeadLength(m_headLength);
	m_graphicsItem->setHeadAngle(m_headAngle);
}

ArrowItem* ArrowElement::createGraphicsItem() const
{
	auto *ai = new ArrowItem(m_startPoint, m_endPoint);
	ai->setHeadLength(m_headLength);
	ai->setHeadAngle(m_headAngle);
	ai->setPen(m_pen);
	return ai;
}

