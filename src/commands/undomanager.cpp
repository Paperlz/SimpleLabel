#include "undomanager.h"
#include "undocommands.h"
#include "../graphics/labelscene.h"
#include "../graphics/textitem.h"
#include "../graphics/qrcodeitem.h"
#include "../graphics/barcodeitem.h"
#include <QAction>

UndoManager::UndoManager(LabelScene* scene, QObject* parent)
    : QObject(parent), m_scene(scene)
{
    // 创建撤销栈
    m_undoStack = new QUndoStack(this);
    
    // 创建撤销/重做动作
    m_undoAction = m_undoStack->createUndoAction(this, tr("撤销(&U)"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    // 图标由资源库统一提供；当前未提供撤销/重做专用图标，保持文字/系统默认
    
    m_redoAction = m_undoStack->createRedoAction(this, tr("重做(&R)"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    // 图标由资源库统一提供；当前未提供撤销/重做专用图标，保持文字/系统默认
    
    // 连接信号
    connect(m_undoStack, &QUndoStack::canUndoChanged, this, &UndoManager::canUndoChanged);
    connect(m_undoStack, &QUndoStack::canRedoChanged, this, &UndoManager::canRedoChanged);
    connect(m_undoStack, &QUndoStack::cleanChanged, this, &UndoManager::cleanChanged);
    connect(m_undoStack, &QUndoStack::undoTextChanged, this, &UndoManager::undoTextChanged);
    connect(m_undoStack, &QUndoStack::redoTextChanged, this, &UndoManager::redoTextChanged);
    connect(m_undoStack, &QUndoStack::indexChanged, this, &UndoManager::onUndoStackChanged);
}

QAction* UndoManager::undoAction() const
{
    return m_undoAction;
}

QAction* UndoManager::redoAction() const
{
    return m_redoAction;
}

QUndoStack* UndoManager::undoStack() const
{
    return m_undoStack;
}

void UndoManager::clear()
{
    m_undoStack->clear();
}

void UndoManager::setClean()
{
    m_undoStack->setClean();
}

bool UndoManager::isClean() const
{
    return m_undoStack->isClean();
}

void UndoManager::beginMacro(const QString& description)
{
    m_undoStack->beginMacro(description);
}

void UndoManager::endMacro()
{
    m_undoStack->endMacro();
}

void UndoManager::addItem(QGraphicsItem* item, const QString& description)
{
    QString desc = description.isEmpty() ? generateAddDescription(item) : description;
    AddItemCommand* command = new AddItemCommand(m_scene, item, desc);
    m_undoStack->push(command);
}

void UndoManager::removeItem(QGraphicsItem* item, const QString& description)
{
    QString desc = description.isEmpty() ? generateRemoveDescription(item) : description;
    RemoveItemCommand* command = new RemoveItemCommand(m_scene, item, desc);
    m_undoStack->push(command);
}

void UndoManager::cutItem(QGraphicsItem* item, const QString& description)
{
    QString desc = description.isEmpty() ? tr("剪切图形项") : description;
    CutItemCommand* command = new CutItemCommand(m_scene, item, desc);
    m_undoStack->push(command);
}

void UndoManager::pasteItem(QGraphicsItem* item, const QString& description)
{
    QString desc = description.isEmpty() ? generatePasteDescription(item) : description;
    AddItemCommand* command = new AddItemCommand(m_scene, item, desc);
    m_undoStack->push(command);
}

void UndoManager::moveItem(QGraphicsItem* item, const QPointF& oldPos, const QPointF& newPos, bool allowMerge)
{
    // 检查位置是否真的发生了变化
    if (oldPos == newPos) {
        return;
    }
    
    MoveItemCommand* command = new MoveItemCommand(m_scene, item, oldPos, newPos, allowMerge);
    m_undoStack->push(command);
}

void UndoManager::resizeItem(QGraphicsItem* item, const QRectF& oldRect, const QRectF& newRect)
{
    // 检查大小是否真的发生了变化
    if (oldRect == newRect) {
        return;
    }
    
    ResizeItemCommand* command = new ResizeItemCommand(m_scene, item, oldRect, newRect);
    m_undoStack->push(command);
}

void UndoManager::movePolygonVertex(QGraphicsItem* item, int index, const QPointF& oldPos, const QPointF& newPos)
{
    if (!item) return;
    auto* cmd = new MovePolygonVertexCommand(m_scene, item, index, oldPos, newPos);
    m_undoStack->push(cmd);
}

void UndoManager::scalePolygon(QGraphicsItem* item,
                               const QVector<QPointF>& oldPoints, const QVector<QPointF>& newPoints,
                               const QSizeF& oldSize, const QSizeF& newSize,
                               const QPointF& oldPos, const QPointF& newPos)
{
    if (!item) return;
    auto* cmd = new ScalePolygonCommand(m_scene, item, oldPoints, newPoints, oldSize, newSize, oldPos, newPos);
    m_undoStack->push(cmd);
}

void UndoManager::changeText(TextItem* textItem, const QString& oldText, const QString& newText)
{
    if (oldText == newText) {
        return;
    }
    
    ChangeTextCommand* command = new ChangeTextCommand(m_scene, textItem, oldText, newText);
    m_undoStack->push(command);
}

void UndoManager::changeQRCodeData(QRCodeItem* qrItem, const QString& oldData, const QString& newData)
{
    if (oldData == newData) {
        return;
    }
    
    ChangeQRCodeDataCommand* command = new ChangeQRCodeDataCommand(m_scene, qrItem, oldData, newData);
    m_undoStack->push(command);
}

void UndoManager::changeBarcodeData(BarcodeItem* barcodeItem, const QString& oldData, const QString& newData)
{
    if (oldData == newData) {
        return;
    }
    
    ChangeBarcodeDataCommand* command = new ChangeBarcodeDataCommand(m_scene, barcodeItem, oldData, newData);
    m_undoStack->push(command);
}

void UndoManager::changeCornerRadius(QGraphicsItem* item, double oldRadius, double newRadius)
{
    if (!item) return;
    if (qFuzzyCompare(1.0 + oldRadius, 1.0 + newRadius)) return;
    ChangeCornerRadiusCommand* cmd = new ChangeCornerRadiusCommand(m_scene, item, oldRadius, newRadius);
    m_undoStack->push(cmd);
}

void UndoManager::changeLinePenStyle(QGraphicsItem* item, const QPen& oldPen, const QPen& newPen)
{
    if (!item) return;
    if (oldPen == newPen) return;
    ChangePenStyleCommand* cmd = new ChangePenStyleCommand(m_scene, item, oldPen, newPen);
    m_undoStack->push(cmd);
}

void UndoManager::changeTableStructure(TableItem* item,
                                       const QVector<qreal>& oldCols,
                                       const QVector<qreal>& newCols,
                                       const QVector<qreal>& oldRows,
                                       const QVector<qreal>& newRows,
                                       const QString& description)
{
    if (!item) {
        return;
    }
    if (oldCols == newCols && oldRows == newRows) {
        return;
    }

    QString desc = description.isEmpty() ? tr("调整表格结构") : description;
    auto* cmd = new ChangeTableStructureCommand(m_scene, item, oldCols, newCols, oldRows, newRows, desc);
    m_undoStack->push(cmd);
}

void UndoManager::undo()
{
    m_undoStack->undo();
}

void UndoManager::redo()
{
    m_undoStack->redo();
}

void UndoManager::onUndoStackChanged()
{
    // 可以在这里添加额外的处理逻辑
    // 比如更新UI状态、保存状态等
}

QString UndoManager::generateAddDescription(QGraphicsItem* item)
{
    if (dynamic_cast<TextItem*>(item)) {
        return tr("添加文本");
    } else if (dynamic_cast<QRCodeItem*>(item)) {
        return tr("添加二维码");
    } else if (dynamic_cast<BarcodeItem*>(item)) {
        return tr("添加条形码");
    }
    return tr("添加图形项");
}

QString UndoManager::generateRemoveDescription(QGraphicsItem* item)
{
    if (dynamic_cast<TextItem*>(item)) {
        return tr("删除文本");
    } else if (dynamic_cast<QRCodeItem*>(item)) {
        return tr("删除二维码");
    } else if (dynamic_cast<BarcodeItem*>(item)) {
        return tr("删除条形码");
    }
    return tr("删除图形项");
}

QString UndoManager::generatePasteDescription(QGraphicsItem* item)
{
    if (dynamic_cast<TextItem*>(item)) {
        return tr("粘贴文本");
    } else if (dynamic_cast<QRCodeItem*>(item)) {
        return tr("粘贴二维码");
    } else if (dynamic_cast<BarcodeItem*>(item)) {
        return tr("粘贴条形码");
    }
    return tr("粘贴图形项");
}
