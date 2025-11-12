#include "arrowitem.h"
#include "labelscene.h"
#include "../commands/undomanager.h"
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QtMath>

// 静态成员初始化
bool ArrowItem::s_hasCopiedItem = false;
QPen ArrowItem::s_copiedPen;
QPointF ArrowItem::s_copiedStartPoint;
QPointF ArrowItem::s_copiedEndPoint;
qreal ArrowItem::s_copiedHeadLength = 15.0;
qreal ArrowItem::s_copiedHeadAngle = 30.0;

ArrowItem::ArrowItem(QGraphicsItem *parent)
	: LineItem(parent)
	, m_headLength(15.0)
	, m_headAngle(30.0)
{
	initializeDefaults();
}

ArrowItem::ArrowItem(const QPointF &startPoint, const QPointF &endPoint, QGraphicsItem *parent)
	: LineItem(startPoint, endPoint, parent)
	, m_headLength(15.0)
	, m_headAngle(30.0)
{
	initializeDefaults();
}

void ArrowItem::initializeDefaults()
{
	// 保持与线条一致的初始画笔
	QPen p = pen();
	p.setWidth(2);
	setPen(p);
}

void ArrowItem::setHeadLength(qreal len)
{
	qreal clamped = qMax<qreal>(2.0, len);
	if (!qFuzzyCompare(1.0 + m_headLength, 1.0 + clamped)) {
		m_headLength = clamped;
		update();
	}
}

void ArrowItem::setHeadAngle(qreal degrees)
{
	qreal clamped = qBound<qreal>(5.0, degrees, 85.0);
	if (!qFuzzyCompare(1.0 + m_headAngle, 1.0 + clamped)) {
		m_headAngle = clamped;
		update();
	}
}

void ArrowItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	// 先调用基类绘制主线和手柄
	LineItem::paint(painter, option, widget);

	// 再绘制箭头
	QPointF s = startPoint();
	QPointF e = endPoint();
	QVector2D v(e - s);
	qreal len = v.length();
	if (len < 1e-3) return;
	QVector2D dir = v / len; // 单位向量

	// 箭头两翼方向
	qreal angleRad = qDegreesToRadians(m_headAngle);
	qreal wingLen = m_headLength;

	// 构建一个与 dir 垂直的向量
	QVector2D perp(-dir.y(), dir.x());

	// 左右翼方向向量
	QVector2D wingDir1 = dir * std::cos(angleRad) + perp * std::sin(angleRad);
	QVector2D wingDir2 = dir * std::cos(angleRad) - perp * std::sin(angleRad);

	QPointF p1 = e - wingDir1.toPointF() * wingLen;
	QPointF p2 = e - wingDir2.toPointF() * wingLen;

	QPen oldPen = painter->pen();
	painter->setPen(pen());
	painter->drawLine(e, p1);
	painter->drawLine(e, p2);
	painter->setPen(oldPen);
}

void ArrowItem::copyItem(ArrowItem *item)
{
	if (!item) return;
	s_hasCopiedItem = true;
	s_copiedPen = item->pen();
	s_copiedStartPoint = item->startPoint();
	s_copiedEndPoint = item->endPoint();
	s_copiedHeadLength = item->headLength();
	s_copiedHeadAngle = item->headAngle();
}

void ArrowItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
	QMenu menu;
	QAction *copyAction = menu.addAction(QObject::tr("复制"));
	QAction *cutAction = menu.addAction(QObject::tr("剪切"));
	QAction *pasteAction = menu.addAction(QObject::tr("粘贴"));
	pasteAction->setEnabled(s_hasCopiedItem);
	QAction *deleteAction = menu.addAction(QObject::tr("删除"));
	menu.addSeparator();
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
			labelScene->undoManager()->cutItem(this, QObject::tr("剪切箭头"));
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
	} else if (selected == lockAction) {
		setLocked(!isLocked());
	} else if (selected == alignToggle) {
		setAlignmentEnabled(alignToggle->isChecked());
	}
}

