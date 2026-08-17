#pragma once

#include <QString>
#include <QVector>

#include "architecture/ServiceDiscoverySnapshot.h"

struct ServiceDiscoveryResult {
    QVector<ServiceDiscoverySnapshot> services;
    QString defaultServiceId;
    bool multipleServicesDetected = false;
};
