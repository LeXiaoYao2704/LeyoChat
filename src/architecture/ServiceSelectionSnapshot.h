#pragma once

#include <QString>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ResourceReference.h"
#include "architecture/ServiceDiscoverySnapshot.h"
#include "architecture/ServiceRegistryEntry.h"

struct ServiceSelectionSnapshot {
    QString workspaceId;
    QString groupId;
    QString serviceId;
    QString serviceName;
    QString selectionSource;
    ServiceRegistryEntry registryEntry;
    ServiceDiscoverySnapshot discoverySnapshot;
    GroupServiceBindingSnapshot groupBinding;
    ResourceReference selectedResource;
    bool bound = false;
};
