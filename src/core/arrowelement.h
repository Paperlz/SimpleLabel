#ifndef ARROWELEMENT_H
#define ARROWELEMENT_H

#include "labelelement.h"
#include <QPen>
#include <QPointF>

class QGraphicsScene;

class ArrowItem;

class ArrowElement : public labelelement
{
public:
	ArrowElement();
	ArrowElement(const QPointF &startPoint, const QPointF &endPoint);
	explicit ArrowElement(ArrowItem *item);
	~ArrowElement() override = default;

	QString getType() const override;
	QJsonObject getData() const override;
	void setData(const QJsonObject &data) override;
	void setPos(const QPointF &pos) override;
	QPointF getPos() const override;
	QGraphicsItem* getItem() const override;
	void addToScene(QGraphicsScene* scene) override;

	// 属性
	QPen pen() const { return m_pen; }
	void setPen(const QPen &pen);
	QPointF startPoint() const { return m_startPoint; }
	void setStartPoint(const QPointF &p);
	QPointF endPoint() const { return m_endPoint; }
	void setEndPoint(const QPointF &p);
	void setLine(const QPointF &s, const QPointF &e);
	qreal length() const;
	qreal angle() const;

	qreal headLength() const { return m_headLength; }
	void setHeadLength(qreal v) { m_headLength = v; syncToItem(); }
	qreal headAngle() const { return m_headAngle; }
	void setHeadAngle(qreal v) { m_headAngle = v; syncToItem(); }

private:
	QPen m_pen;
	QPointF m_startPoint;
	QPointF m_endPoint;
	qreal m_headLength;
	qreal m_headAngle;
	mutable ArrowItem* m_graphicsItem;

	void initializeDefaults();
	void syncFromItem();
	void syncToItem();
	ArrowItem* createGraphicsItem() const;
};

#endif // ARROWELEMENT_H
