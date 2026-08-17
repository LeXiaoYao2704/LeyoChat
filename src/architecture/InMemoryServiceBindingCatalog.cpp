#include "architecture/InMemoryServiceBindingCatalog.h"

#include <utility>

InMemoryServiceBindingCatalog::InMemoryServiceBindingCatalog(
    QVector<ServiceRegistryEntry> serviceRegistry,
    QVector<WorkspaceServiceBindingSnapshot> workspaceBindings,
    QVector<GroupServiceBindingSnapshot> groupBindings)
    : serviceRegistry_(std::move(serviceRegistry))
    , workspaceBindings_(std::move(workspaceBindings))
    , groupBindings_(std::move(groupBindings))
{
}

QVector<ServiceRegistryEntry> InMemoryServiceBindingCatalog::listServiceRegistry() const
{
    return serviceRegistry_;
}

QVector<WorkspaceServiceBindingSnapshot> InMemoryServiceBindingCatalog::listWorkspaceBindings() const
{
    return workspaceBindings_;
}

QVector<GroupServiceBindingSnapshot> InMemoryServiceBindingCatalog::listGroupBindings() const
{
    return groupBindings_;
}

void InMemoryServiceBindingCatalog::setServiceRegistry(QVector<ServiceRegistryEntry> serviceRegistry)
{
    serviceRegistry_ = std::move(serviceRegistry);
}

void InMemoryServiceBindingCatalog::setWorkspaceBindings(QVector<WorkspaceServiceBindingSnapshot> workspaceBindings)
{
    workspaceBindings_ = std::move(workspaceBindings);
}

void InMemoryServiceBindingCatalog::setGroupBindings(QVector<GroupServiceBindingSnapshot> groupBindings)
{
    groupBindings_ = std::move(groupBindings);
}
