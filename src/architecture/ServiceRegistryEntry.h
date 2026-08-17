#pragma once

#include <QtGlobal>
#include <QString>
#include <QVector>

#include "architecture/ServiceCapability.h"

struct ServiceRegistryEntry {
    QString serviceId;
    QString serviceName;
    QString organizationName;
    QString environmentName;
    QString host;
    quint16 port = 0;
    bool tlsEnabled = false;
    QVector<ServiceCapability> capabilities;
};
