#include "barcodeelement.h"
#include "datasource.h"
#include <QGraphicsScene>

BarcodeElement::BarcodeElement(BarcodeItem* item) : m_item(item) {
    if (item) {
        m_data = item->data();
        m_format = item->format();
        m_size = item->size();
        m_foregroundColor = item->foregroundColor();
        m_backgroundColor = item->backgroundColor();
        m_showHumanReadableText = item->showHumanReadableText();
        m_humanReadableTextFont = item->humanReadableTextFont();
    }
}

BarcodeElement::BarcodeElement(const QString& data,
                             ZXing::BarcodeFormat format,
                             const QSizeF& size,
                             const QColor& foreground,
                             const QColor& background)
    : m_data(data), m_format(format), m_size(size),
      m_foregroundColor(foreground), m_backgroundColor(background) {
    // 构造函数中不创建 item，这样可以延迟到需要时才创建
}

QGraphicsItem* BarcodeElement::getItem() const {
    return m_item;
}

QString BarcodeElement::getType() const {
    return "barcode";
}

QJsonObject BarcodeElement::getData() const {
    QJsonObject data;
    data["data"] = m_data;
    data["format"] = static_cast<int>(m_format);
    data["width"] = m_size.width();
    data["height"] = m_size.height();
    data["foregroundColor"] = m_foregroundColor.name();
    data["backgroundColor"] = m_backgroundColor.name();
    data["showHumanReadableText"] = m_showHumanReadableText;
    data["fontFamily"] = m_humanReadableTextFont.family();
    data["fontSize"] = m_humanReadableTextFont.pointSize();
    return data;
}

void BarcodeElement::setData(const QJsonObject& data) {
    m_data = data["data"].toString();
    m_format = static_cast<ZXing::BarcodeFormat>(data["format"].toInt());
    m_size = QSizeF(data["width"].toDouble(), data["height"].toDouble());
    m_foregroundColor = QColor(data["foregroundColor"].toString());
    m_backgroundColor = QColor(data["backgroundColor"].toString());
    m_showHumanReadableText = data["showHumanReadableText"].toBool(true);

    QFont font;
    font.setFamily(data["fontFamily"].toString("Arial"));
    font.setPointSize(data["fontSize"].toInt(10));
    m_humanReadableTextFont = font;

    // 如果已有 item，则更新其属性
    if (m_item) {
        m_item->setData(m_data);
        m_item->setFormat(m_format);
        m_item->setSize(m_size);
        m_item->setForegroundColor(m_foregroundColor);
        m_item->setBackgroundColor(m_backgroundColor);
        m_item->setShowHumanReadableText(m_showHumanReadableText);
        m_item->setHumanReadableTextFont(m_humanReadableTextFont);
    }
}

void BarcodeElement::setPos(const QPointF& pos) {
    if (m_item) {
        m_item->setPos(pos);
    }
}

QPointF BarcodeElement::getPos() const {
    if (m_item) {
        return m_item->pos();
    }
    return QPointF(0, 0);
}

QString BarcodeElement::getBarcodeData() const{
    return m_data;
}

void BarcodeElement::setBarcodeData(const QString& data) {
    m_data = data;
    if (m_item) {
        m_item->setData(data);
    }
}

ZXing::BarcodeFormat BarcodeElement::getFormat() const {
    return m_format;
}

void BarcodeElement::setFormat(ZXing::BarcodeFormat format) {
    m_format = format;
    if (m_item) {
        m_item->setFormat(format);
    }
}

QSizeF BarcodeElement::getSize() const {
    return m_size;
}

void BarcodeElement::setSize(const QSizeF& size) {
    m_size = size;
    if (m_item) {
        m_item->setSize(size);
    }
}

QColor BarcodeElement::getForegroundColor() const {
    return m_foregroundColor;
}

void BarcodeElement::setForegroundColor(const QColor& color) {
    m_foregroundColor = color;
    if (m_item) {
        m_item->setForegroundColor(color);
    }
}

QColor BarcodeElement::getBackgroundColor() const {
    return m_backgroundColor;
}

void BarcodeElement::setBackgroundColor(const QColor& color) {
    m_backgroundColor = color;
    if (m_item) {
        m_item->setBackgroundColor(color);
    }
}

bool BarcodeElement::getShowHumanReadableText() const {
    return m_showHumanReadableText;
}

void BarcodeElement::setShowHumanReadableText(bool show) {
    m_showHumanReadableText = show;
    if (m_item) {
        m_item->setShowHumanReadableText(show);
    }
}

QFont BarcodeElement::getHumanReadableTextFont() const {
    return m_humanReadableTextFont;
}

void BarcodeElement::setHumanReadableTextFont(const QFont& font) {
    m_humanReadableTextFont = font;
    if (m_item) {
        m_item->setHumanReadableTextFont(font);
    }
}

bool BarcodeElement::applyDataSourceRecord(int index)
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
        m_originalData = m_data;
        m_hasOriginalData = true;
    }

    QString newValue = source->at(index);
    if (newValue.isNull()) {
        newValue.clear();
    }

    newValue = newValue.trimmed();

    m_data = newValue;
    if (m_item) {
        m_item->setData(newValue);
    }
    return true;
}

void BarcodeElement::restoreOriginalData()
{
    if (!m_hasOriginalData) {
        return;
    }

    m_data = m_originalData;
    if (m_item) {
        m_item->setData(m_originalData);
    }

    m_hasOriginalData = false;
}

void BarcodeElement::addToScene(QGraphicsScene* scene) {
    if (!scene) return;

    // 如果 item 还不存在，创建一个新的
    if (!m_item) {
        m_item = new BarcodeItem(m_data, m_format);
        m_item->setSize(m_size);
        m_item->setForegroundColor(m_foregroundColor);
        m_item->setBackgroundColor(m_backgroundColor);
        m_item->setShowHumanReadableText(m_showHumanReadableText);
        m_item->setHumanReadableTextFont(m_humanReadableTextFont);
    }

    scene->addItem(m_item);
}