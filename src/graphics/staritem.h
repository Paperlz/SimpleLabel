#ifndef STARITEM_H
#define STARITEM_H

#include "abstractshapeitem.h"

// 星形图元：在 m_size 范围内绘制一个 N 角星
class StarItem : public AbstractShapeItem
{
public:
	explicit StarItem(QGraphicsItem *parent = nullptr);
	explicit StarItem(const QSizeF &size, QGraphicsItem *parent = nullptr);
	virtual ~StarItem() = default;

	// 星形参数
	void setPointCount(int n);
	int pointCount() const { return m_points; }

	// 内外半径比例（0-1）
	void setInnerRatio(qreal r);
	qreal innerRatio() const { return m_innerRatio; }

	// 复制支持（与项目内其它类型一致的模式）
	static bool s_hasCopiedItem;
	static QPen s_copiedPen;
	static QBrush s_copiedBrush;
	static bool s_copiedFillEnabled;
	static bool s_copiedBorderEnabled;
	static QSizeF s_copiedSize;
	static int s_copiedPoints;
	static qreal s_copiedInnerRatio;

	void copyItem(StarItem *item);

protected:
	void paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
	void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
	int m_points;        // 角数（>=3）
	qreal m_innerRatio;  // 内半径比例 0..1
	void initializeDefaults();
	QPolygonF buildStarPolygon() const;
};

#endif // STARITEM_H
