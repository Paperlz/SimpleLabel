#include "selectionframe.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QMarginsF>
#include <QString>

namespace {
constexpr qreal kDefaultPadding = 6.0;
constexpr qreal kStrokeWidth = 1.0;
constexpr qreal kHitMargin = 8.0;
}

SelectionFrame::SelectionFrame(QGraphicsItem *parent)
	: QGraphicsObject(parent)
	, m_localRect()
	, m_padding(kDefaultPadding)
{
	setAcceptedMouseButtons(Qt::LeftButton);
	setFlag(QGraphicsItem::ItemIsMovable, false);
	setFlag(QGraphicsItem::ItemIsSelectable, false);
	setFlag(QGraphicsItem::ItemIsFocusable, false);
	setZValue(10'000.0);
	setCursor(Qt::SizeAllCursor);
	setData(0, QStringLiteral("selectionFrame"));
	setVisible(false);
}

QRectF SelectionFrame::boundingRect() const
{
	if (m_localRect.isNull()) {
		return QRectF();
	}
	return m_localRect.adjusted(-kStrokeWidth, -kStrokeWidth, kStrokeWidth, kStrokeWidth);
}

QPainterPath SelectionFrame::shape() const
{
	QPainterPath path;
	if (m_localRect.isNull()) {
		return path;
	}

	path.addRect(m_localRect);

	const QRectF innerRect = m_localRect.adjusted(kHitMargin, kHitMargin, -kHitMargin, -kHitMargin);
	if (innerRect.isValid() && innerRect.width() > 0 && innerRect.height() > 0) {
		QPainterPath inner;
		inner.addRect(innerRect);
		path.addPath(inner);
	}

	path.setFillRule(Qt::OddEvenFill);
	return path;
}

void SelectionFrame::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option);
	Q_UNUSED(widget);

	if (m_localRect.isNull()) {
		return;
	}

	painter->save();

	QPen pen(Qt::DashLine);
	pen.setColor(QColor(33, 150, 243));
	pen.setWidthF(kStrokeWidth);
	painter->setPen(pen);
	painter->setBrush(Qt::NoBrush);
	painter->drawRect(m_localRect);

	paintAlignmentLines(painter);

	painter->restore();
}

void SelectionFrame::setItems(const QList<QGraphicsItem*> &items)
{
	m_items.clear();
	m_items.reserve(items.size());

	for (QGraphicsItem *item : items) {
		if (!item || item == this) {
			continue;
		}
		if (!m_items.contains(item)) {
			m_items.append(item);
		}
	}

	if (m_items.size() < 2) {
		prepareGeometryChange();
		m_localRect = QRectF();
		setVisible(false);
		handleMouseReleaseForAlignment();
		return;
	}

	refreshGeometry();
}

QList<QGraphicsItem*> SelectionFrame::trackedItems() const
{
	return m_items;
}

void SelectionFrame::refreshGeometry()
{
	if (m_items.size() < 2) {
		prepareGeometryChange();
		m_localRect = QRectF();
		setVisible(false);
		handleMouseReleaseForAlignment();
		return;
	}

	const QRectF itemsRect = computeItemsBoundingRect();
	if (itemsRect.isNull()) {
		prepareGeometryChange();
		m_localRect = QRectF();
		setVisible(false);
		handleMouseReleaseForAlignment();
		return;
	}

	QRectF paddedRect = itemsRect.marginsAdded(QMarginsF(m_padding, m_padding, m_padding, m_padding));

	prepareGeometryChange();
	m_localRect = QRectF(QPointF(0, 0), paddedRect.size());
	setPos(paddedRect.topLeft());
	setVisible(true);
	update();
}

QRectF SelectionFrame::alignmentRectLocal() const
{
	return m_localRect;
}

bool SelectionFrame::shouldIgnoreForAlignment(QGraphicsItem *candidate) const
{
	return m_items.contains(candidate);
}

QRectF SelectionFrame::computeItemsBoundingRect() const
{
	QRectF result;
	bool hasValidRect = false;

	for (QGraphicsItem *item : m_items) {
		if (!item) {
			continue;
		}

		QRectF itemRect;
		if (auto alignable = dynamic_cast<AlignableItem*>(item)) {
			itemRect = alignable->alignmentSceneRect();
		} else {
			itemRect = item->sceneBoundingRect();
		}

		if (!itemRect.isValid()) {
			continue;
		}

		if (!hasValidRect) {
			result = itemRect;
			hasValidRect = true;
		} else {
			result = result.united(itemRect);
		}
	}

	if (!hasValidRect) {
		return QRectF();
	}
	return result;
}
