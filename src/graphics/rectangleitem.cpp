#include "rectangleitem.h"
#include "labelscene.h"
#include "../commands/undomanager.h"
#include "qrcodeitem.h"
#include "barcodeitem.h"
#include "textitem.h"
#include "imageitem.h"
#include "lineitem.h"
#include "circleitem.h"
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QAction>

// 静态成员变量初始化
bool RectangleItem::s_hasCopiedItem = false;
QPen RectangleItem::s_copiedPen;
QBrush RectangleItem::s_copiedBrush;
bool RectangleItem::s_copiedFillEnabled = false;
bool RectangleItem::s_copiedBorderEnabled = true;
QSizeF RectangleItem::s_copiedSize;
double RectangleItem::s_copiedCornerRadius = 0.0;

RectangleItem::RectangleItem(QGraphicsItem *parent)
    : AbstractShapeItem(parent)
{
    initializeDefaults();
}

RectangleItem::RectangleItem(const QSizeF &size, QGraphicsItem *parent)
    : AbstractShapeItem(parent)
{
    setSize(size);
    initializeDefaults();
}

void RectangleItem::initializeDefaults()
{
    // 矩形默认启用填充
    setFillEnabled(true);
    setBrush(QBrush(QColor(173, 216, 230), Qt::SolidPattern)); // 浅蓝色
    m_cornerRadius = 0.0;
}

void RectangleItem::paintShape(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
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

    // 绘制矩形（支持圆角）
    if (m_cornerRadius > 0.0) {
        double r = std::min({m_cornerRadius, rect.width() / 2.0, rect.height() / 2.0});
        painter->drawRoundedRect(rect, r, r);
    } else {
        painter->drawRect(rect);
    }
}

void RectangleItem::copyItem(RectangleItem *item)
{
    if (!item) return;

    s_hasCopiedItem = true;
    s_copiedPen = item->pen();
    s_copiedBrush = item->brush();
    s_copiedFillEnabled = item->fillEnabled();
    s_copiedBorderEnabled = item->borderEnabled();
    s_copiedSize = item->size();
    s_copiedCornerRadius = item->cornerRadius();

    // 清除其他类型的复制状态
    QRCodeItem::s_hasCopiedItem = false;
    BarcodeItem::s_hasCopiedItem = false;
    TextItem::s_hasCopiedItem = false;
    ImageItem::s_hasCopiedItem = false;
    LineItem::s_hasCopiedItem = false;
    CircleItem::s_hasCopiedItem = false;

    // 设置最后复制的项目类型
    LabelScene::s_lastCopiedItemType = LabelScene::Rectangle;

    qDebug() << "矩形已复制到剪贴板";
}

void RectangleItem::setCornerRadius(double radius)
{
    double clamped = std::max(0.0, radius);
    // 限制最大半径为当前尺寸的一半
    clamped = std::min(clamped, std::min(m_size.width(), m_size.height()) / 2.0);
    if (!qFuzzyCompare(1.0 + m_cornerRadius, 1.0 + clamped)) {
        prepareGeometryChange();
        m_cornerRadius = clamped;
        update();
    }
}

void RectangleItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
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
            labelScene->undoManager()->cutItem(this, QObject::tr("剪切矩形"));
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
    } else if (selectedAction == lockAction) {
        setLocked(!isLocked());
    } else if (selectedAction == alignToggle) {
        setAlignmentEnabled(alignToggle->isChecked());
    }
}
