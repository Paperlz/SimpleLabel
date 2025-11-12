#ifndef MYSQLDATASOURCE_H
#define MYSQLDATASOURCE_H

#include "datasource.h"

#include <QHash>
#include <QStringList>
#include <functional>

class QSqlDatabase;

/**
 * @brief MySQL 数据源实现
 *
 * 通过 MySQL 查询提供批量打印所需的记录。
 */
class MySqlDataSource : public DataSource
{
public:
    MySqlDataSource();
    ~MySqlDataSource() override = default;

    Type type() const override { return MySql; }

    void setHost(const QString &host);
    QString host() const { return m_host; }

    void setPort(int port);
    int port() const { return m_port; }

    void setDatabase(const QString &database);
    QString database() const { return m_database; }

    void setUsername(const QString &username);
    QString username() const { return m_username; }

    void setPassword(const QString &password);
    QString password() const { return m_password; }

    void setTableName(const QString &tableName);
    QString tableName() const { return m_tableName; }

    void setColumnName(const QString &columnName);
    QString columnName() const { return m_columnName; }

    void setFilter(const QString &filter);
    QString filter() const { return m_filter; }

    bool testConnection();
    bool loadTables();
    bool loadColumns(const QString &tableName);

    QStringList tables() const { return m_tableNames; }
    QStringList columnsForTable(const QString &tableName) const;

    bool refresh();
    QStringList preview(int maxRows = 10) const;

    int count() const override;
    QString at(int index) const override;
    bool isValid() const override;
    QString errorString() const override;
    QJsonObject toJson() const override;

private:
    bool withConnection(const std::function<bool(QSqlDatabase &, QString &)> &operation);
    static QString escapeIdentifier(const QString &identifier);

    QString m_host;
    int m_port;
    QString m_database;
    QString m_username;
    QString m_password;
    QString m_tableName;
    QString m_columnName;
    QString m_filter;

    QStringList m_dataList;
    QStringList m_previewCache;
    QString m_errorString;

    QStringList m_tableNames;
    QHash<QString, QStringList> m_tableColumns;
};

#endif // MYSQLDATASOURCE_H
