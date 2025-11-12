#ifndef RECTANGLEITEM_H
#define RECTANGLEITEM_H

#include "abstractshapeitem.h"

class RectangleItem : public AbstractShapeItem
{
public:
    explicit RectangleItem(QGraphicsItem *parent = nullptr);
    explicit RectangleItem(const QSizeF &size, QGraphicsItem *parent = nullptr);
    virtual ~RectangleItem() = default;

    // 静态成员变量，用于复制粘贴功能
    static bool s_hasCopiedItem;
    static QPen s_copiedPen;
    static QBrush s_copiedBrush;
    static bool s_copiedFillEnabled;
    static bool s_copiedBorderEnabled;
    static QSizeF s_copiedSize;
    static double s_copiedCornerRadius; // 复制的圆角半径

    // 复制方法
    void copyItem(RectangleItem *item);

    // 圆角半径
    void setCornerRadius(double radius);
    double cornerRadius() const { return m_cornerRadius; }

protected:
    // 实现抽象方法
    void paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // 重写右键菜单以提供矩形特定的选项
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    void initializeDefaults();

    double m_cornerRadius = 0.0; // 圆角半径，0 表示直角
};

#endif // RECTANGLEITEM_H
