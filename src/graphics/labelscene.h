#ifndef LABELSCENE_H
#define LABELSCENE_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QVector>
#include <QList>
#include "qrcodeitem.h"
#include "barcodeitem.h"
#include "textitem.h"
#include "imageitem.h"
#include "lineitem.h"
#include "rectangleitem.h"
#include "circleitem.h"
#include "staritem.h"
#include "arrowitem.h"
#include "polygonitem.h"
#include "selectionframe.h"

class UndoManager;

class LabelScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum Mode {
        SelectMode,
        TextMode,
        BarcodeMode,
        QRCodeMode,
        ImageMode,
        LineMode,
        RectangleMode,
        CircleMode,
        StarMode,
        ArrowMode,
        PolygonMode
    };
    Q_ENUM(Mode)  // 使Mode可以在信号槽中使用

    // 枚举来跟踪最后复制的项目类型
    enum LastCopiedItemType {
        None,
        QRCode,
        Barcode,
        Text,
        Image,
        Line,
        Rectangle,
        Circle,
        Star,
        Arrow,
        Polygon
    };

    // 静态变量来跟踪最后复制的项目类型
    static LastCopiedItemType s_lastCopiedItemType;

    explicit LabelScene(QObject *parent = nullptr);
    ~LabelScene() override;

    // 设置撤销管理器
    void setUndoManager(UndoManager* undoManager);
    
    // 获取撤销管理器
    UndoManager* undoManager() const { return m_undoManager; }

    // 场景操作
    void clearScene();
    void setMode(Mode mode);

    // 获取和设置场景属性
    Mode mode() const { return m_mode; }
    SelectionFrame* selectionFrame() const { return m_selectionFrame; }

    public slots:
        // 图形项操作
        void addLabelItem(QGraphicsItem *item);
    void removeSelectedItems();
    void bringToFront(QGraphicsItem *item);
    void sendToBack(QGraphicsItem *item);

    signals:
    // 信号
    void itemAdded(QGraphicsItem *item);
    void itemRemoved(QGraphicsItem *item);
    void sceneModified();
    void modeChanged(Mode mode);  // 添加模式改变信号
    void textItemDoubleClicked(TextItem *textItem);  // 文本双击信号

protected:
    // 重写的事件处理函数
    void drawBackground(QPainter *painter, const QRectF &rect) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;  // 添加双击事件
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;  // 添加右键菜单事件

private:
    // 私有成员函数
    void initializeScene();
    void recordItemInitialState(QGraphicsItem* item);
    QRectF captureItemGeometry(QGraphicsItem* item) const;
    void setupSelectionFrame();
    void handleSelectionChanged();
    void refreshSelectionFrameGeometry();
    QList<QGraphicsItem*> gatherMovableSelection() const;

    // 成员变量
    QPointF m_lastMousePos; // 上次鼠标位置
    QGraphicsItem *m_activeItem; // 当前活动项
    Mode m_mode;           // 当前模式
    
    // 撤销管理器
    UndoManager* m_undoManager;
    
    // 用于跟踪图形项的初始状态
    QMap<QGraphicsItem*, QPointF> m_itemStartPositions;
    QMap<QGraphicsItem*, QRectF> m_itemStartRects;
    
    // 移动合并控制
    QTimer* m_moveTimer;
    bool m_allowMoveCommandMerge;
    QVector<QGraphicsItem*> m_itemsBeingMoved;
    SelectionFrame* m_selectionFrame;
    bool m_draggingSelectionFrame;

    // 绘制状态变量
    bool m_isDrawing;              // 是否正在绘制
    QPointF m_drawStartPos;        // 绘制起始位置
    QGraphicsItem* m_drawingItem;  // 正在绘制的项目
};

#endif // LABELSCENE_H