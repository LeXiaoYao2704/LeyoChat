#include "storage/ServiceRegistryRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>

namespace {
QString normalizedText(const QString& value)
{
    return value.isNull() ? QStringLiteral("") : value;
}

QJsonArray capabilitiesToJson(const QVector<ServiceCapability>& capabilities)
{
    QJsonArray array;
    for (const ServiceCapability& capability : capabilities) {
        QJsonObject object;
        object.insert(QStringLiteral("capabilityId"), capability.capabilityId);
        object.insert(QStringLiteral("capabilityName"), capability.capabilityName);
        object.insert(QStringLiteral("version"), capability.version);
        object.insert(QStringLiteral("enabled"), capability.enabled);
        array.push_back(object);
    }
    return array;
}

QVector<ServiceCapability> capabilitiesFromJson(const QString& rawJson)
{
    QVector<ServiceCapability> capabilities;
    const QJsonDocument document = QJsonDocument::fromJson(rawJson.toUtf8());
    if (!document.isArray()) {
        return capabilities;
    }

    const QJsonArray array = document.array();
    capabilities.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject object = value.toObject();
        capabilities.push_back(ServiceCapability{
            object.value(QStringLiteral("capabilityId")).toString(),
            object.value(QStringLiteral("capabilityName")).toString(),
            object.value(QStringLiteral("version")).toString(),
            object.value(QStringLiteral("enabled")).toBool()
        });
    }

    return capabilities;
}
}

ServiceRegistryRepository::ServiceRegistryRepository(QString connectionName)
    : m_connectionName(connectionName)
{
}

bool ServiceRegistryRepository::replaceRegistry(const QVector<ServiceRegistryEntry>& registry,
                                                const QString& defaultServiceId,
                                                qint64 observedAtMs) const
{
    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid() || !database.transaction()) {
        return false;
    }

    QSqlQuery deleteQuery(database);
    if (!deleteQuery.exec(QStringLiteral("DELETE FROM service_registry"))) {
        database.rollback();
        return false;
    }

    for (const ServiceRegistryEntry& entry : registry) {
        QSqlQuery insertQuery(database);
        insertQuery.prepare(QStringLiteral(R"(
            INSERT INTO service_registry
            (service_id, service_name, organization_name, environment_name, host, port, tls_enabled,
             manifest_version, is_default, observed_at_ms, raw_capabilities_json)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )"));
        insertQuery.addBindValue(normalizedText(entry.serviceId));
        insertQuery.addBindValue(normalizedText(entry.serviceName));
        insertQuery.addBindValue(normalizedText(entry.organizationName));
        insertQuery.addBindValue(normalizedText(entry.environmentName));
        insertQuery.addBindValue(normalizedText(entry.host));
        insertQuery.addBindValue(static_cast<int>(entry.port));
        insertQuery.addBindValue(entry.tlsEnabled ? 1 : 0);
        insertQuery.addBindValue(QStringLiteral("1"));
        insertQuery.addBindValue(normalizedText(entry.serviceId) == normalizedText(defaultServiceId) ? 1 : 0);
        insertQuery.addBindValue(observedAtMs);
        insertQuery.addBindValue(QString::fromUtf8(
            QJsonDocument(capabilitiesToJson(entry.capabilities)).toJson(QJsonDocument::Compact)));
        if (!insertQuery.exec()) {
            database.rollback();
            return false;
        }
    }

    return database.commit();
}

QVector<ServiceRegistryEntry> ServiceRegistryRepository::loadRegistry() const
{
    QVector<ServiceRegistryEntry> registry;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT service_id, service_name, organization_name, environment_name, host, port, tls_enabled,
               raw_capabilities_json
        FROM service_registry
        ORDER BY is_default DESC, observed_at_ms DESC, service_id ASC
    )"))) {
        return registry;
    }

    while (query.next()) {
        registry.push_back(ServiceRegistryEntry{
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            query.value(4).toString(),
            static_cast<quint16>(query.value(5).toUInt()),
            query.value(6).toInt() != 0,
            capabilitiesFromJson(query.value(7).toString())
        });
    }

    return registry;
}

ServiceDiscoveryResult ServiceRegistryRepository::loadDiscoveryResult() const
{
    QVector<ServiceDiscoverySnapshot> services;
    QString defaultServiceId;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT service_id, service_name, organization_name, environment_name, observed_at_ms,
               raw_capabilities_json, is_default
        FROM service_registry
        ORDER BY is_default DESC, observed_at_ms DESC, service_id ASC
    )"))) {
        return {};
    }

    while (query.next()) {
        const QString serviceId = query.value(0).toString();
        if (query.value(6).toInt() != 0 && defaultServiceId.isEmpty()) {
            defaultServiceId = serviceId;
        }

        services.push_back(ServiceDiscoverySnapshot{
            serviceId,
            query.value(1).toString(),
            query.value(2).toString(),
            query.value(3).toString(),
            query.value(4).toLongLong(),
            capabilitiesFromJson(query.value(5).toString())
        });
    }

    ServiceDiscoveryResult result;
    result.services = std::move(services);
    result.defaultServiceId = defaultServiceId;
    result.multipleServicesDetected = result.services.size() > 1;
    if (result.defaultServiceId.isEmpty() && !result.services.isEmpty()) {
        result.defaultServiceId = result.services.front().serviceId;
    }
    return result;
}
