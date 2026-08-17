#include "storage/ServiceResourceRepository.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QString normalizedText(const QString& value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

QString originToString(ResourceOrigin origin)
{
    return origin == ResourceOrigin::Service ? QStringLiteral("service")
                                             : QStringLiteral("local");
}

ResourceOrigin originFromString(const QString& origin)
{
    return origin.compare(QStringLiteral("service"), Qt::CaseInsensitive) == 0
               ? ResourceOrigin::Service
               : ResourceOrigin::Local;
}
}

ServiceResourceRepository::ServiceResourceRepository(QString connectionName)
    : m_connectionName(connectionName)
{
}

bool ServiceResourceRepository::replaceResources(const QVector<ResourceReference>& resources) const
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid() || !database.transaction()) {
        qWarning() << "ServiceResourceRepository transaction failed" << m_connectionName;
        return false;
    }

    QSqlQuery deleteQuery(database);
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM service_resources"))) {
        qWarning() << "ServiceResourceRepository delete failed" << deleteQuery.lastError().text();
        database.rollback();
        return false;
    }

    for (const ResourceReference& resource : resources) {
        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(R"(
            INSERT INTO service_resources
            (resource_id, service_id, workspace_id, resource_kind, title, version, summary, origin,
             raw_payload_json, updated_at_ms)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )"));
        insertQuery.addBindValue(normalizedText(resource.resourceId));
        insertQuery.addBindValue(normalizedText(resource.serviceId));
        insertQuery.addBindValue(normalizedText(resource.workspaceId));
        insertQuery.addBindValue(normalizedText(resource.resourceKind));
        insertQuery.addBindValue(normalizedText(resource.title));
        insertQuery.addBindValue(normalizedText(resource.version));
        insertQuery.addBindValue(normalizedText(resource.summary));
        insertQuery.addBindValue(originToString(resource.origin));
        insertQuery.addBindValue(QStringLiteral("{}"));
        insertQuery.addBindValue(0);
        if (!insertQuery.exec()) {
            qWarning() << "ServiceResourceRepository insert failed"
                       << insertQuery.lastError().text()
                       << resource.resourceId
                       << resource.resourceKind;
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

bool ServiceResourceRepository::upsertResource(const ResourceReference& resource) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(R"(
        INSERT INTO service_resources
            (resource_id, service_id, workspace_id, resource_kind, title, version, summary, origin)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(resource_id) DO UPDATE SET
            title   = excluded.title,
            version = excluded.version,
            summary = excluded.summary
    )"));
    q.addBindValue(normalizedText(resource.resourceId));
    q.addBindValue(normalizedText(resource.serviceId));
    q.addBindValue(normalizedText(resource.workspaceId));
    q.addBindValue(normalizedText(resource.resourceKind));
    q.addBindValue(normalizedText(resource.title));
    q.addBindValue(normalizedText(resource.version));
    q.addBindValue(normalizedText(resource.summary));
    q.addBindValue(originToString(resource.origin));
    if (!q.exec()) {
        qWarning() << "ServiceResourceRepository::upsertResource failed"
                   << q.lastError().text()
                   << resource.resourceId;
        return false;
    }
    return true;
}

QVector<ResourceReference> ServiceResourceRepository::loadResources() const
{
    QVector<ResourceReference> resources;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT service_id, workspace_id, resource_id, resource_kind, title, version, summary, origin
        FROM service_resources
        ORDER BY resource_id ASC
    )"))) {
        return resources;
    }

    while (query.next()) {
        resources.push_back(ResourceReference{
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            query.value(4).toString(),
            query.value(5).toString(),
            query.value(6).toString(),
            originFromString(query.value(7).toString())
        });
    }

    return resources;
}
