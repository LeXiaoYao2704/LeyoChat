#include "architecture/StaticServiceDiscoveryProvider.h"

#include <utility>

StaticServiceDiscoveryProvider::StaticServiceDiscoveryProvider(ServiceDiscoveryResult discoveryResult)
    : discoveryResult_(std::move(discoveryResult))
{
}

ServiceDiscoveryResult StaticServiceDiscoveryProvider::discoverServices() const
{
    return discoveryResult_;
}

void StaticServiceDiscoveryProvider::setDiscoveryResult(ServiceDiscoveryResult discoveryResult)
{
    discoveryResult_ = std::move(discoveryResult);
}
