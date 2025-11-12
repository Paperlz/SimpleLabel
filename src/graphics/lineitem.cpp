#include "lineitem.h"
#include "labelscene.h"
#include "../commands/undomanager.h"
#include "qrcodeitem.h"
#include "barcodeitem.h"
#include "textitem.h"
#include "imageitem.h"
#include "rectangleitem.h"
#include "circleitem.h"
#include <QGraphicsScene>
#include <QCursor>
#include <QtMath>
#include <cmath>

// 静态成员变量初始化
bool LineItem::s_hasCopiedItem = false;
QPen LineItem::s_copiedPen;
QPointF LineItem::s_copiedStartPoint;
QPointF LineItem::s_copiedEndPoint;
LineItem* LineItem::s_adjustingItem = nullptr;

LineItem::LineItem(QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_startPoint(0, 0)
    , m_endPoint(100, 0)
    , m_isAdjusting(false)
    , m_adjustHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, !isLocked());
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    initializeDefaults();
}

LineItem::LineItem(const QPointF &startPoint, const QPointF &endPoint, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_startPoint(startPoint)
    , m_endPoint(endPoint)
    , m_isAdjusting(false)
    , m_adjustHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    initializeDefaults();
}

void LineItem::initializeDefaults()
{
    // 设置默认画笔
    m_pen = QPen(Qt::black, 2, Qt::SolidLine);
}

QRectF LineItem::boundingRect() const
{
    // 仅按笔宽和一个小余量扩展，避免过大矩形导致误选
    const qreal penWidth = qMax<qreal>(1.0, m_pen.widthF());
    const qreal pickWidth = 6.0; // 命中测试宽度
    const qreal extra = 0.5 * qMax(penWidth, pickWidth);

    QRectF rect = QRectF(m_startPoint, m_endPoint).normalized();
    return rect.adjusted(-extra, -extra, extra, extra);
}

void LineItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    // 绘制线条
    painter->setPen(m_pen);

    painter->drawLine(m_startPoint, m_endPoint);

    // 如果选中，绘制调整手柄
    if (isSelected()) {
        drawResizeHandles(painter);
    }

    // 绘制对齐线
    paintAlignmentLines(painter);
}

QPainterPath LineItem::shape() const
{
    QPainterPath path;
    path.moveTo(m_startPoint);
    path.lineTo(m_endPoint);

    // 使用较宽的描边生成可拾取区域，避免线条两侧空白大量可点
    QPainterPathStroker stroker;
    const qreal pickWidth = 6.0; // 可拾取宽度（像素，项坐标）
    stroker.setWidth(pickWidth);
    stroker.setCapStyle(Qt::SquareCap);
    stroker.setJoinStyle(Qt::MiterJoin);
    QPainterPath stroked = stroker.createStroke(path);

    // 合并手柄区域，保证手柄易于点击
    const qreal handleSize = 8.0;
    const qreal half = handleSize / 2.0;
    stroked.addRect(QRectF(m_startPoint - QPointF(half, half), QSizeF(handleSize, handleSize)));
    stroked.addRect(QRectF(m_endPoint - QPointF(half, half), QSizeF(handleSize, handleSize)));
    return stroked;
}

QRectF LineItem::alignmentRectLocal() const
{
    // 返回线段的包围盒（含笔宽一半），不包含手柄扩展
    QRectF rect = QRectF(m_startPoint, m_endPoint).normalized();
    const qreal penHalf = qMax<qreal>(0.5, m_pen.widthF() / 2.0);
    return rect.adjusted(-penHalf, -penHalf, penHalf, penHalf);
}

void LineItem::setPen(const QPen &pen)
{
    if (m_pen != pen) {
        prepareGeometryChange();
        m_pen = pen;
        update();
    }
}

void LineItem::setStartPoint(const QPointF &point)
{
    if (m_startPoint != point) {
        prepareGeometryChange();
        m_startPoint = point;
        update();
    }
}

void LineItem::setEndPoint(const QPointF &point)
{
    if (m_endPoint != point) {
        prepareGeometryChange();
        m_endPoint = point;
        update();
    }
}

void LineItem::setLine(const QPointF &startPoint, const QPointF &endPoint)
{
    if (m_startPoint != startPoint || m_endPoint != endPoint) {
        prepareGeometryChange();
        m_startPoint = startPoint;
        m_endPoint = endPoint;
        update();
    }
}

void LineItem::setDashed(bool dashed)
{
    Qt::PenStyle target = dashed ? Qt::DashLine : Qt::SolidLine;
    if (m_pen.style() != target) {
        QPen p = m_pen;
        p.setStyle(target);
        setPen(p);
    }
}

qreal LineItem::length() const
{
    QPointF diff = m_endPoint - m_startPoint;
    return qSqrt(diff.x() * diff.x() + diff.y() * diff.y());
}

qreal LineItem::angle() const
{
    QPointF diff = m_endPoint - m_startPoint;
    return qAtan2(diff.y(), diff.x());
}

double LineItem::angleFromVertical() const
{
    QPointF diff = m_endPoint - m_startPoint;
    double len = std::hypot(diff.x(), diff.y());
    if (len < 1e-6) return 0.0;
    // up vector (0,-1)
    double upDot = -diff.y();
    double cosBase = upDot / len;
    if (cosBase > 1.0) cosBase = 1.0;
    if (cosBase < -1.0) cosBase = -1.0;
    double baseAngle = std::acos(cosBase) * 180.0 / M_PI; // 0..180
    if (diff.x() >= 0) return baseAngle;
    return 360.0 - baseAngle;
}

void LineItem::setAngleFromVertical(double degrees)
{
    // normalize degrees
    while (degrees < 0.0) degrees += 360.0;
    while (degrees >= 360.0) degrees -= 360.0;
    QPointF diff = m_endPoint - m_startPoint;
    double len = std::hypot(diff.x(), diff.y());
    if (len < 1e-6) len = 100.0; // default length if collapsed
    double baseAngle = (degrees <= 180.0) ? degrees : (360.0 - degrees); // 0..180
    double rad = baseAngle * M_PI / 180.0;
    double x = len * std::sin(rad);
    double y = -len * std::cos(rad); // up is negative y
    if (degrees > 180.0) x = -x; // flip side
    QPointF newEnd = m_startPoint + QPointF(x, y);
    setLine(m_startPoint, newEnd);
}

void LineItem::copyItem(LineItem *item)
{
    if (!item) return;

    s_hasCopiedItem = true;
    s_copiedPen = item->pen();
    s_copiedStartPoint = item->startPoint();
    s_copiedEndPoint = item->endPoint();

    // 清除其他类型的复制状态
    QRCodeItem::s_hasCopiedItem = false;
    BarcodeItem::s_hasCopiedItem = false;
    TextItem::s_hasCopiedItem = false;
    ImageItem::s_hasCopiedItem = false;
    RectangleItem::s_hasCopiedItem = false;
    CircleItem::s_hasCopiedItem = false;

    // 设置最后复制的项目类型
    LabelScene::s_lastCopiedItemType = LabelScene::Line;

    qDebug() << "线条已复制到剪贴板";
}

void LineItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isLocked()) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton && isSelected()) {
        QPointF pos = event->pos();
        
        // 检查是否点击了调整手柄
        m_adjustHandle = getHandleAt(pos);
        if (m_adjustHandle != NoHandle) {
            m_isAdjusting = true;
            // 标记当前正在被调整的全局项
            LineItem::s_adjustingItem = this;
            m_adjustStartPos = event->scenePos();
            m_adjustStartPoint = m_startPoint;
            m_adjustEndPoint = m_endPoint;
            // 调整端点时禁止整体移动，并抓取鼠标，避免被场景拖动
            setFlag(QGraphicsItem::ItemIsMovable, false);
            grabMouse();
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}
//现有的线段无法重新调整点(例如从AB到AC)，只能整体移动
void LineItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isAdjusting && m_adjustHandle != NoHandle) {
        prepareGeometryChange();
        updateLineFromHandle(m_adjustHandle, event->scenePos());
        update();
        event->accept();
    } else {
        QGraphicsItem::mouseMoveEvent(event);
        qDebug()<<"LineItem moved to:"<<pos();
        // 在移动时检查对齐
        handleMouseMoveForAlignment();
    }
}

void LineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isAdjusting) {
        m_isAdjusting = false;
        m_adjustHandle = NoHandle;
        if (LineItem::s_adjustingItem == this) {
            LineItem::s_adjustingItem = nullptr;
        }
    // 释放鼠标并恢复可移动
    ungrabMouse();
    setFlag(QGraphicsItem::ItemIsMovable, true);
        event->accept();
    } else {
        QGraphicsItem::mouseReleaseEvent(event);
        // 在释放鼠标时隐藏对齐线
        handleMouseReleaseForAlignment();
    }
}

void LineItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
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

void LineItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

void LineItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // 创建右键菜单
    QMenu contextMenu;

    QAction *copyAction = contextMenu.addAction(QObject::tr("复制"));
    QAction *cutAction = contextMenu.addAction(QObject::tr("剪切"));
    QAction *pasteAction = contextMenu.addAction(QObject::tr("粘贴"));

    // 根据是否有复制的内容来启用/禁用粘贴选项
    pasteAction->setEnabled(s_hasCopiedItem);

    QAction *deleteAction = contextMenu.addAction(QObject::tr("删除"));
    contextMenu.addSeparator();
    QAction *lockAction = contextMenu.addAction(isLocked() ? QObject::tr("解除锁定")
                                                       : QObject::tr("锁定"));
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
    } else if (selectedAction == cutAction) {
        // 使用撤销管理器执行剪切操作
        LabelScene* labelScene = qobject_cast<LabelScene*>(scene());
        if (labelScene && labelScene->undoManager()) {
            labelScene->undoManager()->cutItem(this, QObject::tr("剪切线条"));
        } else {
            // 如果没有撤销管理器，回退到复制+删除
            copyItem(this);
            if (scene()) {
                scene()->removeItem(this);
                delete this;
            }
        }
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
    } else if (selectedAction == lockAction) {
        setLocked(!isLocked());
    } else if (selectedAction == alignToggle) {
        setAlignmentEnabled(alignToggle->isChecked());
    }
}

LineItem::HandleType LineItem::getHandleAt(const QPointF &pos) const
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    
    // 检查起点手柄
    if (QRectF(m_startPoint - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return StartHandle;
    
    // 检查终点手柄
    if (QRectF(m_endPoint - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return EndHandle;
    
    return NoHandle;
}

QRectF LineItem::getHandleRect(HandleType handle) const
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    
    switch (handle) {
        case StartHandle:
            return QRectF(m_startPoint - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        case EndHandle:
            return QRectF(m_endPoint - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        default:
            return QRectF();
    }
}

void LineItem::updateLineFromHandle(HandleType handle, const QPointF &scenePos)
{
    QPointF itemPos = mapFromScene(scenePos);
    //qDebug()<< "Updating line from handle:" << handle << "to position:" << itemPos;
    
    switch (handle) {
        case StartHandle:
            setStartPoint(itemPos);
            break;
        case EndHandle:
            setEndPoint(itemPos);
            break;
        default:
            break;
    }
}

void LineItem::setCursorForHandle(HandleType handle)
{
    switch (handle) {
        case StartHandle:
        case EndHandle:
            setCursor(Qt::SizeAllCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}

void LineItem::drawResizeHandles(QPainter *painter)
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    
    painter->setPen(QPen(Qt::blue, 1));
    painter->setBrush(Qt::white);
    
    // 绘制起点手柄
    painter->drawRect(QRectF(m_startPoint - QPointF(halfHandle, halfHandle), 
                            QSizeF(handleSize, handleSize)));
    
    // 绘制终点手柄
    painter->drawRect(QRectF(m_endPoint - QPointF(halfHandle, halfHandle), 
                            QSizeF(handleSize, handleSize)));
}

QPointF LineItem::getHandlePosition(HandleType handle) const
{
    switch (handle) {
        case StartHandle:
            return m_startPoint;
        case EndHandle:
            return m_endPoint;
        default:
            return QPointF();
    }
}
