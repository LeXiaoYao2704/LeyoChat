#pragma once

#include <QString>

struct ServiceCapability {
    QString capabilityId;
    QString capabilityName;
    QString version;
    bool enabled = false;
};
