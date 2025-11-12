#include "abstractshapeitem.h"
#include "labelscene.h"
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QApplication>
#include <QMenu>
#include <QAction>
#include <QtMath>

// 静态成员变量初始化
bool AbstractShapeItem::s_hasCopiedItem = false;
QPen AbstractShapeItem::s_copiedPen;
QBrush AbstractShapeItem::s_copiedBrush;
bool AbstractShapeItem::s_copiedFillEnabled = false;
bool AbstractShapeItem::s_copiedBorderEnabled = true;
QSizeF AbstractShapeItem::s_copiedSize;

AbstractShapeItem::AbstractShapeItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_size(100, 100)  // 默认尺寸
    , m_fillEnabled(false)
    , m_borderEnabled(true)
    , m_isResizing(false)
    , m_resizeHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    initializeDefaults();
}

void AbstractShapeItem::initializeDefaults()
{
    // 设置默认画笔和画刷
    m_pen = QPen(Qt::black, 2, Qt::SolidLine);
    m_brush = QBrush(Qt::blue, Qt::SolidPattern);
}

QRectF AbstractShapeItem::boundingRect() const
{
    // Compute minimal padding to include handles and account for pen width
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    const qreal penHalf = m_pen.widthF() / 2.0;
    const qreal padding = qMax<qreal>(1.0, qMax(halfHandle, penHalf));

    return QRectF(-padding, -padding,
                  m_size.width() + 2 * padding,
                  m_size.height() + 2 * padding);
}

void AbstractShapeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    // 调用子类的具体绘制逻辑
    paintShape(painter, option, widget);

    // 如果选中，绘制选择框和调整手柄
    if (isSelected()) {
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        QRectF shapeRect(0, 0, m_size.width(), m_size.height());
        painter->drawRect(shapeRect);

        // 绘制调整手柄
        drawResizeHandles(painter);
    }

    // 绘制对齐线
    paintAlignmentLines(painter);
}

void AbstractShapeItem::setPen(const QPen &pen)
{
    m_pen = pen;
    update();
}

void AbstractShapeItem::setBrush(const QBrush &brush)
{
    m_brush = brush;
    update();
}

void AbstractShapeItem::setFillEnabled(bool enabled)
{
    m_fillEnabled = enabled;
    update();
}

void AbstractShapeItem::setBorderEnabled(bool enabled)
{
    m_borderEnabled = enabled;
    update();
}

void AbstractShapeItem::setSize(const QSizeF &size)
{
    if (m_size != size) {
        prepareGeometryChange();
        m_size = size;
        update();
    }
}

void AbstractShapeItem::copyItem(AbstractShapeItem *item)
{
    if (!item) return;

    s_hasCopiedItem = true;
    s_copiedPen = item->pen();
    s_copiedBrush = item->brush();
    s_copiedFillEnabled = item->fillEnabled();
    s_copiedBorderEnabled = item->borderEnabled();
    s_copiedSize = item->size();
    
    qDebug() << "形状已复制到剪贴板";
}

void AbstractShapeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isLocked()) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton && isSelected()) {
        QPointF pos = event->pos();
        
        // 检查是否点击了调整手柄
        m_resizeHandle = getHandleAt(pos);
        if (m_resizeHandle != NoHandle) {
            m_isResizing = true;
            m_resizeStartPos = event->scenePos();  // 使用场景坐标
            m_resizeStartSize = m_size;
            m_resizeStartItemPos = this->pos();
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void AbstractShapeItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing && m_resizeHandle != NoHandle) {
        QPointF delta = event->scenePos() - m_resizeStartPos;  // 使用场景坐标
        updateSizeFromHandle(m_resizeHandle, delta);
        event->accept();
    } else {
        QGraphicsItem::mouseMoveEvent(event);
        // 在移动时检查对齐
        handleMouseMoveForAlignment();
    }
}

void AbstractShapeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing) {
        m_isResizing = false;
        m_resizeHandle = NoHandle;
        event->accept();
    } else {
        QGraphicsItem::mouseReleaseEvent(event);
        // 在释放鼠标时隐藏对齐线
        handleMouseReleaseForAlignment();
    }
}

void AbstractShapeItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (isLocked()) {
        setCursor(Qt::ArrowCursor);
        QGraphicsItem::hoverMoveEvent(event);
        return;
    }
    if (!isSelected()) {
        QGraphicsItem::hoverMoveEvent(event);
        return;
    }

    // 根据鼠标位置设置光标
    HandleType handle = getHandleAt(event->pos());
    setCursorForHandle(handle);
    
    QGraphicsItem::hoverMoveEvent(event);
}

void AbstractShapeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

void AbstractShapeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // 创建右键菜单
    QMenu contextMenu;
    
    QAction *copyAction = contextMenu.addAction(QObject::tr("复制"));
    QAction *pasteAction = contextMenu.addAction(QObject::tr("粘贴"));
    
    // 根据是否有复制的内容来启用/禁用粘贴选项
    pasteAction->setEnabled(s_hasCopiedItem);
    
    QAction *deleteAction = contextMenu.addAction(QObject::tr("删除"));
    contextMenu.addSeparator();
    QAction *fillAction = contextMenu.addAction(QObject::tr("启用填充"));
    fillAction->setCheckable(true);
    fillAction->setChecked(m_fillEnabled);
    
    QAction *borderAction = contextMenu.addAction(QObject::tr("启用边框"));
    borderAction->setCheckable(true);
    borderAction->setChecked(m_borderEnabled);

    // 对齐开关
    QAction *alignToggle = contextMenu.addAction(QObject::tr("启用对齐"));
    alignToggle->setCheckable(true);
    alignToggle->setChecked(isAlignmentEnabled());
    
    // 确保当前项被选中
    if (!isSelected()) {
        scene()->clearSelection();
        setSelected(true);
    }
    
    // 显示菜单并获取用户选择
    QAction *selectedAction = contextMenu.exec(event->screenPos());
    
    if (selectedAction == copyAction) {
        copyItem(this);
    } else if (selectedAction == deleteAction) {
        if (scene()) {
            // 确保当前项被选中
            if (!isSelected()) {
                scene()->clearSelection();
                setSelected(true);
            }
        
            // 将 QGraphicsScene* 转换为 LabelScene*
            LabelScene* labelScene = qobject_cast<LabelScene*>(scene());
            if (labelScene) {
                labelScene->removeSelectedItems();  // 调用 LabelScene 的方法
            }
        }
    } else if (selectedAction == fillAction) {
        setFillEnabled(fillAction->isChecked());
    } else if (selectedAction == borderAction) {
        setBorderEnabled(borderAction->isChecked());
    } else if (selectedAction == alignToggle) {
        setAlignmentEnabled(alignToggle->isChecked());
    }
}

AbstractShapeItem::HandleType AbstractShapeItem::getHandleAt(const QPointF &pos) const
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;

    QRectF itemRect(0, 0, m_size.width(), m_size.height());

    // 检查各个调整手柄
    if (QRectF(itemRect.topLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return TopLeft;
    if (QRectF(itemRect.topRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return TopRight;
    if (QRectF(itemRect.bottomLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return BottomLeft;
    if (QRectF(itemRect.bottomRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return BottomRight;

    // 边中点
    if (QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return TopCenter;
    if (QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return BottomCenter;
    if (QRectF(QPointF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return MiddleLeft;
    if (QRectF(QPointF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return MiddleRight;

    return NoHandle;
}

QRectF AbstractShapeItem::getHandleRect(HandleType handle) const
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;

    QRectF itemRect(0, 0, m_size.width(), m_size.height());

    switch (handle) {
        case TopLeft:
            return QRectF(itemRect.topLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        case TopCenter:
            return QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle), QSizeF(handleSize, handleSize));
        case TopRight:
            return QRectF(itemRect.topRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        case MiddleLeft:
            return QRectF(QPointF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize));
        case MiddleRight:
            return QRectF(QPointF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize));
        case BottomLeft:
            return QRectF(itemRect.bottomLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        case BottomCenter:
            return QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle), QSizeF(handleSize, handleSize));
        case BottomRight:
            return QRectF(itemRect.bottomRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        default:
            return QRectF();
    }
}

void AbstractShapeItem::updateSizeFromHandle(HandleType handle, const QPointF &delta)
{
    QSizeF newSize = m_resizeStartSize;
    QPointF newPos = m_resizeStartItemPos;
    const qreal minSize = 10.0;  // 最小尺寸

    switch (handle) {
        case TopLeft:
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;

        case TopCenter:
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;

        case TopRight:
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;

        case MiddleLeft:
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            break;

        case MiddleRight:
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            break;

        case BottomLeft:
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            break;

        case BottomCenter:
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
            break;

        case BottomRight:
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
            break;

        default:
            return;
    }

    prepareGeometryChange();
    m_size = newSize;
    setPos(newPos);
    update();
}

void AbstractShapeItem::setCursorForHandle(HandleType handle)
{
    switch (handle) {
        case TopLeft:
        case BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case TopRight:
        case BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case TopCenter:
        case BottomCenter:
            setCursor(Qt::SizeVerCursor);
            break;
        case MiddleLeft:
        case MiddleRight:
            setCursor(Qt::SizeHorCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}

void AbstractShapeItem::drawResizeHandles(QPainter *painter)
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;

    painter->setPen(QPen(Qt::blue, 1));
    painter->setBrush(Qt::white);

    QRectF itemRect(0, 0, m_size.width(), m_size.height());

    // 绘制四个角的调整手柄
    painter->drawRect(QRectF(itemRect.topLeft() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));
    painter->drawRect(QRectF(itemRect.topRight() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));
    painter->drawRect(QRectF(itemRect.bottomLeft() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));
    painter->drawRect(QRectF(itemRect.bottomRight() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));

    // 绘制四个边中点的调整手柄
    painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle,
                            handleSize, handleSize));  // 上中
    painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle,
                            handleSize, handleSize));  // 下中
    painter->drawRect(QRectF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle,
                            handleSize, handleSize));  // 左中
    painter->drawRect(QRectF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle,
                            handleSize, handleSize));  // 右中
}
