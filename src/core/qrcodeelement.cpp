#include "qrcodeelement.h"
#include "datasource.h"
#include <QGraphicsScene>

QRCodeElement::QRCodeElement(QRCodeItem* item) : m_item(item) {
    if (item) {
        m_text = item->text();
        m_size = item->size();
        m_foregroundColor = item->foregroundColor();
        m_backgroundColor = item->backgroundColor();
    }
}

QRCodeElement::QRCodeElement(const QString& text, const QSizeF& size,
                           const QColor& foreground, const QColor& background)
    : m_text(text), m_size(size), m_foregroundColor(foreground), m_backgroundColor(background) {
    // 构造函数中不创建 item，这样可以延迟到需要时才创建
}

QGraphicsItem* QRCodeElement::getItem() const {
    return m_item;
}

QString QRCodeElement::getType() const {
    return "qrcode";
}

QJsonObject QRCodeElement::getData() const {
    QJsonObject data;
    data["text"] = m_text;
    data["width"] = m_size.width();
    data["height"] = m_size.height();
    data["foregroundColor"] = m_foregroundColor.name();
    data["backgroundColor"] = m_backgroundColor.name();
    return data;
}

void QRCodeElement::setData(const QJsonObject& data) {
    m_text = data["text"].toString();
    m_size = QSizeF(data["width"].toDouble(), data["height"].toDouble());
    m_foregroundColor = QColor(data["foregroundColor"].toString());
    m_backgroundColor = QColor(data["backgroundColor"].toString());

    // 如果已有 item，则更新其属性
    if (m_item) {
        m_item->setText(m_text);
        m_item->setSize(m_size);
        m_item->setForegroundColor(m_foregroundColor);
        m_item->setBackgroundColor(m_backgroundColor);
    }
}

void QRCodeElement::setPos(const QPointF& pos) {
    if (m_item) {
        m_item->setPos(pos);
    }
}

QPointF QRCodeElement::getPos() const {
    if (m_item) {
        return m_item->pos();
    }
    return QPointF(0, 0);
}

QString QRCodeElement::getText() const {
    return m_text;
}

void QRCodeElement::setText(const QString& text) {
    m_text = text;
    if (m_item) {
        m_item->setText(text);
    }
}

QSizeF QRCodeElement::getSize() const {
    return m_size;
}

void QRCodeElement::setSize(const QSizeF& size) {
    m_size = size;
    if (m_item) {
        m_item->setSize(size);
    }
}

QColor QRCodeElement::getForegroundColor() const {
    return m_foregroundColor;
}

void QRCodeElement::setForegroundColor(const QColor& color) {
    m_foregroundColor = color;
    if (m_item) {
        m_item->setForegroundColor(color);
    }
}

QColor QRCodeElement::getBackgroundColor() const {
    return m_backgroundColor;
}

void QRCodeElement::setBackgroundColor(const QColor& color) {
    m_backgroundColor = color;
    if (m_item) {
        m_item->setBackgroundColor(color);
    }
}

void QRCodeElement::addToScene(QGraphicsScene* scene) {
    if (!scene) return;

    // 如果 item 还不存在，创建一个新的
    if (!m_item) {
        m_item = new QRCodeItem(m_text);
        m_item->setSize(m_size);
        m_item->setForegroundColor(m_foregroundColor);
        m_item->setBackgroundColor(m_backgroundColor);
    }

    scene->addItem(m_item);
}

bool QRCodeElement::applyDataSourceRecord(int index)
{
    if (!isDataSourceEnabled()) {
        return false;
    }

    auto source = dataSource();
    if (!source) {
        return false;
    }

    if (index < 0 || index >= source->count()) {
        return false;
    }

    if (!m_hasOriginalData) {
        m_originalData = m_text;
        m_hasOriginalData = true;
    }

    QString newValue = source->at(index);
    if (newValue.isNull()) {
        newValue.clear();
    }

    newValue = newValue.trimmed();

    m_text = newValue;
    if (m_item) {
        m_item->setText(newValue);
    }

    return true;
}

void QRCodeElement::restoreOriginalData()
{
    if (!m_hasOriginalData) {
        return;
    }

    m_text = m_originalData;
    if (m_item) {
        m_item->setText(m_originalData);
    }

    m_hasOriginalData = false;
}