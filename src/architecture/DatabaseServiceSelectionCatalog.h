#pragma once

#include "architecture/IServiceSelectionCatalog.h"

class ServiceBindingRepository;
class ServiceRegistryRepository;
class ServiceResourceRepository;

class DatabaseServiceSelectionCatalog : public IServiceSelectionCatalog {
public:
    DatabaseServiceSelectionCatalog(const ServiceBindingRepository& bindingRepository,
                                    const ServiceRegistryRepository& registryRepository,
                                    const ServiceResourceRepository& resourceRepository);

    ServiceSelectionSnapshot currentSelection() const override;

private:
    const ServiceBindingRepository& bindingRepository_;
    const ServiceRegistryRepository& registryRepository_;
    const ServiceResourceRepository& resourceRepository_;
};
