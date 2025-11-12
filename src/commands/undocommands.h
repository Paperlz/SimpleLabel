#ifndef UNDOCOMMANDS_H
#define UNDOCOMMANDS_H

#include <QUndoCommand>
#include <QGraphicsItem>
#include <QPointF>
#include <QRectF>
#include <QGraphicsScene>
#include <QVector>
#include <QString>

class LabelScene;
class TextItem;
class QRCodeItem;
class BarcodeItem;
class TableItem;

// 基础撤销命令类
class BaseUndoCommand : public QUndoCommand
{
public:
    explicit BaseUndoCommand(LabelScene* scene, QUndoCommand* parent = nullptr);
    
protected:
    LabelScene* m_scene;
};

// 添加图形项命令
class AddItemCommand : public BaseUndoCommand
{
public:
    AddItemCommand(LabelScene* scene, QGraphicsItem* item, const QString& description);
    ~AddItemCommand();
    
    void undo() override;
    void redo() override;
    
private:
    QGraphicsItem* m_item;
    bool m_itemOwned;
};

// 删除图形项命令
class RemoveItemCommand : public BaseUndoCommand
{
public:
    RemoveItemCommand(LabelScene* scene, QGraphicsItem* item, const QString& description);
    ~RemoveItemCommand();
    
    void undo() override;
    void redo() override;
    
private:
    QGraphicsItem* m_item;
    bool m_itemOwned;
    QPointF m_itemPos;      // 保存item被删除时的位置
    qreal m_itemZValue;     // 保存item的Z值
    bool m_itemSelected;    // 保存item的选中状态
};

// 移动图形项命令
class MoveItemCommand : public BaseUndoCommand
{
public:
    MoveItemCommand(LabelScene* scene, QGraphicsItem* item, 
                    const QPointF& oldPos, const QPointF& newPos, bool allowMerge = true);
    
    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return 1; }
    
private:
    QGraphicsItem* m_item;
    QPointF m_oldPos;
    QPointF m_newPos;
    bool m_allowMerge;
};

// 调整大小命令
class ResizeItemCommand : public BaseUndoCommand
{
public:
    ResizeItemCommand(LabelScene* scene, QGraphicsItem* item,
                      const QRectF& oldRect, const QRectF& newRect);
    
    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return 2; }
    
private:
    QGraphicsItem* m_item;
    QRectF m_oldRect;
    QRectF m_newRect;
};

// 修改文本内容命令
class ChangeTextCommand : public BaseUndoCommand
{
public:
    ChangeTextCommand(LabelScene* scene, TextItem* textItem,
                      const QString& oldText, const QString& newText);
    
    void undo() override;
    void redo() override;
    
private:
    TextItem* m_textItem;
    QString m_oldText;
    QString m_newText;
};

// 修改二维码数据命令
class ChangeQRCodeDataCommand : public BaseUndoCommand
{
public:
    ChangeQRCodeDataCommand(LabelScene* scene, QRCodeItem* qrItem,
                           const QString& oldData, const QString& newData);
    
    void undo() override;
    void redo() override;
    
private:
    QRCodeItem* m_qrItem;
    QString m_oldData;
    QString m_newData;
};

// 修改条形码数据命令
class ChangeBarcodeDataCommand : public BaseUndoCommand
{
public:
    ChangeBarcodeDataCommand(LabelScene* scene, BarcodeItem* barcodeItem,
                            const QString& oldData, const QString& newData);
    
    void undo() override;
    void redo() override;
    
private:
    BarcodeItem* m_barcodeItem;
    QString m_oldData;
    QString m_newData;
};

// 修改矩形圆角半径命令
class ChangeCornerRadiusCommand : public BaseUndoCommand
{
public:
    ChangeCornerRadiusCommand(LabelScene* scene, QGraphicsItem* item,
                              double oldRadius, double newRadius);

    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return 101; }

private:
    QGraphicsItem* m_item;
    double m_oldRadius;
    double m_newRadius;
};

// 修改线条笔样式（虚线/实线等）命令
class ChangePenStyleCommand : public BaseUndoCommand
{
public:
    ChangePenStyleCommand(LabelScene* scene, QGraphicsItem* item,
                          const QPen& oldPen, const QPen& newPen);

    void undo() override;
    void redo() override;
    int id() const override { return 102; }
    bool mergeWith(const QUndoCommand* other) override; // 不合并，返回 false

private:
    QGraphicsItem* m_item;
    QPen m_oldPen;
    QPen m_newPen;
};

// 剪切图形项命令 - 同时处理复制和删除，统一撤销
class CutItemCommand : public BaseUndoCommand
{
public:
    CutItemCommand(LabelScene* scene, QGraphicsItem* item, const QString& description);
    ~CutItemCommand();
    
    void undo() override;
    void redo() override;
    
private:
    QGraphicsItem* m_item;
    bool m_itemOwned;
    QPointF m_itemPos;      // 保存item被删除时的位置
    qreal m_itemZValue;     // 保存item的Z值
    bool m_itemSelected;    // 保存item的选中状态
    
    // 保存复制状态用于恢复
    struct CopyState {
        bool qrCodeHasCopied;
        bool barcodeHasCopied;
        bool textHasCopied;
        bool imageHasCopied;
        bool lineHasCopied;
        bool rectangleHasCopied;
        bool circleHasCopied;
        int lastCopiedItemType;
    } m_oldCopyState, m_newCopyState;
    
    void saveCopyState(CopyState& state);
    void restoreCopyState(const CopyState& state);
    void performCut();
};

// 移动多边形顶点命令（节点级编辑）
class MovePolygonVertexCommand : public BaseUndoCommand
{
public:
    MovePolygonVertexCommand(LabelScene* scene, QGraphicsItem* item,
                             int index, const QPointF& oldPos, const QPointF& newPos);
    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return 301; }
private:
    QGraphicsItem* m_item;
    int m_index;
    QPointF m_oldPos;
    QPointF m_newPos;
};

// 缩放多边形（通过外接选择框手柄调整大小）命令
class ScalePolygonCommand : public BaseUndoCommand
{
public:
    ScalePolygonCommand(LabelScene* scene, QGraphicsItem* item,
                        const QVector<QPointF>& oldPoints, const QVector<QPointF>& newPoints,
                        const QSizeF& oldSize, const QSizeF& newSize,
                        const QPointF& oldPos, const QPointF& newPos);
    void undo() override;
    void redo() override;
    int id() const override { return 302; }
private:
    QGraphicsItem* m_item;
    QVector<QPointF> m_oldPoints;
    QVector<QPointF> m_newPoints;
    QSizeF m_oldSize;
    QSizeF m_newSize;
    QPointF m_oldPos;
    QPointF m_newPos;
};

class ChangeTableStructureCommand : public BaseUndoCommand
{
public:
    ChangeTableStructureCommand(LabelScene* scene, TableItem* item,
                                const QVector<qreal>& oldCols,
                                const QVector<qreal>& newCols,
                                const QVector<qreal>& oldRows,
                                const QVector<qreal>& newRows,
                                const QString& description);

    void undo() override;
    void redo() override;

private:
    void apply(const QVector<qreal>& cols, const QVector<qreal>& rows);

    TableItem* m_item;
    QVector<qreal> m_oldCols;
    QVector<qreal> m_newCols;
    QVector<qreal> m_oldRows;
    QVector<qreal> m_newRows;
};

#endif // UNDOCOMMANDS_H
