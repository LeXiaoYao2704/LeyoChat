#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>

#include "architecture/ServiceDiscoveryResult.h"
#include "architecture/ServiceRegistryEntry.h"

class ServiceRegistryRepository {
public:
    explicit ServiceRegistryRepository(QString connectionName);

    bool replaceRegistry(const QVector<ServiceRegistryEntry>& registry,
                         const QString& defaultServiceId = {},
                         qint64 observedAtMs = 0) const;
    QVector<ServiceRegistryEntry> loadRegistry() const;
    ServiceDiscoveryResult loadDiscoveryResult() const;

private:
    QString m_connectionName;
};
