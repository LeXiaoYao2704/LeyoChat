#pragma once

#include "architecture/IServiceDiscoveryProvider.h"

class StaticServiceDiscoveryProvider : public IServiceDiscoveryProvider {
public:
    StaticServiceDiscoveryProvider() = default;
    explicit StaticServiceDiscoveryProvider(ServiceDiscoveryResult discoveryResult);

    ServiceDiscoveryResult discoverServices() const override;
    void setDiscoveryResult(ServiceDiscoveryResult discoveryResult);

private:
    ServiceDiscoveryResult discoveryResult_;
};
