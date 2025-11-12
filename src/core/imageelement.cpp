#include "imageelement.h"
#include <QGraphicsScene>

ImageElement::ImageElement(ImageItem* item) : m_item(item) {
    if (item) {
        syncFromItem();
    } else {
        // 设置默认值
        m_imageData.clear();
        m_imagePath.clear();
        m_size = QSizeF(200, 200);
        m_keepAspectRatio = true;
        m_opacity = 1.0;
    }
}

ImageElement::ImageElement(const QByteArray& imageData,
                          const QSizeF& size,
                          bool keepAspectRatio,
                          qreal opacity)
    : m_item(nullptr), m_imageData(imageData), m_size(size),
      m_keepAspectRatio(keepAspectRatio), m_opacity(opacity) {
    // 构造函数中不创建 item，这样可以延迟到需要时才创建
}

ImageElement::ImageElement(const QString& imagePath,
                          const QSizeF& size,
                          bool keepAspectRatio,
                          qreal opacity)
    : m_item(nullptr), m_imagePath(imagePath), m_size(size),
      m_keepAspectRatio(keepAspectRatio), m_opacity(opacity) {
    // 构造函数中不创建 item，这样可以延迟到需要时才创建
}

QGraphicsItem* ImageElement::getItem() const {
    return m_item;
}

QString ImageElement::getType() const {
    return "image";
}

QJsonObject ImageElement::getData() const {
    QJsonObject data;
    
    // 将图像数据编码为Base64字符串
    data["imageData"] = QString::fromLatin1(m_imageData.toBase64());
    data["imagePath"] = m_imagePath;
    data["width"] = m_size.width();
    data["height"] = m_size.height();
    data["keepAspectRatio"] = m_keepAspectRatio;
    data["opacity"] = m_opacity;
    
    return data;
}

void ImageElement::setData(const QJsonObject& data) {
    // 从Base64字符串解码图像数据
    QString base64Data = data["imageData"].toString();
    m_imageData = QByteArray::fromBase64(base64Data.toLatin1());
    
    m_imagePath = data["imagePath"].toString();
    m_size = QSizeF(data["width"].toDouble(), data["height"].toDouble());
    m_keepAspectRatio = data["keepAspectRatio"].toBool(true);
    m_opacity = data["opacity"].toDouble(1.0);

    // 如果已有 item，则更新其属性
    if (m_item) {
        syncToItem();
    }
}

void ImageElement::setPos(const QPointF& pos) {
    if (m_item) {
        m_item->setPos(pos);
    }
}

QPointF ImageElement::getPos() const {
    if (m_item) {
        return m_item->pos();
    }
    return QPointF(0, 0);
}

QByteArray ImageElement::getImageData() const {
    return m_imageData;
}

void ImageElement::setImageData(const QByteArray& imageData) {
    m_imageData = imageData;
    m_imagePath.clear(); // 清除文件路径，因为现在使用数据
    if (m_item) {
        m_item->setImageData(imageData);
    }
}

QString ImageElement::getImagePath() const {
    return m_imagePath;
}

void ImageElement::setImagePath(const QString& imagePath) {
    m_imagePath = imagePath;
    if (m_item) {
        m_item->loadImage(imagePath);
        // 从item同步回数据
        m_imageData = m_item->imageData();
    }
}

QSizeF ImageElement::getSize() const {
    return m_size;
}

void ImageElement::setSize(const QSizeF& size) {
    m_size = size;
    if (m_item) {
        m_item->setSize(size);
    }
}

bool ImageElement::getKeepAspectRatio() const {
    return m_keepAspectRatio;
}

void ImageElement::setKeepAspectRatio(bool keep) {
    m_keepAspectRatio = keep;
    if (m_item) {
        m_item->setKeepAspectRatio(keep);
    }
}

qreal ImageElement::getOpacity() const {
    return m_opacity;
}

void ImageElement::setOpacity(qreal opacity) {
    m_opacity = opacity;
    if (m_item) {
        m_item->setOpacity(opacity);
    }
}

void ImageElement::addToScene(QGraphicsScene* scene) {
    if (!scene) return;

    // 如果 item 还不存在，创建一个新的
    if (!m_item) {
        if (!m_imageData.isEmpty()) {
            m_item = new ImageItem(m_imageData);
        } else if (!m_imagePath.isEmpty()) {
            m_item = new ImageItem(m_imagePath);
        } else {
            m_item = new ImageItem(); // 创建空的图像项
        }
        
        // 设置属性
        m_item->setSize(m_size);
        m_item->setKeepAspectRatio(m_keepAspectRatio);
        m_item->setOpacity(m_opacity);
    }

    scene->addItem(m_item);
}

void ImageElement::syncFromItem() {
    if (!m_item) return;
    
    m_imageData = m_item->imageData();
    m_imagePath = m_item->imagePath();
    m_size = m_item->size();
    m_keepAspectRatio = m_item->keepAspectRatio();
    m_opacity = m_item->opacity();
}

void ImageElement::syncToItem() {
    if (!m_item) return;
    
    if (!m_imageData.isEmpty()) {
        m_item->setImageData(m_imageData);
    } else if (!m_imagePath.isEmpty()) {
        m_item->loadImage(m_imagePath);
    }
    
    m_item->setSize(m_size);
    m_item->setKeepAspectRatio(m_keepAspectRatio);
    m_item->setOpacity(m_opacity);
}
