#ifndef SELECTIONFRAME_H
#define SELECTIONFRAME_H

#include <QGraphicsObject>
#include <QList>
#include <QRectF>
#include <QPainterPath>
#include "alignableitem.h"

class SelectionFrame : public QGraphicsObject, public AlignableItem
{
	Q_OBJECT

public:
	explicit SelectionFrame(QGraphicsItem *parent = nullptr);
	~SelectionFrame() override = default;

	QRectF boundingRect() const override;
	QPainterPath shape() const override;
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

	void setItems(const QList<QGraphicsItem*> &items);
	QList<QGraphicsItem*> trackedItems() const;
	void refreshGeometry();

	// AlignableItem interface
	QGraphicsItem* asGraphicsItem() override { return this; }
	const QGraphicsItem* asGraphicsItem() const override { return this; }

protected:
	QRectF alignmentRectLocal() const override;
	bool shouldIgnoreForAlignment(QGraphicsItem *candidate) const override;

private:
	QRectF computeItemsBoundingRect() const;
	QList<QGraphicsItem*> m_items;
	QRectF m_localRect;
	qreal m_padding;
};

#endif // SELECTIONFRAME_H
