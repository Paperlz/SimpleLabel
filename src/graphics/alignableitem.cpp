#include "alignableitem.h"
#include <QGraphicsScene>
#include <QApplication>
#include <QDebug>
#include <QtMath>

AlignableItem::AlignableItem()
    : m_showAlignmentLine(false)
    , m_showHorizontalLine(false)
    , m_showVerticalLine(false)
{
}

QRectF AlignableItem::alignmentSceneRect() const
{
    const QGraphicsItem* gi = asGraphicsItem();
    if (!gi) return QRectF();
    // 将本地内容矩形映射到场景坐标
    return gi->mapRectToScene(alignmentRectLocal());
}

void AlignableItem::checkAlignment()
{
    QGraphicsItem* item = asGraphicsItem();
    if (!item || !item->scene()) {
        return;
    }
    // 未启用对齐功能时，清除对齐线并退出
    if (!m_alignmentEnabled) {
        m_showAlignmentLine = false;
        m_showHorizontalLine = false;
        m_showVerticalLine = false;
        return;
    }

    // 获取场景中的所有图形项
    QList<QGraphicsItem*> items = item->scene()->items();
    m_showAlignmentLine = false;
    m_showHorizontalLine = false;
    m_showVerticalLine = false;

    // 获取当前图形项在场景中的矩形
    QRectF currentRect = getSceneRect(item);

    // 首先检查与标签背景的对齐
    QGraphicsItem* labelBackground = nullptr;
    for (QGraphicsItem* otherItem : items) {
        if (!otherItem || shouldIgnoreForAlignment(otherItem)) {
            continue;
        }
        if (otherItem->data(0).toString() == "selectionFrame") {
            continue;
        }
        if (otherItem->data(0).toString() == "labelBackground") {
            labelBackground = otherItem;
            break;
        }
    }

    // 目标：单次拖动中可同时对齐两个轴
    bool horizontalAligned = false;
    bool verticalAligned = false;

    // 如果找到标签背景，先尝试与标签的两个轴逐一对齐（对齐后更新 currentRect）
    if (labelBackground) {
        QRectF labelRect = getSceneRect(labelBackground);

        horizontalAligned = checkLabelHorizontalAlignment(currentRect, labelRect);
        if (horizontalAligned) {
            currentRect = getSceneRect(item);
        }

        verticalAligned = checkLabelVerticalAlignment(currentRect, labelRect);
        if (verticalAligned) {
            currentRect = getSceneRect(item);
        }
    }

    // 然后检查与其他图形项的对齐
    for (QGraphicsItem* otherItem : items) {
        if (otherItem == item) continue; // 忽略自身
        if (!otherItem) continue;
        if (shouldIgnoreForAlignment(otherItem)) continue;
        if (otherItem->data(0).toString() == "selectionFrame") continue;

        // 跳过标签背景（已经检查过了）
        if (otherItem->data(0).toString() == "labelBackground") {
            continue;
        }

        QRectF otherRect = getSceneRect(otherItem);

        // 先补齐未完成的垂直轴（X 对齐：中心/左右边/跨边）
        if (!verticalAligned && checkVerticalAlignment(currentRect, otherRect)) {
            verticalAligned = true;
            currentRect = getSceneRect(item);
            if (horizontalAligned) break; // 两轴都完成则退出
        }

        // 再补齐未完成的水平轴（Y 对齐：中心/上下边/跨边）
        if (!horizontalAligned && checkHorizontalAlignment(currentRect, otherRect)) {
            horizontalAligned = true;
            currentRect = getSceneRect(item);
            if (verticalAligned) break; // 两轴都完成则退出
        }
    }
}

bool AlignableItem::checkHorizontalAlignment(const QRectF& currentRect, const QRectF& otherRect)
{
    QGraphicsItem* item = asGraphicsItem();
    
    // 计算水平中心线
    qreal currentCenterY = currentRect.center().y();
    qreal otherCenterY = otherRect.center().y();

    // 检查水平中心线对齐
    if (qAbs(currentCenterY - otherCenterY) < ALIGNMENT_THRESHOLD) {
        // 计算需要移动的距离
        qreal deltaY = otherCenterY - currentCenterY;
        
        // 移动到对齐位置
        QPointF newPos = item->pos() + QPointF(0, deltaY);
        item->setPos(newPos);
        
        // 设置水平对齐线（使用统一的水平线通道）
        qreal leftX = qMin(currentRect.left(), otherRect.left());
        qreal rightX = qMax(currentRect.right(), otherRect.right());
        m_horizontalP1 = QPointF(leftX, otherCenterY);
        m_horizontalP2 = QPointF(rightX, otherCenterY);
        m_showHorizontalLine = true;

        return true;
    }

    // 顶边对齐（Top-Top）
    qreal currentTop = currentRect.top();
    qreal otherTop = otherRect.top();
    if (qAbs(currentTop - otherTop) < ALIGNMENT_THRESHOLD) {
        qreal deltaY = otherTop - currentTop;
        QPointF newPos = item->pos() + QPointF(0, deltaY);
        item->setPos(newPos);

        qreal leftX = qMin(currentRect.left(), otherRect.left());
        qreal rightX = qMax(currentRect.right(), otherRect.right());
        m_horizontalP1 = QPointF(leftX, otherTop);
        m_horizontalP2 = QPointF(rightX, otherTop);
        m_showHorizontalLine = true;
        return true;
    }

    // 底边对齐（Bottom-Bottom）
    qreal currentBottom = currentRect.bottom();
    qreal otherBottom = otherRect.bottom();
    if (qAbs(currentBottom - otherBottom) < ALIGNMENT_THRESHOLD) {
        qreal deltaY = otherBottom - currentBottom;
        QPointF newPos = item->pos() + QPointF(0, deltaY);
        item->setPos(newPos);

        qreal leftX = qMin(currentRect.left(), otherRect.left());
        qreal rightX = qMax(currentRect.right(), otherRect.right());
        m_horizontalP1 = QPointF(leftX, otherBottom);
        m_horizontalP2 = QPointF(rightX, otherBottom);
        m_showHorizontalLine = true;
        return true;
    }

    // 顶边对齐到对方底边（Top-Bottom）
    {
        qreal currentTop = currentRect.top();
        qreal otherBottom = otherRect.bottom();
        if (qAbs(currentTop - otherBottom) < ALIGNMENT_THRESHOLD) {
            qreal deltaY = otherBottom - currentTop;
            QPointF newPos = item->pos() + QPointF(0, deltaY);
            item->setPos(newPos);

            qreal leftX = qMin(currentRect.left(), otherRect.left());
            qreal rightX = qMax(currentRect.right(), otherRect.right());
            m_horizontalP1 = QPointF(leftX, otherBottom);
            m_horizontalP2 = QPointF(rightX, otherBottom);
            m_showHorizontalLine = true;
            return true;
        }
    }

    // 底边对齐到对方顶边（Bottom-Top）
    {
        qreal currentBottom = currentRect.bottom();
        qreal otherTop = otherRect.top();
        if (qAbs(currentBottom - otherTop) < ALIGNMENT_THRESHOLD) {
            qreal deltaY = otherTop - currentBottom;
            QPointF newPos = item->pos() + QPointF(0, deltaY);
            item->setPos(newPos);

            qreal leftX = qMin(currentRect.left(), otherRect.left());
            qreal rightX = qMax(currentRect.right(), otherRect.right());
            m_horizontalP1 = QPointF(leftX, otherTop);
            m_horizontalP2 = QPointF(rightX, otherTop);
            m_showHorizontalLine = true;
            return true;
        }
    }

    return false;
}

bool AlignableItem::checkVerticalAlignment(const QRectF& currentRect, const QRectF& otherRect)
{
    QGraphicsItem* item = asGraphicsItem();
    
    // 计算垂直中心线
    qreal currentCenterX = currentRect.center().x();
    qreal otherCenterX = otherRect.center().x();

    // 检查垂直中心线对齐
    if (qAbs(currentCenterX - otherCenterX) < ALIGNMENT_THRESHOLD) {
        // 计算需要移动的距离
        qreal deltaX = otherCenterX - currentCenterX;
        
        // 移动到对齐位置
        QPointF newPos = item->pos() + QPointF(deltaX, 0);
        item->setPos(newPos);
        
        // 设置垂直对齐线（使用统一的垂直线通道）
        qreal topY = qMin(currentRect.top(), otherRect.top());
        qreal bottomY = qMax(currentRect.bottom(), otherRect.bottom());
        m_verticalP1 = QPointF(otherCenterX, topY);
        m_verticalP2 = QPointF(otherCenterX, bottomY);
        m_showVerticalLine = true;

        return true;
    }

    // 左边对齐（Left-Left）
    qreal currentLeft = currentRect.left();
    qreal otherLeft = otherRect.left();
    if (qAbs(currentLeft - otherLeft) < ALIGNMENT_THRESHOLD) {
        qreal deltaX = otherLeft - currentLeft;
        QPointF newPos = item->pos() + QPointF(deltaX, 0);
        item->setPos(newPos);

        qreal topY = qMin(currentRect.top(), otherRect.top());
        qreal bottomY = qMax(currentRect.bottom(), otherRect.bottom());
        m_verticalP1 = QPointF(otherLeft, topY);
        m_verticalP2 = QPointF(otherLeft, bottomY);
        m_showVerticalLine = true;
        return true;
    }

    // 右边对齐（Right-Right）
    qreal currentRight = currentRect.right();
    qreal otherRight = otherRect.right();
    if (qAbs(currentRight - otherRight) < ALIGNMENT_THRESHOLD) {
        qreal deltaX = otherRight - currentRight;
        QPointF newPos = item->pos() + QPointF(deltaX, 0);
        item->setPos(newPos);

        qreal topY = qMin(currentRect.top(), otherRect.top());
        qreal bottomY = qMax(currentRect.bottom(), otherRect.bottom());
        m_verticalP1 = QPointF(otherRight, topY);
        m_verticalP2 = QPointF(otherRight, bottomY);
        m_showVerticalLine = true;
        return true;
    }

    // 左边对齐到对方右边（Left-Right）
    {
        qreal currentLeft = currentRect.left();
        qreal otherRight = otherRect.right();
        if (qAbs(currentLeft - otherRight) < ALIGNMENT_THRESHOLD) {
            qreal deltaX = otherRight - currentLeft;
            QPointF newPos = item->pos() + QPointF(deltaX, 0);
            item->setPos(newPos);

            qreal topY = qMin(currentRect.top(), otherRect.top());
            qreal bottomY = qMax(currentRect.bottom(), otherRect.bottom());
            m_verticalP1 = QPointF(otherRight, topY);
            m_verticalP2 = QPointF(otherRight, bottomY);
            m_showVerticalLine = true;
            return true;
        }
    }

    // 右边对齐到对方左边（Right-Left）
    {
        qreal currentRight = currentRect.right();
        qreal otherLeft = otherRect.left();
        if (qAbs(currentRight - otherLeft) < ALIGNMENT_THRESHOLD) {
            qreal deltaX = otherLeft - currentRight;
            QPointF newPos = item->pos() + QPointF(deltaX, 0);
            item->setPos(newPos);

            qreal topY = qMin(currentRect.top(), otherRect.top());
            qreal bottomY = qMax(currentRect.bottom(), otherRect.bottom());
            m_verticalP1 = QPointF(otherLeft, topY);
            m_verticalP2 = QPointF(otherLeft, bottomY);
            m_showVerticalLine = true;
            return true;
        }
    }

    return false;
}

QRectF AlignableItem::getSceneRect(QGraphicsItem* item) const
{
    if (!item) return QRectF();
    // 使用对应图形项自身的“内容矩形”（若支持），否则退回其 boundingRect()
    if (auto otherAlignable = dynamic_cast<const AlignableItem*>(item)) {
        return item->mapRectToScene(otherAlignable->alignmentRectLocal());
    }
    return item->mapRectToScene(item->boundingRect());
}

void AlignableItem::drawAlignmentLine(QPainter *painter, const QPointF& p1, const QPointF& p2)
{
    QGraphicsItem* item = asGraphicsItem();
    if (!item) return;

    // 设置画笔（虚线样式）
    QPen pen;
    pen.setColor(Qt::red);  // 使用红色更显眼
    pen.setStyle(Qt::DashLine);
    pen.setWidth(1);
    painter->setPen(pen);
    
    // 将场景坐标转换为本地坐标
    QPointF localP1 = item->mapFromScene(p1);
    QPointF localP2 = item->mapFromScene(p2);
    
    painter->drawLine(localP1, localP2);
}

void AlignableItem::handleMouseMoveForAlignment()
{
    // 锁定时不参与对齐/吸附，也不显示辅助线
    if (m_locked) {
        m_showAlignmentLine = false;
        m_showHorizontalLine = false;
        m_showVerticalLine = false;
        return;
    }
    // 按住 Alt 键时，临时禁用吸附/对齐（仅显示常规移动，不绘制辅助线）
    if (QApplication::keyboardModifiers() & Qt::AltModifier) {
        m_showAlignmentLine = false;
        m_showHorizontalLine = false;
        m_showVerticalLine = false;
        return;
    }
    // 关闭对齐功能时不进行对齐
    if (!m_alignmentEnabled) {
        m_showAlignmentLine = false;
        m_showHorizontalLine = false;
        m_showVerticalLine = false;
        return;
    }
    checkAlignment();
}

void AlignableItem::handleMouseReleaseForAlignment()
{
    // 清除所有对齐线标志
    m_showAlignmentLine = false;
    m_showHorizontalLine = false;
    m_showVerticalLine = false;
}

void AlignableItem::paintAlignmentLines(QPainter *painter)
{
    if (!m_alignmentEnabled) {
        return;
    }
    // 绘制旧版本的对齐线（用于向后兼容）
    if (m_showAlignmentLine) {
        drawAlignmentLine(painter, m_alignmentP1, m_alignmentP2);
    }
    
    // 绘制水平对齐线
    if (m_showHorizontalLine) {
        drawAlignmentLine(painter, m_horizontalP1, m_horizontalP2);
    }
    
    // 绘制垂直对齐线
    if (m_showVerticalLine) {
        drawAlignmentLine(painter, m_verticalP1, m_verticalP2);
    }
}

bool AlignableItem::checkLabelHorizontalAlignment(const QRectF& currentRect, const QRectF& labelRect)
{
    QGraphicsItem* item = asGraphicsItem();
    
    // 计算水平中心线
    qreal currentCenterY = currentRect.center().y();
    qreal labelCenterY = labelRect.center().y();

    // 检查与标签水平中心线对齐
    if (qAbs(currentCenterY - labelCenterY) < ALIGNMENT_THRESHOLD) {
        // 计算需要移动的距离
        qreal deltaY = labelCenterY - currentCenterY;
        
        // 移动到对齐位置
        QPointF newPos = item->pos() + QPointF(0, deltaY);
        item->setPos(newPos);

        // 设置水平对齐线（横跨整个标签）
        m_horizontalP1 = QPointF(labelRect.left(), labelCenterY);
        m_horizontalP2 = QPointF(labelRect.right(), labelCenterY);
        m_showHorizontalLine = true;

        return true;
    }

    // 顶边与标签顶边对齐
    qreal currentTop = currentRect.top();
    qreal labelTop = labelRect.top();
    if (qAbs(currentTop - labelTop) < ALIGNMENT_THRESHOLD) {
        qreal deltaY = labelTop - currentTop;
        QPointF newPos = item->pos() + QPointF(0, deltaY);
        item->setPos(newPos);

        m_horizontalP1 = QPointF(labelRect.left(), labelTop);
        m_horizontalP2 = QPointF(labelRect.right(), labelTop);
        m_showHorizontalLine = true;
        return true;
    }

    // 底边与标签底边对齐
    qreal currentBottom = currentRect.bottom();
    qreal labelBottom = labelRect.bottom();
    if (qAbs(currentBottom - labelBottom) < ALIGNMENT_THRESHOLD) {
        qreal deltaY = labelBottom - currentBottom;
        QPointF newPos = item->pos() + QPointF(0, deltaY);
        item->setPos(newPos);

        m_horizontalP1 = QPointF(labelRect.left(), labelBottom);
        m_horizontalP2 = QPointF(labelRect.right(), labelBottom);
        m_showHorizontalLine = true;
        return true;
    }

    return false;
}

void AlignableItem::setLocked(bool locked)
{
    if (m_locked == locked) return;
    m_locked = locked;
    if (auto item = asGraphicsItem()) {
        // 仅控制可移动标志，保持选择等其它标志不变
        item->setFlag(QGraphicsItem::ItemIsMovable, !locked);
        // 结束对齐线显示
        m_showAlignmentLine = false;
        m_showHorizontalLine = false;
        m_showVerticalLine = false;
    }
}

void AlignableItem::setAlignmentEnabled(bool enabled)
{
    if (m_alignmentEnabled == enabled) return;
    m_alignmentEnabled = enabled;
    // 关闭时清理对齐线
    if (!m_alignmentEnabled) {
        m_showAlignmentLine = false;
        m_showHorizontalLine = false;
        m_showVerticalLine = false;
    }
    // 触发重绘以去除辅助线
    if (auto item = asGraphicsItem()) {
        item->update();
    }
}

bool AlignableItem::checkLabelVerticalAlignment(const QRectF& currentRect, const QRectF& labelRect)
{
    QGraphicsItem* item = asGraphicsItem();
    
    // 计算垂直中心线
    qreal currentCenterX = currentRect.center().x();
    qreal labelCenterX = labelRect.center().x();

    // 检查与标签垂直中心线对齐
    if (qAbs(currentCenterX - labelCenterX) < ALIGNMENT_THRESHOLD) {
        // 计算需要移动的距离
        qreal deltaX = labelCenterX - currentCenterX;
        
        // 移动到对齐位置
        QPointF newPos = item->pos() + QPointF(deltaX, 0);
        item->setPos(newPos);

        // 设置垂直对齐线（纵跨整个标签）
        m_verticalP1 = QPointF(labelCenterX, labelRect.top());
        m_verticalP2 = QPointF(labelCenterX, labelRect.bottom());
        m_showVerticalLine = true;

        return true;
    }

    // 左边与标签左边对齐
    qreal currentLeft = currentRect.left();
    qreal labelLeft = labelRect.left();
    if (qAbs(currentLeft - labelLeft) < ALIGNMENT_THRESHOLD) {
        qreal deltaX = labelLeft - currentLeft;
        QPointF newPos = item->pos() + QPointF(deltaX, 0);
        item->setPos(newPos);

        m_verticalP1 = QPointF(labelLeft, labelRect.top());
        m_verticalP2 = QPointF(labelLeft, labelRect.bottom());
        m_showVerticalLine = true;
        return true;
    }

    // 右边与标签右边对齐
    qreal currentRight = currentRect.right();
    qreal labelRight = labelRect.right();
    if (qAbs(currentRight - labelRight) < ALIGNMENT_THRESHOLD) {
        qreal deltaX = labelRight - currentRight;
        QPointF newPos = item->pos() + QPointF(deltaX, 0);
        item->setPos(newPos);

        m_verticalP1 = QPointF(labelRight, labelRect.top());
        m_verticalP2 = QPointF(labelRight, labelRect.bottom());
        m_showVerticalLine = true;
        return true;
    }

    return false;
}
