#include "batchdatasource.h"

// ============================================================================
// BatchDataSource 实现
// ============================================================================

BatchDataSource::BatchDataSource()
    : m_delimiter("\n")
{
}

void BatchDataSource::setText(const QString& text, const QString& delimiter)
{
    m_rawText = text;
    m_delimiter = delimiter.isEmpty() ? "\n" : delimiter;
    m_errorString.clear();
    m_dataList.clear();
}

void BatchDataSource::parse()
{
    m_dataList.clear();
    m_errorString.clear();

    if (m_rawText.isEmpty()) {
        m_errorString = QStringLiteral("文本内容为空");
        return;
    }

    // 根据分隔符拆分
    QStringList parts;
    if (m_delimiter == "\n" || m_delimiter == "\\n") {
        // 回车分隔
        parts = m_rawText.split('\n', Qt::SkipEmptyParts);
    }
    else if (m_delimiter == " ") {
        // 空格分隔
        parts = m_rawText.split(' ', Qt::SkipEmptyParts);
    }
    else {
        // 自定义分隔符
        parts = m_rawText.split(m_delimiter, Qt::SkipEmptyParts);
    }

    // 处理每个数据项：trim 前后空白
    for (const QString& part : parts) {
        QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            m_dataList.append(trimmed);
        }
    }

    if (m_dataList.isEmpty()) {
        m_errorString = QStringLiteral("未能解析出有效数据");
    }
}

int BatchDataSource::count() const
{
    return m_dataList.count();
}

QString BatchDataSource::at(int index) const
{
    if (index >= 0 && index < m_dataList.count()) {
        return m_dataList.at(index);
    }
    return QString();
}

bool BatchDataSource::isValid() const
{
    return !m_dataList.isEmpty();
}

QString BatchDataSource::errorString() const
{
    return m_errorString;
}

QJsonObject BatchDataSource::toJson() const
{
    QJsonObject json;
    json["type"] = "batch";
    json["text"] = m_rawText;
    json["delimiter"] = m_delimiter;
    return json;
}
