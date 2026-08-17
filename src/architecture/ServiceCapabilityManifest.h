#pragma once

#include <QString>
#include <QVector>

#include "architecture/ServiceCapability.h"

struct ServiceCapabilityManifest {
    QString serviceId;
    QString serviceName;
    QString version;
    QVector<ServiceCapability> capabilities;
};
