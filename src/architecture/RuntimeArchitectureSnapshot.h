#pragma once

#include <QVector>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ResourceReference.h"
#include "architecture/ServiceDiscoveryResult.h"
#include "architecture/ServiceRegistryEntry.h"
#include "architecture/ServiceSelectionSnapshot.h"
#include "architecture/WorkspaceServiceBindingSnapshot.h"

struct RuntimeArchitectureSnapshot {
    ServiceDiscoveryResult discoveryResult;
    QVector<ServiceRegistryEntry> serviceRegistry;
    QVector<WorkspaceServiceBindingSnapshot> workspaceBindings;
    QVector<GroupServiceBindingSnapshot> groupBindings;
    QVector<ResourceReference> visibleResources;
    ServiceSelectionSnapshot selection;
};
