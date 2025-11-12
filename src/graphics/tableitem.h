#ifndef TABLEITEM_H
#define TABLEITEM_H

#include <QGraphicsItem>
#include <QVector>
#include <QPen>
#include <QBrush>
#include <QMenu>
#include <QString>

class TableItem : public QGraphicsItem
{
public:
    TableItem(int rows = 2, int cols = 2, qreal cellW = 50, qreal cellH = 30, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    int rowCount() const { return m_rows; }
    int columnCount() const { return m_cols; }

    void setGrid(int rows, int cols);
    void setCellSize(qreal w, qreal h);

    QVector<qreal> columnWidths() const { return m_colWidths; }
    QVector<qreal> rowHeights() const { return m_rowHeights; }
    void setColumnWidths(const QVector<qreal>& w);
    void setRowHeights(const QVector<qreal>& h);
    void setStructure(const QVector<qreal>& columnWidths, const QVector<qreal>& rowHeights);

    // 序列化辅助
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& obj);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

private:
    enum DragType { None, DragCol, DragRow, DragLeftEdge, DragRightEdge, DragTopEdge, DragBottomEdge };
    DragType m_dragType = None;
    int m_dragIndex = -1; // 列或行索引（分隔线前的索引）
    QPointF m_lastPos;
    QPointF m_lastScenePos;

    int m_rows;
    int m_cols;
    QVector<qreal> m_colWidths; // size = cols
    QVector<qreal> m_rowHeights; // size = rows

    qreal m_minColWidth = 10.0;
    qreal m_minRowHeight = 10.0;

    QPen m_gridPen{Qt::black, 1};
    QBrush m_background{Qt::transparent};

    int hitTestColumnHandle(const QPointF& pos) const; // 返回列分隔左侧列索引
    int hitTestRowHandle(const QPointF& pos) const;    // 返回行分隔上方行索引
    bool hitTestLeftEdge(const QPointF& pos) const;
    bool hitTestRightEdge(const QPointF& pos) const;
    bool hitTestTopEdge(const QPointF& pos) const;
    bool hitTestBottomEdge(const QPointF& pos) const;

    int rowAtPos(const QPointF& pos) const; // 精确定位点击所在行
    int columnAtPos(const QPointF& pos) const; // 精确定位点击所在列

    // 分隔线增删操作
    bool splitRow(int rowIndex);
    bool splitColumn(int colIndex);
    bool removeRowSeparator(int sepIndex);
    bool removeColumnSeparator(int sepIndex);

    void emitStructureChangeUndo(const QVector<qreal>& oldCols,
                                 const QVector<qreal>& oldRows,
                                 const QString& description);
};

#endif // TABLEITEM_H
