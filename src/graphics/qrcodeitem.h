#ifndef QRCODEITEM_H
#define QRCODEITEM_H

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
#include <QJsonObject>
#include <qjsonvalue.h>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>

#include "../../third_party/QR-Code-generator/cpp/qrcodegen.hpp"
#include "MultiFormatWriter.h"
#include "BitMatrix.h"
#include "BarcodeFormat.h"
#include "TextUtfEncoding.h"
#include "alignableitem.h"

class QRCodeItem : public QGraphicsItem, public AlignableItem
{
public:
    // 调整手柄类型枚举
    enum HandleType {
        NoHandle = -1,
        TopLeft = 0,
        TopCenter = 1,
        TopRight = 2,
        MiddleLeft = 3,
        MiddleRight = 4,
        BottomLeft = 5,
        BottomCenter = 6,
        BottomRight = 7
    };

    explicit QRCodeItem(const QString &text = "", QGraphicsItem *parent = nullptr);
    ~QRCodeItem() override = default;

    // 重写QGraphicsItem的纯虚函数
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // 设置和获取二维码内容
    void setText(const QString &text);
    QString text() const { return m_text; }

    // 设置和获取二维码类型
    void setCodeType(const QString &codeType);
    QString codeType() const { return m_codeType; }

    void copyItem(QRCodeItem *item);

    // 设置和获取二维码大小
    void setSize(const QSizeF &size);
    QSizeF size() const { return m_size; }

    // 设置和获取二维码颜色
    void setForegroundColor(const QColor &color);
    QColor foregroundColor() const { return m_foregroundColor; }    void setBackgroundColor(const QColor &color);
    QColor backgroundColor() const { return m_backgroundColor; }

    // 实现 AlignableItem 接口
    QGraphicsItem* asGraphicsItem() override { return this; }
    const QGraphicsItem* asGraphicsItem() const override { return this; }

    // 静态成员变量，用于复制粘贴功能（公共访问）
    static bool s_hasCopiedItem;
    static QString s_copiedText;
    static QString s_copiedCodeType;
    static QSizeF s_copiedSize;
    static QColor s_copiedForegroundColor;
    static QColor s_copiedBackgroundColor;

    signals:
    void requestDelete(QGraphicsItem *item);

protected:
    // 重写鼠标事件处理函数
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    // 对齐使用的内容矩形（局部坐标）
    QRectF alignmentRectLocal() const override { return QRectF(0, 0, m_size.width(), m_size.height()); }

private:
    QString m_text;
    QString m_codeType;  // 添加代码类型成员变量
    QSizeF m_size;
    QColor m_foregroundColor;
    QColor m_backgroundColor;
    bool m_isResizing;
    QPointF m_resizeStartPos;
    QSizeF m_resizeStartSize;
    QPointF m_resizeStartItemPos;  // 调整开始时的项目位置
    HandleType m_resizeHandle;     // 当前调整手柄类型
    
    // 辅助方法
    HandleType getHandleAt(const QPointF &pos) const;
    QRectF getHandleRect(HandleType handle) const;
    void updateSizeFromHandle(HandleType handle, const QPointF &delta);
    void setCursorForHandle(HandleType handle);
};

#endif // QRCODEITEM_H
