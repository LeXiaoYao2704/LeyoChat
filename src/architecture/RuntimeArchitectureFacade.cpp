#include "architecture/RuntimeArchitectureFacade.h"

RuntimeArchitectureFacade::RuntimeArchitectureFacade(const IServiceDiscoveryProvider& discoveryProvider,
                                                     const IServiceBindingCatalog& bindingCatalog,
                                                     const IResourceCatalog& resourceCatalog,
                                                     const IServiceSelectionCatalog& selectionCatalog)
    : discoveryProvider_(discoveryProvider),
      bindingCatalog_(bindingCatalog),
      resourceCatalog_(resourceCatalog),
      selectionCatalog_(selectionCatalog)
{
}

RuntimeArchitectureSnapshot RuntimeArchitectureFacade::loadSnapshot() const
{
    RuntimeArchitectureSnapshot snapshot;
    snapshot.discoveryResult = discoveryProvider_.discoverServices();
    snapshot.serviceRegistry = bindingCatalog_.listServiceRegistry();
    snapshot.workspaceBindings = bindingCatalog_.listWorkspaceBindings();
    snapshot.groupBindings = bindingCatalog_.listGroupBindings();
    snapshot.selection = selectionCatalog_.currentSelection();
    snapshot.visibleResources =
        filterVisibleResources(resourceCatalog_.listResources(), snapshot.selection);
    return snapshot;
}

QVector<ResourceReference> RuntimeArchitectureFacade::filterVisibleResources(
    const QVector<ResourceReference>& resources,
    const ServiceSelectionSnapshot& selection) const
{
    if (!selection.bound || selection.serviceId.trimmed().isEmpty()) {
        return resources;
    }

    QVector<ResourceReference> filtered;
    filtered.reserve(resources.size());

    for (const ResourceReference& resource : resources) {
        const bool serviceMatches =
            resource.serviceId.trimmed().isEmpty() || resource.serviceId == selection.serviceId;
        const bool workspaceMatches =
            selection.workspaceId.trimmed().isEmpty() || resource.workspaceId.trimmed().isEmpty()
            || resource.workspaceId == selection.workspaceId;

        if (serviceMatches && workspaceMatches) {
            filtered.push_back(resource);
        }
    }

    return filtered;
}
