#include "architecture/DatabaseServiceBindingCatalog.h"

#include <algorithm>

#include "architecture/ArchitectureSnapshotAssembler.h"
#include "storage/ServiceBindingRepository.h"
#include "storage/ServiceRegistryRepository.h"
#include "storage/ServiceResourceRepository.h"

namespace {
const ServiceRegistryEntry* findRegistry(const QVector<ServiceRegistryEntry>& registry,
                                         const QString& serviceId)
{
    const auto it = std::find_if(
        registry.begin(),
        registry.end(),
        [&](const ServiceRegistryEntry& entry) { return entry.serviceId == serviceId; });
    return it == registry.end() ? nullptr : &(*it);
}

const ServiceDiscoverySnapshot* findDiscovery(const QVector<ServiceDiscoverySnapshot>& snapshots,
                                              const QString& serviceId)
{
    const auto it = std::find_if(
        snapshots.begin(),
        snapshots.end(),
        [&](const ServiceDiscoverySnapshot& snapshot) { return snapshot.serviceId == serviceId; });
    return it == snapshots.end() ? nullptr : &(*it);
}

ResourceReference resolveResource(const QVector<ResourceReference>& resources,
                                  const ResourceReference& fallback)
{
    const auto it = std::find_if(
        resources.begin(),
        resources.end(),
        [&](const ResourceReference& resource) {
            return resource.resourceId == fallback.resourceId
                && (fallback.serviceId.isEmpty() || resource.serviceId == fallback.serviceId);
        });
    return it == resources.end() ? fallback : *it;
}
}

DatabaseServiceBindingCatalog::DatabaseServiceBindingCatalog(
    const ServiceBindingRepository& bindingRepository,
    const ServiceRegistryRepository& registryRepository,
    const ServiceResourceRepository& resourceRepository)
    : bindingRepository_(bindingRepository),
      registryRepository_(registryRepository),
      resourceRepository_(resourceRepository)
{
}

QVector<ServiceRegistryEntry> DatabaseServiceBindingCatalog::listServiceRegistry() const
{
    return registryRepository_.loadRegistry();
}

QVector<GroupServiceBindingSnapshot> DatabaseServiceBindingCatalog::listGroupBindings() const
{
    QVector<GroupServiceBindingSnapshot> bindings = bindingRepository_.loadGroupBindings();
    const QVector<ServiceRegistryEntry> registry = registryRepository_.loadRegistry();
    const ServiceDiscoveryResult discoveryResult = registryRepository_.loadDiscoveryResult();
    const QVector<ResourceReference> resources = resourceRepository_.loadResources();

    for (GroupServiceBindingSnapshot& binding : bindings) {
        binding.primaryResource = resolveResource(resources, binding.primaryResource);

        if (const ServiceRegistryEntry* registryEntry =
                findRegistry(registry, binding.binding.boundServiceId)) {
            binding.registryEntry = *registryEntry;
        }

        if (const ServiceDiscoverySnapshot* snapshot =
                findDiscovery(discoveryResult.services, binding.binding.boundServiceId)) {
            binding.discoverySnapshot = *snapshot;
        } else if (!binding.registryEntry.serviceId.isEmpty()) {
            binding.discoverySnapshot = assembleDiscoverySnapshot(
                binding.registryEntry, binding.discoverySnapshot.observedAtMs);
        }
    }

    return bindings;
}

QVector<WorkspaceServiceBindingSnapshot> DatabaseServiceBindingCatalog::listWorkspaceBindings() const
{
    QVector<WorkspaceServiceBindingSnapshot> workspaces = bindingRepository_.loadWorkspaceBindings();
    const QVector<GroupServiceBindingSnapshot> groupBindings = listGroupBindings();

    for (WorkspaceServiceBindingSnapshot& workspace : workspaces) {
        for (const GroupServiceBindingSnapshot& groupBinding : groupBindings) {
            if (groupBinding.primaryResource.workspaceId == workspace.workspaceId) {
                workspace.groupBindings.push_back(groupBinding);
            }
        }
    }

    return workspaces;
}
