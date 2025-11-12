#include "circleitem.h"
#include "labelscene.h"
#include "../commands/undomanager.h"
#include "qrcodeitem.h"
#include "barcodeitem.h"
#include "textitem.h"
#include "imageitem.h"
#include "lineitem.h"
#include "rectangleitem.h"
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QtMath>

// 静态成员变量初始化
bool CircleItem::s_hasCopiedItem = false;
QPen CircleItem::s_copiedPen;
QBrush CircleItem::s_copiedBrush;
bool CircleItem::s_copiedFillEnabled = false;
bool CircleItem::s_copiedBorderEnabled = true;
QSizeF CircleItem::s_copiedSize;
bool CircleItem::s_copiedIsCircle = true;

CircleItem::CircleItem(QGraphicsItem *parent)
    : AbstractShapeItem(parent)
    , m_isCircle(true)
{
    initializeDefaults();
}

CircleItem::CircleItem(const QSizeF &size, QGraphicsItem *parent)
    : AbstractShapeItem(parent)
    , m_isCircle(true)
{
    setSize(size);
    initializeDefaults();
}

void CircleItem::initializeDefaults()
{
    // 圆形默认启用填充
    setFillEnabled(true);
    setBrush(QBrush(QColor(144, 238, 144), Qt::SolidPattern)); // 浅绿色
    
    // 如果是圆形，确保宽高相等
    if (m_isCircle) {
        qreal diameter = qMax(m_size.width(), m_size.height());
        setSize(QSizeF(diameter, diameter));
    }
}

void CircleItem::setIsCircle(bool isCircle)
{
    if (m_isCircle != isCircle) {
        m_isCircle = isCircle;
        
        if (m_isCircle) {
            // 转换为圆形，使用较大的尺寸作为直径
            qreal diameter = qMax(m_size.width(), m_size.height());
            setSize(QSizeF(diameter, diameter));
        }
        
        update();
    }
}

void CircleItem::paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)

    QRectF rect(0, 0, m_size.width(), m_size.height());

    // 设置画笔和画刷
    if (m_borderEnabled) {
        painter->setPen(m_pen);
    } else {
        painter->setPen(Qt::NoPen);
    }

    if (m_fillEnabled) {
        painter->setBrush(m_brush);
    } else {
        painter->setBrush(Qt::NoBrush);
    }

    // 绘制椭圆/圆形
    painter->drawEllipse(rect);
}

void CircleItem::updateSizeFromHandle(HandleType handle, const QPointF &delta)
{
    if (m_isCircle) {
        // 圆形模式：保持宽高相等
        QSizeF newSize = m_resizeStartSize;
        QPointF newPos = m_resizeStartItemPos;
        const qreal minSize = 10.0;
        
        qreal sizeChange = 0;
        
        switch (handle) {
            case TopLeft:
                sizeChange = qMax(-delta.x(), -delta.y());
                break;
            case TopRight:
                sizeChange = qMax(delta.x(), -delta.y());
                break;
            case BottomLeft:
                sizeChange = qMax(-delta.x(), delta.y());
                break;
            case BottomRight:
                sizeChange = qMax(delta.x(), delta.y());
                break;
            case TopCenter:
                sizeChange = -delta.y();
                break;
            case BottomCenter:
                sizeChange = delta.y();
                break;
            case MiddleLeft:
                sizeChange = -delta.x();
                break;
            case MiddleRight:
                sizeChange = delta.x();
                break;
            default:
                return;
        }
        
        qreal newDiameter = qMax(minSize, m_resizeStartSize.width() + sizeChange);
        newSize = QSizeF(newDiameter, newDiameter);
        
        // 调整位置以保持圆心位置
        QPointF centerOffset = QPointF(
            (m_resizeStartSize.width() - newSize.width()) / 2,
            (m_resizeStartSize.height() - newSize.height()) / 2
        );
        
        switch (handle) {
            case TopLeft:
                newPos = m_resizeStartItemPos + centerOffset;
                break;
            case TopCenter:
                newPos = m_resizeStartItemPos + QPointF(centerOffset.x(), centerOffset.y());
                break;
            case TopRight:
                newPos = m_resizeStartItemPos + QPointF(0, centerOffset.y());
                break;
            case MiddleLeft:
                newPos = m_resizeStartItemPos + QPointF(centerOffset.x(), 0);
                break;
            case MiddleRight:
                newPos = m_resizeStartItemPos;
                break;
            case BottomLeft:
                newPos = m_resizeStartItemPos + QPointF(centerOffset.x(), 0);
                break;
            case BottomCenter:
                newPos = m_resizeStartItemPos + QPointF(centerOffset.x(), 0);
                break;
            case BottomRight:
                newPos = m_resizeStartItemPos;
                break;
            default:
                break;
        }
        
        prepareGeometryChange();
        m_size = newSize;
        setPos(newPos);
        update();
    } else {
        // 椭圆模式：使用基类的默认行为
        AbstractShapeItem::updateSizeFromHandle(handle, delta);
    }
}

void CircleItem::copyItem(CircleItem *item)
{
    if (!item) return;

    s_hasCopiedItem = true;
    s_copiedPen = item->pen();
    s_copiedBrush = item->brush();
    s_copiedFillEnabled = item->fillEnabled();
    s_copiedBorderEnabled = item->borderEnabled();
    s_copiedSize = item->size();
    s_copiedIsCircle = item->isCircle();

    // 清除其他类型的复制状态
    QRCodeItem::s_hasCopiedItem = false;
    BarcodeItem::s_hasCopiedItem = false;
    TextItem::s_hasCopiedItem = false;
    ImageItem::s_hasCopiedItem = false;
    LineItem::s_hasCopiedItem = false;
    RectangleItem::s_hasCopiedItem = false;

    // 设置最后复制的项目类型
    LabelScene::s_lastCopiedItemType = LabelScene::Circle;

    qDebug() << "圆形/椭圆已复制到剪贴板";
}

void CircleItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
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

    QAction *fillAction = contextMenu.addAction(QObject::tr("启用填充"));
    fillAction->setCheckable(true);
    fillAction->setChecked(m_fillEnabled);

    QAction *borderAction = contextMenu.addAction(QObject::tr("启用边框"));
    borderAction->setCheckable(true);
    borderAction->setChecked(m_borderEnabled);

    contextMenu.addSeparator();
    QAction *circleAction = contextMenu.addAction(QObject::tr("保持圆形"));
    circleAction->setCheckable(true);
    circleAction->setChecked(m_isCircle);

    // 锁定/解除锁定
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
            labelScene->undoManager()->cutItem(this, QObject::tr("剪切圆形"));
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
    } else if (selectedAction == fillAction) {
        setFillEnabled(fillAction->isChecked());
    } else if (selectedAction == borderAction) {
        setBorderEnabled(borderAction->isChecked());
    } else if (selectedAction == circleAction) {
        setIsCircle(circleAction->isChecked());
    } else if (selectedAction == lockAction) {
        setLocked(!isLocked());
    } else if (selectedAction == alignToggle) {
        setAlignmentEnabled(alignToggle->isChecked());
    }
}

QSizeF CircleItem::calculateCircleSize(const QSizeF &size) const
{
    if (m_isCircle) {
        qreal diameter = qMax(size.width(), size.height());
        return QSizeF(diameter, diameter);
    }
    return size;
}
