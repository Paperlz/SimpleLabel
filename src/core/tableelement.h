#ifndef TABLEELEMENT_H
#define TABLEELEMENT_H

#include "labelelement.h"
#include <QJsonObject>
#include <QGraphicsItem>
#include <QPointer>
#include <QVector>

class TableItem; // forward

class TableElement : public labelelement {
public:
    TableElement();
    explicit TableElement(TableItem* item);
    ~TableElement() override = default;

    QGraphicsItem* getItem() const override;
    QString getType() const override { return QStringLiteral("table"); }
    QJsonObject getData() const override;
    void setData(const QJsonObject& data) override;
    void setPos(const QPointF& pos) override;
    QPointF getPos() const override;
    void addToScene(QGraphicsScene* scene) override;

private:
    TableItem* m_item = nullptr; // TableItem 不是 QObject，不适合 QPointer
};

#endif // TABLEELEMENT_H
