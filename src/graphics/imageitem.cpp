#include "imageitem.h"
#include "labelscene.h"
#include "qrcodeitem.h"
#include "barcodeitem.h"
#include "textitem.h"
#include "lineitem.h"
#include "rectangleitem.h"
#include "circleitem.h"
#include "../commands/undomanager.h"
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QCursor>
#include <QApplication>
#include <QBuffer>
#include <QImageReader>
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QtMath>

// 静态成员变量初始化
bool ImageItem::s_hasCopiedItem = false;
QByteArray ImageItem::s_copiedImageData;
QSizeF ImageItem::s_copiedSize;
bool ImageItem::s_copiedKeepAspectRatio = true;
qreal ImageItem::s_copiedOpacity = 1.0;

ImageItem::ImageItem(const QString &imagePath, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_imagePath(imagePath)
    , m_size(200, 200)  // 默认尺寸
    , m_keepAspectRatio(true)
    , m_opacity(1.0)
    , m_isResizing(false)
    , m_resizeHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    if (!imagePath.isEmpty()) {
        loadImage(imagePath);
    }
}

ImageItem::ImageItem(const QPixmap &pixmap, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_pixmap(pixmap)
    , m_size(pixmap.size())
    , m_keepAspectRatio(true)
    , m_opacity(1.0)
    , m_isResizing(false)
    , m_resizeHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    // 将 QPixmap 转换为 QByteArray 存储
    QBuffer buffer(&m_originalImageData);
    buffer.open(QIODevice::WriteOnly);
    m_pixmap.save(&buffer, "PNG");
}

ImageItem::ImageItem(const QByteArray &imageData, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_originalImageData(imageData)
    , m_size(200, 200)
    , m_keepAspectRatio(true)
    , m_opacity(1.0)
    , m_isResizing(false)
    , m_resizeHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);

    setImageData(imageData);
}

QRectF ImageItem::boundingRect() const
{
    // Compute minimal padding so handles and selection pen are inside the bounding rect
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    const qreal selectionPenHalf = 0.5;
    const qreal padding = qMax<qreal>(1.0, qMax(halfHandle, selectionPenHalf));

    return QRectF(-padding, -padding,
                  m_size.width() + 2 * padding,
                  m_size.height() + 2 * padding);
}

void ImageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    if (m_pixmap.isNull()) {
        // 如果没有图像，绘制占位符
        painter->setPen(QPen(Qt::gray, 2, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        QRectF rect(0, 0, m_size.width(), m_size.height());
        painter->drawRect(rect);
        
        painter->setPen(Qt::gray);
        painter->drawText(rect, Qt::AlignCenter, QObject::tr("No Image"));
        return;
    }

    // 设置透明度
    painter->setOpacity(m_opacity);

    // 绘制图像
    QRectF targetRect(0, 0, m_size.width(), m_size.height());
    painter->drawPixmap(targetRect, m_pixmap, m_pixmap.rect());

    // 恢复透明度
    painter->setOpacity(1.0);

    // 如果选中，绘制选择框和调整手柄
    if (isSelected()) {
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(targetRect);

        // 绘制调整手柄
        drawResizeHandles(painter);
    }

    // 绘制对齐线
    paintAlignmentLines(painter);
}

bool ImageItem::loadImage(const QString &imagePath)
{
    if (imagePath.isEmpty()) {
        qDebug()<<"isempty";
        return false;
    }

    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        qDebug()<<"isnull";
        return false;
    }

    m_imagePath = imagePath;
    m_pixmap = pixmap;

    // 读取原始图像数据
    QFile file(imagePath);
    if (file.open(QIODevice::ReadOnly)) {
        m_originalImageData = file.readAll();
    }

    // 如果保持宽高比，调整尺寸
    if (m_keepAspectRatio) {
        QSizeF originalSize = pixmap.size();
        if (originalSize.width() > 0 && originalSize.height() > 0) {
            // 保持原始宽高比，但限制最大尺寸
            qreal maxSize = 300.0;
            qreal scale = qMin(maxSize / originalSize.width(), maxSize / originalSize.height());
            if (scale < 1.0) {
                m_size = originalSize * scale;
            } else {
                m_size = originalSize;
            }
        }
    }

    prepareGeometryChange();
    update();
    return true;
}

bool ImageItem::setImageData(const QByteArray &imageData)
{
    if (imageData.isEmpty()) {
        return false;
    }

    QPixmap pixmap;
    if (!pixmap.loadFromData(imageData)) {
        return false;
    }

    m_originalImageData = imageData;
    m_pixmap = pixmap;
    m_imagePath.clear(); // 清除文件路径，因为这是从数据加载的

    // 如果保持宽高比，调整尺寸
    if (m_keepAspectRatio) {
        QSizeF originalSize = pixmap.size();
        if (originalSize.width() > 0 && originalSize.height() > 0) {
            qreal maxSize = 300.0;
            qreal scale = qMin(maxSize / originalSize.width(), maxSize / originalSize.height());
            if (scale < 1.0) {
                m_size = originalSize * scale;
            } else {
                m_size = originalSize;
            }
        }
    }

    prepareGeometryChange();
    update();
    return true;
}

void ImageItem::setPixmap(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    m_imagePath.clear();

    // 将 QPixmap 转换为 QByteArray
    QBuffer buffer(&m_originalImageData);
    buffer.open(QIODevice::WriteOnly);
    m_pixmap.save(&buffer, "PNG");

    prepareGeometryChange();
    update();
}

QByteArray ImageItem::imageData() const
{
    return m_originalImageData;
}

void ImageItem::setSize(const QSizeF &size)
{
    if (m_size != size) {
        prepareGeometryChange();
        
        if (m_keepAspectRatio) {
            m_size = calculateAspectRatioSize(size);
        } else {
            m_size = size;
        }
        
        update();
    }
}

void ImageItem::setKeepAspectRatio(bool keep)
{
    m_keepAspectRatio = keep;
}

void ImageItem::setOpacity(qreal opacity)
{
    m_opacity = qBound(0.0, opacity, 1.0);
    update();
}

QSizeF ImageItem::calculateAspectRatioSize(const QSizeF &newSize) const
{
    if (m_pixmap.isNull() || newSize.width() <= 0 || newSize.height() <= 0) {
        return newSize;
    }

    QSizeF originalSize = m_pixmap.size();
    if (originalSize.width() <= 0 || originalSize.height() <= 0) {
        return newSize;
    }

    qreal aspectRatio = originalSize.width() / originalSize.height();
    qreal newAspectRatio = newSize.width() / newSize.height();

    if (newAspectRatio > aspectRatio) {
        // 新尺寸更宽，以高度为准
        return QSizeF(newSize.height() * aspectRatio, newSize.height());
    } else {
        // 新尺寸更高，以宽度为准
        return QSizeF(newSize.width(), newSize.width() / aspectRatio);
    }
}

void ImageItem::copyItem(ImageItem *item)
{
    qDebug() << "=== ImageItem::copyItem called ===";

    if (!item) {
        qDebug() << "ImageItem::copyItem - item is null!";
        return;
    }

    qDebug() << "ImageData size:" << item->imageData().size();
    qDebug() << "Image size:" << item->size();
    qDebug() << "Keep aspect ratio:" << item->keepAspectRatio();
    qDebug() << "Opacity:" << item->opacity();

    // 复制当前图像的属性到静态变量
    s_hasCopiedItem = true;
    s_copiedImageData = item->imageData();
    s_copiedSize = item->size();
    s_copiedKeepAspectRatio = item->keepAspectRatio();
    s_copiedOpacity = item->opacity();

    qDebug() << "After copy - s_hasCopiedItem:" << s_hasCopiedItem;
    qDebug() << "After copy - s_copiedImageData size:" << s_copiedImageData.size();

    // 清除其他类型的复制状态
    QRCodeItem::s_hasCopiedItem = false;
    BarcodeItem::s_hasCopiedItem = false;
    TextItem::s_hasCopiedItem = false;
    LineItem::s_hasCopiedItem = false;
    RectangleItem::s_hasCopiedItem = false;
    CircleItem::s_hasCopiedItem = false;

    // 设置最后复制的项目类型
    LabelScene::s_lastCopiedItemType = LabelScene::Image;

    qDebug() << "图像已复制到剪贴板";
}

void ImageItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isLocked()) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton && isSelected()) {
        QPointF pos = event->pos();

        // 检查是否点击了调整手柄
        m_resizeHandle = getHandleAt(pos);
        if (m_resizeHandle != NoHandle) {
            m_isResizing = true;
            m_resizeStartPos = event->scenePos();  // 使用场景坐标
            m_resizeStartSize = m_size;
            m_resizeStartItemPos = this->pos();
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void ImageItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing && m_resizeHandle != NoHandle) {
        QPointF delta = event->scenePos() - m_resizeStartPos;  // 使用场景坐标
        updateSizeFromHandle(m_resizeHandle, delta);
        event->accept();
    } else {
        QGraphicsItem::mouseMoveEvent(event);
        // 在移动时检查对齐
        handleMouseMoveForAlignment();
    }
}

void ImageItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing) {
        m_isResizing = false;
        m_resizeHandle = NoHandle;
        event->accept();
    } else {
        QGraphicsItem::mouseReleaseEvent(event);
        // 在释放鼠标时隐藏对齐线
        handleMouseReleaseForAlignment();
    }
}

void ImageItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);

    // 双击打开文件对话框更换图像
    QString fileName = QFileDialog::getOpenFileName(
        nullptr,
        QObject::tr("选择图像文件"),
        QString(),
        QObject::tr("图像文件 (*.png *.jpg *.jpeg *.gif *.bmp *.svg)")
    );

    if (!fileName.isEmpty()) {
        if (!loadImage(fileName)) {
            QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("无法加载图像文件"));
        }
    }
}

void ImageItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
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

void ImageItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

void ImageItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // 创建右键菜单
    QMenu contextMenu;

    QAction *changeImageAction = contextMenu.addAction(QObject::tr("更换图像..."));
    contextMenu.addSeparator();
    QAction *copyAction = contextMenu.addAction(QObject::tr("复制"));
    QAction *pasteAction = contextMenu.addAction(QObject::tr("粘贴"));
    // 锁定切换
    QAction *lockAction = contextMenu.addAction(isLocked() ? QObject::tr("解除锁定")
                                                           : QObject::tr("锁定"));

    // 根据是否有复制的内容来启用/禁用粘贴选项
    pasteAction->setEnabled(s_hasCopiedItem);

    QAction *deleteAction = contextMenu.addAction(QObject::tr("删除"));
    contextMenu.addSeparator();
    QAction *aspectRatioAction = contextMenu.addAction(QObject::tr("保持宽高比"));
    aspectRatioAction->setCheckable(true);
    aspectRatioAction->setChecked(m_keepAspectRatio);

    // 启用/禁用对齐
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

    if (selectedAction == changeImageAction) {
        QString fileName = QFileDialog::getOpenFileName(
            nullptr,
            QObject::tr("选择图像文件"),
            QString(),
            QObject::tr("图像文件 (*.png *.jpg *.jpeg *.gif *.bmp *.svg)")
        );

        if (!fileName.isEmpty()) {
            if (!loadImage(fileName)) {
                QMessageBox::warning(nullptr, QObject::tr("错误"), QObject::tr("无法加载图像文件"));
            }
        }
    } else if (selectedAction == copyAction) {
        qDebug() << "ImageItem context menu copy action triggered";
        copyItem(this);
    } else if (selectedAction == pasteAction) {
        // 只有当有复制的内容时才能粘贴
        if (s_hasCopiedItem) {
            ImageItem* newImage = new ImageItem(s_copiedImageData);
            newImage->setSize(s_copiedSize);
            newImage->setKeepAspectRatio(s_copiedKeepAspectRatio);
            newImage->setOpacity(s_copiedOpacity);

            // 获取右键点击的位置并设置新位置
            QPointF sceneClickPos = event->scenePos(); // 场景坐标

            // 在点击位置创建新的图像（稍微偏移避免重叠）
            QPointF newPos = sceneClickPos + QPointF(20, 20);
            newImage->setPos(newPos);

            scene()->addItem(newImage);

            // 使用撤销管理器记录粘贴操作
            LabelScene* labelScene = qobject_cast<LabelScene*>(scene());
            if (labelScene && labelScene->undoManager()) {
                labelScene->undoManager()->addItem(newImage, QObject::tr("粘贴图像"));
            }

            scene()->clearSelection();
            newImage->setSelected(true);

            qDebug() << "图像已粘贴";
        }
    } else if (selectedAction == lockAction) {
        setLocked(!isLocked());
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
    } else if (selectedAction == aspectRatioAction) {
        bool newKeepAspectRatio = aspectRatioAction->isChecked();
        qDebug() << "Aspect ratio toggled to:" << newKeepAspectRatio;
        setKeepAspectRatio(newKeepAspectRatio);
    } else if (selectedAction == alignToggle) {
        setAlignmentEnabled(alignToggle->isChecked());
    }
}

ImageItem::HandleType ImageItem::getHandleAt(const QPointF &pos) const
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;

    QRectF itemRect(0, 0, m_size.width(), m_size.height());

    // 检查各个调整手柄
    if (QRectF(itemRect.topLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return TopLeft;
    if (QRectF(itemRect.topRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return TopRight;
    if (QRectF(itemRect.bottomLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return BottomLeft;
    if (QRectF(itemRect.bottomRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return BottomRight;

    // 边中点
    if (QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return TopCenter;
    if (QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return BottomCenter;
    if (QRectF(QPointF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return MiddleLeft;
    if (QRectF(QPointF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize)).contains(pos))
        return MiddleRight;

    return NoHandle;
}

QRectF ImageItem::getHandleRect(HandleType handle) const
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;

    QRectF itemRect(0, 0, m_size.width(), m_size.height());

    switch (handle) {
        case TopLeft:
            return QRectF(itemRect.topLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        case TopCenter:
            return QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle), QSizeF(handleSize, handleSize));
        case TopRight:
            return QRectF(itemRect.topRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        case MiddleLeft:
            return QRectF(QPointF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize));
        case MiddleRight:
            return QRectF(QPointF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle), QSizeF(handleSize, handleSize));
        case BottomLeft:
            return QRectF(itemRect.bottomLeft() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        case BottomCenter:
            return QRectF(QPointF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle), QSizeF(handleSize, handleSize));
        case BottomRight:
            return QRectF(itemRect.bottomRight() - QPointF(halfHandle, halfHandle), QSizeF(handleSize, handleSize));
        default:
            return QRectF();
    }
}

void ImageItem::updateSizeFromHandle(HandleType handle, const QPointF &delta)
{
    QSizeF newSize = m_resizeStartSize;
    QPointF newPos = m_resizeStartItemPos;
    const qreal minSize = 10.0;  // 最小尺寸

    switch (handle) {
        case TopLeft:
            // 左上角：减少宽高，移动位置
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            break;

        case TopCenter:
            // 上中：改变高度
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            break;

        case TopRight:
            // 右上角：增加宽度，减少高度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            break;

        case MiddleLeft:
            // 左中：改变宽度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            break;

        case MiddleRight:
            // 右中：改变宽度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            break;

        case BottomLeft:
            // 左下角：减少宽度，增加高度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
            break;

        case BottomCenter:
            // 下中：改变高度
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
            break;

        case BottomRight:
            // 右下角：增加宽高
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
            break;

        default:
            return;
    }

    // 如果保持宽高比，对所有手柄都应用宽高比约束
    if (m_keepAspectRatio) {
        QSizeF originalSize = newSize;
        newSize = calculateAspectRatioSize(newSize);
        qDebug() << "Aspect ratio applied - Handle:" << handle << "Original:" << originalSize << "Adjusted:" << newSize;
    }

    // 根据手柄类型调整位置
    switch (handle) {
        case TopLeft:
            // 只有在实际改变尺寸时才移动位置
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;

        case TopCenter:
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            // 如果保持宽高比，宽度也可能改变，需要调整X位置
            if (m_keepAspectRatio && newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()) / 2);
            }
            break;

        case TopRight:
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;

        case MiddleLeft:
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            // 如果保持宽高比，高度也可能改变，需要调整Y位置
            if (m_keepAspectRatio && newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()) / 2);
            }
            break;

        case MiddleRight:
            // 如果保持宽高比，高度也可能改变，需要调整Y位置
            if (m_keepAspectRatio && newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()) / 2);
            }
            break;

        case BottomLeft:
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            break;

        case BottomCenter:
            // 如果保持宽高比，宽度也可能改变，需要调整X位置
            if (m_keepAspectRatio && newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()) / 2);
            }
            break;

        case BottomRight:
            // 右下角不需要调整位置
            break;

        default:
            break;
    }

    prepareGeometryChange();
    m_size = newSize;
    setPos(newPos);
    update();
}

void ImageItem::setCursorForHandle(HandleType handle)
{
    switch (handle) {
        case TopLeft:
        case BottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case TopRight:
        case BottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case TopCenter:
        case BottomCenter:
            setCursor(Qt::SizeVerCursor);
            break;
        case MiddleLeft:
        case MiddleRight:
            setCursor(Qt::SizeHorCursor);
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}

void ImageItem::drawResizeHandles(QPainter *painter)
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;

    painter->setPen(QPen(Qt::blue, 1));
    painter->setBrush(Qt::white);

    QRectF itemRect(0, 0, m_size.width(), m_size.height());

    // 绘制四个角的调整手柄
    painter->drawRect(QRectF(itemRect.topLeft() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));
    painter->drawRect(QRectF(itemRect.topRight() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));
    painter->drawRect(QRectF(itemRect.bottomLeft() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));
    painter->drawRect(QRectF(itemRect.bottomRight() - QPointF(halfHandle, halfHandle),
                            QSizeF(handleSize, handleSize)));

    // 绘制四个边中点的调整手柄
    painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle,
                            handleSize, handleSize));  // 上中
    painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle,
                            handleSize, handleSize));  // 下中
    painter->drawRect(QRectF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle,
                            handleSize, handleSize));  // 左中
    painter->drawRect(QRectF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle,
                            handleSize, handleSize));  // 右中
}
