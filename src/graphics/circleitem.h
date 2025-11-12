#ifndef CIRCLEITEM_H
#define CIRCLEITEM_H

#include "abstractshapeitem.h"

class CircleItem : public AbstractShapeItem
{
public:
    explicit CircleItem(QGraphicsItem *parent = nullptr);
    explicit CircleItem(const QSizeF &size, QGraphicsItem *parent = nullptr);
    virtual ~CircleItem() = default;

    // 圆形特有属性
    bool isCircle() const { return m_isCircle; }
    void setIsCircle(bool isCircle);

    // 静态成员变量，用于复制粘贴功能
    static bool s_hasCopiedItem;
    static QPen s_copiedPen;
    static QBrush s_copiedBrush;
    static bool s_copiedFillEnabled;
    static bool s_copiedBorderEnabled;
    static QSizeF s_copiedSize;
    static bool s_copiedIsCircle;

    // 复制方法
    void copyItem(CircleItem *item);

protected:
    // 实现抽象方法
    void paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    // 重写调整大小方法以支持圆形约束
    void updateSizeFromHandle(HandleType handle, const QPointF &delta) override;

    // 重写右键菜单以提供圆形特定的选项
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

private:
    bool m_isCircle;  // true为圆形，false为椭圆形
    
    void initializeDefaults();
    QSizeF calculateCircleSize(const QSizeF &size) const;
};

#endif // CIRCLEITEM_H
