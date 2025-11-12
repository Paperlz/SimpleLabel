#ifndef IMAGEITEM_H
#define IMAGEITEM_H

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
#include <QPixmap>
#include <QByteArray>
#include "alignableitem.h"

class ImageItem : public QGraphicsItem, public AlignableItem
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

    explicit ImageItem(const QString &imagePath = "", QGraphicsItem *parent = nullptr);
    explicit ImageItem(const QPixmap &pixmap, QGraphicsItem *parent = nullptr);
    explicit ImageItem(const QByteArray &imageData, QGraphicsItem *parent = nullptr);
    ~ImageItem() override = default;

    // 重写QGraphicsItem的纯虚函数
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;

    // 图像相关方法
    bool loadImage(const QString &imagePath);
    bool setImageData(const QByteArray &imageData);
    void setPixmap(const QPixmap &pixmap);
    QPixmap pixmap() const { return m_pixmap; }
    
    // 获取图像数据（用于序列化）
    QByteArray imageData() const;
    QString imagePath() const { return m_imagePath; }

    // 设置和获取图像大小
    void setSize(const QSizeF &size);
    QSizeF size() const { return m_size; }

    // 保持宽高比设置
    void setKeepAspectRatio(bool keep);
    bool keepAspectRatio() const { return m_keepAspectRatio; }

    // 透明度设置
    void setOpacity(qreal opacity);
    qreal opacity() const { return m_opacity; }

    // 实现 AlignableItem 接口
    QGraphicsItem* asGraphicsItem() override { return this; }
    const QGraphicsItem* asGraphicsItem() const override { return this; }

    // 静态成员变量，用于复制粘贴功能
    static bool s_hasCopiedItem;
    static QByteArray s_copiedImageData;
    static QSizeF s_copiedSize;
    static bool s_copiedKeepAspectRatio;
    static qreal s_copiedOpacity;

    // 复制方法
    void copyItem(ImageItem *item);

protected:
    // 鼠标事件处理
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent *event) override;

    // 右键菜单
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    // 对齐使用的内容矩形（局部坐标）
    QRectF alignmentRectLocal() const override { return QRectF(0, 0, m_size.width(), m_size.height()); }

private:
    // 调整手柄相关的辅助方法
    HandleType getHandleAt(const QPointF &pos) const;
    QRectF getHandleRect(HandleType handle) const;
    void updateSizeFromHandle(HandleType handle, const QPointF &delta);
    void setCursorForHandle(HandleType handle);
    void drawResizeHandles(QPainter *painter);

    // 计算保持宽高比的尺寸
    QSizeF calculateAspectRatioSize(const QSizeF &newSize) const;

    // 成员变量
    QPixmap m_pixmap;               // 图像数据
    QByteArray m_originalImageData; // 原始图像数据（用于序列化）
    QString m_imagePath;            // 图像文件路径（如果从文件加载）
    QSizeF m_size;                  // 显示尺寸
    bool m_keepAspectRatio;         // 是否保持宽高比
    qreal m_opacity;                // 透明度

    // 调整大小相关
    bool m_isResizing;              // 是否正在调整大小
    HandleType m_resizeHandle;      // 当前调整手柄
    QPointF m_resizeStartPos;       // 调整开始位置
    QSizeF m_resizeStartSize;       // 调整开始尺寸
    QPointF m_resizeStartItemPos;   // 调整开始时的图像位置
};

#endif // IMAGEITEM_H
