#ifndef POLYGONITEM_H
#define POLYGONITEM_H

#include "abstractshapeitem.h"
#include <QVector>

// 任意多边形（简单多边形，不做自交检查），点坐标为本地坐标
class PolygonItem : public AbstractShapeItem
{
public:
	explicit PolygonItem(QGraphicsItem *parent = nullptr);
	explicit PolygonItem(const QSizeF &size, QGraphicsItem *parent = nullptr);
	virtual ~PolygonItem() = default;

	// 设置/获取点集（局部坐标）
	void setPoints(const QVector<QPointF>& pts);
	QVector<QPointF> points() const { return m_points; }

	// 动态编辑：添加/更新末尾点（用于绘制预览）
	void appendPoint(const QPointF& p);
	void updateLastPoint(const QPointF& p);

	// 复制支持
	static bool s_hasCopiedItem;
	static QPen s_copiedPen;
	static QBrush s_copiedBrush;
	static bool s_copiedFillEnabled;
	static bool s_copiedBorderEnabled;
	static QSizeF s_copiedSize;
	static QVector<QPointF> s_copiedPoints;

	void copyItem(PolygonItem *item);

	// 顶点编辑支持
	int vertexCount() const { return m_points.size(); }
	QPointF vertexAt(int i) const { return (i>=0 && i<m_points.size()) ? m_points[i] : QPointF(); }
	void setVertex(int i, const QPointF &p);
	void insertVertex(int i, const QPointF &p);
	void removeVertex(int i);

	// 进入/退出顶点编辑（外部可扩展为专用模式，这里用标志）
	void setVertexEditingEnabled(bool on) { m_vertexEditing = on; update(); }
	bool vertexEditingEnabled() const { return m_vertexEditing; }


protected:
	void paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
	void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
	void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
	void updateSizeFromHandle(HandleType handle, const QPointF &delta) override;

	void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
	int hitTestVertex(const QPointF &pos) const; // 命中测试（局部坐标）
	void normalizeGeometry(); // 归一化尺寸/位置与点集
	bool m_vertexEditing = false;
	int m_activeVertex = -1;
	QPointF m_pressScenePos; // 用于判断拖动
	QPointF m_oldVertexPos;  // 撤销所需旧位置（暂存）
	QVector<QPointF> m_pointsOnResizeStart; // 记录开始调整时的点集
	// 选择框缩放撤销所需的快照
	bool m_resizingBox = false;
	QVector<QPointF> m_resizeOldPoints;
	QSizeF m_resizeOldSize;
	QPointF m_resizeOldPos;
	QVector<QPointF> m_points; // 局部坐标
	void initializeDefaults();
};

#endif // POLYGONITEM_H
