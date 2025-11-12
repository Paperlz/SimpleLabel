#include "tabledatasource.h"

#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QJsonObject>
#include <QtGlobal>
#include <QMetaType>
#include <algorithm>

#ifdef HAVE_QXLSX
#include <xlsxcellrange.h>
#include <xlsxdocument.h>
#include <xlsxworksheet.h>
#endif

// ============================================================================
// TableDataSource 实现
// ============================================================================

TableDataSource::TableDataSource()
    : m_headerRow(1)
    , m_startRow(2)
    , m_columnIndex(0)
    , m_endRow(0)
    , m_lastSheetRow(0)
    , m_columnCount(0)
{
}

bool TableDataSource::loadFile(const QString& filePath)
{
    m_filePath = filePath;
    m_errorString.clear();
    m_dataList.clear();
    m_sheetNames.clear();
    m_sheetName.clear();
    m_previewCache.clear();
    m_columnHeaders.clear();
    m_lastSheetRow = 0;
    m_columnCount = 0;
    m_endRow = 0;

    // 检查文件是否存在
    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists()) {
        m_errorString = QStringLiteral("文件不存在: ") + filePath;
        return false;
    }

    if (!fileInfo.isFile()) {
        m_errorString = QStringLiteral("路径不是文件: ") + filePath;
        return false;
    }

    // 检查文件扩展名
    QString suffix = fileInfo.suffix().toLower();
    if (suffix != "xlsx" && suffix != "xls") {
        m_errorString = QStringLiteral("不支持的文件格式: ") + suffix + QStringLiteral(" (仅支持 .xlsx 和 .xls)");
        return false;
    }

#ifdef HAVE_QXLSX
    QXlsx::Document document(filePath);
    const QStringList sheets = document.sheetNames();
    if (sheets.isEmpty()) {
        m_errorString = QStringLiteral("文件中未找到任何工作表");
        return false;
    }

    m_sheetNames = sheets;
    if (!m_sheetNames.contains(m_sheetName)) {
        m_sheetName = m_sheetNames.constFirst();
    }

    qDebug() << "TableDataSource: 成功加载Excel文件" << filePath;
    return true;
#else
    m_errorString = QStringLiteral("当前构建未启用QXlsx库，无法读取Excel文件");
    return false;
#endif
}

void TableDataSource::setSheet(const QString& sheetName)
{
    if (m_sheetNames.contains(sheetName)) {
        m_sheetName = sheetName;
        m_errorString.clear();
    } else {
        m_errorString = QStringLiteral("Sheet不存在: ") + sheetName;
    }
}

void TableDataSource::setHeaderRow(int row)
{
    m_headerRow = qMax(0, row);
}

void TableDataSource::setStartRow(int row)
{
    m_startRow = qMax(1, row);
}

void TableDataSource::setColumnIndex(int column)
{
    if (column < 0) {
        column = 0;
    }
    if (m_columnCount > 0 && column >= m_columnCount) {
        column = m_columnCount - 1;
    }
    m_columnIndex = column;
}

void TableDataSource::setEndRow(int row)
{
    if (row < 0) {
        row = 0;
    }
    m_endRow = row;
}

QStringList TableDataSource::preview(int maxRows) const
{
    QStringList result;
    int rows = qMin(maxRows, m_previewCache.count());
    for (int i = 0; i < rows; ++i) {
        result.append(m_previewCache.at(i));
    }
    return result;
}

bool TableDataSource::refresh()
{
    if (m_filePath.isEmpty()) {
        m_errorString = QStringLiteral("未指定文件路径");
        return false;
    }

    const bool ok = parseExcelFile();
    if (!ok && m_dataList.isEmpty()) {
        qDebug() << "TableDataSource: 刷新失败 ->" << m_errorString;
    }
    return ok;
}

int TableDataSource::count() const
{
    return m_dataList.count();
}

QString TableDataSource::at(int index) const
{
    if (index >= 0 && index < m_dataList.count()) {
        return m_dataList.at(index);
    }
    return QString();
}

bool TableDataSource::isValid() const
{
    return !m_filePath.isEmpty() &&
           !m_sheetName.isEmpty() &&
           QFileInfo::exists(m_filePath) &&
           !m_dataList.isEmpty();
}

QString TableDataSource::errorString() const
{
    return m_errorString;
}

QJsonObject TableDataSource::toJson() const
{
    QJsonObject json;
    json["type"] = "table";
    json["filePath"] = m_filePath;
    json["sheet"] = m_sheetName;
    json["headerRow"] = m_headerRow;
    json["startRow"] = m_startRow;
    json["endRow"] = m_endRow;
    json["columnIndex"] = m_columnIndex;
    return json;
}

bool TableDataSource::parseExcelFile()
{
    m_dataList.clear();
    m_previewCache.clear();
    m_errorString.clear();
    m_lastSheetRow = 0;
    m_columnCount = 0;
    m_columnHeaders.clear();

    if (m_filePath.isEmpty()) {
        m_errorString = QStringLiteral("文件路径为空");
        return false;
    }

#ifdef HAVE_QXLSX
    QXlsx::Document document(m_filePath);
    m_sheetNames = document.sheetNames();

    if (m_sheetName.isEmpty()) {
        if (m_sheetNames.isEmpty()) {
            m_errorString = QStringLiteral("Excel 文件中未找到工作表");
            return false;
        }
        m_sheetName = m_sheetNames.constFirst();
    }

    if (!document.selectSheet(m_sheetName)) {
        m_errorString = QStringLiteral("无法打开Sheet: ") + m_sheetName;
        return false;
    }

    QXlsx::Worksheet *worksheet = document.currentWorksheet();
    if (!worksheet) {
        m_errorString = QStringLiteral("工作表无效: ") + m_sheetName;
        return false;
    }

    const QXlsx::CellRange range = worksheet->dimension();
    if (!range.isValid()) {
        m_errorString = QStringLiteral("工作表不包含可读取的数据");
        return false;
    }

    m_lastSheetRow = range.lastRow();
    m_columnCount = range.lastColumn();

    if (m_lastSheetRow <= 0 || m_columnCount <= 0) {
        m_errorString = QStringLiteral("工作表不包含可读取的数据");
        return false;
    }

    if (m_headerRow < 0) {
        m_headerRow = 0;
    }
    if (m_headerRow > m_lastSheetRow) {
        m_headerRow = m_lastSheetRow;
    }

    if (m_startRow < 1) {
        m_startRow = 1;
    }
    if (m_headerRow > 0 && m_startRow <= m_headerRow) {
        m_startRow = m_headerRow + 1;
    }
    if (m_startRow > m_lastSheetRow) {
        m_startRow = m_lastSheetRow;
    }

    if (m_endRow < 0) {
        m_endRow = 0;
    }
    int effectiveEndRow = (m_endRow == 0) ? m_lastSheetRow : qMin(m_endRow, m_lastSheetRow);
    if (effectiveEndRow < m_startRow) {
        effectiveEndRow = m_startRow;
    }

    // 尝试从表头及部分数据行推断更大的列范围，以处理某些工作表维度信息不完整的情况
    constexpr int kMaxColumnScan = 256;
    constexpr int kSampleRowLimit = 32;
    int detectedMaxColumn = m_columnCount;
    auto scanRowForColumns = [&](int row) {
        if (row <= 0 || row > m_lastSheetRow) {
            return;
        }
        for (int col = 1; col <= kMaxColumnScan; ++col) {
            const QVariant value = document.read(row, col);
            if (!value.isValid() || value.isNull()) {
                continue;
            }

            bool hasContent = true;
            if (value.metaType().id() == QMetaType::QString) {
                hasContent = !value.toString().trimmed().isEmpty();
            }

            if (hasContent) {
                detectedMaxColumn = qMax(detectedMaxColumn, col);
            }
        }
    };

    if (m_headerRow > 0) {
        scanRowForColumns(m_headerRow);
    }

    const int sampleEndRow = qMin(effectiveEndRow, qMin(m_lastSheetRow, m_startRow + kSampleRowLimit - 1));
    for (int row = m_startRow; row <= sampleEndRow; ++row) {
        scanRowForColumns(row);
    }

    if (effectiveEndRow > sampleEndRow) {
        scanRowForColumns(effectiveEndRow);
    }

    if (detectedMaxColumn > m_columnCount) {
        m_columnCount = detectedMaxColumn;
    }

    if (m_columnCount <= 0) {
        m_errorString = QStringLiteral("工作表不包含可读取的数据");
        return false;
    }

    if (m_columnIndex < 0) {
        m_columnIndex = 0;
    }
    if (m_columnIndex >= m_columnCount) {
        m_columnIndex = m_columnCount - 1;
    }

    // 生成列标题
    m_columnHeaders.clear();
    for (int col = 1; col <= m_columnCount; ++col) {
        QString headerText;
        if (m_headerRow > 0) {
            const QVariant headerValue = document.read(m_headerRow, col);
            headerText = headerValue.toString().trimmed();
        }
        if (headerText.isEmpty()) {
            headerText = columnNameForIndex(col - 1);
        }
        m_columnHeaders.append(headerText);
    }

    for (int row = m_startRow; row <= effectiveEndRow; ++row) {
        const QVariant cellValue = document.read(row, m_columnIndex + 1);
        QString value = cellValue.toString();
        if (value.isNull()) {
            value.clear();
        }
        value = value.trimmed();
        m_dataList.append(value);
        m_previewCache.append(QStringLiteral("%1 %2").arg(row).arg(value));
    }

    if (m_dataList.isEmpty()) {
        m_errorString = QStringLiteral("所选范围内没有可用数据");
        return false;
    }

    return true;
#else
    m_errorString = QStringLiteral("当前构建未启用QXlsx库");
    return false;
#endif
}

QString TableDataSource::columnNameForIndex(int index)
{
    QString name;
    int value = index;
    while (value >= 0) {
        const int remainder = value % 26;
        name.prepend(QChar('A' + remainder));
        value = (value / 26) - 1;
    }
    return name;
}
