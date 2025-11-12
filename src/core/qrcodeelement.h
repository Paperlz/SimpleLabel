#ifndef QRCODEELEMENT_H
#define QRCODEELEMENT_H

#include "labelelement.h"
#include "../graphics/qrcodeitem.h"
#include <QJsonObject>
#include <memory>

class QRCodeElement : public labelelement {
public:
    // 构造函数
    QRCodeElement() = default;
    explicit QRCodeElement(QRCodeItem* item);
    explicit QRCodeElement(const QString& text, const QSizeF& size = QSizeF(100, 100),
                           const QColor& foreground = Qt::black, const QColor& background = Qt::white);

    // 实现基类的虚函数
    QGraphicsItem* getItem() const override;
    QString getType() const override;
    QJsonObject getData() const override;
    void setData(const QJsonObject& data) override;
    void setPos(const QPointF& pos) override;
    QPointF getPos() const override;

    // QRCodeElement 特有的方法
    QString getText() const;
    void setText(const QString& text);

    QSizeF getSize() const;
    void setSize(const QSizeF& size);

    QColor getForegroundColor() const;
    void setForegroundColor(const QColor& color);

    QColor getBackgroundColor() const;
    void setBackgroundColor(const QColor& color);

    // 创建 QRCodeItem 并添加到场景
    void addToScene(QGraphicsScene* scene);

    bool applyDataSourceRecord(int index);
    void restoreOriginalData();

private:
    QRCodeItem* m_item = nullptr;
    QString m_text;
    QSizeF m_size;
    QColor m_foregroundColor = Qt::black;
    QColor m_backgroundColor = Qt::white;
    QString m_originalData;
    bool m_hasOriginalData = false;
};

#endif // QRCODEELEMENT_H