#include "architecture/ArchitectureSnapshotAssembler.h"

#include <algorithm>

namespace {

bool containsServiceId(const QVector<ServiceDiscoverySnapshot>& services, const QString& serviceId)
{
    return std::any_of(services.begin(), services.end(), [&](const ServiceDiscoverySnapshot& snapshot) {
        return snapshot.serviceId == serviceId;
    });
}

} // namespace

ServiceDiscoveryResult assembleDiscoveryResult(
    QVector<ServiceDiscoverySnapshot> services,
    const QString& preferredDefaultServiceId)
{
    ServiceDiscoveryResult result;
    result.services = std::move(services);
    result.multipleServicesDetected = result.services.size() > 1;

    if (!preferredDefaultServiceId.isEmpty() && containsServiceId(result.services, preferredDefaultServiceId)) {
        result.defaultServiceId = preferredDefaultServiceId;
    } else if (!result.services.isEmpty()) {
        result.defaultServiceId = result.services.front().serviceId;
    }

    return result;
}

ServiceDiscoverySnapshot assembleDiscoverySnapshot(
    const ServiceRegistryEntry& registryEntry,
    qint64 observedAtMs)
{
    ServiceDiscoverySnapshot snapshot;
    snapshot.serviceId = registryEntry.serviceId;
    snapshot.serviceName = registryEntry.serviceName;
    snapshot.organizationName = registryEntry.organizationName;
    snapshot.environmentName = registryEntry.environmentName;
    snapshot.observedAtMs = observedAtMs;
    snapshot.capabilities = registryEntry.capabilities;
    return snapshot;
}

ServiceEndpoint assembleServiceEndpoint(
    const ServiceRegistryEntry& registryEntry,
    const QString& routePrefix)
{
    ServiceEndpoint endpoint;
    endpoint.serviceId = registryEntry.serviceId;
    endpoint.host = registryEntry.host;
    endpoint.port = registryEntry.port;
    endpoint.tlsEnabled = registryEntry.tlsEnabled;
    endpoint.routePrefix = routePrefix;
    return endpoint;
}

ServiceRegistryEntry assembleRegistryEntry(
    const ServiceDiscoverySnapshot& discoverySnapshot,
    const ServiceEndpoint& endpoint)
{
    ServiceRegistryEntry registryEntry;
    registryEntry.serviceId = discoverySnapshot.serviceId;
    registryEntry.serviceName = discoverySnapshot.serviceName;
    registryEntry.organizationName = discoverySnapshot.organizationName;
    registryEntry.environmentName = discoverySnapshot.environmentName;
    registryEntry.host = endpoint.host;
    registryEntry.port = endpoint.port;
    registryEntry.tlsEnabled = endpoint.tlsEnabled;
    registryEntry.capabilities = discoverySnapshot.capabilities;
    return registryEntry;
}

GroupServiceBindingSnapshot assembleGroupBindingSnapshot(
    const QString& groupId,
    const QString& groupName,
    const ServiceBinding& binding,
    const ServiceRegistryEntry& registryEntry,
    const ResourceReference& primaryResource,
    bool enabled,
    qint64 observedAtMs)
{
    GroupServiceBindingSnapshot snapshot;
    snapshot.groupId = groupId;
    snapshot.groupName = groupName;
    snapshot.binding = binding;
    snapshot.registryEntry = registryEntry;
    snapshot.discoverySnapshot = assembleDiscoverySnapshot(registryEntry, observedAtMs);
    snapshot.primaryResource = primaryResource;
    snapshot.enabled = enabled;
    return snapshot;
}

WorkspaceServiceBindingSnapshot assembleWorkspaceBindingSnapshot(
    const QString& workspaceId,
    const QString& workspaceName,
    QVector<GroupServiceBindingSnapshot> groupBindings)
{
    WorkspaceServiceBindingSnapshot snapshot;
    snapshot.workspaceId = workspaceId;
    snapshot.workspaceName = workspaceName;
    snapshot.groupBindings = std::move(groupBindings);
    return snapshot;
}

ServiceSelectionSnapshot resolveServiceSelectionSnapshot(
    const WorkspaceServiceBindingSnapshot& workspaceBinding,
    const QString& groupId,
    const QString& selectionSource)
{
    ServiceSelectionSnapshot selection;
    selection.workspaceId = workspaceBinding.workspaceId;
    selection.selectionSource = selectionSource;

    const auto it = std::find_if(
        workspaceBinding.groupBindings.begin(),
        workspaceBinding.groupBindings.end(),
        [&](const GroupServiceBindingSnapshot& binding) {
            return binding.groupId == groupId;
        });
    if (it == workspaceBinding.groupBindings.end()) {
        return selection;
    }

    selection.groupId = it->groupId;
    selection.serviceId = it->binding.boundServiceId;
    selection.serviceName = it->registryEntry.serviceName;
    selection.registryEntry = it->registryEntry;
    selection.discoverySnapshot = it->discoverySnapshot;
    selection.groupBinding = *it;
    selection.selectedResource = it->primaryResource;
    selection.bound = it->enabled && !it->binding.boundServiceId.isEmpty();
    return selection;
}
