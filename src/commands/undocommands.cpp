#include "undocommands.h"
#include "../graphics/labelscene.h"
#include "../graphics/textitem.h"
#include "../graphics/qrcodeitem.h"
#include "../graphics/barcodeitem.h"
#include "../graphics/imageitem.h"
#include "../graphics/lineitem.h"
#include "../graphics/rectangleitem.h"
#include "../graphics/circleitem.h"
#include "../graphics/polygonitem.h"
#include "../graphics/tableitem.h"
#include <QGraphicsItem>

// BaseUndoCommand 实现
BaseUndoCommand::BaseUndoCommand(LabelScene* scene, QUndoCommand* parent)
    : QUndoCommand(parent), m_scene(scene)
{
}

// AddItemCommand 实现
AddItemCommand::AddItemCommand(LabelScene* scene, QGraphicsItem* item, const QString& description)
    : BaseUndoCommand(scene), m_item(item), m_itemOwned(false)
{
    setText(description);
}

AddItemCommand::~AddItemCommand()
{
    if (m_itemOwned && m_item) {
        delete m_item;
    }
}

void AddItemCommand::undo()
{
    if (m_item && m_scene) {
        m_scene->removeItem(m_item);
        m_itemOwned = true;
    }
}

void AddItemCommand::redo()
{
    if (m_item && m_scene) {
        m_scene->addItem(m_item);
        m_itemOwned = false;
    }
}

// RemoveItemCommand 实现
RemoveItemCommand::RemoveItemCommand(LabelScene* scene, QGraphicsItem* item, const QString& description)
    : BaseUndoCommand(scene), m_item(item), m_itemOwned(false)
{
    setText(description);
    
    // 保存item的状态信息
    if (m_item) {
        m_itemPos = m_item->pos();
        m_itemZValue = m_item->zValue();
        m_itemSelected = m_item->isSelected();
        
        // 立即从场景中移除item并获取所有权
        if (m_scene) {
            m_scene->removeItem(m_item);
            m_itemOwned = true;
        }
    }
}

RemoveItemCommand::~RemoveItemCommand()
{
    if (m_itemOwned && m_item) {
        delete m_item;
    }
}

void RemoveItemCommand::undo()
{
    if (m_item && m_scene) {
        // 恢复item的状态
        m_item->setPos(m_itemPos);
        m_item->setZValue(m_itemZValue);
        m_item->setSelected(m_itemSelected);
        
        // 将item重新添加到场景中
        m_scene->addItem(m_item);
        m_itemOwned = false;
    }
}

void RemoveItemCommand::redo()
{
    if (m_item && m_scene) {
        m_scene->removeItem(m_item);
        m_itemOwned = true;
    }
}

// MoveItemCommand 实现
MoveItemCommand::MoveItemCommand(LabelScene* scene, QGraphicsItem* item,
                                const QPointF& oldPos, const QPointF& newPos, bool allowMerge)
    : BaseUndoCommand(scene), m_item(item), m_oldPos(oldPos), m_newPos(newPos), m_allowMerge(allowMerge)
{
    setText(QObject::tr("移动图形项"));
}

void MoveItemCommand::undo()
{
    if (m_item) {
        m_item->setPos(m_oldPos);
    }
}

void MoveItemCommand::redo()
{
    if (m_item) {
        m_item->setPos(m_newPos);
    }
}

bool MoveItemCommand::mergeWith(const QUndoCommand* other)
{
    // 如果不允许合并，直接返回false
    if (!m_allowMerge) {
        return false;
    }
    
    const MoveItemCommand* moveCommand = static_cast<const MoveItemCommand*>(other);
    if (moveCommand->m_item != m_item) {
        return false;
    }
    
    // 如果新命令也不允许合并，则不合并
    if (!moveCommand->m_allowMerge) {
        return false;
    }
    
    // 计算移动距离，如果距离太大就不合并
    QPointF distance = moveCommand->m_newPos - m_oldPos;
    qreal moveDistance = QPointF::dotProduct(distance, distance); // 平方距离
    const qreal MAX_MERGE_DISTANCE = 100.0; // 最大合并距离的平方
    
    if (moveDistance > MAX_MERGE_DISTANCE) {
        return false; // 距离太大，不合并
    }
    
    m_newPos = moveCommand->m_newPos;
    return true;
}

// ResizeItemCommand 实现
ResizeItemCommand::ResizeItemCommand(LabelScene* scene, QGraphicsItem* item,
                                    const QRectF& oldRect, const QRectF& newRect)
    : BaseUndoCommand(scene), m_item(item), m_oldRect(oldRect), m_newRect(newRect)
{
    setText(QObject::tr("调整大小"));
}

void ResizeItemCommand::undo()
{
    if (auto textItem = dynamic_cast<TextItem*>(m_item)) {
        textItem->setSize(m_oldRect.size());
    } else if (auto qrItem = dynamic_cast<QRCodeItem*>(m_item)) {
        qrItem->setSize(m_oldRect.size());
    } else if (auto barcodeItem = dynamic_cast<BarcodeItem*>(m_item)) {
        barcodeItem->setSize(m_oldRect.size());
    } else if (auto imageItem = dynamic_cast<ImageItem*>(m_item)) {
        imageItem->setSize(m_oldRect.size());
    } else if (auto rectItem = dynamic_cast<RectangleItem*>(m_item)) {
        rectItem->setSize(m_oldRect.size());
    } else if (auto circleItem = dynamic_cast<CircleItem*>(m_item)) {
        circleItem->setSize(m_oldRect.size());
    } else if (auto lineItem = dynamic_cast<LineItem*>(m_item)) {
        // LineItem doesn't have setSize, use setLine instead
        QPointF startPoint = m_oldRect.topLeft();
        QPointF endPoint = m_oldRect.bottomRight();
        lineItem->setLine(startPoint, endPoint);
    }
}

void ResizeItemCommand::redo()
{
    if (auto textItem = dynamic_cast<TextItem*>(m_item)) {
        textItem->setSize(m_newRect.size());
    } else if (auto qrItem = dynamic_cast<QRCodeItem*>(m_item)) {
        qrItem->setSize(m_newRect.size());
    } else if (auto barcodeItem = dynamic_cast<BarcodeItem*>(m_item)) {
        barcodeItem->setSize(m_newRect.size());
    } else if (auto imageItem = dynamic_cast<ImageItem*>(m_item)) {
        imageItem->setSize(m_newRect.size());
    } else if (auto rectItem = dynamic_cast<RectangleItem*>(m_item)) {
        rectItem->setSize(m_newRect.size());
    } else if (auto circleItem = dynamic_cast<CircleItem*>(m_item)) {
        circleItem->setSize(m_newRect.size());
    } else if (auto lineItem = dynamic_cast<LineItem*>(m_item)) {
        // LineItem doesn't have setSize, use setLine instead
        QPointF startPoint = m_newRect.topLeft();
        QPointF endPoint = m_newRect.bottomRight();
        lineItem->setLine(startPoint, endPoint);
    }
}

bool ResizeItemCommand::mergeWith(const QUndoCommand* other)
{
    const ResizeItemCommand* resizeCommand = static_cast<const ResizeItemCommand*>(other);
    if (resizeCommand->m_item != m_item) {
        return false;
    }
    // 针对 LineItem：不要合并，确保每次拖动都是一个独立撤销步
    if (dynamic_cast<LineItem*>(m_item)) {
        return false;
    }
    // 其他类型可以合并到最新尺寸
    m_newRect = resizeCommand->m_newRect;
    return true;
}

// ChangeTextCommand 实现
ChangeTextCommand::ChangeTextCommand(LabelScene* scene, TextItem* textItem,
                                    const QString& oldText, const QString& newText)
    : BaseUndoCommand(scene), m_textItem(textItem), m_oldText(oldText), m_newText(newText)
{
    setText(QObject::tr("修改文本"));
}

void ChangeTextCommand::undo()
{
    if (m_textItem) {
        m_textItem->setText(m_oldText);
    }
}

void ChangeTextCommand::redo()
{
    if (m_textItem) {
        m_textItem->setText(m_newText);
    }
}

// ChangeQRCodeDataCommand 实现
ChangeQRCodeDataCommand::ChangeQRCodeDataCommand(LabelScene* scene, QRCodeItem* qrItem,
                                                const QString& oldData, const QString& newData)
    : BaseUndoCommand(scene), m_qrItem(qrItem), m_oldData(oldData), m_newData(newData)
{
    setText(QObject::tr("修改二维码数据"));
}

void ChangeQRCodeDataCommand::undo()
{
    if (m_qrItem) {
        m_qrItem->setText(m_oldData);
    }
}

void ChangeQRCodeDataCommand::redo()
{
    if (m_qrItem) {
        m_qrItem->setText(m_newData);
    }
}

// ChangeBarcodeDataCommand 实现
ChangeBarcodeDataCommand::ChangeBarcodeDataCommand(LabelScene* scene, BarcodeItem* barcodeItem,
                                                  const QString& oldData, const QString& newData)
    : BaseUndoCommand(scene), m_barcodeItem(barcodeItem), m_oldData(oldData), m_newData(newData)
{
    setText(QObject::tr("修改条形码数据"));
}

void ChangeBarcodeDataCommand::undo()
{
    if (m_barcodeItem) {
        m_barcodeItem->setData(m_oldData);
    }
}

void ChangeBarcodeDataCommand::redo()
{
    if (m_barcodeItem) {
        m_barcodeItem->setData(m_newData);
    }
}

// CutItemCommand 实现
CutItemCommand::CutItemCommand(LabelScene* scene, QGraphicsItem* item, const QString& description)
    : BaseUndoCommand(scene), m_item(item), m_itemOwned(false)
{
    setText(description);
    
    // 保存item的当前状态
    m_itemPos = item->pos();
    m_itemZValue = item->zValue();
    m_itemSelected = item->isSelected();
    
    // 保存当前复制状态
    saveCopyState(m_oldCopyState);
}

CutItemCommand::~CutItemCommand()
{
    if (m_itemOwned && m_item) {
        delete m_item;
    }
}

void CutItemCommand::undo()
{
    if (m_item && m_scene) {
        // 恢复item到场景
        m_scene->addItem(m_item);
        m_item->setPos(m_itemPos);
        m_item->setZValue(m_itemZValue);
        m_item->setSelected(m_itemSelected);
        m_itemOwned = false;
        
        // 恢复复制状态
        restoreCopyState(m_oldCopyState);
        
        qDebug() << "撤销剪切:" << m_item;
    }
}

void CutItemCommand::redo()
{
    if (m_item && m_scene) {
        // 执行剪切操作
        performCut();
        
        // 从场景中移除item
        m_scene->removeItem(m_item);
        m_itemOwned = true;
        
        // 保存新的复制状态
        saveCopyState(m_newCopyState);
        
        qDebug() << "重做剪切:" << m_item;
    }
}

void CutItemCommand::saveCopyState(CopyState& state)
{
    state.qrCodeHasCopied = QRCodeItem::s_hasCopiedItem;
    state.barcodeHasCopied = BarcodeItem::s_hasCopiedItem;
    state.textHasCopied = TextItem::s_hasCopiedItem;
    state.imageHasCopied = ImageItem::s_hasCopiedItem;
    state.lineHasCopied = LineItem::s_hasCopiedItem;
    state.rectangleHasCopied = RectangleItem::s_hasCopiedItem;
    state.circleHasCopied = CircleItem::s_hasCopiedItem;
    state.lastCopiedItemType = static_cast<int>(LabelScene::s_lastCopiedItemType);
}

void CutItemCommand::restoreCopyState(const CopyState& state)
{
    QRCodeItem::s_hasCopiedItem = state.qrCodeHasCopied;
    BarcodeItem::s_hasCopiedItem = state.barcodeHasCopied;
    TextItem::s_hasCopiedItem = state.textHasCopied;
    ImageItem::s_hasCopiedItem = state.imageHasCopied;
    LineItem::s_hasCopiedItem = state.lineHasCopied;
    RectangleItem::s_hasCopiedItem = state.rectangleHasCopied;
    CircleItem::s_hasCopiedItem = state.circleHasCopied;
    LabelScene::s_lastCopiedItemType = static_cast<LabelScene::LastCopiedItemType>(state.lastCopiedItemType);
}

void CutItemCommand::performCut()
{
    // 根据item类型执行复制操作
    if (auto textItem = dynamic_cast<TextItem*>(m_item)) {
        textItem->copyItem(textItem);
    } else if (auto qrItem = dynamic_cast<QRCodeItem*>(m_item)) {
        qrItem->copyItem(qrItem);
    } else if (auto barcodeItem = dynamic_cast<BarcodeItem*>(m_item)) {
        barcodeItem->copyItem(barcodeItem);
    } else if (auto imageItem = dynamic_cast<ImageItem*>(m_item)) {
        imageItem->copyItem(imageItem);
    } else if (auto lineItem = dynamic_cast<LineItem*>(m_item)) {
        lineItem->copyItem(lineItem);
    } else if (auto rectItem = dynamic_cast<RectangleItem*>(m_item)) {
        rectItem->copyItem(rectItem);
    } else if (auto circleItem = dynamic_cast<CircleItem*>(m_item)) {
        circleItem->copyItem(circleItem);
    }
}

// ChangeCornerRadiusCommand 实现
ChangeCornerRadiusCommand::ChangeCornerRadiusCommand(LabelScene* scene, QGraphicsItem* item,
                                                     double oldRadius, double newRadius)
    : BaseUndoCommand(scene), m_item(item), m_oldRadius(oldRadius), m_newRadius(newRadius)
{
    setText(QObject::tr("修改圆角半径"));
}

void ChangeCornerRadiusCommand::undo()
{
    if (auto rect = dynamic_cast<RectangleItem*>(m_item)) {
        rect->setCornerRadius(m_oldRadius);
    }
}

void ChangeCornerRadiusCommand::redo()
{
    if (auto rect = dynamic_cast<RectangleItem*>(m_item)) {
        rect->setCornerRadius(m_newRadius);
    }
}

bool ChangeCornerRadiusCommand::mergeWith(const QUndoCommand* other)
{
    const ChangeCornerRadiusCommand* cmd = static_cast<const ChangeCornerRadiusCommand*>(other);
    if (cmd->m_item != m_item) return false;
    // 将最新目标半径合并
    m_newRadius = cmd->m_newRadius;
    return true;
}

// ChangePenStyleCommand 实现
ChangePenStyleCommand::ChangePenStyleCommand(LabelScene* scene, QGraphicsItem* item,
                                             const QPen& oldPen, const QPen& newPen)
    : BaseUndoCommand(scene), m_item(item), m_oldPen(oldPen), m_newPen(newPen)
{
    setText(QObject::tr("修改线条样式"));
}

void ChangePenStyleCommand::undo()
{
    if (auto line = dynamic_cast<LineItem*>(m_item)) {
        line->setPen(m_oldPen);
    } else if (auto rect = dynamic_cast<RectangleItem*>(m_item)) {
        rect->setPen(m_oldPen);
    } else if (auto circle = dynamic_cast<CircleItem*>(m_item)) {
        circle->setPen(m_oldPen);
    }
}

void ChangePenStyleCommand::redo()
{
    if (auto line = dynamic_cast<LineItem*>(m_item)) {
        line->setPen(m_newPen);
    } else if (auto rect = dynamic_cast<RectangleItem*>(m_item)) {
        rect->setPen(m_newPen);
    } else if (auto circle = dynamic_cast<CircleItem*>(m_item)) {
        circle->setPen(m_newPen);
    }
}

bool ChangePenStyleCommand::mergeWith(const QUndoCommand* other)
{
    Q_UNUSED(other);
    return false; // 不合并
}

// MovePolygonVertexCommand 实现
MovePolygonVertexCommand::MovePolygonVertexCommand(LabelScene* scene, QGraphicsItem* item,
                                                   int index, const QPointF& oldPos, const QPointF& newPos)
    : BaseUndoCommand(scene), m_item(item), m_index(index), m_oldPos(oldPos), m_newPos(newPos)
{
    setText(QObject::tr("移动多边形顶点"));
}

void MovePolygonVertexCommand::undo()
{
    if (auto poly = dynamic_cast<PolygonItem*>(m_item)) {
        poly->setVertex(m_index, m_oldPos);
    }
}

void MovePolygonVertexCommand::redo()
{
    if (auto poly = dynamic_cast<PolygonItem*>(m_item)) {
        poly->setVertex(m_index, m_newPos);
    }
}

bool MovePolygonVertexCommand::mergeWith(const QUndoCommand* other)
{
    const MovePolygonVertexCommand* cmd = dynamic_cast<const MovePolygonVertexCommand*>(other);
    if (!cmd) return false;
    if (cmd->m_item != m_item || cmd->m_index != m_index) return false;
    // 合并到最新的位置
    m_newPos = cmd->m_newPos;
    return true;
}

// ScalePolygonCommand 实现
ScalePolygonCommand::ScalePolygonCommand(LabelScene* scene, QGraphicsItem* item,
                                         const QVector<QPointF>& oldPoints, const QVector<QPointF>& newPoints,
                                         const QSizeF& oldSize, const QSizeF& newSize,
                                         const QPointF& oldPos, const QPointF& newPos)
    : BaseUndoCommand(scene), m_item(item), m_oldPoints(oldPoints), m_newPoints(newPoints),
      m_oldSize(oldSize), m_newSize(newSize), m_oldPos(oldPos), m_newPos(newPos)
{
    setText(QObject::tr("缩放多边形"));
}

void ScalePolygonCommand::undo()
{
    if (auto poly = dynamic_cast<PolygonItem*>(m_item)) {
        poly->setPoints(m_oldPoints);
        poly->setSize(m_oldSize);
        poly->setPos(m_oldPos);
        poly->update();
    }
}

void ScalePolygonCommand::redo()
{
    if (auto poly = dynamic_cast<PolygonItem*>(m_item)) {
        poly->setPoints(m_newPoints);
        poly->setSize(m_newSize);
        poly->setPos(m_newPos);
        poly->update();
    }
}

ChangeTableStructureCommand::ChangeTableStructureCommand(LabelScene* scene, TableItem* item,
                                                         const QVector<qreal>& oldCols,
                                                         const QVector<qreal>& newCols,
                                                         const QVector<qreal>& oldRows,
                                                         const QVector<qreal>& newRows,
                                                         const QString& description)
    : BaseUndoCommand(scene),
      m_item(item),
      m_oldCols(oldCols),
      m_newCols(newCols),
      m_oldRows(oldRows),
      m_newRows(newRows)
{
    if (!description.isEmpty()) {
        setText(description);
    } else {
        setText(QObject::tr("调整表格结构"));
    }
}

void ChangeTableStructureCommand::apply(const QVector<qreal>& cols, const QVector<qreal>& rows)
{
    if (!m_item) {
        return;
    }
    m_item->setStructure(cols, rows);
}

void ChangeTableStructureCommand::undo()
{
    apply(m_oldCols, m_oldRows);
}

void ChangeTableStructureCommand::redo()
{
    apply(m_newCols, m_newRows);
}
