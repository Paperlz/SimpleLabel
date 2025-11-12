#ifndef UNDOMANAGER_H
#define UNDOMANAGER_H

#include <QObject>
#include <QUndoStack>
#include <QAction>
#include <QPointF>
#include <QRectF>
#include <QGraphicsItem>
#include <QVector>
#include <QString>

class LabelScene;
class TextItem;
class QRCodeItem;
class BarcodeItem;
class TableItem;

/**
 * @brief 撤销管理器类
 * 
 * 这个类负责管理所有的撤销/重做操作，提供统一的接口给UI层使用
 * 它封装了QUndoStack，并提供了便捷的方法来执行各种撤销命令
 */
class UndoManager : public QObject
{
    Q_OBJECT
    
public:
    explicit UndoManager(LabelScene* scene, QObject* parent = nullptr);
    
    // 获取撤销/重做动作，用于添加到菜单和工具栏
    QAction* undoAction() const;
    QAction* redoAction() const;
    
    // 获取撤销栈，用于高级操作
    QUndoStack* undoStack() const;
    
    // 清空撤销栈
    void clear();
    
    // 设置撤销栈的清洁状态
    void setClean();
    
    // 检查是否有未保存的修改
    bool isClean() const;
    
    // 开始/结束宏命令（用于组合多个操作）
    void beginMacro(const QString& description);
    void endMacro();
    
public slots:
    // 图形项操作
    void addItem(QGraphicsItem* item, const QString& description = QString());
    void removeItem(QGraphicsItem* item, const QString& description = QString());
    void cutItem(QGraphicsItem* item, const QString& description = QString());
    void moveItem(QGraphicsItem* item, const QPointF& oldPos, const QPointF& newPos, bool allowMerge = true);
    void resizeItem(QGraphicsItem* item, const QRectF& oldRect, const QRectF& newRect);
    void movePolygonVertex(QGraphicsItem* item, int index, const QPointF& oldPos, const QPointF& newPos);
    void scalePolygon(QGraphicsItem* item,
                      const QVector<QPointF>& oldPoints, const QVector<QPointF>& newPoints,
                      const QSizeF& oldSize, const QSizeF& newSize,
                      const QPointF& oldPos, const QPointF& newPos);
    void pasteItem(QGraphicsItem* item, const QString& description);
    
    // 内容修改操作
    void changeText(TextItem* textItem, const QString& oldText, const QString& newText);
    void changeQRCodeData(QRCodeItem* qrItem, const QString& oldData, const QString& newData);
    void changeBarcodeData(BarcodeItem* barcodeItem, const QString& oldData, const QString& newData);
    void changeCornerRadius(QGraphicsItem* item, double oldRadius, double newRadius);
    void changeLinePenStyle(QGraphicsItem* item, const QPen& oldPen, const QPen& newPen);
    void changeTableStructure(TableItem* item,
                              const QVector<qreal>& oldCols,
                              const QVector<qreal>& newCols,
                              const QVector<qreal>& oldRows,
                              const QVector<qreal>& newRows,
                              const QString& description = QString());
    
    // 手动触发撤销/重做
    void undo();
    void redo();
    
signals:
    // 当撤销栈状态改变时发射的信号
    void canUndoChanged(bool canUndo);
    void canRedoChanged(bool canRedo);
    void cleanChanged(bool clean);
    void undoTextChanged(const QString& undoText);
    void redoTextChanged(const QString& redoText);
    
private slots:
    void onUndoStackChanged();
    
private:
    LabelScene* m_scene;
    QUndoStack* m_undoStack;
    QAction* m_undoAction;
    QAction* m_redoAction;
    
    // 生成默认的操作描述
    QString generateAddDescription(QGraphicsItem* item);
    QString generateRemoveDescription(QGraphicsItem* item);
    QString generatePasteDescription(QGraphicsItem* item);
};

#endif // UNDOMANAGER_H
