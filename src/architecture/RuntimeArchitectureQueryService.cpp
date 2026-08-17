#include "architecture/RuntimeArchitectureQueryService.h"

namespace {
QString resolveServiceName(const GroupServiceBindingSnapshot& binding)
{
    if (!binding.registryEntry.serviceName.trimmed().isEmpty()) {
        return binding.registryEntry.serviceName.trimmed();
    }
    if (!binding.discoverySnapshot.serviceName.trimmed().isEmpty()) {
        return binding.discoverySnapshot.serviceName.trimmed();
    }
    return binding.binding.boundServiceId.trimmed();
}

bool matchesBindingScope(const ResourceReference& resource,
                         const GroupServiceBindingSnapshot& binding)
{
    const QString boundServiceId = binding.binding.boundServiceId.trimmed();
    const QString workspaceId = binding.primaryResource.workspaceId.trimmed();
    const bool serviceMatches = boundServiceId.isEmpty() || resource.serviceId.trimmed() == boundServiceId;
    const bool workspaceMatches = workspaceId.isEmpty() || resource.workspaceId.trimmed() == workspaceId;
    return serviceMatches && workspaceMatches;
}
}

RuntimeArchitectureQueryService::RuntimeArchitectureQueryService(
    const RuntimeArchitectureSnapshot& snapshot)
    : snapshot_(snapshot)
{
}

const GroupServiceBindingSnapshot* RuntimeArchitectureQueryService::findGroupBinding(
    const QString& groupId) const
{
    const QString trimmedGroupId = groupId.trimmed();
    if (trimmedGroupId.isEmpty()) {
        return nullptr;
    }

    for (const GroupServiceBindingSnapshot& binding : snapshot_.groupBindings) {
        if (binding.groupId == trimmedGroupId) {
            return &binding;
        }
    }

    return nullptr;
}

bool RuntimeArchitectureQueryService::hasBoundServiceForGroup(const QString& groupId) const
{
    const GroupServiceBindingSnapshot* binding = findGroupBinding(groupId);
    return binding && binding->enabled && !binding->binding.boundServiceId.trimmed().isEmpty();
}

bool RuntimeArchitectureQueryService::sharedFilesEnabledForGroup(const QString& groupId) const
{
    const GroupServiceBindingSnapshot* binding = findGroupBinding(groupId);
    return binding && binding->enabled && binding->binding.sharedFilesEnabled;
}

QString RuntimeArchitectureQueryService::boundServiceIdForGroup(const QString& groupId) const
{
    const GroupServiceBindingSnapshot* binding = findGroupBinding(groupId);
    return binding ? binding->binding.boundServiceId.trimmed() : QString();
}

QString RuntimeArchitectureQueryService::serviceNameForGroup(const QString& groupId) const
{
    const QString trimmedGroupId = groupId.trimmed();
    if (trimmedGroupId.isEmpty()) {
        return {};
    }

    if (snapshot_.selection.groupId == trimmedGroupId
        && !snapshot_.selection.serviceName.trimmed().isEmpty()) {
        return snapshot_.selection.serviceName.trimmed();
    }

    const GroupServiceBindingSnapshot* binding = findGroupBinding(trimmedGroupId);
    return binding ? resolveServiceName(*binding) : QString();
}

ResourceReference RuntimeArchitectureQueryService::primaryResourceForGroup(
    const QString& groupId) const
{
    const GroupServiceBindingSnapshot* binding = findGroupBinding(groupId);
    return binding ? binding->primaryResource : ResourceReference{};
}

ResourceReference RuntimeArchitectureQueryService::selectedResourceForGroup(
    const QString& groupId) const
{
    const QString trimmedGroupId = groupId.trimmed();
    if (trimmedGroupId.isEmpty()) {
        return {};
    }

    if (snapshot_.selection.groupId == trimmedGroupId
        && (!snapshot_.selection.selectedResource.resourceId.trimmed().isEmpty()
            || !snapshot_.selection.selectedResource.title.trimmed().isEmpty())) {
        return snapshot_.selection.selectedResource;
    }

    return {};
}

QVector<ResourceReference> RuntimeArchitectureQueryService::visibleResourcesForGroup(
    const QString& groupId) const
{
    QVector<ResourceReference> resources;
    const GroupServiceBindingSnapshot* binding = findGroupBinding(groupId);
    if (!binding || !binding->enabled || binding->binding.boundServiceId.trimmed().isEmpty()) {
        return resources;
    }

    resources.reserve(snapshot_.visibleResources.size());
    for (const ResourceReference& resource : snapshot_.visibleResources) {
        if (matchesBindingScope(resource, *binding)) {
            resources.push_back(resource);
        }
    }
    return resources;
}

QVector<ResourceReference> RuntimeArchitectureQueryService::sharedFileResourcesForGroup(
    const QString& groupId) const
{
    QVector<ResourceReference> resources;
    const QVector<ResourceReference> visibleResources = visibleResourcesForGroup(groupId);
    resources.reserve(visibleResources.size());
    for (const ResourceReference& resource : visibleResources) {
        const QString kind = resource.resourceKind.trimmed();
        if (kind == QStringLiteral("shared_file") || kind == QStringLiteral("group_file")) {
            resources.push_back(resource);
        }
    }
    return resources;
}

ServiceSelectionSnapshot RuntimeArchitectureQueryService::selectionForGroup(
    const QString& groupId) const
{
    const QString trimmedGroupId = groupId.trimmed();
    if (trimmedGroupId.isEmpty()) {
        return {};
    }

    if (snapshot_.selection.groupId == trimmedGroupId) {
        return snapshot_.selection;
    }

    ServiceSelectionSnapshot derived;
    const GroupServiceBindingSnapshot* binding = findGroupBinding(trimmedGroupId);
    if (!binding) {
        return derived;
    }

    derived.groupId = binding->groupId;
    derived.workspaceId = binding->primaryResource.workspaceId;
    derived.serviceId = binding->binding.boundServiceId;
    derived.serviceName = resolveServiceName(*binding);
    derived.selectionSource = QStringLiteral("group-binding");
    derived.registryEntry = binding->registryEntry;
    derived.discoverySnapshot = binding->discoverySnapshot;
    derived.groupBinding = *binding;
    derived.selectedResource = binding->primaryResource;
    derived.bound = binding->enabled && !binding->binding.boundServiceId.trimmed().isEmpty();
    return derived;
}
