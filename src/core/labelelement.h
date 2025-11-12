#ifndef LABELELEMENT_H
#define LABELELEMENT_H
#include <QGraphicsItem>
#include <QJsonObject>
#include <memory>

class QRCodeElement;
class DataSource;

class labelelement {
public:
    labelelement() = default;
    virtual ~labelelement() = default;

    // 获取图形项
    virtual QGraphicsItem* getItem() const = 0;

    // 获取图形项类型
    virtual QString getType() const = 0;

    // 获取图形项数据
    virtual QJsonObject getData() const = 0;

    // 设置图形项数据
    virtual void setData(const QJsonObject& data) = 0;

    // 设置图形项位置
    virtual void setPos(const QPointF& pos) = 0;

    // 获取图形项位置
    virtual QPointF getPos() const = 0;

    // 序列化整个元素到JSON对象（包括位置和类型信息）
    QJsonObject toJson() const;

    virtual void addToScene(QGraphicsScene* scene) = 0;

    // 工厂方法：根据类型创建元素
    static std::unique_ptr<labelelement> createFromType(const QString& type);

    // 工厂方法：从JSON创建元素
    static std::unique_ptr<labelelement> createFromJson(const QJsonObject& json);

    // 数据源绑定
    void setDataSource(const std::shared_ptr<DataSource>& source);
    std::shared_ptr<DataSource> dataSource() const { return m_dataSource; }
    bool hasDataSource() const { return static_cast<bool>(m_dataSource); }
    void clearDataSource();

    void setDataSourceEnabled(bool enabled);
    bool isDataSourceEnabled() const { return m_useDataSource && m_dataSource != nullptr; }

protected:
    std::shared_ptr<DataSource> m_dataSource;
    bool m_useDataSource = false;
};



#endif //LABELELEMENT_H
