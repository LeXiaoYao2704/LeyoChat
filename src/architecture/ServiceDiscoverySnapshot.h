#pragma once

#include <QString>
#include <QtGlobal>
#include <QVector>

#include "architecture/ServiceCapability.h"

struct ServiceDiscoverySnapshot {
    QString serviceId;
    QString serviceName;
    QString organizationName;
    QString environmentName;
    qint64 observedAtMs = 0;
    QVector<ServiceCapability> capabilities;
};
