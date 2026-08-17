#pragma once

#include <QString>
#include <QVector>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ServiceDiscoveryResult.h"
#include "architecture/ServiceEndpoint.h"
#include "architecture/ServiceRegistryEntry.h"
#include "architecture/ServiceSelectionSnapshot.h"
#include "architecture/WorkspaceServiceBindingSnapshot.h"

ServiceDiscoveryResult assembleDiscoveryResult(
    QVector<ServiceDiscoverySnapshot> services,
    const QString& preferredDefaultServiceId = {});

ServiceDiscoverySnapshot assembleDiscoverySnapshot(
    const ServiceRegistryEntry& registryEntry,
    qint64 observedAtMs);

ServiceEndpoint assembleServiceEndpoint(
    const ServiceRegistryEntry& registryEntry,
    const QString& routePrefix = {});

ServiceRegistryEntry assembleRegistryEntry(
    const ServiceDiscoverySnapshot& discoverySnapshot,
    const ServiceEndpoint& endpoint);

GroupServiceBindingSnapshot assembleGroupBindingSnapshot(
    const QString& groupId,
    const QString& groupName,
    const ServiceBinding& binding,
    const ServiceRegistryEntry& registryEntry,
    const ResourceReference& primaryResource,
    bool enabled,
    qint64 observedAtMs);

WorkspaceServiceBindingSnapshot assembleWorkspaceBindingSnapshot(
    const QString& workspaceId,
    const QString& workspaceName,
    QVector<GroupServiceBindingSnapshot> groupBindings);

ServiceSelectionSnapshot resolveServiceSelectionSnapshot(
    const WorkspaceServiceBindingSnapshot& workspaceBinding,
    const QString& groupId,
    const QString& selectionSource);
