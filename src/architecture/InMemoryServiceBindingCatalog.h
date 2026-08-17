#pragma once

#include "architecture/IServiceBindingCatalog.h"

class InMemoryServiceBindingCatalog : public IServiceBindingCatalog {
public:
    InMemoryServiceBindingCatalog() = default;
    InMemoryServiceBindingCatalog(QVector<ServiceRegistryEntry> serviceRegistry,
                                  QVector<WorkspaceServiceBindingSnapshot> workspaceBindings,
                                  QVector<GroupServiceBindingSnapshot> groupBindings);

    QVector<ServiceRegistryEntry> listServiceRegistry() const override;
    QVector<WorkspaceServiceBindingSnapshot> listWorkspaceBindings() const override;
    QVector<GroupServiceBindingSnapshot> listGroupBindings() const override;

    void setServiceRegistry(QVector<ServiceRegistryEntry> serviceRegistry);
    void setWorkspaceBindings(QVector<WorkspaceServiceBindingSnapshot> workspaceBindings);
    void setGroupBindings(QVector<GroupServiceBindingSnapshot> groupBindings);

private:
    QVector<ServiceRegistryEntry> serviceRegistry_;
    QVector<WorkspaceServiceBindingSnapshot> workspaceBindings_;
    QVector<GroupServiceBindingSnapshot> groupBindings_;
};
