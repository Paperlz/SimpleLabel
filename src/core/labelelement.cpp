#include "labelelement.h"

#include "qrcodeelement.h"
#include "barcodeelement.h"
#include "textelement.h"
#include "imageelement.h"
#include "lineelement.h"
#include "rectangleelement.h"
#include "circleelement.h"
#include "starelement.h"
#include "arrowelement.h"
#include "polygonelement.h"
#include "tableelement.h"
#include "datasource.h"

// 由于 labelelement 是一个抽象类，主要包含纯虚函数，
// 实现文件可以添加一些辅助函数或默认实现

// 可以添加一些静态工厂方法来创建不同类型的元素
std::unique_ptr<labelelement> labelelement::createFromType(const QString& type) {
    // 支持二维码和条形码
    const QString normalized = type.trimmed();
    const QString lower = normalized.toLower();

    if (lower == "qrcode") {
        return std::make_unique<QRCodeElement>();
    }
    if (lower == "barcode") {
        return std::make_unique<BarcodeElement>();
    }
    if (lower == "text") {
        return std::make_unique<TextElement>();
    }
    if (lower == "image") {
        return std::make_unique<ImageElement>();
    }
    if (lower == "line") {
        return std::make_unique<LineElement>();
    }
    if (lower == "rectangle") {
        return std::make_unique<RectangleElement>();
    }
    if (lower == "circle") {
        return std::make_unique<CircleElement>();
    }
    if (lower == "ellipse") {
        auto element = std::make_unique<CircleElement>();
        element->setIsCircle(false);
        return element;
    }
    if (lower == "star") {
        return std::make_unique<StarElement>();
    }
    if (lower == "arrow") {
        return std::make_unique<ArrowElement>();
    }
    if (lower == "polygon") {
        return std::make_unique<PolygonElement>();
    }
    if (lower == "table") {
        return std::make_unique<TableElement>();
    }

    // 如果类型未识别，返回nullptr
    return nullptr;
}

// 从JSON对象创建元素
std::unique_ptr<labelelement> labelelement::createFromJson(const QJsonObject& json) {
    QString type = json["itemType"].toString();
    auto element = createFromType(type);

    if (element) {
        element->setData(json);

        if (json.contains("dataSource") && json["dataSource"].isObject()) {
            const QJsonObject dsJson = json.value("dataSource").toObject();
            const bool enabled = dsJson.value("enabled").toBool(false);
            auto source = DataSource::fromJson(dsJson);
            if (source) {
                element->setDataSource(source);
                element->setDataSourceEnabled(enabled);
            } else {
                element->setDataSourceEnabled(false);
            }
        }
    }

    return element;
}

// 序列化完整元素（包括位置信息等）到JSON
QJsonObject labelelement::toJson() const {
    QJsonObject json;

    // 添加元素类型
    json["itemType"] = getType();

    // 添加位置信息
    QPointF pos = getPos();
    json["x"] = pos.x();
    json["y"] = pos.y();

    // 获取并添加元素特定数据
    QJsonObject elementData = getData();
    for (auto it = elementData.begin(); it != elementData.end(); ++it) {
        json[it.key()] = it.value();
    }

    if (m_dataSource) {
        QJsonObject dsJson = m_dataSource->toJson();
        dsJson["enabled"] = m_useDataSource;
        json["dataSource"] = dsJson;
    } else if (m_useDataSource) {
        QJsonObject dsJson;
        dsJson["enabled"] = false;
        json["dataSource"] = dsJson;
    }

    return json;
}

void labelelement::setDataSource(const std::shared_ptr<DataSource>& source)
{
    m_dataSource = source;
    if (!m_dataSource) {
        m_useDataSource = false;
    }
}

void labelelement::clearDataSource()
{
    m_dataSource.reset();
    m_useDataSource = false;
}

void labelelement::setDataSourceEnabled(bool enabled)
{
    if (!m_dataSource) {
        m_useDataSource = false;
        return;
    }
    m_useDataSource = enabled;
}
