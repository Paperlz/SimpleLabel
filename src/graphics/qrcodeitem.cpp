#include "qrcodeitem.h"
#include "labelscene.h"
#include "../commands/undomanager.h"
#include "barcodeitem.h"
#include "textitem.h"
#include "imageitem.h"
#include "lineitem.h"
#include "rectangleitem.h"
#include "circleitem.h"
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QDebug>

// 静态成员变量定义
bool QRCodeItem::s_hasCopiedItem = false;
QString QRCodeItem::s_copiedText = "";
QString QRCodeItem::s_copiedCodeType = "QR";
QSizeF QRCodeItem::s_copiedSize = QSizeF(200, 200);
QColor QRCodeItem::s_copiedForegroundColor = Qt::black;
QColor QRCodeItem::s_copiedBackgroundColor = Qt::white;

QRCodeItem::QRCodeItem(const QString &text, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_text(text)
    , m_codeType("QR")  // 默认为QR码
    , m_size(200, 200)  // 初始大小
    , m_foregroundColor(Qt::black)
    , m_backgroundColor(Qt::white)
    , m_isResizing(false)
    , m_resizeHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptHoverEvents(true);  // 启用悬停事件
}

QRectF QRCodeItem::boundingRect() const
{
    // Compute minimal padding to include handles and selection pen.
    const qreal handleSize = 8.0;
    const qreal halfHandle = handleSize / 2.0;
    const qreal selectionPenHalf = 0.5; // selection pen uses width 1
    const qreal padding = qMax<qreal>(1.0, qMax(halfHandle, selectionPenHalf));
    return QRectF(-padding, -padding,
                  m_size.width() + 2 * padding,
                  m_size.height() + 2 * padding);
}

void QRCodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    // 绘制背景
    painter->fillRect(QRectF(0, 0, m_size.width(), m_size.height()), m_backgroundColor);

    if (!m_text.isEmpty()) {
        if (m_codeType == "QR") {
            // 使用原有的QR码生成方式
            const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(m_text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
            const int qrSize = qr.getSize();

            // 计算缩放比例，确保QR码完全适配到边框内
            qreal scaleX = m_size.width() / qrSize;
            qreal scaleY = m_size.height() / qrSize;
            qreal scale = qMin(scaleX, scaleY);
            
            // 计算实际绘制尺寸
            qreal drawSize = qrSize * scale;
            
            // 计算居中偏移
            qreal offsetX = (m_size.width() - drawSize) / 2;
            qreal offsetY = (m_size.height() - drawSize) / 2;

            // 绘制二维码
            painter->setPen(Qt::NoPen);
            painter->setBrush(m_foregroundColor);

            for (int y = 0; y < qrSize; y++) {
                for (int x = 0; x < qrSize; x++) {
                    if (qr.getModule(x, y)) {
                        painter->drawRect(QRectF(offsetX + x * scale, offsetY + y * scale, scale, scale));
                    }
                }
            }
        } else {
            // 使用ZXing库生成其他类型的二维码
            try {
                ZXing::BarcodeFormat format;
                
                if (m_codeType == "PDF417") {
                    format = ZXing::BarcodeFormat::PDF417;
                    ZXing::MultiFormatWriter writer(format);
                    
                    // PDF417特殊处理：设置适当的参数
                    writer.setMargin(1);  // 最小边距
                    writer.setEccLevel(2); // 中等错误纠正级别
                    
                    // PDF417让库自动决定最佳列数和行数
                    // 我们只提供一个参考尺寸，让ZXing优化
                    int referenceSize = qMax(50, qMin(300, static_cast<int>(qMax(m_size.width(), m_size.height()))));
                    
                    std::string utf8Data = m_text.toUtf8().toStdString();
                    ZXing::BitMatrix bitMatrix = writer.encode(utf8Data, referenceSize, referenceSize / 3);
                    
                    const int matrixWidth = bitMatrix.width();
                    const int matrixHeight = bitMatrix.height();
                    
                    if (matrixWidth > 0 && matrixHeight > 0) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(m_foregroundColor);
                        
                        // PDF417特殊缩放逻辑：优先保持条码的宽高比
                        qreal scaleX = m_size.width() / matrixWidth;
                        qreal scaleY = m_size.height() / matrixHeight;
                        
                        // 对于PDF417，我们允许不同的X和Y缩放，但要确保条码清晰可读
                        qreal minScale = qMin(scaleX, scaleY);
                        
                        // 确保最小模块尺寸至少是1像素
                        if (minScale < 1.0) {
                            minScale = 1.0;
                        }
                        
                        // 对于PDF417，我们可以使用稍微不同的X和Y比例
                        // 但要确保不会造成条码失真
                        qreal actualScaleX = scaleX;
                        qreal actualScaleY = scaleY;
                        
                        // 如果某一个方向的缩放过小，使用最小缩放
                        if (actualScaleX < 1.0) actualScaleX = minScale;
                        if (actualScaleY < 1.0) actualScaleY = minScale;
                        
                        // 计算实际绘制尺寸
                        qreal drawWidth = matrixWidth * actualScaleX;
                        qreal drawHeight = matrixHeight * actualScaleY;
                        
                        // 确保不超出边界
                        if (drawWidth > m_size.width()) {
                            actualScaleX = m_size.width() / matrixWidth;
                            drawWidth = m_size.width();
                        }
                        if (drawHeight > m_size.height()) {
                            actualScaleY = m_size.height() / matrixHeight;
                            drawHeight = m_size.height();
                        }
                        
                        // 计算居中偏移
                        qreal offsetX = (m_size.width() - drawWidth) / 2;
                        qreal offsetY = (m_size.height() - drawHeight) / 2;
                        
                        // 绘制PDF417
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
                    } else {
                        painter->setPen(Qt::red);
                        painter->drawText(QRectF(0, 0, m_size.width(), m_size.height()), 
                                        Qt::AlignCenter, "PDF417: 数据为空");
                    }
                    
                } else if (m_codeType == "DataMatrix") {
                    format = ZXing::BarcodeFormat::DataMatrix;
                    ZXing::MultiFormatWriter writer(format);
                    writer.setMargin(1);
                    
                    // DataMatrix使用正方形逻辑
                    int targetSize = qMin(static_cast<int>(m_size.width()), static_cast<int>(m_size.height()));
                    std::string utf8Data = m_text.toUtf8().toStdString();
                    ZXing::BitMatrix bitMatrix = writer.encode(utf8Data, targetSize, targetSize);
                    
                    const int matrixWidth = bitMatrix.width();
                    const int matrixHeight = bitMatrix.height();
                    
                    if (matrixWidth > 0 && matrixHeight > 0) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(m_foregroundColor);
                        
                        qreal scale = qMin(m_size.width() / matrixWidth, m_size.height() / matrixHeight);
                        if (scale < 1.0) scale = 1.0;
                        
                        qreal drawWidth = matrixWidth * scale;
                        qreal drawHeight = matrixHeight * scale;
                        qreal offsetX = (m_size.width() - drawWidth) / 2;
                        qreal offsetY = (m_size.height() - drawHeight) / 2;
                        
                        for (int y = 0; y < matrixHeight; ++y) {
                            for (int x = 0; x < matrixWidth; ++x) {
                                if (bitMatrix.get(x, y)) {
                                    painter->drawRect(QRectF(offsetX + x * scale, offsetY + y * scale, scale, scale));
                                }
                            }
                        }
                    }
                    
                } else if (m_codeType == "Aztec") {
                    format = ZXing::BarcodeFormat::Aztec;
                    ZXing::MultiFormatWriter writer(format);
                    writer.setMargin(1);
                    
                    // Aztec使用正方形逻辑
                    int targetSize = qMin(static_cast<int>(m_size.width()), static_cast<int>(m_size.height()));
                    std::string utf8Data = m_text.toUtf8().toStdString();
                    ZXing::BitMatrix bitMatrix = writer.encode(utf8Data, targetSize, targetSize);
                    
                    const int matrixWidth = bitMatrix.width();
                    const int matrixHeight = bitMatrix.height();
                    
                    if (matrixWidth > 0 && matrixHeight > 0) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(m_foregroundColor);
                        
                        qreal scale = qMin(m_size.width() / matrixWidth, m_size.height() / matrixHeight);
                        if (scale < 1.0) scale = 1.0;
                        
                        qreal drawWidth = matrixWidth * scale;
                        qreal drawHeight = matrixHeight * scale;
                        qreal offsetX = (m_size.width() - drawWidth) / 2;
                        qreal offsetY = (m_size.height() - drawHeight) / 2;
                        
                        for (int y = 0; y < matrixHeight; ++y) {
                            for (int x = 0; x < matrixWidth; ++x) {
                                if (bitMatrix.get(x, y)) {
                                    painter->drawRect(QRectF(offsetX + x * scale, offsetY + y * scale, scale, scale));
                                }
                            }
                        }
                    }
                    
                } else {
                    // 默认使用QR码
                    format = ZXing::BarcodeFormat::QRCode;
                    ZXing::MultiFormatWriter writer(format);
                    writer.setMargin(1);
                    
                    int targetSize = qMin(static_cast<int>(m_size.width()), static_cast<int>(m_size.height()));
                    std::string utf8Data = m_text.toUtf8().toStdString();
                    ZXing::BitMatrix bitMatrix = writer.encode(utf8Data, targetSize, targetSize);
                    
                    const int matrixWidth = bitMatrix.width();
                    const int matrixHeight = bitMatrix.height();
                    
                    if (matrixWidth > 0 && matrixHeight > 0) {
                        painter->setPen(Qt::NoPen);
                        painter->setBrush(m_foregroundColor);
                        
                        qreal scale = qMin(m_size.width() / matrixWidth, m_size.height() / matrixHeight);
                        if (scale < 1.0) scale = 1.0;
                        
                        qreal drawWidth = matrixWidth * scale;
                        qreal drawHeight = matrixHeight * scale;
                        qreal offsetX = (m_size.width() - drawWidth) / 2;
                        qreal offsetY = (m_size.height() - drawHeight) / 2;
                        
                        for (int y = 0; y < matrixHeight; ++y) {
                            for (int x = 0; x < matrixWidth; ++x) {
                                if (bitMatrix.get(x, y)) {
                                    painter->drawRect(QRectF(offsetX + x * scale, offsetY + y * scale, scale, scale));
                                }
                            }
                        }
                    }
                }
            } catch (const std::exception &e) {
                qDebug() << "QRCodeItem: Error generating code for data [" << m_text << "]: " << e.what();
                painter->setPen(Qt::red);
                painter->drawText(QRectF(0, 0, m_size.width(), m_size.height()), Qt::AlignCenter | Qt::TextWordWrap, QString("Error: %1").arg(e.what()));
            }
        }
    }

    // 如果被选中，绘制选择框和调整手柄
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
                                
        // 四个边中点（只有PDF417有）
        if (m_codeType == "PDF417") {
            painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.top() - halfHandle, 
                                    handleSize, handleSize));  // 上中
            painter->drawRect(QRectF(itemRect.center().x() - halfHandle, itemRect.bottom() - halfHandle, 
                                    handleSize, handleSize));  // 下中
            painter->drawRect(QRectF(itemRect.left() - halfHandle, itemRect.center().y() - halfHandle, 
                                    handleSize, handleSize));  // 左中
            painter->drawRect(QRectF(itemRect.right() - halfHandle, itemRect.center().y() - halfHandle, 
                                    handleSize, handleSize));  // 右中
        }
    }
    
    // 绘制对齐线
    paintAlignmentLines(painter);
}

void QRCodeItem::setText(const QString &text)
{
    if (m_text != text) {
        m_text = text;
        update();
    }
}

void QRCodeItem::setCodeType(const QString &codeType)
{
    if (m_codeType != codeType) {
        QString oldCodeType = m_codeType;
        m_codeType = codeType;
        
        // 为不同的二维码类型调整默认尺寸
        if (codeType == "PDF417") {
            // PDF417 更适合长方形形状 (3:1 比例)
            if (qAbs(m_size.width() - m_size.height()) < 10) { // 如果是接近正方形
                qreal height = m_size.height();
                setSize(QSizeF(height * 3, height)); // 设置为 3:1 比例
            }
        } else if (codeType == "DataMatrix" || codeType == "Aztec" || codeType == "QR") {
            // 这些类型更适合正方形
            // 从任何类型切换到这些类型时，都调整为正方形
            if (oldCodeType == "PDF417" || qAbs(m_size.width() - m_size.height()) > 20) {
                // 如果从PDF417切换过来，或者当前不是正方形，调整为正方形
                // 取较小的尺寸作为正方形边长，保证内容能完整显示
                qreal squareSize = qMin(m_size.width(), m_size.height());
                setSize(QSizeF(squareSize, squareSize));
            }
        }
        
        update();
    }
}

void QRCodeItem::setSize(const QSizeF &size)
{
    if (m_size != size) {
        prepareGeometryChange();
        m_size = size;
        update();
    }
}

void QRCodeItem::setForegroundColor(const QColor &color)
{
    if (m_foregroundColor != color) {
        m_foregroundColor = color;
        update();
    }
}

void QRCodeItem::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        update();
    }
}

void QRCodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
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

void QRCodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
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

void QRCodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
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

void QRCodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{    // 创建右键菜单
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
        // 复制当前二维码的属性到静态变量
        s_hasCopiedItem = true;
        s_copiedText = m_text;
        s_copiedCodeType = m_codeType;
        s_copiedSize = m_size;
        s_copiedForegroundColor = m_foregroundColor;
        s_copiedBackgroundColor = m_backgroundColor;
        
        // 清除其他类型的复制状态
        BarcodeItem::s_hasCopiedItem = false;
        TextItem::s_hasCopiedItem = false;
        
        // 设置最后复制的项目类型
        LabelScene::s_lastCopiedItemType = LabelScene::QRCode;
        
        qDebug() << "二维码已复制到剪贴板";
    }
    else if(selectedAction == pasteAction){
        // 只有当有复制的内容时才能粘贴
        if(s_hasCopiedItem){
            QRCodeItem* newQRCode = new QRCodeItem(s_copiedText);
            newQRCode->setCodeType(s_copiedCodeType);
            newQRCode->setSize(s_copiedSize);
            newQRCode->setForegroundColor(s_copiedForegroundColor);
            newQRCode->setBackgroundColor(s_copiedBackgroundColor);
            
            // 获取右键点击的位置并设置新位置
            QPointF sceneClickPos = event->scenePos(); // 场景坐标
            
            // 在点击位置创建新的二维码（稍微偏移避免重叠）
            QPointF newPos = sceneClickPos + QPointF(20, 20);
            newQRCode->setPos(newPos);
            
            scene()->addItem(newQRCode);
            
            // 使用撤销管理器记录粘贴操作
            LabelScene* labelScene = qobject_cast<LabelScene*>(scene());
            if (labelScene && labelScene->undoManager()) {
                labelScene->undoManager()->pasteItem(newQRCode, QObject::tr("粘贴二维码"));
            }
            
            scene()->clearSelection();
            newQRCode->setSelected(true);
            
            qDebug() << "在位置" << newPos << "粘贴了二维码";
        } else {
            qDebug() << "没有可粘贴的内容";
        }
    }
    else if (selectedAction == deleteAction) {
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

void QRCodeItem::copyItem(QRCodeItem *item)
{
    // 复制当前二维码的属性到静态变量
    s_hasCopiedItem = true;
    s_copiedText = m_text;
    s_copiedCodeType = m_codeType;
    s_copiedSize = m_size;
    s_copiedForegroundColor = m_foregroundColor;
    s_copiedBackgroundColor = m_backgroundColor;
    
    // 清除其他类型的复制状态
    BarcodeItem::s_hasCopiedItem = false;
    TextItem::s_hasCopiedItem = false;
    ImageItem::s_hasCopiedItem = false;
    LineItem::s_hasCopiedItem = false;
    RectangleItem::s_hasCopiedItem = false;
    CircleItem::s_hasCopiedItem = false;
    
    // 设置最后复制的项目类型
    LabelScene::s_lastCopiedItemType = LabelScene::QRCode;
    
    qDebug() << "二维码已复制到剪贴板";
}

// 悬停事件处理
void QRCodeItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
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

void QRCodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

// 获取指定位置的调整手柄类型
QRCodeItem::HandleType QRCodeItem::getHandleAt(const QPointF &pos) const
{
    const qreal tolerance = 5.0;  // 增加容差，便于点击
    
    // 对于非PDF417类型，只检查四个角点
    if (m_codeType != "PDF417") {
        // 只检查四个角点
        HandleType cornerHandles[] = {TopLeft, TopRight, BottomLeft, BottomRight};
        for (HandleType handle : cornerHandles) {
            QRectF handleRect = getHandleRect(handle);
            handleRect.adjust(-tolerance, -tolerance, tolerance, tolerance);
            if (handleRect.contains(pos)) {
                return handle;
            }
        }
    } else {
        // PDF417检查所有8个手柄
        for (int i = TopLeft; i <= BottomRight; ++i) {
            HandleType handle = static_cast<HandleType>(i);
            QRectF handleRect = getHandleRect(handle);
            handleRect.adjust(-tolerance, -tolerance, tolerance, tolerance);
            if (handleRect.contains(pos)) {
                return handle;
            }
        }
    }
    
    return NoHandle;
}

// 获取指定手柄的矩形区域
QRectF QRCodeItem::getHandleRect(HandleType handle) const
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
void QRCodeItem::updateSizeFromHandle(HandleType handle, const QPointF &delta)
{
    QSizeF newSize = m_resizeStartSize;
    QPointF newPos = m_resizeStartItemPos;
    const qreal minSize = 30.0;  // 最小尺寸
    
    // 对于QR、DataMatrix、Aztec，所有角点都保持正方形比例
    if (m_codeType == "QR" || m_codeType == "DataMatrix" || m_codeType == "Aztec") {
        qreal ratio = 0;
        
        switch (handle) {
            case TopLeft:
                // 左上角：取较小的变化量（因为是减少）
                ratio = qMin(-delta.x(), -delta.y());
                newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + ratio));
                newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + ratio));
                // 计算位置偏移
                if (newSize.width() != m_resizeStartSize.width()) {
                    newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
                    newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
                }
                break;
                
            case TopRight:
                // 右上角：x增加，y减少
                ratio = qMax(delta.x(), -delta.y());
                newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + ratio));
                newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + ratio));
                if (newSize.height() != m_resizeStartSize.height()) {
                    newPos.setY(m_resizeStartItemPos.y() + (m_resizeStartSize.height() - newSize.height()));
                }
                break;
                
            case BottomLeft:
                // 左下角：x减少，y增加
                ratio = qMax(-delta.x(), delta.y());
                newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + ratio));
                newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + ratio));
                if (newSize.width() != m_resizeStartSize.width()) {
                    newPos.setX(m_resizeStartItemPos.x() + (m_resizeStartSize.width() - newSize.width()));
                }
                break;
                
            case BottomRight:
                // 右下角：都增加
                ratio = qMax(delta.x(), delta.y());
                newSize.setWidth(qMax(minSize, m_resizeStartSize.width() + ratio));
                newSize.setHeight(qMax(minSize, m_resizeStartSize.height() + ratio));
                break;
                
            default:
                return;  // 非角点手柄，不处理
        }
    } else {
        // PDF417 允许不同的宽高比，保持原有逻辑
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
    }
    
    // 更新位置和尺寸
    setPos(newPos);
    setSize(newSize);
}

// 为不同的手柄设置相应的光标
void QRCodeItem::setCursorForHandle(HandleType handle)
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

