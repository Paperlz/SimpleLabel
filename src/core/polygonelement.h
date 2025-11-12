#ifndef POLYGONELEMENT_H
#define POLYGONELEMENT_H

#include "shapeelement.h"
#include <QVector>

class PolygonItem;

class PolygonElement : public ShapeElement
{
public:
	PolygonElement();
	explicit PolygonElement(const QSizeF &size);
	explicit PolygonElement(PolygonItem *item);
	~PolygonElement() override = default;

	QString getType() const override;
	QJsonObject getData() const override;
	void setData(const QJsonObject &data) override;

	void setPoints(const QVector<QPointF>& pts) { m_points = pts; }
	QVector<QPointF> points() const { return m_points; }

protected:
	AbstractShapeItem* createGraphicsItem() const override;

private:
	QVector<QPointF> m_points; // 局部点
	void initializeDefaults();
};

#endif // POLYGONELEMENT_H
