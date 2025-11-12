#include "datasource.h"
#include "batchdatasource.h"
#include "tabledatasource.h"
#include "mysqldatasource.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

// ============================================================================
// DataSource 静态方法实现
// ============================================================================

std::shared_ptr<DataSource> DataSource::fromJson(const QJsonObject& json)
{
    if (!json.contains("type")) {
        return nullptr;
    }

    QString typeStr = json["type"].toString();
    
    if (typeStr == "batch") {
        auto source = std::make_shared<BatchDataSource>();
        QString text = json.value("text").toString();
        QString delimiter = json.value("delimiter").toString("\n");
        source->setText(text, delimiter);
        source->parse();
        return source;
    }
    else if (typeStr == "table") {
        auto source = std::make_shared<TableDataSource>();
        QString filePath = json.value("filePath").toString();
        
        if (!filePath.isEmpty() && source->loadFile(filePath)) {
            QString sheetName = json.value("sheet").toString();
            if (!sheetName.isEmpty()) {
                source->setSheet(sheetName);
            }
            
            source->setHeaderRow(json.value("headerRow").toInt(1));
            source->setStartRow(json.value("startRow").toInt(2));
            source->setEndRow(json.value("endRow").toInt(0));
            source->setColumnIndex(json.value("columnIndex").toInt(0));
            source->refresh();
        }
        
        return source;
    }
    else if (typeStr == "mysql") {
        auto source = std::make_shared<MySqlDataSource>();
        source->setHost(json.value("host").toString(QStringLiteral("localhost")));
        source->setPort(json.value("port").toInt(3306));
        source->setDatabase(json.value("database").toString());
        source->setUsername(json.value("username").toString());
        source->setPassword(json.value("password").toString());
        source->setTableName(json.value("table").toString());
        source->setColumnName(json.value("column").toString());
        source->setFilter(json.value("filter").toString());

        if (!source->tableName().isEmpty() && !source->columnName().isEmpty()) {
            source->refresh();
        }

        return source;
    }

    return nullptr;
}

