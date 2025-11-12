#ifndef CIRCLEELEMENT_H
#define CIRCLEELEMENT_H

#include "shapeelement.h"

class CircleItem;

class CircleElement : public ShapeElement
{
public:
    explicit CircleElement();
    explicit CircleElement(const QSizeF &size);
    explicit CircleElement(CircleItem *item);
    virtual ~CircleElement() = default;

    // 重写基类方法
    QString getType() const override;

    // 圆形特有属性
    bool isCircle() const { return m_isCircle; }
    void setIsCircle(bool isCircle);

private:
    bool m_isCircle;  // true为圆形，false为椭圆形
    
    void initializeDefaults();

protected:
    AbstractShapeItem* createGraphicsItem() const override;
    void setShapeType(ShapeType type) override;
};

#endif // CIRCLEELEMENT_H
