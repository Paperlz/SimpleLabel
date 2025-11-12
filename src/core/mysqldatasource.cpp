#include "mysqldatasource.h"

#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlRecord>
#include <QtCore/QUuid>
#include <QtCore/QVariant>
#include <QtCore/QtGlobal>

namespace {
constexpr int kDefaultPort = 3306;
constexpr int kPreviewLimit = 200;
}

MySqlDataSource::MySqlDataSource()
    : m_host(QStringLiteral("localhost"))
    , m_port(kDefaultPort)
{
}

void MySqlDataSource::setHost(const QString &host)
{
    m_host = host.trimmed();
}

void MySqlDataSource::setPort(int port)
{
    if (port <= 0 || port > 65535) {
        m_port = kDefaultPort;
    } else {
        m_port = port;
    }
}

void MySqlDataSource::setDatabase(const QString &database)
{
    m_database = database.trimmed();
}

void MySqlDataSource::setUsername(const QString &username)
{
    m_username = username;
}

void MySqlDataSource::setPassword(const QString &password)
{
    m_password = password;
}

void MySqlDataSource::setTableName(const QString &tableName)
{
    m_tableName = tableName.trimmed();
}

void MySqlDataSource::setColumnName(const QString &columnName)
{
    m_columnName = columnName.trimmed();
}

void MySqlDataSource::setFilter(const QString &filter)
{
    m_filter = filter.trimmed();
}

QStringList MySqlDataSource::columnsForTable(const QString &tableName) const
{
    return m_tableColumns.value(tableName);
}

bool MySqlDataSource::testConnection()
{
    return withConnection([](QSqlDatabase &, QString &) {
        return true;
    });
}

bool MySqlDataSource::loadTables()
{
    return withConnection([this](QSqlDatabase &db, QString &) {
        QStringList tables = db.tables(QSql::AllTables);
        tables.sort(Qt::CaseInsensitive);
        m_tableNames = tables;
        m_tableColumns.clear();
        return true;
    });
}

bool MySqlDataSource::loadColumns(const QString &tableName)
{
    const QString trimmedTable = tableName.trimmed();
    if (trimmedTable.isEmpty()) {
        m_errorString = QStringLiteral("表名不能为空");
        return false;
    }

    return withConnection([this, trimmedTable](QSqlDatabase &db, QString &opError) {
        QStringList columns;
        QSqlRecord record = db.record(trimmedTable);
        if (!record.isEmpty()) {
            const int fieldCount = record.count();
            for (int i = 0; i < fieldCount; ++i) {
                columns.append(record.fieldName(i));
            }
        } else {
            QSqlQuery query(db);
            const QString sql = QStringLiteral("SHOW COLUMNS FROM %1").arg(escapeIdentifier(trimmedTable));
            if (!query.exec(sql)) {
                opError = query.lastError().text();
                return false;
            }
            while (query.next()) {
                columns.append(query.value(0).toString());
            }
        }

        if (columns.isEmpty()) {
            opError = QStringLiteral("未能获取表 %1 的列信息").arg(trimmedTable);
            return false;
        }

        columns.removeDuplicates();
        columns.sort(Qt::CaseInsensitive);
        m_tableColumns.insert(trimmedTable, columns);
        return true;
    });
}

bool MySqlDataSource::refresh()
{
    m_dataList.clear();
    m_previewCache.clear();

    if (m_tableName.isEmpty()) {
        m_errorString = QStringLiteral("未指定数据表");
        return false;
    }

    if (m_columnName.isEmpty()) {
        m_errorString = QStringLiteral("未指定列名");
        return false;
    }

    const QString table = m_tableName;
    const QString column = m_columnName;
    const QString filter = m_filter;

    return withConnection([this, table, column, filter](QSqlDatabase &db, QString &opError) {
        QString sql = QStringLiteral("SELECT %1 FROM %2")
                          .arg(escapeIdentifier(column), escapeIdentifier(table));
        if (!filter.isEmpty()) {
            sql.append(QStringLiteral(" WHERE ")).append(filter);
        }

        QSqlQuery query(db);
        if (!query.exec(sql)) {
            opError = query.lastError().text();
            return false;
        }

        int rowIndex = 0;
        while (query.next()) {
            QString value = query.value(0).toString();
            if (value.isNull()) {
                value.clear();
            }
            value = value.trimmed();
            m_dataList.append(value);
            if (rowIndex < kPreviewLimit) {
                m_previewCache.append(QStringLiteral("%1\t%2")
                                          .arg(rowIndex + 1)
                                          .arg(value));
            }
            ++rowIndex;
        }

        if (m_dataList.isEmpty()) {
            opError = QStringLiteral("查询结果为空");
            return false;
        }

        return true;
    });
}

QStringList MySqlDataSource::preview(int maxRows) const
{
    if (maxRows <= 0 || m_previewCache.isEmpty()) {
        return QStringList();
    }
    const int limit = qMin(maxRows, m_previewCache.size());
    return m_previewCache.mid(0, limit);
}

int MySqlDataSource::count() const
{
    return m_dataList.count();
}

QString MySqlDataSource::at(int index) const
{
    if (index >= 0 && index < m_dataList.count()) {
        return m_dataList.at(index);
    }
    return QString();
}

bool MySqlDataSource::isValid() const
{
    return !m_dataList.isEmpty();
}

QString MySqlDataSource::errorString() const
{
    return m_errorString;
}

QJsonObject MySqlDataSource::toJson() const
{
    QJsonObject json;
    json["type"] = "mysql";
    json["host"] = m_host;
    json["port"] = m_port;
    json["database"] = m_database;
    json["username"] = m_username;
    json["password"] = m_password;
    json["table"] = m_tableName;
    json["column"] = m_columnName;
    json["filter"] = m_filter;
    return json;
}

bool MySqlDataSource::withConnection(const std::function<bool(QSqlDatabase &, QString &)> &operation)
{
    m_errorString.clear();

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QMYSQL"))) {
        m_errorString = QStringLiteral("Qt 未启用 MySQL 驱动 (QMYSQL)");
        return false;
    }

    if (m_database.trimmed().isEmpty()) {
        m_errorString = QStringLiteral("未指定数据库名称");
        return false;
    }

    const QString connectionName = QStringLiteral("SimpleLabelMySQL_%1")
                                        .arg(QUuid::createUuid().toString(QUuid::Id128));

    bool result = false;
    QString opError;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), connectionName);
        db.setHostName(m_host);
        db.setPort(m_port);
        db.setDatabaseName(m_database);
        db.setUserName(m_username);
        db.setPassword(m_password);

        if (!db.open()) {
            m_errorString = db.lastError().text();
        } else {
            result = operation(db, opError);
            if (!result && !opError.isEmpty()) {
                m_errorString = opError;
            }
        }
    }

    QSqlDatabase::removeDatabase(connectionName);

    if (result) {
        m_errorString.clear();
    } else if (m_errorString.isEmpty()) {
        m_errorString = QStringLiteral("MySQL 操作失败");
    }

    return result;
}

QString MySqlDataSource::escapeIdentifier(const QString &identifier)
{
    QString escaped = identifier;
    escaped.replace('`', "``");
    return QStringLiteral("`%1`").arg(escaped);
}
