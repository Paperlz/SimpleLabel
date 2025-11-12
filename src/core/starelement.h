#ifndef STARELEMENT_H
#define STARELEMENT_H

#include "shapeelement.h"

class StarItem;

class StarElement : public ShapeElement
{
public:
	StarElement();
	explicit StarElement(const QSizeF &size);
	explicit StarElement(StarItem *item);
	~StarElement() override = default;

	QString getType() const override;
	QJsonObject getData() const override;
	void setData(const QJsonObject &data) override;

	void setPointCount(int n) { m_points = n; }
	int pointCount() const { return m_points; }
	void setInnerRatio(qreal r) { m_innerRatio = r; }
	qreal innerRatio() const { return m_innerRatio; }

protected:
	AbstractShapeItem* createGraphicsItem() const override;

private:
	int m_points;
	qreal m_innerRatio;
	void initializeDefaults();
};

#endif // STARELEMENT_H
