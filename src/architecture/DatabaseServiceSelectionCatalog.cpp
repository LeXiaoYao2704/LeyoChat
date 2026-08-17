#include "architecture/DatabaseServiceSelectionCatalog.h"

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

const GroupServiceBindingSnapshot* findGroupBinding(const QVector<GroupServiceBindingSnapshot>& bindings,
                                                    const QString& groupId)
{
    const auto it = std::find_if(
        bindings.begin(),
        bindings.end(),
        [&](const GroupServiceBindingSnapshot& binding) { return binding.groupId == groupId; });
    return it == bindings.end() ? nullptr : &(*it);
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

DatabaseServiceSelectionCatalog::DatabaseServiceSelectionCatalog(
    const ServiceBindingRepository& bindingRepository,
    const ServiceRegistryRepository& registryRepository,
    const ServiceResourceRepository& resourceRepository)
    : bindingRepository_(bindingRepository),
      registryRepository_(registryRepository),
      resourceRepository_(resourceRepository)
{
}

ServiceSelectionSnapshot DatabaseServiceSelectionCatalog::currentSelection() const
{
    ServiceSelectionSnapshot selection = bindingRepository_.loadCurrentSelection();
    if (selection.serviceId.isEmpty() && selection.groupId.isEmpty()) {
        return selection;
    }

    const QVector<ServiceRegistryEntry> registry = registryRepository_.loadRegistry();
    const ServiceDiscoveryResult discoveryResult = registryRepository_.loadDiscoveryResult();
    const QVector<ResourceReference> resources = resourceRepository_.loadResources();
    const QVector<GroupServiceBindingSnapshot> groupBindings = bindingRepository_.loadGroupBindings();

    if (const ServiceRegistryEntry* registryEntry = findRegistry(registry, selection.serviceId)) {
        selection.registryEntry = *registryEntry;
    }

    if (const ServiceDiscoverySnapshot* snapshot =
            findDiscovery(discoveryResult.services, selection.serviceId)) {
        selection.discoverySnapshot = *snapshot;
    } else if (!selection.registryEntry.serviceId.isEmpty()) {
        selection.discoverySnapshot =
            assembleDiscoverySnapshot(selection.registryEntry, selection.discoverySnapshot.observedAtMs);
    }

    if (const GroupServiceBindingSnapshot* groupBinding =
            findGroupBinding(groupBindings, selection.groupId)) {
        selection.groupBinding = *groupBinding;
        if (selection.serviceName.isEmpty()) {
            selection.serviceName = groupBinding->registryEntry.serviceName;
        }
        if (selection.selectedResource.resourceId.isEmpty()) {
            selection.selectedResource = groupBinding->primaryResource;
        }
    }

    selection.selectedResource = resolveResource(resources, selection.selectedResource);
    if (selection.serviceName.isEmpty()) {
        selection.serviceName = selection.registryEntry.serviceName;
    }

    return selection;
}
