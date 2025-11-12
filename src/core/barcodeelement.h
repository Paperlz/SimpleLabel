#ifndef BARCODEELEMENT_H
#define BARCODEELEMENT_H

#include "labelelement.h"
#include "../graphics/barcodeitem.h"
#include <QJsonObject>
#include <QPointF>
#include <memory>
#include "MultiFormatWriter.h"  // ZXing库头文件

class BarcodeElement : public labelelement {
public:
    // 构造函数
    BarcodeElement() = default;
    explicit BarcodeElement(BarcodeItem* item);
    explicit BarcodeElement(const QString& data,
                          ZXing::BarcodeFormat format = ZXing::BarcodeFormat::Code128,
                          const QSizeF& size = QSizeF(300, 100),
                          const QColor& foreground = Qt::black,
                          const QColor& background = Qt::white);

    // 实现基类的虚函数
    QGraphicsItem* getItem() const override;
    QString getType() const override;
    QJsonObject getData() const override;
    void setData(const QJsonObject& data) override;
    void setPos(const QPointF& pos) override;
    QPointF getPos() const override;

    // BarcodeElement 特有的方法
    QString getBarcodeData() const ;
    void setBarcodeData(const QString& data);

    ZXing::BarcodeFormat getFormat() const;
    void setFormat(ZXing::BarcodeFormat format);

    QSizeF getSize() const;
    void setSize(const QSizeF& size);

    QColor getForegroundColor() const;
    void setForegroundColor(const QColor& color);

    QColor getBackgroundColor() const;
    void setBackgroundColor(const QColor& color);

    bool getShowHumanReadableText() const;
    void setShowHumanReadableText(bool show);

    QFont getHumanReadableTextFont() const;
    void setHumanReadableTextFont(const QFont& font);

    // 数据源相关
    bool applyDataSourceRecord(int index);
    void restoreOriginalData();

    // 创建 BarcodeItem 并添加到场景
    void addToScene(QGraphicsScene* scene);

private:
    BarcodeItem* m_item = nullptr;
    QString m_data;
    ZXing::BarcodeFormat m_format = ZXing::BarcodeFormat::Code128;
    QSizeF m_size;
    QColor m_foregroundColor = Qt::black;
    QColor m_backgroundColor = Qt::white;
    bool m_showHumanReadableText = true;
    QFont m_humanReadableTextFont = QFont("Arial", 10);

    QString m_originalData;
    bool m_hasOriginalData = false;
};

#endif // BARCODEELEMENT_H