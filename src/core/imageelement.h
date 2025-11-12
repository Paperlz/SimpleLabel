#ifndef IMAGEELEMENT_H
#define IMAGEELEMENT_H

#include "labelelement.h"
#include "../graphics/imageitem.h"
#include <QJsonObject>
#include <QPointF>
#include <QSizeF>
#include <QByteArray>
#include <memory>

class ImageElement : public labelelement {
public:
    // 构造函数
    ImageElement() = default;
    explicit ImageElement(ImageItem* item);
    explicit ImageElement(const QByteArray& imageData,
                         const QSizeF& size = QSizeF(200, 200),
                         bool keepAspectRatio = true,
                         qreal opacity = 1.0);
    explicit ImageElement(const QString& imagePath,
                         const QSizeF& size = QSizeF(200, 200),
                         bool keepAspectRatio = true,
                         qreal opacity = 1.0);

    // 实现基类的虚函数
    QGraphicsItem* getItem() const override;
    QString getType() const override;
    QJsonObject getData() const override;
    void setData(const QJsonObject& data) override;
    void setPos(const QPointF& pos) override;
    QPointF getPos() const override;

    // ImageElement 特有的方法
    QByteArray getImageData() const;
    void setImageData(const QByteArray& imageData);

    QString getImagePath() const;
    void setImagePath(const QString& imagePath);

    QSizeF getSize() const;
    void setSize(const QSizeF& size);

    bool getKeepAspectRatio() const;
    void setKeepAspectRatio(bool keep);

    qreal getOpacity() const;
    void setOpacity(qreal opacity);

    // 创建 ImageItem 并添加到场景
    void addToScene(QGraphicsScene* scene) override;

private:
    ImageItem* m_item = nullptr;
    QByteArray m_imageData;
    QString m_imagePath;
    QSizeF m_size;
    bool m_keepAspectRatio = true;
    qreal m_opacity = 1.0;

    // 私有辅助方法
    void syncFromItem();
    void syncToItem();
};

#endif // IMAGEELEMENT_H
