#include "polygonitem.h"
#include "labelscene.h"
#include "../commands/undomanager.h"
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QGraphicsSceneMouseEvent>
#include <QtMath>

// 静态成员初始化
bool PolygonItem::s_hasCopiedItem = false;
QPen PolygonItem::s_copiedPen;
QBrush PolygonItem::s_copiedBrush;
bool PolygonItem::s_copiedFillEnabled = true;
bool PolygonItem::s_copiedBorderEnabled = true;
QSizeF PolygonItem::s_copiedSize(80, 80);
QVector<QPointF> PolygonItem::s_copiedPoints;

PolygonItem::PolygonItem(QGraphicsItem *parent)
	: AbstractShapeItem(parent)
{
	initializeDefaults();
}

PolygonItem::PolygonItem(const QSizeF &size, QGraphicsItem *parent)
	: AbstractShapeItem(parent)
{
	setSize(size);
	initializeDefaults();
}

void PolygonItem::initializeDefaults()
{
	setFillEnabled(true);
	setBrush(QBrush(QColor(135, 206, 235), Qt::SolidPattern)); // 天蓝色
	// 默认启用顶点编辑（选中时显示并可拖动）
	setVertexEditingEnabled(true);
}

void PolygonItem::setPoints(const QVector<QPointF> &pts)
{
	m_points = pts;
	update();
}

void PolygonItem::appendPoint(const QPointF &p)
{
	m_points.append(p);
	update();
}

void PolygonItem::updateLastPoint(const QPointF &p)
{
	if (!m_points.isEmpty()) {
		m_points.last() = p;
		update();
	}
}

void PolygonItem::paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
	Q_UNUSED(option)
	Q_UNUSED(widget)

	painter->setPen(m_borderEnabled ? m_pen : Qt::NoPen);
	painter->setBrush(m_fillEnabled ? m_brush : Qt::NoBrush);

	if (m_points.size() >= 2) {
		QPolygonF poly(m_points);
		if (m_points.size() >= 3) {
			painter->drawPolygon(poly);
		} else {
			painter->drawPolyline(poly);
		}
	}

	// 顶点编辑可视化：选中且启用时绘制锚点
	if (isSelected() && m_vertexEditing && !m_points.isEmpty()) {
		const qreal handle = 6.0;
		const qreal half = handle / 2.0;
		QPen hp(Qt::blue, 1);
		QBrush hb(Qt::white);
		painter->setPen(hp);
		painter->setBrush(hb);
		for (int i = 0; i < m_points.size(); ++i) {
			QPointF p = m_points[i];
			QRectF r(p.x() - half, p.y() - half, handle, handle);
			if (i == m_activeVertex) {
				painter->setBrush(QBrush(Qt::cyan));
			} else {
				painter->setBrush(hb);
			}
			painter->drawRect(r);
		}
	}
}

void PolygonItem::copyItem(PolygonItem *item)
{
	if (!item) return;
	s_hasCopiedItem = true;
	s_copiedPen = item->pen();
	s_copiedBrush = item->brush();
	s_copiedFillEnabled = item->fillEnabled();
	s_copiedBorderEnabled = item->borderEnabled();
	s_copiedSize = item->size();
	s_copiedPoints = item->points();
}

void PolygonItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
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
	QAction *vertexEditToggle = menu.addAction(QObject::tr("顶点编辑"));
	vertexEditToggle->setCheckable(true);
	vertexEditToggle->setChecked(m_vertexEditing);

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
			labelScene->undoManager()->cutItem(this, QObject::tr("剪切多边形"));
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
	} else if (selected == vertexEditToggle) {
		setVertexEditingEnabled(vertexEditToggle->isChecked());
	}
}

int PolygonItem::hitTestVertex(const QPointF &pos) const
{
	const qreal handle = 6.0;
	const qreal half = handle / 2.0;
	for (int i = 0; i < m_points.size(); ++i) {
		const QPointF &p = m_points[i];
		QRectF r(p.x() - half, p.y() - half, handle, handle);
		if (r.contains(pos)) return i;
	}
	return -1;
}

void PolygonItem::normalizeGeometry()
{
	if (m_points.isEmpty()) return;
	QPolygonF poly(m_points);
	QRectF bounds = poly.boundingRect();
	if (bounds.topLeft() == QPointF(0,0) && bounds.size() == QRectF(QPointF(0,0), size()).size()) {
		return; // 已经规范
	}
	// 平移点，使最小包围矩形左上角为 (0,0)
	for (QPointF &p : m_points) {
		p -= bounds.topLeft();
	}
	// 更新尺寸并保持场景位置不变
	QPointF oldScenePos = scenePos();
	setSize(bounds.size());
	setPos(pos() + bounds.topLeft());
	Q_UNUSED(oldScenePos);
	update();
}

void PolygonItem::setVertex(int i, const QPointF &p)
{
	if (i < 0 || i >= m_points.size()) return;
	m_points[i] = p;
	// 优化：拖动过程中不每次做归一化（避免频繁 prepareGeometryChange / 重定位）
	// 如果当前处于活动拖动顶点，则只 redraw；否则执行规范化。
	if (m_vertexEditing && m_activeVertex == i) {
		update();
	} else {
		normalizeGeometry();
	}
}

void PolygonItem::insertVertex(int i, const QPointF &p)
{
	if (i < 0) i = 0;
	if (i > m_points.size()) i = m_points.size();
	m_points.insert(i, p);
	normalizeGeometry();
}

void PolygonItem::removeVertex(int i)
{
	if (i < 0 || i >= m_points.size()) return;
	if (m_points.size() <= 3) return; // 最少保留三点
	m_points.removeAt(i);
	normalizeGeometry();
}

void PolygonItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_vertexEditing && event->button() == Qt::LeftButton && isSelected()) {
		// 转换到本地坐标
		QPointF local = event->pos();
		int hit = hitTestVertex(local);
		if (hit >= 0) {
			m_activeVertex = hit;
			m_pressScenePos = event->scenePos();
			m_oldVertexPos = m_points[hit];
			event->accept();
			return;
		}
		// 如果点在空白但处于 resize 手柄拖动，基类逻辑会处理；在此记录多边形缩放起点
	}
	AbstractShapeItem::mousePressEvent(event);
	// 如果即将开始通过选择框手柄进行大小调整（基类已设置 m_isResizing != false）
	if (m_isResizing && !m_resizingBox) {
		m_resizingBox = true;
		m_resizeOldPoints = m_points;
		m_resizeOldSize = size();
		m_resizeOldPos = pos();
		m_pointsOnResizeStart = m_points; // 供缩放比例计算
	}
}

void PolygonItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_vertexEditing && m_activeVertex >= 0) {
		// 使用局部坐标移动该顶点
		QPointF local = event->pos();
		setVertex(m_activeVertex, local);
		event->accept();
		return;
	}
	AbstractShapeItem::mouseMoveEvent(event);
}

void PolygonItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
	if (m_vertexEditing && m_activeVertex >= 0 && event->button() == Qt::LeftButton) {
		if (auto labelScene = qobject_cast<LabelScene*>(scene())) {
			// 推入撤销命令
			QPointF newPos = m_points[m_activeVertex];
			if (labelScene->undoManager() && m_oldVertexPos != newPos) {
				labelScene->undoManager()->movePolygonVertex(this, m_activeVertex, m_oldVertexPos, newPos);
			}
			emit labelScene->sceneModified();
		}
		// 在释放时统一做一次归一化，保证选择框/尺寸同步
		normalizeGeometry();
		m_activeVertex = -1;
		event->accept();
		return;
	}
	// 处理选择框缩放结束的撤销命令
	if (m_resizingBox && event->button() == Qt::LeftButton) {
		m_resizingBox = false;
		// 将当前点/尺寸与旧快照比较
		if ((m_resizeOldSize != size()) || (m_resizeOldPos != pos())) {
			if (auto labelScene = qobject_cast<LabelScene*>(scene())) {
				if (labelScene->undoManager()) {
					labelScene->undoManager()->scalePolygon(this,
						m_resizeOldPoints, m_points,
						m_resizeOldSize, size(),
						m_resizeOldPos, pos());
				}
				emit labelScene->sceneModified();
			}
		}
		m_pointsOnResizeStart.clear();
	}
	AbstractShapeItem::mouseReleaseEvent(event);
}

void PolygonItem::updateSizeFromHandle(HandleType handle, const QPointF &delta)
{
	// 缩放策略：将所有点相对于当前内容矩形按比例缩放，使其始终位于选择框内
	if (m_pointsOnResizeStart.isEmpty()) {
		m_pointsOnResizeStart = m_points; // 记录起点集（已在 mousePress 做过，容错）
	}

	// 复用基类逻辑计算 new size 和 new pos，但不立即提交到成员；这里先复制计算流程
	QSizeF newSize = m_resizeStartSize;
	QPointF newPos = m_resizeStartItemPos;
	const qreal minSize = 10.0;

	switch (handle) {
		case TopLeft:
			newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
			newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
			if (newSize.width() != m_resizeStartSize.width()) {
				newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
			}
			if (newSize.height() != m_resizeStartSize.height()) {
				newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
			}
			break;
		case TopCenter:
			newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
			if (newSize.height() != m_resizeStartSize.height()) {
				newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
			}
			break;
		case TopRight:
			newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
			newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
			if (newSize.height() != m_resizeStartSize.height()) {
				newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
			}
			break;
		case MiddleLeft:
			newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
			if (newSize.width() != m_resizeStartSize.width()) {
				newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
			}
			break;
		case MiddleRight:
			newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
			break;
		case BottomLeft:
			newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
			newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
			if (newSize.width() != m_resizeStartSize.width()) {
				newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
			}
			break;
		case BottomCenter:
			newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
			break;
		case BottomRight:
			newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
			newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
			break;
		default:
			return;
	}

	// 计算缩放比例（基于起始 size → 新 size）
	const qreal sx = (m_resizeStartSize.width()  <= 0.0) ? 1.0 : (newSize.width()  / m_resizeStartSize.width());
	const qreal sy = (m_resizeStartSize.height() <= 0.0) ? 1.0 : (newSize.height() / m_resizeStartSize.height());
	const QPointF origin(0, 0); // 多边形点集是相对于局部 (0,0)

	// 按比例缩放所有点（保持在 [0,newSize] 区间）
	m_points.clear();
	m_points.reserve(m_pointsOnResizeStart.size());
	for (const QPointF &p0 : m_pointsOnResizeStart) {
		QPointF rel = p0 - origin; // 即 p0 本身
		QPointF p1(origin.x() + rel.x() * sx, origin.y() + rel.y() * sy);
		// Clamp 以确保落在新尺寸内
		p1.setX(qBound<qreal>(0.0, p1.x(), newSize.width()));
		p1.setY(qBound<qreal>(0.0, p1.y(), newSize.height()));
		m_points.push_back(p1);
	}

	// 应用新尺寸与位置（遵循基类 resize 的移动规则）
	prepareGeometryChange();
	setPos(newPos);
	AbstractShapeItem::setSize(newSize);
	update();

}

