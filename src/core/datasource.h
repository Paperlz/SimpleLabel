#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <memory>

/**
 * @brief 数据源抽象基类
 * 
 * 为批量打印提供统一的数据访问接口，屏蔽底层数据来源差异
 */
class DataSource
{
public:
    enum Type {
        Batch,  // 批量导入（文本）
        Table,  // 表格导入（Excel）
        MySql   // MySQL 查询
    };

    virtual ~DataSource() = default;

    /**
     * @brief 获取数据源类型
     */
    virtual Type type() const = 0;

    /**
     * @brief 获取数据总条数
     */
    virtual int count() const = 0;

    /**
     * @brief 按索引获取单条数据
     * @param index 数据索引（0-based）
     * @return 数据字符串，如果索引越界返回空字符串
     */
    virtual QString at(int index) const = 0;

    /**
     * @brief 检查数据源是否有效
     */
    virtual bool isValid() const = 0;

    /**
     * @brief 获取错误信息
     */
    virtual QString errorString() const = 0;

    /**
     * @brief 序列化为JSON
     */
    virtual QJsonObject toJson() const = 0;

    /**
     * @brief 从JSON反序列化创建数据源
     */
    static std::shared_ptr<DataSource> fromJson(const QJsonObject& json);
};

#endif // DATASOURCE_H
