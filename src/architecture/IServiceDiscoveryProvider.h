#pragma once

#include "architecture/ServiceDiscoveryResult.h"

class IServiceDiscoveryProvider {
public:
    virtual ~IServiceDiscoveryProvider() = default;

    virtual ServiceDiscoveryResult discoverServices() const = 0;
};
