#include "textelement.h"
#include "datasource.h"
#include <QGraphicsScene>

TextElement::TextElement(TextItem* item) : m_item(item) {
    if (item) {
        syncFromItem();
    } else {
        // 设置默认值
        m_text = "";
        m_font = QFont("Arial", 12);
        m_textColor = Qt::black;
        m_backgroundColor = Qt::white;
        m_borderPen = QPen(Qt::black, 1.0);
        m_borderEnabled = false;
        m_backgroundEnabled = false;
        m_size = QSizeF(100, 30);
        m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
        m_wordWrap = false;
        m_autoResize = true;
    }
}

TextElement::TextElement(const QString& text,
                         const QFont& font,
                         const QColor& textColor,
                         const QColor& backgroundColor,
                         const QSizeF& size)
    : m_item(nullptr), m_text(text), m_font(font), m_textColor(textColor),
      m_backgroundColor(backgroundColor), m_size(size) {
    
    // 设置其他默认值
    m_borderPen = QPen(Qt::black, 1.0);
    m_borderEnabled = false;
    m_backgroundEnabled = false;
    m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    m_wordWrap = false;
    m_autoResize = true;
}

QGraphicsItem* TextElement::getItem() const {
    return m_item;
}

QString TextElement::getType() const {
    return "text";
}

QJsonObject TextElement::getData() const {
    QJsonObject data;
    data["text"] = m_text;
    data["fontFamily"] = m_font.family();
    data["fontSize"] = m_font.pointSize();
    data["fontBold"] = m_font.bold();
    data["fontItalic"] = m_font.italic();
    data["fontUnderline"] = m_font.underline();
    data["textColor"] = m_textColor.name();
    data["backgroundColor"] = m_backgroundColor.name();
    data["width"] = m_size.width();
    data["height"] = m_size.height();
    data["borderEnabled"] = m_borderEnabled;
    data["borderColor"] = m_borderPen.color().name();
    data["borderWidth"] = m_borderPen.widthF();
    data["borderStyle"] = static_cast<int>(m_borderPen.style());
    data["backgroundEnabled"] = m_backgroundEnabled;
    data["alignment"] = static_cast<int>(m_alignment);
    data["wordWrap"] = m_wordWrap;
    data["autoResize"] = m_autoResize;
    return data;
}

void TextElement::setData(const QJsonObject& data) {
    m_text = data["text"].toString();
    
    // 恢复字体
    QFont font;
    font.setFamily(data["fontFamily"].toString("Arial"));
    font.setPointSize(data["fontSize"].toInt(12));
    font.setBold(data["fontBold"].toBool(false));
    font.setItalic(data["fontItalic"].toBool(false));
    font.setUnderline(data["fontUnderline"].toBool(false));
    m_font = font;
    
    m_textColor = QColor(data["textColor"].toString("#000000"));
    m_backgroundColor = QColor(data["backgroundColor"].toString("#FFFFFF"));
    m_size = QSizeF(data["width"].toDouble(100), data["height"].toDouble(30));
    m_borderEnabled = data["borderEnabled"].toBool(false);
    
    // 恢复边框画笔
    QPen pen;
    pen.setColor(QColor(data["borderColor"].toString("#000000")));
    pen.setWidthF(data["borderWidth"].toDouble(1.0));
    pen.setStyle(static_cast<Qt::PenStyle>(data["borderStyle"].toInt(Qt::SolidLine)));
    m_borderPen = pen;
    
    m_backgroundEnabled = data["backgroundEnabled"].toBool(false);
    m_alignment = static_cast<Qt::Alignment>(data["alignment"].toInt(Qt::AlignLeft | Qt::AlignVCenter));
    m_wordWrap = data["wordWrap"].toBool(false);
    m_autoResize = data["autoResize"].toBool(true);

    // 如果已有图形项，同步数据到图形项
    if (m_item) {
        syncToItem();
    }
}

void TextElement::setPos(const QPointF& pos) {
    if (m_item) {
        m_item->setPos(pos);
    }
}

QPointF TextElement::getPos() const {
    if (m_item) {
        return m_item->pos();
    }
    return QPointF(0, 0);
}

QString TextElement::getText() const {
    return m_text;
}

void TextElement::setText(const QString& text) {
    m_text = text;
    if (m_item) {
        m_item->setText(text);
    }
}

QFont TextElement::getFont() const {
    return m_font;
}

void TextElement::setFont(const QFont& font) {
    m_font = font;
    if (m_item) {
        m_item->setFont(font);
    }
}

QColor TextElement::getTextColor() const {
    return m_textColor;
}

void TextElement::setTextColor(const QColor& color) {
    m_textColor = color;
    if (m_item) {
        m_item->setTextColor(color);
    }
}

QColor TextElement::getBackgroundColor() const {
    return m_backgroundColor;
}

void TextElement::setBackgroundColor(const QColor& color) {
    m_backgroundColor = color;
    if (m_item) {
        m_item->setBackgroundColor(color);
    }
}

QSizeF TextElement::getSize() const {
    return m_size;
}

void TextElement::setSize(const QSizeF& size) {
    m_size = size;
    if (m_item) {
        m_item->setSize(size);
    }
}

Qt::Alignment TextElement::getAlignment() const {
    return m_alignment;
}

void TextElement::setAlignment(Qt::Alignment alignment) {
    m_alignment = alignment;
    if (m_item) {
        m_item->setAlignment(alignment);
    }
}

QPen TextElement::getBorderPen() const {
    return m_borderPen;
}

void TextElement::setBorderPen(const QPen& pen) {
    m_borderPen = pen;
    if (m_item) {
        m_item->setBorderPen(pen);
    }
}

bool TextElement::isBorderEnabled() const {
    return m_borderEnabled;
}

void TextElement::setBorderEnabled(bool enabled) {
    m_borderEnabled = enabled;
    if (m_item) {
        m_item->setBorderEnabled(enabled);
    }
}

bool TextElement::isBackgroundEnabled() const {
    return m_backgroundEnabled;
}

void TextElement::setBackgroundEnabled(bool enabled) {
    m_backgroundEnabled = enabled;
    if (m_item) {
        m_item->setBackgroundEnabled(enabled);
    }
}

bool TextElement::getWordWrap() const {
    return m_wordWrap;
}

void TextElement::setWordWrap(bool wrap) {
    m_wordWrap = wrap;
    if (m_item) {
        m_item->setWordWrap(wrap);
    }
}

bool TextElement::getAutoResize() const {
    return m_autoResize;
}

void TextElement::setAutoResize(bool autoResize) {
    m_autoResize = autoResize;
    if (m_item) {
        m_item->setAutoResize(autoResize);
    }
}

void TextElement::addToScene(QGraphicsScene* scene) {
    if (!scene) return;

    // 如果图形项还不存在，创建一个新的
    if (!m_item) {
        m_item = new TextItem(m_text);
        syncToItem();
    }

    scene->addItem(m_item);
}

bool TextElement::applyDataSourceRecord(int index)
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

void TextElement::restoreOriginalData()
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

void TextElement::syncFromItem() {
    if (!m_item) return;

    m_text = m_item->text();
    m_font = m_item->font();
    m_textColor = m_item->textColor();
    m_backgroundColor = m_item->backgroundColor();
    m_borderPen = m_item->borderPen();
    m_borderEnabled = m_item->isBorderEnabled();
    m_backgroundEnabled = m_item->isBackgroundEnabled();
    m_size = m_item->size();
    m_alignment = m_item->alignment();
    m_wordWrap = m_item->wordWrap();
    m_autoResize = m_item->autoResize();
}

void TextElement::syncToItem() {
    if (!m_item) return;

    m_item->setText(m_text);
    m_item->setFont(m_font);
    m_item->setTextColor(m_textColor);
    m_item->setBackgroundColor(m_backgroundColor);
    m_item->setBorderPen(m_borderPen);
    m_item->setBorderEnabled(m_borderEnabled);
    m_item->setBackgroundEnabled(m_backgroundEnabled);
    m_item->setSize(m_size);
    m_item->setAlignment(m_alignment);
    m_item->setWordWrap(m_wordWrap);
    m_item->setAutoResize(m_autoResize);
}
