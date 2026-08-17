#pragma once

#include "architecture/IServiceBindingCatalog.h"

class ServiceBindingRepository;
class ServiceRegistryRepository;
class ServiceResourceRepository;

class DatabaseServiceBindingCatalog : public IServiceBindingCatalog {
public:
    DatabaseServiceBindingCatalog(const ServiceBindingRepository& bindingRepository,
                                  const ServiceRegistryRepository& registryRepository,
                                  const ServiceResourceRepository& resourceRepository);

    QVector<ServiceRegistryEntry> listServiceRegistry() const override;
    QVector<WorkspaceServiceBindingSnapshot> listWorkspaceBindings() const override;
    QVector<GroupServiceBindingSnapshot> listGroupBindings() const override;

private:
    const ServiceBindingRepository& bindingRepository_;
    const ServiceRegistryRepository& registryRepository_;
    const ServiceResourceRepository& resourceRepository_;
};
