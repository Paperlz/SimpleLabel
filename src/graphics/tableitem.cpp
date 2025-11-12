#include "tableitem.h"
#include "labelscene.h"
#include <QPainter>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QJsonObject>
#include <QJsonArray>
#include <QtMath>
#include <QObject>
#include "../commands/undomanager.h"

TableItem::TableItem(int rows, int cols, qreal cellW, qreal cellH, QGraphicsItem* parent)
    : QGraphicsItem(parent), m_rows(rows), m_cols(cols)
{
    if (m_rows < 1) m_rows = 1;
    if (m_cols < 1) m_cols = 1;
    m_colWidths.fill(cellW, m_cols);
    m_rowHeights.fill(cellH, m_rows);
    setFlags(ItemIsSelectable | ItemIsMovable);
    setAcceptHoverEvents(true);
}

QRectF TableItem::boundingRect() const
{
    qreal w = 0.0;
    for (qreal cw : m_colWidths) w += cw;
    qreal h = 0.0;
    for (qreal rh : m_rowHeights) h += rh;
    return QRectF(0,0,w,h);
}

void TableItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    Q_UNUSED(option); Q_UNUSED(widget);
    const QRectF rect = boundingRect();
    painter->fillRect(rect, m_background);
    painter->setPen(m_gridPen);

    // 外框
    painter->drawRect(rect);

    // 竖线
    qreal x = 0.0;
    for (int c = 0; c < m_cols - 1; ++c) {
        x += m_colWidths[c];
        painter->drawLine(QPointF(x, 0), QPointF(x, rect.height()));
    }

    // 横线
    qreal y = 0.0;
    for (int r = 0; r < m_rows - 1; ++r) {
        y += m_rowHeights[r];
        painter->drawLine(QPointF(0, y), QPointF(rect.width(), y));
    }

    if (isSelected()) {
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->drawRect(rect.adjusted(0.5,0.5,-0.5,-0.5));
    }
}

void TableItem::setGrid(int rows, int cols)
{
    if (rows < 1) rows = 1;
    if (cols < 1) cols = 1;
    if (rows != m_rows) {
        QVector<qreal> newHeights;
        newHeights.reserve(rows);
        for (int i=0;i<rows;++i) newHeights.append(i < m_rowHeights.size() ? m_rowHeights[i] : (m_rowHeights.isEmpty()?30.0:m_rowHeights.last()));
        m_rowHeights = newHeights;
        m_rows = rows;
    }
    if (cols != m_cols) {
        QVector<qreal> newWidths;
        newWidths.reserve(cols);
        for (int i=0;i<cols;++i) newWidths.append(i < m_colWidths.size() ? m_colWidths[i] : (m_colWidths.isEmpty()?50.0:m_colWidths.last()));
        m_colWidths = newWidths;
        m_cols = cols;
    }
    prepareGeometryChange();
}

void TableItem::setCellSize(qreal w, qreal h)
{
    if (w < m_minColWidth) w = m_minColWidth;
    if (h < m_minRowHeight) h = m_minRowHeight;
    for (int i=0;i<m_cols;++i) m_colWidths[i] = w;
    for (int j=0;j<m_rows;++j) m_rowHeights[j] = h;
    prepareGeometryChange();
}

void TableItem::setColumnWidths(const QVector<qreal>& w)
{
    if (w.size() != m_cols) return;
    for (int i=0;i<m_cols;++i) m_colWidths[i] = qMax(m_minColWidth, w[i]);
    prepareGeometryChange();
    update();
}

void TableItem::setRowHeights(const QVector<qreal>& h)
{
    if (h.size() != m_rows) return;
    for (int i=0;i<m_rows;++i) m_rowHeights[i] = qMax(m_minRowHeight, h[i]);
    prepareGeometryChange();
    update();
}

void TableItem::setStructure(const QVector<qreal>& columnWidths, const QVector<qreal>& rowHeights)
{
    if (columnWidths.isEmpty() || rowHeights.isEmpty()) {
        return;
    }

    QVector<qreal> cols = columnWidths;
    QVector<qreal> rows = rowHeights;
    for (qreal& cw : cols) {
        cw = qMax(m_minColWidth, cw);
    }
    for (qreal& rh : rows) {
        rh = qMax(m_minRowHeight, rh);
    }

    prepareGeometryChange();
    m_colWidths = cols;
    m_rowHeights = rows;
    m_cols = m_colWidths.size();
    m_rows = m_rowHeights.size();
    update();
}

QJsonObject TableItem::toJson() const
{
    QJsonObject obj;
    obj["itemType"] = "table";
    obj["rows"] = m_rows;
    obj["cols"] = m_cols;
    QJsonArray cws;
    for (qreal cw : m_colWidths) cws.append(cw);
    obj["colWidths"] = cws;
    QJsonArray rhs;
    for (qreal rh : m_rowHeights) rhs.append(rh);
    obj["rowHeights"] = rhs;
    return obj;
}

void TableItem::fromJson(const QJsonObject& obj)
{
    int rows = obj.value("rows").toInt(2);
    int cols = obj.value("cols").toInt(2);
    setGrid(rows, cols);
    if (obj.contains("colWidths") && obj.value("colWidths").isArray()) {
        QJsonArray a = obj.value("colWidths").toArray();
        QVector<qreal> w; w.reserve(a.size());
        for (auto v : a) w.append(v.toDouble());
        if (w.size() == m_cols) m_colWidths = w;
    }
    if (obj.contains("rowHeights") && obj.value("rowHeights").isArray()) {
        QJsonArray a = obj.value("rowHeights").toArray();
        QVector<qreal> h; h.reserve(a.size());
        for (auto v : a) h.append(v.toDouble());
        if (h.size() == m_rows) m_rowHeights = h;
    }
    prepareGeometryChange();
}

int TableItem::hitTestColumnHandle(const QPointF& pos) const
{
    qreal x = 0.0;
    for (int c=0;c<m_cols-1;++c) {
        x += m_colWidths[c];
        if (qAbs(pos.x() - x) < 4.0 && pos.y() >= 0 && pos.y() <= boundingRect().height()) {
            return c; // 分隔线前的列索引
        }
    }
    return -1;
}

int TableItem::hitTestRowHandle(const QPointF& pos) const
{
    qreal y = 0.0;
    for (int r=0;r<m_rows-1;++r) {
        y += m_rowHeights[r];
        if (qAbs(pos.y() - y) < 4.0 && pos.x() >= 0 && pos.x() <= boundingRect().width()) {
            return r; // 分隔线上方行索引
        }
    }
    return -1;
}

bool TableItem::hitTestLeftEdge(const QPointF& pos) const {
    return qAbs(pos.x() - 0.0) < 4.0 && pos.y() >= 0.0 && pos.y() <= boundingRect().height();
}
bool TableItem::hitTestRightEdge(const QPointF& pos) const {
    return qAbs(pos.x() - boundingRect().width()) < 4.0 && pos.y() >= 0.0 && pos.y() <= boundingRect().height();
}
bool TableItem::hitTestTopEdge(const QPointF& pos) const {
    return qAbs(pos.y() - 0.0) < 4.0 && pos.x() >= 0.0 && pos.x() <= boundingRect().width();
}
bool TableItem::hitTestBottomEdge(const QPointF& pos) const {
    return qAbs(pos.y() - boundingRect().height()) < 4.0 && pos.x() >= 0.0 && pos.x() <= boundingRect().width();
}

int TableItem::rowAtPos(const QPointF& pos) const {
    if (pos.y() < 0) return -1;
    qreal yAccum = 0.0;
    for (int r=0; r<m_rows; ++r) {
        qreal h = m_rowHeights[r];
        if (pos.y() >= yAccum && pos.y() < yAccum + h) return r;
        yAccum += h;
    }
    return -1;
}

int TableItem::columnAtPos(const QPointF& pos) const {
    if (pos.x() < 0) return -1;
    qreal xAccum = 0.0;
    for (int c=0; c<m_cols; ++c) {
        qreal w = m_colWidths[c];
        if (pos.x() >= xAccum && pos.x() < xAccum + w) return c;
        xAccum += w;
    }
    return -1;
}

bool TableItem::splitRow(int rowIndex) {
    if (rowIndex < 0 || rowIndex >= m_rows) return false;

    QVector<qreal> oldCols = columnWidths();
    QVector<qreal> oldRows = rowHeights();

    QVector<qreal> newRows = oldRows;
    qreal orig = newRows[rowIndex];
    qreal first = qMax(m_minRowHeight, orig / 2.0);
    qreal second = qMax(m_minRowHeight, orig - first);

    newRows[rowIndex] = first;
    newRows.insert(rowIndex + 1, second);

    setStructure(oldCols, newRows);
    emitStructureChangeUndo(oldCols, oldRows, QObject::tr("拆分表格行"));
    return true;
}

bool TableItem::splitColumn(int colIndex) {
    if (colIndex < 0 || colIndex >= m_cols) return false;

    QVector<qreal> oldCols = columnWidths();
    QVector<qreal> oldRows = rowHeights();

    QVector<qreal> newCols = oldCols;
    qreal orig = newCols[colIndex];
    qreal first = qMax(m_minColWidth, orig / 2.0);
    qreal second = qMax(m_minColWidth, orig - first);

    newCols[colIndex] = first;
    newCols.insert(colIndex + 1, second);

    setStructure(newCols, oldRows);
    emitStructureChangeUndo(oldCols, oldRows, QObject::tr("拆分表格列"));
    return true;
}

bool TableItem::removeRowSeparator(int sepIndex) {
    if (sepIndex < 0 || sepIndex >= m_rows - 1) return false;

    QVector<qreal> oldCols = columnWidths();
    QVector<qreal> oldRows = rowHeights();

    QVector<qreal> newRows = oldRows;
    newRows[sepIndex] += newRows[sepIndex + 1];
    newRows.remove(sepIndex + 1);

    setStructure(oldCols, newRows);
    emitStructureChangeUndo(oldCols, oldRows, QObject::tr("删除表格行分隔线"));
    return true;
}

bool TableItem::removeColumnSeparator(int sepIndex) {
    if (sepIndex < 0 || sepIndex >= m_cols - 1) return false;

    QVector<qreal> oldCols = columnWidths();
    QVector<qreal> oldRows = rowHeights();

    QVector<qreal> newCols = oldCols;
    newCols[sepIndex] += newCols[sepIndex + 1];
    newCols.remove(sepIndex + 1);

    setStructure(newCols, oldRows);
    emitStructureChangeUndo(oldCols, oldRows, QObject::tr("删除表格列分隔线"));
    return true;
}

void TableItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    const QPointF localPos = event->pos();
    int colIdx = hitTestColumnHandle(localPos);
    int rowIdx = hitTestRowHandle(localPos);
    // 先检测内部分隔线
    if (colIdx >= 0) {
        m_dragType = DragCol;
        m_dragIndex = colIdx;
        m_lastPos = localPos;
        event->accept();
        return;
    } else if (rowIdx >= 0) {
        m_dragType = DragRow;
        m_dragIndex = rowIdx;
        m_lastPos = localPos;
        event->accept();
        return;
    }
    // 检测外边框
    if (hitTestLeftEdge(localPos)) {
        m_dragType = DragLeftEdge;
        m_lastPos = localPos;
        m_lastScenePos = event->scenePos();
        event->accept();
        return;
    } else if (hitTestRightEdge(localPos)) {
        m_dragType = DragRightEdge;
        m_lastPos = localPos;
        event->accept();
        return;
    } else if (hitTestTopEdge(localPos)) {
        m_dragType = DragTopEdge;
        m_lastPos = localPos;
        m_lastScenePos = event->scenePos();
        event->accept();
        return;
    } else if (hitTestBottomEdge(localPos)) {
        m_dragType = DragBottomEdge;
        m_lastPos = localPos;
        event->accept();
        return;
    }
    m_dragType = None;
    QGraphicsItem::mousePressEvent(event);
}

void TableItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_dragType == DragCol && m_dragIndex >= 0) {
        qreal dx = event->pos().x() - m_lastPos.x();
        // 左列增加，右列减少保持总宽度 OR 自由调整：这里采用左右列独立（拖动改变左列宽度，右列起点随之移动）
        qreal newWidth = qMax(m_minColWidth, m_colWidths[m_dragIndex] + dx);
        qreal delta = newWidth - m_colWidths[m_dragIndex];
        m_colWidths[m_dragIndex] = newWidth;
        m_lastPos = event->pos();
        prepareGeometryChange();
        update();
        event->accept();
        return;
    } else if (m_dragType == DragRow && m_dragIndex >= 0) {
        qreal dy = event->pos().y() - m_lastPos.y();
        qreal newHeight = qMax(m_minRowHeight, m_rowHeights[m_dragIndex] + dy);
        qreal delta = newHeight - m_rowHeights[m_dragIndex];
        m_rowHeights[m_dragIndex] = newHeight;
        m_lastPos = event->pos();
        prepareGeometryChange();
        update();
        event->accept();
        return;
    } else if (m_dragType == DragLeftEdge) {
        // 左边框拖动：改变第一列宽度并整体左移
        qreal dxScene = event->scenePos().x() - m_lastScenePos.x();
        if (!m_colWidths.isEmpty()) {
            qreal newFirst = qMax(m_minColWidth, m_colWidths.first() - dxScene); // 左拖动向右 dxScene>0 收缩
            qreal delta = m_colWidths.first() - newFirst;
            if (qAbs(delta) > 0.1) {
                m_colWidths[0] = newFirst;
                // 位置随拖动改变（保持右侧不动）
                setPos(pos().x() + dxScene, pos().y());
            }
        }
        m_lastScenePos = event->scenePos();
        prepareGeometryChange();
        update();
        event->accept();
        return;
    } else if (m_dragType == DragRightEdge) {
        // 右边框拖动：改变最后一列宽度
        if (!m_colWidths.isEmpty()) {
            qreal dxLocal = event->pos().x() - m_lastPos.x();
            qreal newLast = qMax(m_minColWidth, m_colWidths.last() + dxLocal);
            m_colWidths[m_colWidths.size()-1] = newLast;
        }
        m_lastPos = event->pos();
        prepareGeometryChange();
        update();
        event->accept();
        return;
    } else if (m_dragType == DragTopEdge) {
        // 顶边拖动：改变第一行高度并整体上移
        qreal dyScene = event->scenePos().y() - m_lastScenePos.y();
        if (!m_rowHeights.isEmpty()) {
            qreal newFirst = qMax(m_minRowHeight, m_rowHeights.first() - dyScene);
            if (qAbs(m_rowHeights.first() - newFirst) > 0.1) {
                m_rowHeights[0] = newFirst;
                setPos(pos().x(), pos().y() + dyScene);
            }
        }
        m_lastScenePos = event->scenePos();
        prepareGeometryChange();
        update();
        event->accept();
        return;
    } else if (m_dragType == DragBottomEdge) {
        // 底边拖动：改变最后一行高度
        if (!m_rowHeights.isEmpty()) {
            qreal dyLocal = event->pos().y() - m_lastPos.y();
            qreal newLast = qMax(m_minRowHeight, m_rowHeights.last() + dyLocal);
            m_rowHeights[m_rowHeights.size()-1] = newLast;
        }
        m_lastPos = event->pos();
        prepareGeometryChange();
        update();
        event->accept();
        return;
    }
    QGraphicsItem::mouseMoveEvent(event);
}

void TableItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    m_dragType = None;
    m_dragIndex = -1;
    QGraphicsItem::mouseReleaseEvent(event);
}

void TableItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    const QPointF p = event->pos();
    if (hitTestColumnHandle(p) >= 0) {
        setCursor(QCursor(Qt::SplitHCursor));
    } else if (hitTestRowHandle(p) >= 0) {
        setCursor(QCursor(Qt::SplitVCursor));
    } else if (hitTestLeftEdge(p) || hitTestRightEdge(p)) {
        setCursor(QCursor(Qt::SizeHorCursor));
    } else if (hitTestTopEdge(p) || hitTestBottomEdge(p)) {
        setCursor(QCursor(Qt::SizeVerCursor));
    } else {
        unsetCursor();
    }
    QGraphicsItem::hoverMoveEvent(event);
}

void TableItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
    QMenu menu;
    const QPointF localPos = event->pos();
    int colSep = hitTestColumnHandle(localPos);
    int rowSep = hitTestRowHandle(localPos);
    int rowIndex = rowAtPos(localPos);
    int colIndex = columnAtPos(localPos);

    // 优先分隔线删除
    if (rowSep >= 0) {
        QAction* removeRowLine = menu.addAction(QObject::tr("删除该行分隔线"));
        QObject::connect(removeRowLine, &QAction::triggered, [this, rowSep]() { removeRowSeparator(rowSep); });
    }
    if (colSep >= 0) {
        QAction* removeColLine = menu.addAction(QObject::tr("删除该列分隔线"));
        QObject::connect(removeColLine, &QAction::triggered, [this, colSep]() { removeColumnSeparator(colSep); });
    }
    // 如果点击在某个单元格内部，可添加拆分操作
    if (rowIndex >= 0) {
        QAction* splitRowAct = menu.addAction(QObject::tr("在此行中间插入分隔线"));
        QObject::connect(splitRowAct, &QAction::triggered, [this, rowIndex]() { splitRow(rowIndex); });
    }
    if (colIndex >= 0) {
        QAction* splitColAct = menu.addAction(QObject::tr("在此列中间插入分隔线"));
        QObject::connect(splitColAct, &QAction::triggered, [this, colIndex]() { splitColumn(colIndex); });
    }

    if (menu.actions().isEmpty()) {
        return QGraphicsItem::contextMenuEvent(event); // 没有可用动作
    }
    menu.exec(event->screenPos());
}

void TableItem::emitStructureChangeUndo(const QVector<qreal>& oldCols,
                                        const QVector<qreal>& oldRows,
                                        const QString& description)
{
    auto* labelScene = qobject_cast<LabelScene*>(scene());
    if (!labelScene) {
        return;
    }
    auto* undo = labelScene->undoManager();
    if (!undo) {
        return;
    }
    QVector<qreal> newCols = columnWidths();
    QVector<qreal> newRows = rowHeights();
    if (oldCols == newCols && oldRows == newRows) {
        return;
    }
    undo->changeTableStructure(this, oldCols, newCols, oldRows, newRows, description);
}
