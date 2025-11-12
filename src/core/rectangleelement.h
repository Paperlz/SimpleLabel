#ifndef RECTANGLEELEMENT_H
#define RECTANGLEELEMENT_H

#include "shapeelement.h"

class RectangleItem;

class RectangleElement : public ShapeElement
{
public:
    explicit RectangleElement();
    explicit RectangleElement(const QSizeF &size);
    explicit RectangleElement(RectangleItem *item);
    virtual ~RectangleElement() = default;

    // 重写基类方法
    QString getType() const override;
    QJsonObject getData() const override;
    void setData(const QJsonObject &data) override;

private:
    void initializeDefaults();

protected:
    AbstractShapeItem* createGraphicsItem() const override;

private:
    double m_cornerRadius = 0.0;
};

#endif // RECTANGLEELEMENT_H
