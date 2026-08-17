#pragma once

#include "architecture/IServiceDiscoveryProvider.h"

class ServiceRegistryRepository;

class DatabaseServiceDiscoveryProvider : public IServiceDiscoveryProvider {
public:
    explicit DatabaseServiceDiscoveryProvider(const ServiceRegistryRepository& repository);

    ServiceDiscoveryResult discoverServices() const override;

private:
    const ServiceRegistryRepository& repository_;
};
