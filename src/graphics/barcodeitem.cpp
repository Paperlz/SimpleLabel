#include "barcodeitem.h"
#include <QGraphicsScene>
#include "labelscene.h"
#include "../commands/undomanager.h"
#include "qrcodeitem.h"
#include "textitem.h"
#include "imageitem.h"
#include "lineitem.h"
#include "rectangleitem.h"
#include "circleitem.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QDebug>
#include <stdexcept> // For std::runtime_error

// 静态成员变量定义
bool BarcodeItem::s_hasCopiedItem = false;
QString BarcodeItem::s_copiedData = "";
ZXing::BarcodeFormat BarcodeItem::s_copiedFormat = ZXing::BarcodeFormat::Code128;
QSizeF BarcodeItem::s_copiedSize = QSizeF(300, 100);
QColor BarcodeItem::s_copiedForegroundColor = Qt::black;
QColor BarcodeItem::s_copiedBackgroundColor = Qt::white;
bool BarcodeItem::s_copiedShowHumanReadableText = true;
QFont BarcodeItem::s_copiedHumanReadableTextFont = QFont("Arial", 10);

BarcodeItem::BarcodeItem(const QString &data, ZXing::BarcodeFormat format, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_data(data)
    , m_format(format)
    , m_size(300, 100)  // Initial default size for a barcode
    , m_foregroundColor(Qt::black)
    , m_backgroundColor(Qt::white)
    , m_isResizing(false)
    , m_resizeHandle(NoHandle)
    , m_showHumanReadableText(true) // Default to show text
    , m_humanReadableTextFont("Arial", 10) // Default font
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);  // 启用悬停事件
}

QRectF BarcodeItem::boundingRect() const
{
    // Compute minimal padding to include handles and selection pen.
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    const qreal selectionPenHalf = 0.5;
    const qreal padding = qMax<qreal>(1.0, qMax(halfHandle, selectionPenHalf));

    // Human readable text is already included in m_size, so no extra height required
    return QRectF(-padding, -padding,
                  m_size.width() + 2 * padding,
                  m_size.height() + 2 * padding);
}

// Helper function to determine if a format is 1D (linear)
// This replaces the potentially missing ZXing::BarcodeFormatIsLinear
bool isLinearFormat(ZXing::BarcodeFormat format) {
    switch (format) {
        case ZXing::BarcodeFormat::Codabar:
        case ZXing::BarcodeFormat::Code39:
        case ZXing::BarcodeFormat::Code93:
        case ZXing::BarcodeFormat::Code128:
        case ZXing::BarcodeFormat::EAN8:
        case ZXing::BarcodeFormat::EAN13:
        case ZXing::BarcodeFormat::ITF:
        case ZXing::BarcodeFormat::DataBar :
        case ZXing::BarcodeFormat::DataBarExpanded:
        case ZXing::BarcodeFormat::UPCA:
        case ZXing::BarcodeFormat::UPCE:
            return true;
        default:
            return false;
    }
}

void BarcodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    // 使用原始字体，但对打印设备进行大幅缩小
    QFont adjustedFont = m_humanReadableTextFont;
    
    // 检查设备类型
    QPaintDevice* device = painter->device();
    if (device) {
        // 获取设备的逻辑DPI
        int deviceDpiX = device->logicalDpiX();
        int deviceDpiY = device->logicalDpiY();
        
        // 标准屏幕DPI通常是96
        const int standardDpi = 96;
        
        // 如果是高DPI设备（通常是打印机），大幅缩小字体
        if (deviceDpiX > standardDpi * 1.5 || deviceDpiY > standardDpi * 1.5) {
            // 使用更小的字体并设置为像素大小，更精确控制
            adjustedFont.setPixelSize(10); // 从8像素调整到10像素，稍微大一点
        }
    }

    // 计算文本高度，如果显示人类可读文本
    qreal textHeight = 0;
    qreal textSpacing = 2; // 进一步减小间距
    QFontMetrics fm(adjustedFont);
    if (m_showHumanReadableText && !m_data.isEmpty()) {
        // 为文本预留更多空间，确保不被裁剪
        textHeight = fm.height() + textSpacing + 3; // 额外增加3像素缓冲
    }

    // 计算条形码主体的实际绘制区域（为文本预留空间）
    qreal barcodeHeight = m_size.height() - textHeight;
    QRectF barcodeRect(0, 0, m_size.width(), barcodeHeight);

    // 绘制背景
    painter->fillRect(QRectF(0, 0, m_size.width(), m_size.height()), m_backgroundColor);

    if (!m_data.isEmpty()) {
        try {
            ZXing::MultiFormatWriter writer(m_format);
            std::string utf8Data = ZXing::TextUtfEncoding::ToUtf8(m_data.toStdWString());

            ZXing::BitMatrix bitMatrix;

            // 使用条形码主体区域的尺寸来生成位矩阵
            bitMatrix = writer.encode(utf8Data,
                                       static_cast<int>(barcodeRect.width()),
                                       static_cast<int>(barcodeRect.height())
                                       );
            // --- End Alternative ---



            const int matrixWidth = bitMatrix.width();
            const int matrixHeight = bitMatrix.height();

            if (matrixWidth > 0 && matrixHeight > 0) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(m_foregroundColor);

                // Use the local helper function isLinearFormat or check matrixHeight
                if (isLinearFormat(m_format) || matrixHeight == 1) { // 1D Barcode
                    // 使用条形码主体区域来计算缩放
                    qreal scaleX = barcodeRect.width() / matrixWidth;
                    qreal scaleY = barcodeRect.height() / matrixHeight;
                    
                    // 对于1D条形码，确保最小模块尺寸至少是1像素
                    qreal minScale = qMin(scaleX, scaleY);
                    if (minScale < 1.0) {
                        minScale = 1.0;
                    }
                    
                    // 对于1D条形码，我们可以使用不同的X和Y比例
                    qreal actualScaleX = scaleX;
                    qreal actualScaleY = scaleY;
                    
                    // 如果某一个方向的缩放过小，使用最小缩放
                    if (actualScaleX < 1.0) actualScaleX = minScale;
                    if (actualScaleY < 1.0) actualScaleY = minScale;
                    
                    // 计算实际绘制尺寸
                    qreal drawWidth = matrixWidth * actualScaleX;
                    qreal drawHeight = matrixHeight * actualScaleY;
                    
                    // 确保不超出条形码主体区域边界
                    if (drawWidth > barcodeRect.width()) {
                        actualScaleX = barcodeRect.width() / matrixWidth;
                        drawWidth = barcodeRect.width();
                    }
                    if (drawHeight > barcodeRect.height()) {
                        actualScaleY = barcodeRect.height() / matrixHeight;
                        drawHeight = barcodeRect.height();
                    }
                    
                    // 计算在条形码主体区域内的居中偏移
                    qreal offsetX = (barcodeRect.width() - drawWidth) / 2;
                    qreal offsetY = (barcodeRect.height() - drawHeight) / 2;

                    for (int x = 0; x < matrixWidth; ++x) {
                        if (bitMatrix.get(x, 0)) { // For 1D, typically check the first row
                            painter->drawRect(QRectF(
                                offsetX + x * actualScaleX, 
                                offsetY, 
                                actualScaleX, 
                                drawHeight
                            ));
                        }
                    }
                    m_displayText = m_data;
                } else { // Potentially a 2D barcode (e.g. QR_CODE, DATA_MATRIX, AZTEC, PDF_417 if passed to this item)
                    // 使用条形码主体区域来绘制2D条形码
                    qreal scaleX = barcodeRect.width() / matrixWidth;
                    qreal scaleY = barcodeRect.height() / matrixHeight;
                    
                    qreal minScale = qMin(scaleX, scaleY);
                    if (minScale < 1.0) {
                        minScale = 1.0;
                    }
                    
                    qreal actualScaleX = scaleX;
                    qreal actualScaleY = scaleY;
                    
                    if (actualScaleX < 1.0) actualScaleX = minScale;
                    if (actualScaleY < 1.0) actualScaleY = minScale;
                    
                    qreal drawWidth = matrixWidth * actualScaleX;
                    qreal drawHeight = matrixHeight * actualScaleY;
                    
                    // 确保不超出条形码主体区域边界
                    if (drawWidth > barcodeRect.width()) {
                        actualScaleX = barcodeRect.width() / matrixWidth;
                        drawWidth = barcodeRect.width();
                    }
                    if (drawHeight > barcodeRect.height()) {
                        actualScaleY = barcodeRect.height() / matrixHeight;
                        drawHeight = barcodeRect.height();
                    }
                    
                    // 计算在条形码主体区域内的居中偏移
                    qreal offsetX = (barcodeRect.width() - drawWidth) / 2;
                    qreal offsetY = (barcodeRect.height() - drawHeight) / 2;

                    for (int y = 0; y < matrixHeight; ++y) {
                        for (int x = 0; x < matrixWidth; ++x) {
                            if (bitMatrix.get(x, y)) {
                                painter->drawRect(QRectF(
                                    offsetX + x * actualScaleX, 
                                    offsetY + y * actualScaleY, 
                                    actualScaleX, 
                                    actualScaleY
                                ));
                            }
                        }
                    }
                    m_displayText = m_data;
                }
            } else {
                 qDebug() << "BarcodeItem: Generated BitMatrix is empty for data:" << m_data << "format:" << static_cast<int>(m_format);
                 m_displayText.clear();
            }

        } catch (const std::exception &e) {
            qDebug() << "BarcodeItem: Error generating barcode for data [" << m_data << "]: " << e.what();
            painter->setPen(Qt::red);
            painter->drawText(QRectF(0, 0, m_size.width(), m_size.height()), Qt::AlignCenter | Qt::TextWordWrap, QString("Error: %1").arg(e.what()));
            m_displayText.clear();
        }
    } else {
        m_displayText.clear();
    }

    // 绘制人类可读文本（在条形码主体下方，但在总体尺寸内）
    if (m_showHumanReadableText && !m_displayText.isEmpty()) {
        painter->setPen(m_foregroundColor);
        painter->setFont(adjustedFont); // 使用调整后的字体
        
        QFontMetrics adjustedFm(adjustedFont);
        qreal adjustedTextHeight = adjustedFm.height();
        
        // 计算文本区域：在条形码主体下方，使用预留的空间
        qreal spacing = 1; // 最小间距
        QRectF textRect(0, barcodeHeight + spacing, m_size.width(), adjustedTextHeight + 2); // 额外增加2像素高度
        
        // 确保文本区域不超出总边界
        if (textRect.bottom() > m_size.height()) {
            textRect.setHeight(m_size.height() - textRect.top());
        }
        
        painter->drawText(textRect, Qt::AlignCenter | Qt::AlignTop, m_displayText);
    }

    if (option->state & QStyle::State_Selected) {
        // 绘制选择框
        painter->setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(QRectF(0, 0, m_size.width(), m_size.height()));

        // 绘制调整手柄
        const qreal handleSize = 8.0;
        const qreal halfHandle = handleSize / 2.0;
        
        painter->setBrush(Qt::white);
        painter->setPen(QPen(Qt::blue, 1, Qt::SolidLine));
        
        QRectF itemRect(0, 0, m_size.width(), m_size.height());
        
        // 四个角（所有类型都有）
        painter->drawRect(QRectF(itemRect.left() - halfHandle, itemRect.top() - halfHandle, 
                                handleSize, handleSize));  // 左上
        painter->drawRect(QRectF(itemRect.right() - halfHandle, itemRect.top() - halfHandle, 
                                handleSize, handleSize));  // 右上
        painter->drawRect(QRectF(itemRect.left() - halfHandle, itemRect.bottom() - halfHandle, 
                                handleSize, handleSize));  // 左下
        painter->drawRect(QRectF(itemRect.right() - halfHandle, itemRect.bottom() - halfHandle, 
                                handleSize, handleSize));  // 右下
                                
        // 四个边中点（对于条形码，提供完整的调整功能）
        painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle, 
                                handleSize, handleSize));  // 上中
        painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle, 
                                handleSize, handleSize));  // 下中
        painter->drawRect(QRectF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle, 
                                handleSize, handleSize));  // 左中
        painter->drawRect(QRectF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle, 
                                handleSize, handleSize));  // 右中
    }
    
    // 绘制对齐线
    paintAlignmentLines(painter);
}


void BarcodeItem::setData(const QString &data)
{
    if (m_data != data) {
        m_data = data;
        prepareGeometryChange();
        update();
    }
}

void BarcodeItem::setFormat(ZXing::BarcodeFormat format)
{
    if (m_format != format) {
        m_format = format;
        prepareGeometryChange(); // Format change might affect BitMatrix dimensions
        update();
    }
}

void BarcodeItem::setSize(const QSizeF &size)
{
    QSizeF newSize = size;
    if (newSize.width() < 30) newSize.setWidth(30);
    if (newSize.height() < 20) newSize.setHeight(20);

    if (m_size != newSize) {
        prepareGeometryChange();
        m_size = newSize;
        update();
    }
}

void BarcodeItem::setForegroundColor(const QColor &color)
{
    if (m_foregroundColor != color) {
        m_foregroundColor = color;
        update();
    }
}

void BarcodeItem::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        update();
    }
}

void BarcodeItem::setShowHumanReadableText(bool show) {
    if (m_showHumanReadableText != show) {
        m_showHumanReadableText = show;
        prepareGeometryChange();
        update();
    }
}

void BarcodeItem::setHumanReadableTextFont(const QFont &font) {
    if (m_humanReadableTextFont != font) {
        m_humanReadableTextFont = font;
        prepareGeometryChange();
        update();
    }
}


void BarcodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
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
            m_resizeStartPos = event->scenePos();
            m_resizeStartSize = m_size;
            m_resizeStartItemPos = this->pos();
            event->accept();
            return;
        }
    }
    QGraphicsItem::mousePressEvent(event);
}

void BarcodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing && m_resizeHandle != NoHandle) {
        QPointF delta = event->scenePos() - m_resizeStartPos;
        updateSizeFromHandle(m_resizeHandle, delta);
        event->accept();
    } else {
        QGraphicsItem::mouseMoveEvent(event);
        // 在移动时检查对齐
        handleMouseMoveForAlignment();
    }
}

void BarcodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
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

void BarcodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    // 创建右键菜单
    QMenu contextMenu;
    
    QAction *copyAction = contextMenu.addAction("复制");
    QAction *pasteAction = contextMenu.addAction("粘贴");
    
    // 根据是否有复制的内容来启用/禁用粘贴选项
    pasteAction->setEnabled(s_hasCopiedItem);
    
    QAction *deleteAction = contextMenu.addAction("删除");
    contextMenu.addSeparator();
    QAction *rotateAction = contextMenu.addAction("旋转90°");
    contextMenu.addSeparator();
    QAction *bringToFrontAction = contextMenu.addAction("置于顶层");
    QAction *bringForwardAction = contextMenu.addAction("上移一层");
    QAction *sendBackwardAction = contextMenu.addAction("下移一层");
    QAction *sendToBackAction = contextMenu.addAction("置于底层");

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
        // 复制当前条形码的属性到静态变量
        s_hasCopiedItem = true;
        s_copiedData = m_data;
        s_copiedFormat = m_format;
        s_copiedSize = m_size;
        s_copiedForegroundColor = m_foregroundColor;
        s_copiedBackgroundColor = m_backgroundColor;
        s_copiedShowHumanReadableText = m_showHumanReadableText;
        s_copiedHumanReadableTextFont = m_humanReadableTextFont;
        
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        
        // 设置最后复制的项目类型
        LabelScene::s_lastCopiedItemType = LabelScene::Barcode;
        
        qDebug() << "条形码已复制到剪贴板";
    }
    else if(selectedAction == pasteAction){
        // 只有当有复制的内容时才能粘贴
        if(s_hasCopiedItem){
            BarcodeItem* newBarcode = new BarcodeItem(s_copiedData, s_copiedFormat);
            newBarcode->setSize(s_copiedSize);
            newBarcode->setForegroundColor(s_copiedForegroundColor);
            newBarcode->setBackgroundColor(s_copiedBackgroundColor);
            newBarcode->setShowHumanReadableText(s_copiedShowHumanReadableText);
            newBarcode->setHumanReadableTextFont(s_copiedHumanReadableTextFont);
            
            // 获取右键点击的位置并设置新位置
            QPointF sceneClickPos = event->scenePos(); // 场景坐标
            
            // 在点击位置创建新的条形码（稍微偏移避免重叠）
            QPointF newPos = sceneClickPos + QPointF(20, 20);
            newBarcode->setPos(newPos);
            
            scene()->addItem(newBarcode);
            
            // 使用撤销管理器记录粘贴操作
            LabelScene* labelScene = qobject_cast<LabelScene*>(scene());
            if (labelScene && labelScene->undoManager()) {
                labelScene->undoManager()->addItem(newBarcode, QObject::tr("粘贴条形码"));
            }
            
            scene()->clearSelection();
            newBarcode->setSelected(true);
            
            qDebug() << "在位置" << newPos << "粘贴了条形码";
        } else {
            qDebug() << "没有可粘贴的内容";
        }
    }
    else if (selectedAction == deleteAction) {
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
    } else if (selectedAction == rotateAction) {
        // 设置变换原点为项目中心，使旋转时位置保持不变
        QRectF boundingRect = this->boundingRect();
        QPointF center = boundingRect.center();
        setTransformOriginPoint(center);
        
        setRotation(rotation() + 90);
    } else if (selectedAction == lockAction) {
        setLocked(!isLocked());
    } else if (selectedAction == bringToFrontAction) {
        // 找到最高的Z值并设置为更高
        qreal maxZ = 0;
        for (QGraphicsItem* item : scene()->items()) {
            if (item != this) {
                maxZ = qMax(maxZ, item->zValue());
            }
        }
        setZValue(maxZ + 1);
    } else if (selectedAction == sendToBackAction) {
        // 找到最低的Z值并设置为更低
        qreal minZ = 0;
        for (QGraphicsItem* item : scene()->items()) {
            if (item != this) {
                minZ = qMin(minZ, item->zValue());
            }
        }
        setZValue(minZ - 1);
    } else if (selectedAction == bringForwardAction) {
        setZValue(zValue() + 1);
    } else if (selectedAction == sendBackwardAction) {
        setZValue(zValue() - 1);
    } else if (selectedAction == alignToggle) {
        setAlignmentEnabled(alignToggle->isChecked());
    }
    
    event->accept();
}

void BarcodeItem::copyItem(BarcodeItem *item)
{
    // 复制当前条形码的属性到静态变量
    s_hasCopiedItem = true;
    s_copiedData = m_data;
    s_copiedFormat = m_format;
    s_copiedSize = m_size;
    s_copiedForegroundColor = m_foregroundColor;
    s_copiedBackgroundColor = m_backgroundColor;
    s_copiedShowHumanReadableText = m_showHumanReadableText;
    s_copiedHumanReadableTextFont = m_humanReadableTextFont;
    
    // 清除其他类型的复制状态
    QRCodeItem::s_hasCopiedItem = false;
    TextItem::s_hasCopiedItem = false;
    ImageItem::s_hasCopiedItem = false;
    LineItem::s_hasCopiedItem = false;
    RectangleItem::s_hasCopiedItem = false;
    CircleItem::s_hasCopiedItem = false;
    
    // 设置最后复制的项目类型
    LabelScene::s_lastCopiedItemType = LabelScene::Barcode;
    
    qDebug() << "条形码已复制到剪贴板";
}

// 悬停事件处理
void BarcodeItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (isLocked()) {
        setCursor(Qt::ArrowCursor);
        QGraphicsItem::hoverMoveEvent(event);
        return;
    }
    if (isSelected()) {
        HandleType handle = getHandleAt(event->pos());
        setCursorForHandle(handle);
    }
    QGraphicsItem::hoverMoveEvent(event);
}

void BarcodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

// 获取指定位置的调整手柄类型
BarcodeItem::HandleType BarcodeItem::getHandleAt(const QPointF &pos) const
{
    const qreal tolerance = 5.0;  // 增加容差，便于点击
    
    // 检查所有8个手柄
    for (int i = TopLeft; i <= BottomRight; ++i) {
        HandleType handle = static_cast<HandleType>(i);
        QRectF handleRect = getHandleRect(handle);
        handleRect.adjust(-tolerance, -tolerance, tolerance, tolerance);
        if (handleRect.contains(pos)) {
            return handle;
        }
    }
    
    return NoHandle;
}

// 获取指定手柄的矩形区域
QRectF BarcodeItem::getHandleRect(HandleType handle) const
{
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    QRectF itemRect(0, 0, m_size.width(), m_size.height());
    
    switch (handle) {
        case TopLeft:
            return QRectF(itemRect.left() - halfHandle, itemRect.top() - halfHandle, 
                         handleSize, handleSize);
        case TopCenter:
            return QRectF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle, 
                         handleSize, handleSize);
        case TopRight:
            return QRectF(itemRect.right() - halfHandle, itemRect.top() - halfHandle, 
                         handleSize, handleSize);
        case MiddleLeft:
            return QRectF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle, 
                         handleSize, handleSize);
        case MiddleRight:
            return QRectF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle, 
                         handleSize, handleSize);
        case BottomLeft:
            return QRectF(itemRect.left() - halfHandle, itemRect.bottom() - halfHandle, 
                         handleSize, handleSize);
        case BottomCenter:
            return QRectF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle, 
                         handleSize, handleSize);
        case BottomRight:
            return QRectF(itemRect.right() - halfHandle, itemRect.bottom() - halfHandle, 
                         handleSize, handleSize);
        default:
            return QRectF();
    }
}

// 根据手柄类型和位移更新尺寸
void BarcodeItem::updateSizeFromHandle(HandleType handle, const QPointF &delta)
{
    QSizeF newSize = m_resizeStartSize;
    QPointF newPos = m_resizeStartItemPos;
    const qreal minSize = 30.0;  // 最小尺寸
    
    // 条形码允许不同的宽高比，提供完整的调整功能
    switch (handle) {
        case TopLeft:
            // 左上角：减少宽高，移动位置
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            // 只有在实际改变尺寸时才移动位置
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;
            
        case TopCenter:
            // 上中：只改变高度
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;
            
        case TopRight:
            // 右上角：增加宽度，减少高度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() - delta.y()));
            if (newSize.height() != m_resizeStartSize.height()) {
                newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
            }
            break;
            
        case MiddleLeft:
            // 左中：只改变宽度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            break;
            
        case MiddleRight:
            // 右中：只改变宽度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + delta.x()));
            break;
            
        case BottomLeft:
            // 左下角：减少宽度，增加高度
            newSize.setWidth(qMax(minSize, m_resizeStartSize.width() - delta.x()));
            newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + delta.y()));
            if (newSize.width() != m_resizeStartSize.width()) {
                newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
            }
            break;
            
        case BottomCenter:
            // 下中：只改变高度
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
    
    // 更新位置和尺寸
    setPos(newPos);
    setSize(newSize);
}

// 为不同的手柄设置相应的光标
void BarcodeItem::setCursorForHandle(HandleType handle)
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
