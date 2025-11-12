#include "textitem.h"
#include <QGraphicsScene>
#include "labelscene.h"
#include "../commands/undomanager.h"
#include "qrcodeitem.h"
#include "barcodeitem.h"
#include "imageitem.h"
#include "lineitem.h"
#include "rectangleitem.h"
#include "circleitem.h"
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QDebug>
#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QTextEdit>
#include <QTextDocument>
#include <QScreen>

// 常量定义
static const qreal HANDLE_SIZE = 8.0;
static const qreal PADDING = 5.0;

namespace {
constexpr double kMillimetresPerInch = 25.4;
constexpr double kScenePixelsPerMillimetre = 7.559056; // keep in sync with layout scaling
constexpr double kSceneDpi = kScenePixelsPerMillimetre * kMillimetresPerInch;
}

// 静态成员变量定义
bool TextItem::s_hasCopiedItem = false;
QString TextItem::s_copiedText = "";
QFont TextItem::s_copiedFont = QFont("Arial", 12);
QColor TextItem::s_copiedTextColor = Qt::black;
QColor TextItem::s_copiedBackgroundColor = Qt::white;
QPen TextItem::s_copiedBorderPen = QPen(Qt::black, 1.0, Qt::SolidLine);
bool TextItem::s_copiedBorderEnabled = false;
QSizeF TextItem::s_copiedSize = QSizeF(100, 30);
Qt::Alignment TextItem::s_copiedAlignment = Qt::AlignLeft | Qt::AlignVCenter;
bool TextItem::s_copiedWordWrap = false;
bool TextItem::s_copiedAutoResize = false;
bool TextItem::s_copiedBackgroundEnabled = false;
qreal TextItem::s_copiedLetterSpacing = 0.0;

TextItem::TextItem(const QString &text, QGraphicsItem *parent)
    : QGraphicsItem(parent)
    , m_text(text)
    , m_font("Arial", 12)
    , m_textColor(Qt::black)
    , m_backgroundColor(Qt::white)
    , m_borderPen(Qt::black, 1.0, Qt::SolidLine)
    , m_borderEnabled(false)
    , m_backgroundEnabled(false)
    , m_size(100, 30)
    , m_alignment(Qt::AlignLeft | Qt::AlignVCenter)
    , m_letterSpacing(0.0)
    , m_wordWrap(false)
    , m_autoResize(false)
    , m_isEditing(false)
    , m_isResizing(false)
    , m_resizeHandle(NoHandle)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    
    // 接收鼠标悬浮事件
    setAcceptHoverEvents(true);
    
    // 确保能接收鼠标事件
    setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
    
    // 确保不被事件过滤器阻止
    setFiltersChildEvents(false);

    // 应用初始字距
    m_font.setLetterSpacing(QFont::AbsoluteSpacing, m_letterSpacing);

    // 如果启用自动调整大小，计算初始尺寸
    if (m_autoResize && !m_text.isEmpty()) {
        m_size = calculateMinimumSize();
    }
}

QRectF TextItem::boundingRect() const
{
    // Compute minimal padding: include half handle size and any border pen half-width.
    const qreal handleHalf = HANDLE_SIZE / 2.0;
    const qreal borderPenHalf = m_borderPen.widthF() / 2.0;
    // Ensure at least 1px padding so selection/handles never touch the content
    const qreal totalPadding = qMax<qreal>(1.0, qMax(handleHalf, borderPenHalf));

    QRectF rect(-totalPadding, -totalPadding,
                m_size.width() + 2 * totalPadding,
                m_size.height() + 2 * totalPadding);
                
    // 调试信息
    // qDebug() << "TextItem::boundingRect - 返回区域:" << rect << "文本尺寸:" << m_size;
    
    return rect;
}

void TextItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    // 获取文本框的矩形区域
    QRectF textRect(0, 0, m_size.width(), m_size.height());

    // 绘制背景
    if (m_backgroundEnabled) {
        drawBackground(painter, textRect);
    }

    // 绘制边框
    if (m_borderEnabled) {
        drawBorder(painter, textRect);
    }

    // 绘制文本
    drawText(painter, textRect);

    // 如果选中且不在编辑状态，绘制调整手柄
    if (isSelected() && !m_isEditing) {
        drawResizeHandles(painter);
    }

    // 如果启用对齐功能，绘制对齐线
    paintAlignmentLines(painter);
}

void TextItem::setText(const QString &text)
{
    if (m_text != text) {
        prepareGeometryChange();
        m_text = text;
        
        // 如果启用自动调整大小，重新计算尺寸
        if (m_autoResize) {
            m_size = calculateMinimumSize();
        }
        
        update();
    }
}

void TextItem::setFont(const QFont &font)
{
    if (m_font != font) {
        prepareGeometryChange();
        m_font = font;
        // 保持当前字距设置
        m_font.setLetterSpacing(QFont::AbsoluteSpacing, m_letterSpacing);
        
        // 如果启用自动调整大小，重新计算尺寸
        if (m_autoResize) {
            m_size = calculateMinimumSize();
        }
        
        update();
    }
}

void TextItem::setTextColor(const QColor &color)
{
    if (m_textColor != color) {
        m_textColor = color;
        update();
    }
}

void TextItem::setBackgroundColor(const QColor &color)
{
    if (m_backgroundColor != color) {
        m_backgroundColor = color;
        update();
    }
}

void TextItem::setBorderPen(const QPen &pen)
{
    if (m_borderPen != pen) {
        m_borderPen = pen;
        update();
    }
}

void TextItem::setBorderEnabled(bool enabled)
{
    if (m_borderEnabled != enabled) {
        m_borderEnabled = enabled;
        update();
    }
}

void TextItem::setBorderWidth(int width)
{
    if (width >= 0 && m_borderPen.width() != width) {
        m_borderPen.setWidth(width);
        update();
    }
}

int TextItem::borderWidth() const
{
    return m_borderPen.width();
}

void TextItem::setBorderColor(const QColor &color)
{
    if (color.isValid() && m_borderPen.color() != color) {
        m_borderPen.setColor(color);
        update();
    }
}

QColor TextItem::borderColor() const
{
    return m_borderPen.color();
}

void TextItem::setSize(const QSizeF &size)
{
    if (m_size != size && size.width() > 0 && size.height() > 0) {
        prepareGeometryChange();
        m_size = size;
        update();
    }
}

void TextItem::setSizeInternal(const QSizeF &size)
{
    // 内部尺寸设置，不触发prepareGeometryChange()
    if (m_size != size && size.width() > 0 && size.height() > 0) {
        m_size = size;
    }
}

void TextItem::setAlignment(Qt::Alignment alignment)
{
    if (m_alignment != alignment) {
        m_alignment = alignment;
        update();
    }
}

void TextItem::setLetterSpacing(qreal pixels)
{
    // 限制范围，避免过大或过小
    qreal clamped = qBound<qreal>(-5.0, pixels, 20.0);
    if (!qFuzzyCompare(1 + m_letterSpacing, 1 + clamped)) {
        prepareGeometryChange();
        m_letterSpacing = clamped;
        m_font.setLetterSpacing(QFont::AbsoluteSpacing, m_letterSpacing);
        if (m_autoResize) {
            m_size = calculateMinimumSize();
        }
        update();
    }
}

void TextItem::setWordWrap(bool wrap)
{
    if (m_wordWrap != wrap) {
        prepareGeometryChange();
        m_wordWrap = wrap;
        
        // 如果启用自动调整大小，重新计算尺寸
        if (m_autoResize) {
            m_size = calculateMinimumSize();
        }
        
        update();
    }
}

void TextItem::setAutoResize(bool autoResize)
{
    if (m_autoResize != autoResize) {
        m_autoResize = autoResize;
        
        // 如果启用自动调整大小，立即重新计算尺寸
        if (m_autoResize) {
            prepareGeometryChange();
            m_size = calculateMinimumSize();
            update();
        }
    }
}

void TextItem::setBackgroundEnabled(bool enabled)
{
    if (m_backgroundEnabled != enabled) {
        m_backgroundEnabled = enabled;
        update();
    }
}

void TextItem::setEditing(bool editing)
{
    if (m_isEditing != editing) {
        m_isEditing = editing;
        update();
    }
}

QSizeF TextItem::calculateMinimumSize() const
{
    if (m_text.isEmpty()) {
        return QSizeF(100, 30); // 默认最小尺寸
    }

    double baseDpi = 96.0;
    if (QScreen *screen = QApplication::primaryScreen()) {
        baseDpi = screen->logicalDotsPerInch();
    }

    const double scale = baseDpi > 0.0 ? kSceneDpi / baseDpi : 1.0;
    QFont metricsFont = m_font;
    if (scale > 0.0 && !qFuzzyCompare(scale, 1.0)) {
        if (metricsFont.pointSizeF() > 0) {
            metricsFont.setPointSizeF(metricsFont.pointSizeF() * scale);
        } else if (metricsFont.pointSize() > 0) {
            metricsFont.setPointSizeF(static_cast<double>(metricsFont.pointSize()) * scale);
        } else if (metricsFont.pixelSize() > 0) {
            const double basePointSize = static_cast<double>(metricsFont.pixelSize()) * 72.0 / kSceneDpi;
            metricsFont.setPointSizeF(basePointSize * scale);
            metricsFont.setPixelSize(-1);
        }
    }
    metricsFont.setLetterSpacing(QFont::AbsoluteSpacing, m_letterSpacing * scale);

    QFontMetrics fm(metricsFont);
    if (m_wordWrap) {
        // 如果启用换行，使用当前宽度进行计算
        qreal width = qMax(100.0, m_size.width());
        QRect boundingRect(0, 0, static_cast<int>(width), 0);
        QRect rect = fm.boundingRect(boundingRect, Qt::TextWordWrap | m_alignment, m_text);
        // 增加更多的垂直空间，确保文本完全显示
        return QSizeF(width, qMax(30.0, static_cast<qreal>(rect.height()) + 20));
    } else {
        // 单行文本
        QRect rect = fm.boundingRect(m_text);
        // 增加更多的水平和垂直空间，确保文本完全显示
        return QSizeF(qMax(100.0, static_cast<qreal>(rect.width()) + 30), 
                     qMax(30.0, static_cast<qreal>(rect.height()) + 20));
    }
}

void TextItem::drawText(QPainter *painter, const QRectF &rect)
{
    if (m_text.isEmpty()) {
        return;
    }

    double scale = 1.0;
    if (const QPaintDevice *device = painter->device()) {
        const double dpiX = static_cast<double>(device->logicalDpiX());
        const double dpiY = static_cast<double>(device->logicalDpiY());
        if (dpiX > 0.0 && dpiY > 0.0) {
            const double scaleX = kSceneDpi / dpiX;
            const double scaleY = kSceneDpi / dpiY;
            scale = (scaleX + scaleY) * 0.5;
        }
    }

    QFont adjustedFont = m_font;
    if (scale > 0.0 && !qFuzzyCompare(scale, 1.0)) {
        if (adjustedFont.pointSizeF() > 0) {
            adjustedFont.setPointSizeF(adjustedFont.pointSizeF() * scale);
        } else if (adjustedFont.pointSize() > 0) {
            adjustedFont.setPointSizeF(static_cast<double>(adjustedFont.pointSize()) * scale);
        } else if (adjustedFont.pixelSize() > 0) {
            const double basePointSize = static_cast<double>(adjustedFont.pixelSize()) * 72.0 / kSceneDpi;
            adjustedFont.setPointSizeF(basePointSize * scale);
            adjustedFont.setPixelSize(-1); // fall back to using point size
        }
    }

    painter->setPen(m_textColor);
    adjustedFont.setLetterSpacing(QFont::AbsoluteSpacing, m_letterSpacing * scale);
    painter->setFont(adjustedFont);

    QRectF textRect = rect.adjusted(2, 1, -2, -1);

    int flags = m_alignment;
    if (m_wordWrap) {
        flags |= Qt::TextWordWrap;
    }

    painter->drawText(textRect, flags, m_text);
}

void TextItem::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, m_backgroundColor);
}

void TextItem::drawBorder(QPainter *painter, const QRectF &rect)
{
    painter->setPen(m_borderPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(rect);
}

void TextItem::drawResizeHandles(QPainter *painter)
{
    painter->setPen(QPen(Qt::black, 1));
    painter->setBrush(Qt::white);

    // 绘制8个调整手柄
    for (int i = 0; i < 8; ++i) {
        QRectF handleRect = getHandleRect(static_cast<HandleType>(i));
        painter->drawRect(handleRect);
    }
}

TextItem::HandleType TextItem::getHandleAt(const QPointF &pos) const
{
    if (!isSelected()) {
        return NoHandle;
    }

    const qreal tolerance = 3.0;  // 增加点击容差
    
    // 检查所有8个手柄，使用容差扩展检测区域
    for (int i = 0; i < 8; ++i) {
        HandleType handle = static_cast<HandleType>(i);
        QRectF handleRect = getHandleRect(handle);
        // 扩展检测区域，提高点击精度
        handleRect.adjust(-tolerance, -tolerance, tolerance, tolerance);
        if (handleRect.contains(pos)) {
            return handle;
        }
    }
    return NoHandle;
}

QRectF TextItem::getHandleRect(HandleType handle) const
{
    QRectF rect(0, 0, m_size.width(), m_size.height());
    const qreal halfHandle = HANDLE_SIZE / 2.0;

    switch (handle) {
        case TopLeft:
            return QRectF(rect.left() - halfHandle, rect.top() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case TopCenter:
            return QRectF(rect.center().x() - halfHandle, rect.top() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case TopRight:
            return QRectF(rect.right() - halfHandle, rect.top() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case MiddleLeft:
            return QRectF(rect.left() - halfHandle, rect.center().y() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case MiddleRight:
            return QRectF(rect.right() - halfHandle, rect.center().y() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case BottomLeft:
            return QRectF(rect.left() - halfHandle, rect.bottom() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case BottomCenter:
            return QRectF(rect.center().x() - halfHandle, rect.bottom() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case BottomRight:
            return QRectF(rect.right() - halfHandle, rect.bottom() - halfHandle, HANDLE_SIZE, HANDLE_SIZE);
        case NoHandle:
        default:
            return QRectF();
    }
}

void TextItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (isLocked()) {
        QGraphicsItem::mousePressEvent(event);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        QPointF pos = event->pos();
        
        // 首先检查是否点击了调整手柄（优先级最高）
        if (isSelected()) {
            m_resizeHandle = getHandleAt(pos);
            
            if (m_resizeHandle != NoHandle) {
                m_isResizing = true;
                m_resizeStartPos = event->scenePos();
                m_resizeStartSize = m_size;
                m_resizeStartItemPos = this->pos();
                
                // 临时禁用移动功能，避免冲突
                setFlag(QGraphicsItem::ItemIsMovable, false);
                
                // 设置适当的光标
                setCursorForHandle(m_resizeHandle);
                
                event->accept();
                return;
            }
        }
    }
    
    // 如果不是调整大小操作，调用父类处理（包括选择和移动）
    QGraphicsItem::mousePressEvent(event);
}

void TextItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
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

void TextItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_isResizing) {
        m_isResizing = false;
        m_resizeHandle = NoHandle;
        
    // 恢复移动功能：根据锁定状态恢复
    setFlag(QGraphicsItem::ItemIsMovable, !isLocked());
        
        // 重置光标为箭头，之后的hover事件会根据位置重新设置
        setCursor(Qt::ArrowCursor);
        
        event->accept();
    } else {
        QGraphicsItem::mouseReleaseEvent(event);
        // 在释放鼠标时隐藏对齐线
        handleMouseReleaseForAlignment();
    }
}

void TextItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    Q_UNUSED(event);
    
    // 双击进入编辑模式
    if (scene()) {
        // 通过场景发送信号，请求编辑文本
        scene()->setFocusItem(this);
        
        // 这里可以发射一个自定义信号，通知主窗口开始文本编辑
        // 由于QGraphicsItem不继承自QObject，我们需要通过场景来处理
        setEditing(true);
    }
}

void TextItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    QMenu contextMenu;
    
    contextMenu.addSeparator();
    
    // 边框设置
    QAction *borderAction = contextMenu.addAction(m_borderEnabled ? "隐藏边框" : "显示边框");
    
    // 背景设置
    QAction *bgAction = contextMenu.addAction(m_backgroundEnabled ? "隐藏背景" : "显示背景");
    
    // 字距设置
    QMenu *spacingMenu = contextMenu.addMenu("字距");
    QAction *incSpacing = spacingMenu->addAction("增加 (+0.5px)");
    QAction *decSpacing = spacingMenu->addAction("减少 (-0.5px)");
    QAction *resetSpacing = spacingMenu->addAction("重置 (0px)");

    contextMenu.addSeparator();
    
    // 对齐设置
    QMenu *alignMenu = contextMenu.addMenu("文本对齐");
    QAction *leftAlign = alignMenu->addAction("左对齐");
    QAction *centerAlign = alignMenu->addAction("居中对齐");
    QAction *rightAlign = alignMenu->addAction("右对齐");
    
    contextMenu.addSeparator();
    
    // 层级操作
    QAction *bringToFrontAction = contextMenu.addAction("置于顶层");
    QAction *sendToBackAction = contextMenu.addAction("置于底层");
    QAction *bringForwardAction = contextMenu.addAction("向前一层");
    QAction *sendBackwardAction = contextMenu.addAction("向后一层");
            

    // 启用/禁用对齐
    QAction *alignToggle = contextMenu.addAction(QObject::tr("启用对齐"));
    alignToggle->setCheckable(true);
    alignToggle->setChecked(isAlignmentEnabled());
    
    contextMenu.addSeparator();
    
    // 其他操作
    QAction *copyAction = contextMenu.addAction("复制");
    QAction *pasteAction = contextMenu.addAction("粘贴");
    
    // 根据是否有复制的内容来启用/禁用粘贴选项
    pasteAction->setEnabled(s_hasCopiedItem);
    
    QAction *deleteAction = contextMenu.addAction("删除");
    
    // 锁定/解除锁定
    contextMenu.addSeparator();
    QAction *lockAction = contextMenu.addAction(isLocked() ? QObject::tr("解除锁定")
                                                          : QObject::tr("锁定"));

    QAction *selectedAction = contextMenu.exec(event->screenPos());
    
    if (selectedAction == borderAction) {
        setBorderEnabled(!m_borderEnabled);
    } else if (selectedAction == bgAction) {
        setBackgroundEnabled(!m_backgroundEnabled);
    } else if (selectedAction == incSpacing) {
        setLetterSpacing(m_letterSpacing + 0.5);
    } else if (selectedAction == decSpacing) {
        setLetterSpacing(m_letterSpacing - 0.5);
    } else if (selectedAction == resetSpacing) {
        setLetterSpacing(0.0);
    } else if (selectedAction == leftAlign) {
        setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    } else if (selectedAction == centerAlign) {
        setAlignment(Qt::AlignCenter);
    } else if (selectedAction == rightAlign) {
        setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    } else if (selectedAction == copyAction) {
        // 复制当前文本项的属性到静态变量
        s_hasCopiedItem = true;
        s_copiedText = m_text;
        s_copiedFont = m_font;
        s_copiedTextColor = m_textColor;
        s_copiedBackgroundColor = m_backgroundColor;
        s_copiedBorderPen = m_borderPen;
        s_copiedBorderEnabled = m_borderEnabled;
        s_copiedSize = m_size;
        s_copiedAlignment = m_alignment;
    s_copiedWordWrap = m_wordWrap;
        s_copiedAutoResize = m_autoResize;
    s_copiedBackgroundEnabled = m_backgroundEnabled;
    s_copiedLetterSpacing = m_letterSpacing;
        
        // 清除其他类型的复制状态
        QRCodeItem::s_hasCopiedItem = false;
        BarcodeItem::s_hasCopiedItem = false;
        // 设置最后复制的项目类型
        LabelScene::s_lastCopiedItemType = LabelScene::Text;
        
    } else if(selectedAction == pasteAction){
        // 只有当有复制的内容时才能粘贴
        if(s_hasCopiedItem){
            TextItem* newText = new TextItem(s_copiedText);
            newText->setSize(s_copiedSize);
            newText->setFont(s_copiedFont);
            newText->setTextColor(s_copiedTextColor);
            newText->setBackgroundColor(s_copiedBackgroundColor);
            newText->setBorderPen(s_copiedBorderPen);
            newText->setBorderEnabled(s_copiedBorderEnabled);
            newText->setAlignment(s_copiedAlignment);
            newText->setWordWrap(s_copiedWordWrap);
            newText->setAutoResize(s_copiedAutoResize);
            newText->setBackgroundEnabled(s_copiedBackgroundEnabled);
            newText->setLetterSpacing(s_copiedLetterSpacing);
            
            // 获取右键点击的位置并设置新位置
            QPointF sceneClickPos = event->scenePos(); // 场景坐标
            
            // 在点击位置创建新的文本项（稍微偏移避免重叠）
            QPointF newPos = sceneClickPos + QPointF(20, 20);
            newText->setPos(newPos);
            
            scene()->addItem(newText);
            
            // 使用撤销管理器记录粘贴操作
            LabelScene* labelScene = qobject_cast<LabelScene*>(scene());
            if (labelScene && labelScene->undoManager()) {
                labelScene->undoManager()->addItem(newText, QObject::tr("粘贴文本"));
            }
            
            scene()->clearSelection();
            newText->setSelected(true);
            
        } else {
            // 没有可粘贴的内容
        }
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
        setZValue(zValue() - 1);    } else if (selectedAction == deleteAction) {
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
    } else if (selectedAction == lockAction) {
        setLocked(!isLocked());
    }
    else if(selectedAction = alignToggle) {
        setAlignmentEnabled(alignToggle->isChecked());
    }
    
    event->accept();
}

void TextItem::hoverEnterEvent(QGraphicsSceneHoverEvent *event)
{
    if (isSelected() && !m_autoResize) {
        HandleType handle = getHandleAt(event->pos());
        setCursorForHandle(handle);
    }

    QGraphicsItem::hoverEnterEvent(event);
}

void TextItem::hoverLeaveEvent(QGraphicsSceneHoverEvent *event)
{
    setCursor(Qt::ArrowCursor);
    QGraphicsItem::hoverLeaveEvent(event);
}

void TextItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    if (isLocked()) {
        setCursor(Qt::ArrowCursor);
        QGraphicsItem::hoverMoveEvent(event);
        return;
    }
    if (isSelected() && !m_autoResize && !m_isResizing) {
        HandleType handle = getHandleAt(event->pos());
        setCursorForHandle(handle);
    } else if (m_isResizing) {
        // 在调整大小过程中保持当前光标
        // 不改变光标状态
    } else {
        setCursor(Qt::ArrowCursor);
    }

    QGraphicsItem::hoverMoveEvent(event);
}

// 调试用场景事件处理
bool TextItem::sceneEvent(QEvent *event)
{
    return QGraphicsItem::sceneEvent(event);
}

// 为不同的手柄设置相应的光标
void TextItem::setCursorForHandle(HandleType handle)
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

void TextItem::copyItem(TextItem *item)
{
    // 复制当前文本项的属性到静态变量
    s_hasCopiedItem = true;
    s_copiedText = m_text;
    s_copiedFont = m_font;
    s_copiedTextColor = m_textColor;
    s_copiedBackgroundColor = m_backgroundColor;
    s_copiedBorderPen = m_borderPen;
    s_copiedBorderEnabled = m_borderEnabled;
    s_copiedSize = m_size;
    s_copiedAlignment = m_alignment;
    s_copiedWordWrap = m_wordWrap;
    s_copiedAutoResize = m_autoResize;
    s_copiedBackgroundEnabled = m_backgroundEnabled;
    
    // 清除其他类型的复制状态
    QRCodeItem::s_hasCopiedItem = false;
    BarcodeItem::s_hasCopiedItem = false;
    ImageItem::s_hasCopiedItem = false;
    LineItem::s_hasCopiedItem = false;
    RectangleItem::s_hasCopiedItem = false;
    CircleItem::s_hasCopiedItem = false;
    
    // 设置最后复制的项目类型
    LabelScene::s_lastCopiedItemType = LabelScene::Text;
}

// 根据手柄类型和位移更新尺寸
void TextItem::updateSizeFromHandle(HandleType handle, const QPointF &delta)
{
    QSizeF newSize = m_resizeStartSize;
    QPointF newPos = m_resizeStartItemPos;
    const qreal minSize = 20.0;  // 最小尺寸
    
    // 文本框允许不同的宽高比，提供完整的调整功能
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


