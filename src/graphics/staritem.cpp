#include "staritem.h"
#include "labelscene.h"
#include "../commands/undomanager.h"
#include "rectangleitem.h"
#include "circleitem.h"
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QtMath>

// 静态成员初始化
bool StarItem::s_hasCopiedItem = false;
QPen StarItem::s_copiedPen;
QBrush StarItem::s_copiedBrush;
bool StarItem::s_copiedFillEnabled = true;
bool StarItem::s_copiedBorderEnabled = true;
QSizeF StarItem::s_copiedSize(80, 80);
int StarItem::s_copiedPoints = 5;
qreal StarItem::s_copiedInnerRatio = 0.5;

StarItem::StarItem(QGraphicsItem *parent)
	: AbstractShapeItem(parent)
	, m_points(5)
	, m_innerRatio(0.5)
{
	initializeDefaults();
}

StarItem::StarItem(const QSizeF &size, QGraphicsItem *parent)
	: AbstractShapeItem(parent)
	, m_points(5)
	, m_innerRatio(0.5)
{
	setSize(size);
	initializeDefaults();
}

void StarItem::initializeDefaults()
{
	setFillEnabled(true);
	setBrush(QBrush(QColor(255, 215, 0), Qt::SolidPattern)); // 金色
}

void StarItem::setPointCount(int n)
{
	int clamped = qMax(3, n);
	if (m_points != clamped) {
		m_points = clamped;
		update();
	}
}

void StarItem::setInnerRatio(qreal r)
{
	qreal clamped = qBound<qreal>(0.0, r, 1.0);
	if (!qFuzzyCompare(1.0 + m_innerRatio, 1.0 + clamped)) {
		m_innerRatio = clamped;
		update();
	}
}

QPolygonF StarItem::buildStarPolygon() const
{
	const int n = m_points;
	if (n < 3) return QPolygonF();

	const QRectF rect(0, 0, m_size.width(), m_size.height());
	const QPointF center = rect.center();
	const qreal R = 0.5 * qMin(rect.width(), rect.height());
	const qreal r = qMax<qreal>(0.0, qMin<qreal>(R, R * m_innerRatio));

	QPolygonF poly;
	poly.reserve(n * 2);
	// 从顶部开始（-90 度）
	const qreal startAngle = -M_PI_2;
	for (int i = 0; i < n; ++i) {
		qreal outerA = startAngle + (2.0 * M_PI * i) / n;
		qreal innerA = outerA + M_PI / n;
		QPointF outer(center.x() + R * std::cos(outerA), center.y() + R * std::sin(outerA));
		QPointF inner(center.x() + r * std::cos(innerA), center.y() + r * std::sin(innerA));
		poly << outer << inner;
	}
	return poly;
}

void StarItem::paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	// 设置画笔和画刷
	painter->setPen(m_borderEnabled ? m_pen : Qt::NoPen);
	painter->setBrush(m_fillEnabled ? m_brush : Qt::NoBrush);

	QPolygonF star = buildStarPolygon();
	if (star.size() >= 3) {
		painter->drawPolygon(star);
	}
}

void StarItem::copyItem(StarItem *item)
{
	if (!item) return;
	s_hasCopiedItem = true;
	s_copiedPen = item->pen();
	s_copiedBrush = item->brush();
	s_copiedFillEnabled = item->fillEnabled();
	s_copiedBorderEnabled = item->borderEnabled();
	s_copiedSize = item->size();
	s_copiedPoints = item->pointCount();
	s_copiedInnerRatio = item->innerRatio();

	// 清除其他类型复制状态（与项目内一致的做法）
	RectangleItem::s_hasCopiedItem = false;
	CircleItem::s_hasCopiedItem = false;
}

void StarItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	QMenu menu;
	QAction *copyAction = menu.addAction(QObject::tr("复制"));
	QAction *cutAction = menu.addAction(QObject::tr("剪切"));
	QAction *pasteAction = menu.addAction(QObject::tr("粘贴"));
	pasteAction->setEnabled(s_hasCopiedItem);
	QAction *deleteAction = menu.addAction(QObject::tr("删除"));
	menu.addSeparator();
	QAction *fillAction = menu.addAction(QObject::tr("启用填充"));
	fillAction->setCheckable(true);
	fillAction->setChecked(m_fillEnabled);
	QAction *borderAction = menu.addAction(QObject::tr("启用边框"));
	borderAction->setCheckable(true);
	borderAction->setChecked(m_borderEnabled);
	QAction *lockAction = menu.addAction(isLocked() ? QObject::tr("解除锁定") : QObject::tr("锁定"));
	QAction *alignToggle = menu.addAction(QObject::tr("启用对齐"));
	alignToggle->setCheckable(true);
	alignToggle->setChecked(isAlignmentEnabled());

	if (!isSelected()) {
		scene()->clearSelection();
		setSelected(true);
	}

	QAction *selected = menu.exec(event->screenPos());
	if (selected == copyAction) {
		copyItem(this);
	} else if (selected == cutAction) {
		auto labelScene = qobject_cast<LabelScene*>(scene());
		if (labelScene && labelScene->undoManager()) {
			labelScene->undoManager()->cutItem(this, QObject::tr("剪切星形"));
		} else {
			copyItem(this);
			if (scene()) { scene()->removeItem(this); delete this; }
		}
	} else if (selected == deleteAction) {
		if (scene()) {
			if (!isSelected()) { scene()->clearSelection(); setSelected(true);}            
			auto labelScene = qobject_cast<LabelScene*>(scene());
			if (labelScene) { labelScene->removeSelectedItems(); }
		}
	} else if (selected == fillAction) {
		setFillEnabled(fillAction->isChecked());
	} else if (selected == borderAction) {
		setBorderEnabled(borderAction->isChecked());
	} else if (selected == lockAction) {
		setLocked(!isLocked());
	} else if (selected == alignToggle) {
		setAlignmentEnabled(alignToggle->isChecked());
	}
}

