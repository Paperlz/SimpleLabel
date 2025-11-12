#ifndef TEXTELEMENT_H
#define TEXTELEMENT_H

#include "labelelement.h"
#include "../graphics/textitem.h"
#include <QString>
#include <QFont>
#include <QColor>
#include <QPen>
#include <QJsonObject>

/**
 * @brief 文本元素数据模型类
 * 
 * 负责文本标签的数据存储、序列化和与TextItem的交互。
 * 继承自labelelement，提供统一的数据管理接口。
 */
class TextElement : public labelelement
{
public:
    // 构造函数
    explicit TextElement(TextItem* item = nullptr);
    TextElement(const QString& text,
                const QFont& font = QFont("Arial", 12),
                const QColor& textColor = Qt::black,
                const QColor& backgroundColor = Qt::white,
                const QSizeF& size = QSizeF(100, 30));

    // 继承自LabelElement的虚函数
    QGraphicsItem* getItem() const override;
    QString getType() const override;
    QJsonObject getData() const override;
    void setData(const QJsonObject& data) override;
    void setPos(const QPointF& pos) override;
    QPointF getPos() const override;

    // 文本相关方法
    QString getText() const;
    void setText(const QString& text);

    // 字体相关方法
    QFont getFont() const;
    void setFont(const QFont& font);

    // 颜色相关方法
    QColor getTextColor() const;
    void setTextColor(const QColor& color);

    QColor getBackgroundColor() const;
    void setBackgroundColor(const QColor& color);

    // 尺寸相关方法
    QSizeF getSize() const;
    void setSize(const QSizeF& size);

    // 对齐相关方法
    Qt::Alignment getAlignment() const;
    void setAlignment(Qt::Alignment alignment);

    // 边框相关方法
    QPen getBorderPen() const;
    void setBorderPen(const QPen& pen);

    bool isBorderEnabled() const;
    void setBorderEnabled(bool enabled);

    // 背景相关方法
    bool isBackgroundEnabled() const;
    void setBackgroundEnabled(bool enabled);

    // 文本选项相关方法
    bool getWordWrap() const;
    void setWordWrap(bool wrap);

    bool getAutoResize() const;
    void setAutoResize(bool autoResize);    // 将元素添加到场景
    void addToScene(QGraphicsScene* scene) override;

    bool applyDataSourceRecord(int index);
    void restoreOriginalData();

private:
    // 私有成员变量
    TextItem* m_item;               // 关联的图形项
    QString m_text;                 // 文本内容
    QFont m_font;                   // 字体
    QColor m_textColor;             // 文本颜色
    QColor m_backgroundColor;       // 背景颜色
    QPen m_borderPen;               // 边框画笔
    bool m_borderEnabled;           // 是否启用边框
    bool m_backgroundEnabled;       // 是否启用背景
    QSizeF m_size;                  // 文本框尺寸
    Qt::Alignment m_alignment;      // 文本对齐方式
    bool m_wordWrap;                // 是否自动换行
    bool m_autoResize;              // 是否自动调整大小

    QString m_originalData;
    bool m_hasOriginalData = false;

    // 私有方法
    void syncFromItem();            // 从图形项同步数据
    void syncToItem();              // 将数据同步到图形项
};

#endif // TEXTELEMENT_H
