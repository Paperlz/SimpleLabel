#ifndef TABLEDATASOURCE_H
#define TABLEDATASOURCE_H

#include "datasource.h"

#include <QStringList>

/**
 * @brief 表格数据源
 * 
 * 从Excel文件中读取数据
 */
class TableDataSource : public DataSource
{
public:
    TableDataSource();
    ~TableDataSource() override = default;

    Type type() const override { return Table; }

    /**
     * @brief 加载Excel文件
     * @param filePath 文件路径
     * @return 加载成功返回true
     */
    bool loadFile(const QString& filePath);

    /**
     * @brief 获取文件路径
     */
    QString filePath() const { return m_filePath; }

    /**
     * @brief 获取所有sheet名称列表
     */
    QStringList sheetNames() const { return m_sheetNames; }

    /**
     * @brief 设置要使用的sheet
     * @param sheetName sheet名称
     */
    void setSheet(const QString& sheetName);

    /**
     * @brief 获取当前sheet名称
     */
    QString currentSheet() const { return m_sheetName; }

    /**
     * @brief 设置表头所在行（1-based）
     * @param row 表头行号，0表示无表头
     */
    void setHeaderRow(int row);

    /**
     * @brief 获取表头行号
     */
    int headerRow() const { return m_headerRow; }

    /**
     * @brief 设置数据起始行（1-based）
     * @param row 起始行号
     */
    void setStartRow(int row);

    /**
     * @brief 获取数据起始行号
     */
    int startRow() const { return m_startRow; }

    /**
     * @brief 设置要读取的列索引（0-based）
     * @param column 列索引
     */
    void setColumnIndex(int column);

    /**
     * @brief 获取列索引
     */
    int columnIndex() const { return m_columnIndex; }

    /**
     * @brief 设置数据结束行（1-based，0表示自动探测最后一行）
     */
    void setEndRow(int row);

    /**
     * @brief 获取数据结束行
     */
    int endRow() const { return m_endRow; }

    /**
     * @brief 获取数据预览（前N行）
     * @param maxRows 最大行数
     * @return 预览数据列表
     */
    QStringList preview(int maxRows = 10) const;

    /**
     * @brief 刷新数据（重新从文件读取）
     */
    bool refresh();

    // DataSource interface
    int count() const override;
    QString at(int index) const override;
    bool isValid() const override;
    QString errorString() const override;
    QJsonObject toJson() const override;

    // 元数据访问
    int lastSheetRow() const { return m_lastSheetRow; }
    int columnCount() const { return m_columnCount; }
    QStringList columnHeaders() const { return m_columnHeaders; }

private:
    QString m_filePath;
    QStringList m_sheetNames;
    QString m_sheetName;
    int m_headerRow;      // 1-based, 0表示无表头
    int m_startRow;       // 1-based
    int m_columnIndex;    // 0-based
    int m_endRow;         // 1-based, 0表示读取至最后一行
    QStringList m_dataList;
    QStringList m_previewCache;
    QString m_errorString;
    int m_lastSheetRow;   // 当前sheet的最大行数
    int m_columnCount;    // 当前sheet的列数
    QStringList m_columnHeaders;

    bool parseExcelFile();
    static QString columnNameForIndex(int index);
};

#endif // TABLEDATASOURCE_H
