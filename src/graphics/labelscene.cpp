#include "labelscene.h"
#include "../commands/undomanager.h"
#include <QPainter>
#include <QGraphicsItem>
#include <QDebug>
#include <QTimer>
#include <QMenu>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsProxyWidget>
#include "alignableitem.h"
#include "staritem.h"
#include "arrowitem.h"
#include "polygonitem.h"

// 定义静态变量
LabelScene::LastCopiedItemType LabelScene::s_lastCopiedItemType = LabelScene::None;

// 构造函数
LabelScene::LabelScene(QObject *parent)
    : QGraphicsScene(parent)
    , m_activeItem(nullptr)
    , m_mode(SelectMode)
    , m_undoManager(nullptr)
    , m_allowMoveCommandMerge(true)
    , m_isDrawing(false)
    , m_drawingItem(nullptr)
    , m_selectionFrame(nullptr)
    , m_draggingSelectionFrame(false)
{
    initializeScene();
    setupSelectionFrame();
    connect(this, &QGraphicsScene::selectionChanged, this, [this]() {
        handleSelectionChanged();
    });
    
    // 初始化移动定时器
    m_moveTimer = new QTimer(this);
    m_moveTimer->setSingleShot(true);
    m_moveTimer->setInterval(500); // 500毫秒后禁止合并
    connect(m_moveTimer, &QTimer::timeout, this, [this]() {
        m_allowMoveCommandMerge = false;
    });
}

// 设置撤销管理器
void LabelScene::setUndoManager(UndoManager* undoManager)
{
    m_undoManager = undoManager;
    if (m_undoManager && m_undoManager->undoStack()) {
        // 撤销/重做后刷新选择框几何，确保外框跟随多选项回退/前进的位置
        connect(m_undoManager->undoStack(), &QUndoStack::indexChanged, this, [this](int){
            if (m_selectionFrame && m_selectionFrame->isVisible()) {
                m_selectionFrame->refreshGeometry();
            }
        });
    }
}

// 析构函数
LabelScene::~LabelScene()
{
}

// 初始化场景
void LabelScene::initializeScene()
{
    setSceneRect(0, 0, 800, 600); // 设置默认场景大小
    setBackgroundBrush(Qt::white);
}

void LabelScene::setupSelectionFrame()
{
    if (m_selectionFrame) {
        removeItem(m_selectionFrame);
        delete m_selectionFrame;
        m_selectionFrame = nullptr;
    }

    m_selectionFrame = new SelectionFrame();
    addItem(m_selectionFrame);
    m_selectionFrame->setVisible(false);
    m_draggingSelectionFrame = false;
}

void LabelScene::handleSelectionChanged()
{
    if (!m_selectionFrame) {
        return;
    }

    m_draggingSelectionFrame = false;

    QList<QGraphicsItem*> selected = selectedItems();
    QList<QGraphicsItem*> filtered;
    filtered.reserve(selected.size());

    for (QGraphicsItem* item : selected) {
        if (!item) {
            continue;
        }
        if (item == m_selectionFrame) {
            continue;
        }
        if (item->data(0).toString() == "labelBackground") {
            continue;
        }
        filtered.append(item);
    }

    m_selectionFrame->setItems(filtered);
    if (m_selectionFrame->isVisible()) {
        m_selectionFrame->refreshGeometry();
    }
}

void LabelScene::refreshSelectionFrameGeometry()
{
    if (m_selectionFrame && m_selectionFrame->isVisible()) {
        m_selectionFrame->refreshGeometry();
    }
}

QList<QGraphicsItem*> LabelScene::gatherMovableSelection() const
{
    QList<QGraphicsItem*> result;
    const QList<QGraphicsItem*> currentSelection = selectedItems();
    result.reserve(currentSelection.size());

    for (QGraphicsItem* item : currentSelection) {
        if (!item) {
            continue;
        }
        if (item == m_selectionFrame) {
            continue;
        }
        if (item->data(0).toString() == "labelBackground") {
            continue;
        }
        if (qgraphicsitem_cast<QGraphicsProxyWidget*>(item)) {
            continue;
        }
        if (auto alignable = dynamic_cast<AlignableItem*>(item)) {
            if (alignable->isLocked()) {
                continue;
            }
        }
        if (!result.contains(item)) {
            result.append(item);
        }
    }

    return result;
}

void LabelScene::recordItemInitialState(QGraphicsItem* item)
{
    if (!item || item == m_selectionFrame) {
        return;
    }

    m_itemStartPositions[item] = item->pos();

    if (auto textItem = dynamic_cast<TextItem*>(item)) {
        m_itemStartRects[item] = QRectF(QPointF(0, 0), textItem->size());
    } else if (auto qrItem = dynamic_cast<QRCodeItem*>(item)) {
        m_itemStartRects[item] = QRectF(QPointF(0, 0), qrItem->size());
    } else if (auto barcodeItem = dynamic_cast<BarcodeItem*>(item)) {
        m_itemStartRects[item] = QRectF(QPointF(0, 0), barcodeItem->size());
    } else if (auto imageItem = dynamic_cast<ImageItem*>(item)) {
        m_itemStartRects[item] = QRectF(QPointF(0, 0), imageItem->size());
    } else if (auto rectItem = dynamic_cast<RectangleItem*>(item)) {
        m_itemStartRects[item] = QRectF(QPointF(0, 0), rectItem->size());
    } else if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
        m_itemStartRects[item] = QRectF(QPointF(0, 0), circleItem->size());
    } else if (auto lineItem = dynamic_cast<LineItem*>(item)) {
        m_itemStartRects[item] = QRectF(lineItem->startPoint(), lineItem->endPoint());
    } else if (auto starItem = dynamic_cast<StarItem*>(item)) {
        m_itemStartRects[item] = QRectF(QPointF(0,0), starItem->size());
    } else if (auto arrowItem = dynamic_cast<ArrowItem*>(item)) {
        m_itemStartRects[item] = QRectF(arrowItem->startPoint(), arrowItem->endPoint());
    } else if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
        // 使用其 boundingRect 作为几何快照（局部坐标）
        m_itemStartRects[item] = polyItem->boundingRect();
    } else {
        m_itemStartRects.remove(item);
    }
}

QRectF LabelScene::captureItemGeometry(QGraphicsItem* item) const
{
    if (auto textItem = dynamic_cast<TextItem*>(item)) {
        return QRectF(QPointF(0, 0), textItem->size());
    }
    if (auto qrItem = dynamic_cast<QRCodeItem*>(item)) {
        return QRectF(QPointF(0, 0), qrItem->size());
    }
    if (auto barcodeItem = dynamic_cast<BarcodeItem*>(item)) {
        return QRectF(QPointF(0, 0), barcodeItem->size());
    }
    if (auto imageItem = dynamic_cast<ImageItem*>(item)) {
        return QRectF(QPointF(0, 0), imageItem->size());
    }
    if (auto rectItem = dynamic_cast<RectangleItem*>(item)) {
        return QRectF(QPointF(0, 0), rectItem->size());
    }
    if (auto circleItem = dynamic_cast<CircleItem*>(item)) {
        return QRectF(QPointF(0, 0), circleItem->size());
    }
    if (auto lineItem = dynamic_cast<LineItem*>(item)) {
        return QRectF(lineItem->startPoint(), lineItem->endPoint());
    }
    if (auto starItem = dynamic_cast<StarItem*>(item)) {
        return QRectF(QPointF(0,0), starItem->size());
    }
    if (auto arrowItem = dynamic_cast<ArrowItem*>(item)) {
        return QRectF(arrowItem->startPoint(), arrowItem->endPoint());
    }
    if (auto polyItem = dynamic_cast<PolygonItem*>(item)) {
        return polyItem->boundingRect();
    }
    if (item == m_selectionFrame) {
        return QRectF();
    }

    return QRectF();
}

// 清除场景
void LabelScene::clearScene()
{
    // 先清空撤销栈，避免后续对已删除对象执行撤销导致空指针
    if (m_undoManager) {
        m_undoManager->clear();
    }

    if (m_selectionFrame) {
        removeItem(m_selectionFrame);
        delete m_selectionFrame;
        m_selectionFrame = nullptr;
    }
    clear();
    // 清理拖动/记录状态，防止残留引用
    m_itemStartPositions.clear();
    m_itemStartRects.clear();
    m_itemsBeingMoved.clear();
    m_activeItem = nullptr;
    m_draggingSelectionFrame = false;

    setupSelectionFrame();
    handleSelectionChanged();
    emit sceneModified();
}

// 添加标签项
void LabelScene::addLabelItem(QGraphicsItem *item)
{
    if (!item) return;

    addItem(item);
    emit itemAdded(item);
    emit sceneModified();
}

// 删除选中项
void LabelScene::removeSelectedItems()
{
    QList<QGraphicsItem*> selectedItems = this->selectedItems();
    foreach (QGraphicsItem *item, selectedItems) {
        // 使用撤销管理器记录删除操作
        if (m_undoManager) {
            // RemoveItemCommand构造函数会自动从场景中移除item
            m_undoManager->removeItem(item);
            emit itemRemoved(item);
        } else {
            // 如果没有撤销管理器，直接删除
            emit itemRemoved(item);
            removeItem(item);
            delete item;
        }
    }
    emit sceneModified();
}

// 将项目置于顶层
void LabelScene::bringToFront(QGraphicsItem *item)
{
    if (!item) return;
    qreal maxZ = 0;
    foreach (QGraphicsItem *otherItem, items()) {
        if (otherItem->data(0).toString() == "selectionFrame") {
            continue;
        }
        maxZ = qMax(maxZ, otherItem->zValue());   // 获取当前最大Z值  zValue是QGraphicsItem的一个属性，用来控制图形项的绘制顺序（Z轴方向的堆叠顺序）
    }                                                 //遍历所有图形项，找到最大的Z值(otherItem会依次指向items()中的每个图形项)
    item->setZValue(maxZ + 1);                        // 设置当前项的Z值为最大Z值加1
    emit sceneModified();                             // 发送场景修改信号
}

// 将项目置于底层
void LabelScene::sendToBack(QGraphicsItem *item)
{
    if (!item) return;

    qreal minZ = 0;
    foreach (QGraphicsItem *otherItem, items()) {
        if (otherItem->data(0).toString() == "selectionFrame") {
            continue;
        }
        minZ = qMin(minZ, otherItem->zValue());
    }
    item->setZValue(minZ - 1);
    emit sceneModified();
}

// 绘制背景（包括网格）
void LabelScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    QGraphicsScene::drawBackground(painter, rect);
}

// 鼠标按下事件
void LabelScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->scenePos();        
        
        switch (m_mode) {
            case TextMode: {
                // 在点击位置创建文本框
                TextItem *textItem = new TextItem("双击编辑文本");
                textItem->setPos(pos);
                
                // 使用撤销管理器记录添加操作（撤销管理器会自动添加到场景）
                if (m_undoManager) {
                    m_undoManager->addItem(textItem, tr("添加文本"));
                } else {
                    addItem(textItem);
                }
                
                emit itemAdded(textItem);
                emit sceneModified();

                // 创建完文本框后自动切换回选择模式
                setMode(SelectMode);
                break;
            }
            case QRCodeMode: {
                // 在点击位置创建二维码
                QRCodeItem *qrCode = new QRCodeItem("https://example.com");
                qrCode->setPos(pos);
                
                // 使用撤销管理器记录添加操作（撤销管理器会自动添加到场景）
                if (m_undoManager) {
                    m_undoManager->addItem(qrCode, tr("添加二维码"));
                } else {
                    addItem(qrCode);
                }
                
                emit itemAdded(qrCode);
                emit sceneModified();

                // 创建完二维码后自动切换回选择模式
                setMode(SelectMode);
                break;
            }
            case BarcodeMode: {
                // 在点击位置创建条形码
                BarcodeItem *barcode = new BarcodeItem("12345678", ZXing::BarcodeFormat::Code128);
                barcode->setPos(pos);

                // 使用撤销管理器记录添加操作（撤销管理器会自动添加到场景）
                if (m_undoManager) {
                    m_undoManager->addItem(barcode, tr("添加条形码"));
                } else {
                    addItem(barcode);
                }

                emit itemAdded(barcode);
                emit sceneModified();

                // 创建完条形码后自动切换回选择模式
                setMode(SelectMode);
                break;
            }
            case ImageMode: {
                // 在点击位置创建图像项（先创建空的，用户可以双击或右键更换图像）
                ImageItem *imageItem = new ImageItem();
                imageItem->setPos(pos);

                // 使用撤销管理器记录添加操作（撤销管理器会自动添加到场景）
                if (m_undoManager) {
                    m_undoManager->addItem(imageItem, tr("添加图像"));
                } else {
                    addItem(imageItem);
                }

                emit itemAdded(imageItem);
                emit sceneModified();

                // 创建完图像后自动切换回选择模式
                setMode(SelectMode);
                break;
            }
            case LineMode: {
                // 开始绘制线条
                m_isDrawing = true;
                m_drawStartPos = pos;

                // 创建线条项
                LineItem *lineItem = new LineItem(pos, pos);
                m_drawingItem = lineItem;
                addItem(lineItem);

                emit sceneModified();
                break;
            }
            case RectangleMode: {
                // 开始绘制矩形
                m_isDrawing = true;
                m_drawStartPos = pos;

                // 创建矩形项
                RectangleItem *rectItem = new RectangleItem(QSizeF(1, 1));
                rectItem->setPos(pos);
                m_drawingItem = rectItem;
                addItem(rectItem);

                emit sceneModified();
                break;
            }
            case CircleMode: {
                // 开始绘制圆形
                m_isDrawing = true;
                m_drawStartPos = pos;

                // 创建圆形项
                CircleItem *circleItem = new CircleItem(QSizeF(1, 1));
                circleItem->setPos(pos);
                m_drawingItem = circleItem;
                addItem(circleItem);

                emit sceneModified();
                break;
            }
            case StarMode: {
                // 开始绘制星形
                m_isDrawing = true;
                m_drawStartPos = pos;
                StarItem *starItem = new StarItem(QSizeF(1,1));
                starItem->setPos(pos);
                m_drawingItem = starItem;
                addItem(starItem);
                emit sceneModified();
                break;
            }
            case ArrowMode: {
                // 开始绘制箭头（线段）
                m_isDrawing = true;
                m_drawStartPos = pos;
                ArrowItem *arrowItem = new ArrowItem(pos, pos);
                m_drawingItem = arrowItem;
                addItem(arrowItem);
                emit sceneModified();
                break;
            }
            case PolygonMode: {
                // 多边形：首次点击开始，后续点击增加点，双击结束
                if (!m_isDrawing) {
                    m_isDrawing = true;
                    m_drawStartPos = pos;
                    PolygonItem *polyItem = new PolygonItem();
                    polyItem->setPos(pos);
                    // 第一个点（原点）
                    polyItem->appendPoint(QPointF(0,0));
                    // 预览点
                    polyItem->appendPoint(QPointF(0,0));
                    m_drawingItem = polyItem;
                    addItem(polyItem);
                    emit sceneModified();
                } else {
                    // 正在绘制：单击将当前预览点固化为顶点并添加新的预览点
                    if (auto polyItem = dynamic_cast<PolygonItem*>(m_drawingItem)) {
                        QPointF local = pos - polyItem->pos();
                        polyItem->updateLastPoint(local); // 固化
                        polyItem->appendPoint(local);     // 新预览
                        emit sceneModified();
                    }
                }
                break;
            }
            default: {
                m_lastMousePos = pos;
                m_itemsBeingMoved.clear();
                m_draggingSelectionFrame = false;

                // 获取点击位置的项
                QGraphicsItem* clickedItem = itemAt(pos, QTransform());

                if (!clickedItem && m_selectionFrame && m_selectionFrame->isVisible()) {
                    QPointF framePoint = m_selectionFrame->mapFromScene(pos);
                    if (m_selectionFrame->boundingRect().contains(framePoint)) {
                        clickedItem = m_selectionFrame;
                    }
                }

                if (clickedItem && clickedItem->data(0).toString() == "selectionFrame") {
                    m_activeItem = clickedItem;
                } else if (clickedItem && qgraphicsitem_cast<QGraphicsProxyWidget*>(clickedItem)) {
                    m_activeItem = nullptr;
                    QGraphicsScene::mousePressEvent(event);
                    break;
                } else if (clickedItem && clickedItem->data(0).toString() == "labelBackground") {
                    clickedItem->setSelected(false);
                    m_activeItem = nullptr;
                    QGraphicsScene::mousePressEvent(event);
                    break;
                } else {
                    m_activeItem = clickedItem;
                }

                QGraphicsScene::mousePressEvent(event);

                if (!m_activeItem) {
                    break;
                }

                auto appendMovable = [this](QGraphicsItem* item) {
                    if (!item) {
                        return;
                    }
                    if (item == m_selectionFrame) {
                        return;
                    }
                    if (item->data(0).toString() == "labelBackground") {
                        return;
                    }
                    if (qgraphicsitem_cast<QGraphicsProxyWidget*>(item)) {
                        return;
                    }
                    if (auto alignable = dynamic_cast<AlignableItem*>(item)) {
                        if (alignable->isLocked()) {
                            return;
                        }
                    }
                    if (!m_itemsBeingMoved.contains(item)) {
                        m_itemsBeingMoved.append(item);
                        recordItemInitialState(item);
                    }
                };

                QGraphicsItem* mandatoryItem = (m_activeItem == m_selectionFrame) ? nullptr : m_activeItem;

                if (m_activeItem == m_selectionFrame && m_selectionFrame) {
                    const QList<QGraphicsItem*> frameItems = m_selectionFrame->trackedItems();
                    for (QGraphicsItem* item : frameItems) {
                        appendMovable(item);
                    }
                    m_draggingSelectionFrame = true;
                } else {
                    QList<QGraphicsItem*> selection = gatherMovableSelection();
                    if (selection.isEmpty() || (mandatoryItem && !selection.contains(mandatoryItem))) {
                        selection = {m_activeItem};
                    }
                    for (QGraphicsItem* item : selection) {
                        appendMovable(item);
                    }
                    // 多选时，哪怕是从某个元素上开始拖动，也以选择框为对齐基准
                    if (m_selectionFrame && m_selectionFrame->isVisible() && m_itemsBeingMoved.size() > 1) {
                        m_draggingSelectionFrame = true;
                    }
                }

                if (m_itemsBeingMoved.isEmpty() || (mandatoryItem && !m_itemsBeingMoved.contains(mandatoryItem))) {
                    m_draggingSelectionFrame = false;
                    m_activeItem = nullptr;
                    m_itemsBeingMoved.clear();
                    break;
                }

                m_allowMoveCommandMerge = (m_itemsBeingMoved.size() == 1);
                m_moveTimer->stop();
                break;
            }
        }
    } else {
        QGraphicsScene::mousePressEvent(event);
    }
}

// 鼠标移动事件
void LabelScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isDrawing && m_drawingItem && (event->buttons() & Qt::LeftButton)) {
        // 处理绘制模式下的鼠标移动
        QPointF currentPos = event->scenePos();

        switch (m_mode) {
            case LineMode: {
                if (auto lineItem = dynamic_cast<LineItem*>(m_drawingItem)) {
                    lineItem->setEndPoint(lineItem->mapFromScene(currentPos));
                }
                break;
            }
            case RectangleMode: {
                if (auto rectItem = dynamic_cast<RectangleItem*>(m_drawingItem)) {
                    QPointF topLeft = QPointF(qMin(m_drawStartPos.x(), currentPos.x()),
                                             qMin(m_drawStartPos.y(), currentPos.y()));
                    QSizeF size = QSizeF(qAbs(currentPos.x() - m_drawStartPos.x()),
                                        qAbs(currentPos.y() - m_drawStartPos.y()));
                    rectItem->setPos(topLeft);
                    rectItem->setSize(size);
                }
                break;
            }
            case CircleMode: {
                if (auto circleItem = dynamic_cast<CircleItem*>(m_drawingItem)) {
                    QPointF topLeft = QPointF(qMin(m_drawStartPos.x(), currentPos.x()),
                                             qMin(m_drawStartPos.y(), currentPos.y()));
                    QSizeF size = QSizeF(qAbs(currentPos.x() - m_drawStartPos.x()),
                                        qAbs(currentPos.y() - m_drawStartPos.y()));
                    circleItem->setPos(topLeft);
                    circleItem->setSize(size);
                }
                break;
            }
            case StarMode: {
                if (auto starItem = dynamic_cast<StarItem*>(m_drawingItem)) {
                    QPointF topLeft = QPointF(qMin(m_drawStartPos.x(), currentPos.x()),
                                             qMin(m_drawStartPos.y(), currentPos.y()));
                    QSizeF size = QSizeF(qAbs(currentPos.x() - m_drawStartPos.x()),
                                        qAbs(currentPos.y() - m_drawStartPos.y()));
                    starItem->setPos(topLeft);
                    starItem->setSize(size);
                }
                break;
            }
            case ArrowMode: {
                if (auto arrowItem = dynamic_cast<ArrowItem*>(m_drawingItem)) {
                    arrowItem->setEndPoint(arrowItem->mapFromScene(currentPos));
                }
                break;
            }
            case PolygonMode: {
                if (auto polyItem = dynamic_cast<PolygonItem*>(m_drawingItem)) {
                    // 更新预览点位置
                    QPointF local = currentPos - polyItem->pos();
                    polyItem->updateLastPoint(local);
                }
                break;
            }
            default:
                break;
        }
        emit sceneModified();
    } else if (!m_itemsBeingMoved.isEmpty() && (event->buttons() & Qt::LeftButton)) {
        if (m_activeItem && m_activeItem != m_selectionFrame) {
            if (qgraphicsitem_cast<QGraphicsProxyWidget*>(m_activeItem)) {
                m_lastMousePos = event->scenePos();
                QGraphicsScene::mouseMoveEvent(event);
                return;
            }
            if (m_activeItem->data(0).toString() == "labelBackground") {
                m_lastMousePos = event->scenePos();
                QGraphicsScene::mouseMoveEvent(event);
                return;
            }
            if (auto alignable = dynamic_cast<AlignableItem*>(m_activeItem)) {
                if (alignable->isLocked()) {
                    m_lastMousePos = event->scenePos();
                    QGraphicsScene::mouseMoveEvent(event);
                    return;
                }
            }
            if (auto line = dynamic_cast<LineItem*>(m_activeItem)) {
                if (LineItem::s_adjustingItem == line) {
                    m_lastMousePos = event->scenePos();
                    QGraphicsScene::mouseMoveEvent(event);
                    return;
                }
            }
        }

        // 单项拖动：交给图形项自身处理（以保留其对齐逻辑）
        if (!m_draggingSelectionFrame && m_itemsBeingMoved.size() == 1) {
            QGraphicsScene::mouseMoveEvent(event);
            m_lastMousePos = event->scenePos();
            return;
        }

        QPointF newPos = event->scenePos();
        QPointF delta = newPos - m_lastMousePos;
        bool moved = false;

        if (m_draggingSelectionFrame && m_selectionFrame) {
            QPointF beforePos = m_selectionFrame->pos();
            if (!delta.isNull()) {
                m_selectionFrame->moveBy(delta.x(), delta.y());
            }
            m_selectionFrame->handleMouseMoveForAlignment();
            QPointF adjusted = m_selectionFrame->pos() - beforePos;
            delta = adjusted;
        }

        if (!delta.isNull()) {
            for (QGraphicsItem* item : m_itemsBeingMoved) {
                if (!item) {
                    continue;
                }
                if (item == m_selectionFrame) {
                    continue;
                }
                if (item->data(0).toString() == "labelBackground") {
                    continue;
                }
                if (qgraphicsitem_cast<QGraphicsProxyWidget*>(item)) {
                    continue;
                }
                if (auto alignable = dynamic_cast<AlignableItem*>(item)) {
                    if (alignable->isLocked()) {
                        continue;
                    }
                }
                if (auto line = dynamic_cast<LineItem*>(item)) {
                    if (LineItem::s_adjustingItem == line) {
                        continue;
                    }
                }

                item->moveBy(delta.x(), delta.y());
                moved = true;
            }

            if (m_selectionFrame && m_selectionFrame->isVisible() && !m_draggingSelectionFrame) {
                m_selectionFrame->moveBy(delta.x(), delta.y());
                moved = true;
            }
        }

        m_lastMousePos = newPos;
        if (moved) {
            emit sceneModified();
        }

        if (m_selectionFrame && m_selectionFrame->isVisible()) {
            m_selectionFrame->refreshGeometry();
        }
    }
    // 仅在非绘制、非分组拖动场景下，将事件传递给子项
    const bool isGroupDragging = (m_draggingSelectionFrame || (m_itemsBeingMoved.size() > 1)) && (event->buttons() & Qt::LeftButton);
    if (!(m_isDrawing && (event->buttons() & Qt::LeftButton)) && !isGroupDragging) {
        QGraphicsScene::mouseMoveEvent(event);
    }
}

// 鼠标释放事件
void LabelScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isDrawing && m_drawingItem) {
        // 完成绘制操作
        QString description;
        switch (m_mode) {
            case LineMode:
                description = tr("添加直线");
                break;
            case RectangleMode:
                description = tr("添加矩形");
                break;
            case CircleMode:
                description = tr("添加圆形");
                break;
            case StarMode:
                description = tr("添加星形");
                break;
            case ArrowMode:
                description = tr("添加箭头");
                break;
            case PolygonMode:
                // 多边形不在释放时完成，而是在双击时完成
                QGraphicsScene::mouseReleaseEvent(event);
                return;
            default:
                description = tr("添加图形");
                break;
        }

        // 使用撤销管理器记录添加操作
        if (m_undoManager) {
            m_undoManager->addItem(m_drawingItem, description);
        }

        emit itemAdded(m_drawingItem);

        // 选中新创建的项目
        clearSelection();
        m_drawingItem->setSelected(true);

        // 重置绘制状态
        m_isDrawing = false;
        m_drawingItem = nullptr;

        // 切换回选择模式
        setMode(SelectMode);

    } else if (event->button() == Qt::LeftButton && !m_itemsBeingMoved.isEmpty()) {
        if (m_activeItem && m_activeItem != m_selectionFrame && qgraphicsitem_cast<QGraphicsProxyWidget*>(m_activeItem)) {
            m_itemsBeingMoved.clear();
            m_activeItem = nullptr;
            QGraphicsScene::mouseReleaseEvent(event);
            return;
        }

        const bool multipleItems = m_itemsBeingMoved.size() > 1;
        bool macroOpened = false;
        bool anyMoveCommand = false;

        for (QGraphicsItem* item : m_itemsBeingMoved) {
            if (!item || item == m_selectionFrame) {
                continue;
            }
            if (qgraphicsitem_cast<QGraphicsProxyWidget*>(item)) {
                m_itemStartPositions.remove(item);
                m_itemStartRects.remove(item);
                continue;
            }

            if (m_itemStartPositions.contains(item)) {
                QPointF startPos = m_itemStartPositions.take(item);
                QPointF currentPos = item->pos();

                if (startPos != currentPos && m_undoManager) {
                    if (multipleItems && !macroOpened) {
                        m_undoManager->beginMacro(tr("移动多个图形项"));
                        macroOpened = true;
                    }
                    bool allowMerge = !multipleItems && m_allowMoveCommandMerge;
                    m_undoManager->moveItem(item, startPos, currentPos, allowMerge);
                    anyMoveCommand = true;
                }
            }

            if (m_itemStartRects.contains(item)) {
                QRectF startRect = m_itemStartRects.take(item);
                QRectF currentRect = captureItemGeometry(item);

                if (startRect != currentRect && m_undoManager) {
                    if (multipleItems && !macroOpened) {
                        m_undoManager->beginMacro(tr("移动多个图形项"));
                        macroOpened = true;
                    }
                    m_undoManager->resizeItem(item, startRect, currentRect);
                }
            }
        }

        if (macroOpened && m_undoManager) {
            m_undoManager->endMacro();
        }

        if (!multipleItems && anyMoveCommand && m_allowMoveCommandMerge) {
            m_moveTimer->start();
        }

        m_itemsBeingMoved.clear();
        m_activeItem = nullptr;
        m_draggingSelectionFrame = false;
        if (m_selectionFrame) {
            m_selectionFrame->handleMouseReleaseForAlignment();
            refreshSelectionFrameGeometry();
        }
    }
    QGraphicsScene::mouseReleaseEvent(event);
}

// 键盘事件
void LabelScene::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
        case Qt::Key_Delete:
            removeSelectedItems();
            break;
        case Qt::Key_Home:
            if (!selectedItems().isEmpty())
                bringToFront(selectedItems().first());
            break;
        case Qt::Key_End:
            if (!selectedItems().isEmpty())
                sendToBack(selectedItems().first());
            break;
        default:
            QGraphicsScene::keyPressEvent(event);
    }
}

void LabelScene::setMode(Mode mode)
{
    if (m_mode != mode) {
        m_mode = mode;
        emit modeChanged(mode);  // 发送模式改变信号
    }
}

// 鼠标双击事件处理
void LabelScene::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    QPointF pos = event->scenePos();
    QGraphicsItem* clickedItem = itemAt(pos, QTransform());
    
    // 检查是否双击的是文本项
    TextItem* textItem = dynamic_cast<TextItem*>(clickedItem);
    if (textItem) {
        // 发射文本双击信号，让主窗口处理编辑
        emit textItemDoubleClicked(textItem);
        return;
    }

    // 多边形绘制的结束：双击时如果正在 PolygonMode 绘制
    if (m_mode == PolygonMode && m_isDrawing && m_drawingItem) {
        if (auto polyItem = dynamic_cast<PolygonItem*>(m_drawingItem)) {
            // 固化最终点
            QPointF local = pos - polyItem->pos();
            polyItem->updateLastPoint(local);
            // 至少需要 3 个实际顶点 (points.size()-1 >=3)
            QVector<QPointF> pts = polyItem->points();
            if (pts.size() >= 4) { // 最后一个是预览点
                // 1) 去掉预览点
                pts.removeLast();

                // 2) 规范化到局部(0,0) 起点，并同步尺寸与位置，保证选择框与绘制一致
                QPolygonF poly(pts);
                QRectF bounds = poly.boundingRect();
                QVector<QPointF> normalized;
                normalized.reserve(pts.size());
                for (const QPointF &p : pts) {
                    normalized.append(p - bounds.topLeft());
                }
                // 更新图元：先设置点，再设置尺寸，再平移位置
                polyItem->setPoints(normalized);
                if (bounds.size().isValid()) {
                    // 使用 AbstractShapeItem 的尺寸作为选择框与手柄基础
                    if (auto asi = dynamic_cast<AbstractShapeItem*>(polyItem)) {
                        asi->setSize(bounds.size());
                    }
                }
                polyItem->setPos(polyItem->pos() + bounds.topLeft());

                // 3) 提交撤销命令与选中
                QString desc = tr("添加多边形");
                if (m_undoManager) {
                    m_undoManager->addItem(polyItem, desc);
                }
                emit itemAdded(polyItem);
                clearSelection();
                polyItem->setSelected(true);
            } else {
                // 顶点不足，取消
                removeItem(polyItem);
                delete polyItem;
            }
            m_isDrawing = false;
            m_drawingItem = nullptr;
            setMode(SelectMode);
            return; // 已处理
        }
    }
    
    // 调用基类处理其他情况
    QGraphicsScene::mouseDoubleClickEvent(event);
}

void LabelScene::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // 检查是否点击在某个图形项上
    QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
    
    // 如果没有项，或者点击的是标签背景项，显示场景级别的右键菜单
    if (!item || (item && item->data(0).toString() == "labelBackground")) {
        // 点击在空白区域或标签背景，显示场景级别的右键菜单
        QMenu contextMenu;
        QAction *pasteAction = contextMenu.addAction("粘贴");
        
        // 检查是否有可粘贴的内容
    pasteAction->setEnabled(QRCodeItem::s_hasCopiedItem||BarcodeItem::s_hasCopiedItem||TextItem::s_hasCopiedItem||ImageItem::s_hasCopiedItem||
                   LineItem::s_hasCopiedItem||RectangleItem::s_hasCopiedItem||CircleItem::s_hasCopiedItem||StarItem::s_hasCopiedItem||
                   ArrowItem::s_hasCopiedItem||PolygonItem::s_hasCopiedItem);
        

        contextMenu.addSeparator();
        QAction *clearAction = contextMenu.addAction("清空场景");
        
        // 显示菜单并获取用户选择
        QAction *selectedAction = contextMenu.exec(event->screenPos());
        
        if (selectedAction == pasteAction) {
            // 根据最后复制的项目类型进行粘贴
            switch (s_lastCopiedItemType) {
                case QRCode:
                    if (QRCodeItem::s_hasCopiedItem) {
                        // 在空白区域粘贴二维码
                        QRCodeItem* newQRCode = new QRCodeItem(QRCodeItem::s_copiedText);
                        newQRCode->setSize(QRCodeItem::s_copiedSize);
                        newQRCode->setForegroundColor(QRCodeItem::s_copiedForegroundColor);
                        newQRCode->setBackgroundColor(QRCodeItem::s_copiedBackgroundColor);
                        
                        // 在右键点击位置创建新的二维码
                        QPointF newPos = event->scenePos();
                        newQRCode->setPos(newPos);
                        
                        addItem(newQRCode);
                        
                        // 使用撤销管理器记录粘贴操作
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newQRCode, tr("粘贴二维码"));
                        }
                        
                        clearSelection();
                        newQRCode->setSelected(true);
                        
                    }
                    break;
                    
                case Barcode:
                    if (BarcodeItem::s_hasCopiedItem) {
                        // 在空白区域粘贴条形码
                        BarcodeItem* newBarcode = new BarcodeItem(BarcodeItem::s_copiedData, BarcodeItem::s_copiedFormat);
                        newBarcode->setSize(BarcodeItem::s_copiedSize);
                        newBarcode->setForegroundColor(BarcodeItem::s_copiedForegroundColor);
                        newBarcode->setBackgroundColor(BarcodeItem::s_copiedBackgroundColor);
                        
                        // 在右键点击位置创建新的条形码
                        QPointF newPos = event->scenePos();
                        newBarcode->setPos(newPos);
                        
                        addItem(newBarcode);
                        
                        // 使用撤销管理器记录粘贴操作
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newBarcode, tr("粘贴条形码"));
                        }
                        
                        clearSelection();
                        newBarcode->setSelected(true);
                        
                    }
                    break;
                    
                case Text:
                    if (TextItem::s_hasCopiedItem) {
                        // 在空白区域粘贴文本
                        TextItem* newTextItem = new TextItem(TextItem::s_copiedText);
                        newTextItem->setFont(TextItem::s_copiedFont);
                        newTextItem->setTextColor(TextItem::s_copiedTextColor);
                        newTextItem->setBackgroundColor(TextItem::s_copiedBackgroundColor);
                        newTextItem->setBorderPen(TextItem::s_copiedBorderPen);
                        newTextItem->setBorderEnabled(TextItem::s_copiedBorderEnabled);
                        newTextItem->setSize(TextItem::s_copiedSize);
                        newTextItem->setAlignment(TextItem::s_copiedAlignment);
                        newTextItem->setWordWrap(TextItem::s_copiedWordWrap);
                        newTextItem->setAutoResize(TextItem::s_copiedAutoResize);
                        newTextItem->setBackgroundEnabled(TextItem::s_copiedBackgroundEnabled);

                        // 在右键点击位置创建新的文本项
                        QPointF newPos = event->scenePos();
                        newTextItem->setPos(newPos);

                        addItem(newTextItem);

                        // 使用撤销管理器记录粘贴操作
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newTextItem, tr("粘贴文本"));
                        }

                        clearSelection();
                        newTextItem->setSelected(true);

                    }
                    break;

                case Image:
                    if (ImageItem::s_hasCopiedItem) {
                        // 在空白区域粘贴图像
                        ImageItem* newImageItem = new ImageItem(ImageItem::s_copiedImageData);
                        newImageItem->setSize(ImageItem::s_copiedSize);
                        newImageItem->setKeepAspectRatio(ImageItem::s_copiedKeepAspectRatio);
                        newImageItem->setOpacity(ImageItem::s_copiedOpacity);

                        // 在右键点击位置创建新的图像项
                        QPointF newPos = event->scenePos();
                        newImageItem->setPos(newPos);

                        addItem(newImageItem);

                        // 使用撤销管理器记录粘贴操作
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newImageItem, tr("粘贴图像"));
                        }

                        clearSelection();
                        newImageItem->setSelected(true);

                    }
                    break;

                case Line:
                    if (LineItem::s_hasCopiedItem) {
                        // 在空白区域粘贴线条
                        LineItem* newLine = new LineItem();
                        newLine->setPen(LineItem::s_copiedPen);
                        newLine->setLine(LineItem::s_copiedStartPoint, LineItem::s_copiedEndPoint);

                        // 在右键点击位置创建新的线条
                        QPointF newPos = event->scenePos();
                        newLine->setPos(newPos);

                        addItem(newLine);

                        // 使用撤销管理器记录粘贴操作
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newLine, tr("粘贴线条"));
                        }

                        clearSelection();
                        newLine->setSelected(true);
                    }
                    break;

                case Rectangle:
                    if (RectangleItem::s_hasCopiedItem) {
                        // 在空白区域粘贴矩形
                        RectangleItem* newRect = new RectangleItem(RectangleItem::s_copiedSize);
                        newRect->setPen(RectangleItem::s_copiedPen);
                        newRect->setBrush(RectangleItem::s_copiedBrush);
                        newRect->setFillEnabled(RectangleItem::s_copiedFillEnabled);
                        newRect->setBorderEnabled(RectangleItem::s_copiedBorderEnabled);

                        // 在右键点击位置创建新的矩形
                        QPointF newPos = event->scenePos();
                        newRect->setPos(newPos);

                        addItem(newRect);

                        // 使用撤销管理器记录粘贴操作
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newRect, tr("粘贴矩形"));
                        }

                        clearSelection();
                        newRect->setSelected(true);
                    }
                    break;

                case Circle: {
                    if (CircleItem::s_hasCopiedItem) {
                        // 在空白区域粘贴圆形
                        CircleItem* newCircle = new CircleItem(CircleItem::s_copiedSize);
                        newCircle->setPen(CircleItem::s_copiedPen);
                        newCircle->setBrush(CircleItem::s_copiedBrush);
                        newCircle->setFillEnabled(CircleItem::s_copiedFillEnabled);
                        newCircle->setBorderEnabled(CircleItem::s_copiedBorderEnabled);
                        newCircle->setIsCircle(CircleItem::s_copiedIsCircle);

                        // 在右键点击位置创建新的圆形
                        QPointF newPos = event->scenePos();
                        newCircle->setPos(newPos);

                        addItem(newCircle);

                        // 使用撤销管理器记录粘贴操作
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newCircle, tr("粘贴圆形"));
                        }

                        clearSelection();
                        newCircle->setSelected(true);
                    }
                    break;
                }
                case Star: {
                    if (StarItem::s_hasCopiedItem) {
                        StarItem* newStar = new StarItem(StarItem::s_copiedSize);
                        newStar->setPen(StarItem::s_copiedPen);
                        newStar->setBrush(StarItem::s_copiedBrush);
                        newStar->setFillEnabled(StarItem::s_copiedFillEnabled);
                        newStar->setBorderEnabled(StarItem::s_copiedBorderEnabled);
                        newStar->setPointCount(StarItem::s_copiedPoints);
                        newStar->setInnerRatio(StarItem::s_copiedInnerRatio);
                        QPointF newPos = event->scenePos();
                        newStar->setPos(newPos);
                        addItem(newStar);
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newStar, tr("粘贴星形"));
                        }
                        clearSelection();
                        newStar->setSelected(true);
                    }
                    break;
                }
                case Arrow: {
                    if (ArrowItem::s_hasCopiedItem) {
                        ArrowItem* newArrow = new ArrowItem();
                        newArrow->setPen(ArrowItem::s_copiedPen);
                        newArrow->setLine(ArrowItem::s_copiedStartPoint, ArrowItem::s_copiedEndPoint);
                        newArrow->setHeadLength(ArrowItem::s_copiedHeadLength);
                        newArrow->setHeadAngle(ArrowItem::s_copiedHeadAngle);
                        QPointF newPos = event->scenePos();
                        newArrow->setPos(newPos);
                        addItem(newArrow);
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newArrow, tr("粘贴箭头"));
                        }
                        clearSelection();
                        newArrow->setSelected(true);
                    }
                    break;
                }
                case Polygon: {
                    if (PolygonItem::s_hasCopiedItem) {
                        PolygonItem* newPoly = new PolygonItem(PolygonItem::s_copiedSize);
                        newPoly->setPen(PolygonItem::s_copiedPen);
                        newPoly->setBrush(PolygonItem::s_copiedBrush);
                        newPoly->setFillEnabled(PolygonItem::s_copiedFillEnabled);
                        newPoly->setBorderEnabled(PolygonItem::s_copiedBorderEnabled);
                        newPoly->setPoints(PolygonItem::s_copiedPoints);
                        QPointF newPos = event->scenePos();
                        newPoly->setPos(newPos);
                        addItem(newPoly);
                        if (m_undoManager) {
                            m_undoManager->pasteItem(newPoly, tr("粘贴多边形"));
                        }
                        clearSelection();
                        newPoly->setSelected(true);
                    }
                    break;
                }

                default:
                    break;
            }
        }
        else if (selectedAction == clearAction) {
            // 清空场景
            clearScene();
        }
        
        event->accept();
    } else {
        // 点击在其他图形项上，让图形项处理
        QGraphicsScene::contextMenuEvent(event);
    }
}

