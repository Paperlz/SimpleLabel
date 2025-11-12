#ifndef ARROWITEM_H
#define ARROWITEM_H

#include "lineitem.h"

// 箭头：基于 LineItem，在线段末端绘制箭头
class ArrowItem : public LineItem
{
public:
	explicit ArrowItem(QGraphicsItem *parent = nullptr);
	explicit ArrowItem(const QPointF &startPoint, const QPointF &endPoint, QGraphicsItem *parent = nullptr);
	virtual ~ArrowItem() = default;

	// 箭头尺寸参数
	void setHeadLength(qreal len);
	qreal headLength() const { return m_headLength; }

	void setHeadAngle(qreal degrees);
	qreal headAngle() const { return m_headAngle; }

	// 复制支持
	static bool s_hasCopiedItem;
	static QPen s_copiedPen;
	static QPointF s_copiedStartPoint;
	static QPointF s_copiedEndPoint;
	static qreal s_copiedHeadLength;
	static qreal s_copiedHeadAngle;

	void copyItem(ArrowItem *item);

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
	void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
	qreal m_headLength; // 箭头长度（像素）
	qreal m_headAngle;  // 箭头两翼与主线的夹角（度）
	void initializeDefaults();
};

#endif // ARROWITEM_H
