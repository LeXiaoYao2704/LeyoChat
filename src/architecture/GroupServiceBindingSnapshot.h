#pragma once

#include <QString>

#include "architecture/ResourceReference.h"
#include "architecture/ServiceBinding.h"
#include "architecture/ServiceDiscoverySnapshot.h"
#include "architecture/ServiceRegistryEntry.h"

struct GroupServiceBindingSnapshot {
    QString groupId;
    QString groupName;
    ServiceBinding binding;
    ServiceRegistryEntry registryEntry;
    ServiceDiscoverySnapshot discoverySnapshot;
    ResourceReference primaryResource;
    bool enabled = false;
};
