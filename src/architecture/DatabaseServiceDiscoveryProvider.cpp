#include "architecture/DatabaseServiceDiscoveryProvider.h"

#include "storage/ServiceRegistryRepository.h"

DatabaseServiceDiscoveryProvider::DatabaseServiceDiscoveryProvider(
    const ServiceRegistryRepository& repository)
    : repository_(repository)
{
}

ServiceDiscoveryResult DatabaseServiceDiscoveryProvider::discoverServices() const
{
    return repository_.loadDiscoveryResult();
}
