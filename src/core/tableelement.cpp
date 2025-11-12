#include "tableelement.h"
#include "../graphics/tableitem.h"
#include <QGraphicsScene>
#include <QJsonObject>
#include <QJsonArray>

TableElement::TableElement() : m_item(new TableItem(3,3,50,30)) {}
TableElement::TableElement(TableItem* item) : m_item(item) {}

QGraphicsItem* TableElement::getItem() const { return m_item; }

QJsonObject TableElement::getData() const
{
    QJsonObject json;
    if (!m_item) return json;
    json["itemType"] = QStringLiteral("table");
    // 尺寸与网格
    json["rows"] = m_item->rowCount();
    json["cols"] = m_item->columnCount();
    QJsonArray cws; for (qreal w : m_item->columnWidths()) cws.append(w); json["colWidths"] = cws;
    QJsonArray rhs; for (qreal h : m_item->rowHeights()) rhs.append(h); json["rowHeights"] = rhs;
    return json;
}

void TableElement::setData(const QJsonObject& data)
{
    if (!m_item) m_item = new TableItem(2,2,50,30);
    int rows = data.value("rows").toInt(2);
    int cols = data.value("cols").toInt(2);
    m_item->setGrid(rows, cols);
    if (data.contains("colWidths") && data.value("colWidths").isArray()) {
        QVector<qreal> cw; for (auto v : data.value("colWidths").toArray()) cw.append(v.toDouble());
        if (cw.size() == cols) m_item->setColumnWidths(cw);
    }
    if (data.contains("rowHeights") && data.value("rowHeights").isArray()) {
        QVector<qreal> rh; for (auto v : data.value("rowHeights").toArray()) rh.append(v.toDouble());
        if (rh.size() == rows) m_item->setRowHeights(rh);
    }
}

void TableElement::setPos(const QPointF& pos)
{
    if (m_item) m_item->setPos(pos);
}

QPointF TableElement::getPos() const
{
    return m_item ? m_item->pos() : QPointF();
}

void TableElement::addToScene(QGraphicsScene* scene)
{
    if (!scene) return;
    if (!m_item) m_item = new TableItem(3,3,50,30);
    scene->addItem(m_item);
}
