#ifndef BATCHDATASOURCE_H
#define BATCHDATASOURCE_H

#include "datasource.h"

/**
 * @brief 批量导入数据源
 * 
 * 从用户输入的文本中按分隔符拆分数据
 */
class BatchDataSource : public DataSource
{
public:
    BatchDataSource();
    ~BatchDataSource() override = default;

    Type type() const override { return Batch; }

    /**
     * @brief 设置原始文本和分隔符
     * @param text 原始文本
     * @param delimiter 分隔符："\n"（回车）、" "（空格）或自定义字符
     */
    void setText(const QString& text, const QString& delimiter);

    /**
     * @brief 解析文本，拆分为数据列表
     */
    void parse();

    /**
     * @brief 获取原始文本
     */
    QString rawText() const { return m_rawText; }

    /**
     * @brief 获取分隔符
     */
    QString delimiter() const { return m_delimiter; }

    /**
     * @brief 获取解析后的数据列表
     */
    QStringList dataList() const { return m_dataList; }

    // DataSource interface
    int count() const override;
    QString at(int index) const override;
    bool isValid() const override;
    QString errorString() const override;
    QJsonObject toJson() const override;

private:
    QString m_rawText;
    QString m_delimiter;
    QStringList m_dataList;
    QString m_errorString;
};

#endif // BATCHDATASOURCE_H
