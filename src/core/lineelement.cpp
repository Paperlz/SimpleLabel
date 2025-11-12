#include "lineelement.h"
#include "../graphics/lineitem.h"
#include <QJsonObject>
#include <QtMath>
#include <QGraphicsScene>

LineElement::LineElement()
    : labelelement()
    , m_startPoint(0, 0)
    , m_endPoint(100, 0)
    , m_graphicsItem(nullptr)
{
    initializeDefaults();
}

LineElement::LineElement(const QPointF &startPoint, const QPointF &endPoint)
    : labelelement()
    , m_startPoint(startPoint)
    , m_endPoint(endPoint)
    , m_graphicsItem(nullptr)
{
    initializeDefaults();
}

LineElement::LineElement(LineItem *item)
    : labelelement()
    , m_startPoint(0, 0)
    , m_endPoint(100, 0)
    , m_graphicsItem(item)
{
    initializeDefaults();
    if (m_graphicsItem) {
        syncFromItem();
    }
}

void LineElement::initializeDefaults()
{
    // 设置默认画笔
    m_pen = QPen(Qt::black, 2, Qt::SolidLine);
}

QString LineElement::getType() const
{
    return "Line";
}

QJsonObject LineElement::getData() const
{
    QJsonObject json;

    // 保存画笔属性
    QJsonObject penObj;
    penObj["color"] = m_pen.color().name();
    penObj["width"] = m_pen.widthF();
    penObj["style"] = static_cast<int>(m_pen.style());
    json["pen"] = penObj;

    // 保存起点
    QJsonObject startObj;
    startObj["x"] = m_startPoint.x();
    startObj["y"] = m_startPoint.y();
    json["startPoint"] = startObj;

    // 保存终点
    QJsonObject endObj;
    endObj["x"] = m_endPoint.x();
    endObj["y"] = m_endPoint.y();
    json["endPoint"] = endObj;

    return json;
}

void LineElement::setData(const QJsonObject &json)
{
    // 读取画笔属性
    QJsonObject penObj = json["pen"].toObject();
    m_pen.setColor(QColor(penObj["color"].toString()));
    m_pen.setWidthF(penObj["width"].toDouble());
    m_pen.setStyle(static_cast<Qt::PenStyle>(penObj["style"].toInt()));

    // 读取起点
    QJsonObject startObj = json["startPoint"].toObject();
    m_startPoint = QPointF(startObj["x"].toDouble(), startObj["y"].toDouble());

    // 读取终点
    QJsonObject endObj = json["endPoint"].toObject();
    m_endPoint = QPointF(endObj["x"].toDouble(), endObj["y"].toDouble());

    syncToItem();
}

void LineElement::setPos(const QPointF &pos)
{
    if (m_graphicsItem) {
        m_graphicsItem->setPos(pos);
    }
}

QPointF LineElement::getPos() const
{
    if (m_graphicsItem) {
        return m_graphicsItem->pos();
    }
    return QPointF(0, 0);
}

QGraphicsItem* LineElement::getItem() const
{
    return m_graphicsItem;
}

void LineElement::addToScene(QGraphicsScene* scene)
{
    if (!scene) {
        return;
    }

    if (!m_graphicsItem) {
        m_graphicsItem = new LineItem(m_startPoint, m_endPoint);
        syncToItem();
    }

    scene->addItem(m_graphicsItem);
}

void LineElement::setLine(const QPointF &startPoint, const QPointF &endPoint)
{
    m_startPoint = startPoint;
    m_endPoint = endPoint;

    if (m_graphicsItem) {
        m_graphicsItem->setLine(startPoint, endPoint);
    }
}

qreal LineElement::length() const
{
    QPointF diff = m_endPoint - m_startPoint;
    return qSqrt(diff.x() * diff.x() + diff.y() * diff.y());
}

qreal LineElement::angle() const
{
    QPointF diff = m_endPoint - m_startPoint;
    return qAtan2(diff.y(), diff.x());
}

void LineElement::setPen(const QPen &pen)
{
    m_pen = pen;
    if (m_graphicsItem) {
        m_graphicsItem->setPen(pen);
    }
}

void LineElement::setStartPoint(const QPointF &point)
{
    m_startPoint = point;
    if (m_graphicsItem) {
        m_graphicsItem->setStartPoint(point);
    }
}

void LineElement::setEndPoint(const QPointF &point)
{
    m_endPoint = point;
    if (m_graphicsItem) {
        m_graphicsItem->setEndPoint(point);
    }
}

void LineElement::syncFromItem()
{
    if (!m_graphicsItem) {
        return;
    }

    m_pen = m_graphicsItem->pen();
    m_startPoint = m_graphicsItem->startPoint();
    m_endPoint = m_graphicsItem->endPoint();
}

void LineElement::syncToItem()
{
    if (!m_graphicsItem) {
        return;
    }

    m_graphicsItem->setPen(m_pen);
    m_graphicsItem->setLine(m_startPoint, m_endPoint);
}

void LineElement::setDashed(bool dashed)
{
    Qt::PenStyle target = dashed ? Qt::DashLine : Qt::SolidLine;
    if (m_pen.style() != target) {
        m_pen.setStyle(target);
        if (m_graphicsItem) {
            m_graphicsItem->setDashed(dashed);
        }
    }
}
