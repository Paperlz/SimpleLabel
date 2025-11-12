#include "shapeelement.h"

#include "../graphics/abstractshapeitem.h"
#include "../graphics/rectangleitem.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QGraphicsScene>

ShapeElement::ShapeElement(ShapeType type)
    : labelelement()
    , m_shapeType(type)
    , m_fillEnabled(false)
    , m_borderEnabled(true)
    , m_size(100, 100)
    , m_startPoint(0, 0)
    , m_endPoint(100, 100)
    , m_graphicsItem(nullptr)
{
    initializeDefaults();
}

ShapeElement::ShapeElement(AbstractShapeItem *item, ShapeType type)
    : labelelement()
    , m_shapeType(type)
    , m_fillEnabled(false)
    , m_borderEnabled(true)
    , m_size(100, 100)
    , m_startPoint(0, 0)
    , m_endPoint(100, 100)
    , m_graphicsItem(item)
{
    initializeDefaults();
    syncFromItem();
}

void ShapeElement::initializeDefaults()
{
    // 设置默认画笔和画刷
    m_pen = QPen(Qt::black, 2, Qt::SolidLine);
    m_brush = QBrush(Qt::blue, Qt::SolidPattern);
}

QString ShapeElement::getType() const
{
    return "Shape";
}

QJsonObject ShapeElement::getData() const
{
    QJsonObject json;

    json["shapeType"] = shapeTypeToString();
    json["fillEnabled"] = m_fillEnabled;
    json["borderEnabled"] = m_borderEnabled;

    // 保存尺寸
    QJsonObject sizeObj;
    sizeObj["width"] = m_size.width();
    sizeObj["height"] = m_size.height();
    json["size"] = sizeObj;

    // 保存画笔属性
    QJsonObject penObj;
    penObj["color"] = m_pen.color().name();
    penObj["width"] = m_pen.widthF();
    penObj["style"] = static_cast<int>(m_pen.style());
    json["pen"] = penObj;

    // 保存画刷属性
    QJsonObject brushObj;
    brushObj["color"] = m_brush.color().name();
    brushObj["style"] = static_cast<int>(m_brush.style());
    json["brush"] = brushObj;

    // 如果是线条，保存起点和终点
    if (m_shapeType == Line) {
        QJsonObject startObj;
        startObj["x"] = m_startPoint.x();
        startObj["y"] = m_startPoint.y();
        json["startPoint"] = startObj;

        QJsonObject endObj;
        endObj["x"] = m_endPoint.x();
        endObj["y"] = m_endPoint.y();
        json["endPoint"] = endObj;
    }

    return json;
}

void ShapeElement::setData(const QJsonObject &json)
{
    setShapeType(stringToShapeType(json["shapeType"].toString()));
    m_fillEnabled = json["fillEnabled"].toBool();
    m_borderEnabled = json["borderEnabled"].toBool();

    // 读取尺寸
    QJsonObject sizeObj = json["size"].toObject();
    m_size = QSizeF(sizeObj["width"].toDouble(), sizeObj["height"].toDouble());

    // 读取画笔属性
    QJsonObject penObj = json["pen"].toObject();
    m_pen.setColor(QColor(penObj["color"].toString()));
    m_pen.setWidthF(penObj["width"].toDouble());
    m_pen.setStyle(static_cast<Qt::PenStyle>(penObj["style"].toInt()));

    // 读取画刷属性
    QJsonObject brushObj = json["brush"].toObject();
    m_brush.setColor(QColor(brushObj["color"].toString()));
    m_brush.setStyle(static_cast<Qt::BrushStyle>(brushObj["style"].toInt()));

    // 如果是线条，读取起点和终点
    if (m_shapeType == Line) {
        QJsonObject startObj = json["startPoint"].toObject();
        m_startPoint = QPointF(startObj["x"].toDouble(), startObj["y"].toDouble());

        QJsonObject endObj = json["endPoint"].toObject();
        m_endPoint = QPointF(endObj["x"].toDouble(), endObj["y"].toDouble());
    }

    syncToItem();
}

void ShapeElement::setPos(const QPointF &pos)
{
    if (m_graphicsItem) {
        m_graphicsItem->setPos(pos);
    }
}

QPointF ShapeElement::getPos() const
{
    if (m_graphicsItem) {
        return m_graphicsItem->pos();
    }
    return QPointF(0, 0);
}

QGraphicsItem* ShapeElement::getItem() const
{
    return m_graphicsItem;
}

void ShapeElement::addToScene(QGraphicsScene* scene)
{
    if (!scene) {
        return;
    }

    if (!m_graphicsItem) {
        m_graphicsItem = createGraphicsItem();
        syncToItem();
    }

    if (m_graphicsItem) {
        scene->addItem(m_graphicsItem);
    }
}

QString ShapeElement::shapeTypeToString() const
{
    switch (m_shapeType) {
        case Line: return "Line";
        case Rectangle: return "Rectangle";
        case Circle: return "Circle";
        case Ellipse: return "Ellipse";
        default: return "Rectangle";
    }
}

ShapeElement::ShapeType ShapeElement::stringToShapeType(const QString &str)
{
    const QString lower = str.trimmed().toLower();
    if (lower == "line") return Line;
    if (lower == "rectangle") return Rectangle;
    if (lower == "circle") return Circle;
    if (lower == "ellipse") return Ellipse;
    return Rectangle;  // 默认值
}

void ShapeElement::setShapeType(ShapeType type)
{
    m_shapeType = type;
}

void ShapeElement::setPen(const QPen &pen)
{
    m_pen = pen;
    syncToItem();
}

void ShapeElement::setBrush(const QBrush &brush)
{
    m_brush = brush;
    syncToItem();
}

void ShapeElement::setFillEnabled(bool enabled)
{
    m_fillEnabled = enabled;
    syncToItem();
}

void ShapeElement::setBorderEnabled(bool enabled)
{
    m_borderEnabled = enabled;
    syncToItem();
}

void ShapeElement::setSize(const QSizeF &size)
{
    m_size = size;
    syncToItem();
}

void ShapeElement::syncFromItem()
{
    if (!m_graphicsItem) {
        return;
    }

    m_pen = m_graphicsItem->pen();
    m_brush = m_graphicsItem->brush();
    m_fillEnabled = m_graphicsItem->fillEnabled();
    m_borderEnabled = m_graphicsItem->borderEnabled();
    m_size = m_graphicsItem->size();
}

void ShapeElement::syncToItem()
{
    if (!m_graphicsItem) {
        return;
    }

    m_graphicsItem->setPen(m_pen);
    m_graphicsItem->setBrush(m_brush);
    m_graphicsItem->setFillEnabled(m_fillEnabled);
    m_graphicsItem->setBorderEnabled(m_borderEnabled);
    m_graphicsItem->setSize(m_size);
}

AbstractShapeItem* ShapeElement::createGraphicsItem() const
{
    return new RectangleItem(m_size);
}
