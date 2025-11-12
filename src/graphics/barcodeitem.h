#ifndef BARCODEITEM_H
#define BARCODEITEM_H

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QPen>
#include <QBrush>
#include <QStyleOptionGraphicsItem>
#include <QWidget>
#include <QDebug>
#include <QSizeF>
#include <QColor>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QMenu>

// ZXing Headers (adjust paths if necessary)
#include "MultiFormatWriter.h"
#include "BitMatrix.h"
#include "BarcodeFormat.h"
#include "TextUtfEncoding.h" // For UTF-8 conversion
#include "alignableitem.h"
// #include "ZXing/ImageView.h" // Removed as not used
// #include "ZXing/Image.h"     // Removed as not used

class BarcodeItem : public QGraphicsItem, public AlignableItem
{
public:
    // 调整手柄类型枚举
    enum HandleType {
        NoHandle = 0,
        TopLeft,
        TopCenter,
        TopRight,
        MiddleLeft,
        MiddleRight,
        BottomLeft,
        BottomCenter,
        BottomRight
    };

    explicit BarcodeItem(const QString &data = "",
                         ZXing::BarcodeFormat format = ZXing::BarcodeFormat::Code128,
                         QGraphicsItem *parent = nullptr);
    ~BarcodeItem() override = default;

    // Override QGraphicsItem pure virtual functions
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // Setters and Getters
    void setData(const QString &data);
    QString data() const { return m_data; }

    void setFormat(ZXing::BarcodeFormat format);
    ZXing::BarcodeFormat format() const { return m_format; }

    void setSize(const QSizeF &size);
    QSizeF size() const { return m_size; }

    void setForegroundColor(const QColor &color);
    QColor foregroundColor() const { return m_foregroundColor; }

    void setBackgroundColor(const QColor &color);
    QColor backgroundColor() const { return m_backgroundColor; }

    void setShowHumanReadableText(bool show);
    bool showHumanReadableText() const { return m_showHumanReadableText; }    void setHumanReadableTextFont(const QFont &font);
    QFont humanReadableTextFont() const { return m_humanReadableTextFont; }

    // 实现 AlignableItem 接口
    QGraphicsItem* asGraphicsItem() override { return this; }
    const QGraphicsItem* asGraphicsItem() const override { return this; }

    // 静态成员变量，用于复制粘贴功能
    static bool s_hasCopiedItem;
    static QString s_copiedData;
    static ZXing::BarcodeFormat s_copiedFormat;
    static QSizeF s_copiedSize;
    static QColor s_copiedForegroundColor;
    static QColor s_copiedBackgroundColor;
    static bool s_copiedShowHumanReadableText;
    static QFont s_copiedHumanReadableTextFont;

    // 复制方法
    void copyItem(BarcodeItem *item);


protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    // 对齐使用的内容矩形（局部坐标）
    QRectF alignmentRectLocal() const override { return QRectF(0, 0, m_size.width(), m_size.height()); }

private:
    // 调整手柄相关的辅助方法
    HandleType getHandleAt(const QPointF &pos) const;
    QRectF getHandleRect(HandleType handle) const;
    void updateSizeFromHandle(HandleType handle, const QPointF &delta);
    void setCursorForHandle(HandleType handle);

    QString m_data;
    ZXing::BarcodeFormat m_format;
    QSizeF m_size;
    QColor m_foregroundColor;
    QColor m_backgroundColor;

    bool m_isResizing;
    HandleType m_resizeHandle;
    QPointF m_resizeStartPos;
    QSizeF m_resizeStartSize;
    QPointF m_resizeStartItemPos;

    bool m_showHumanReadableText;
    QFont m_humanReadableTextFont;
    QString m_displayText;

    // QImage generateBarcodeImage() const; // This helper was not fully implemented and paint() does direct rendering
};

#endif // BARCODEITEM_H
