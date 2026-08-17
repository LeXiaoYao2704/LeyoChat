#pragma once

#include "architecture/IResourceCatalog.h"
#include "architecture/IServiceBindingCatalog.h"
#include "architecture/IServiceDiscoveryProvider.h"
#include "architecture/IServiceSelectionCatalog.h"
#include "architecture/RuntimeArchitectureSnapshot.h"

class RuntimeArchitectureFacade {
public:
    RuntimeArchitectureFacade(const IServiceDiscoveryProvider& discoveryProvider,
                              const IServiceBindingCatalog& bindingCatalog,
                              const IResourceCatalog& resourceCatalog,
                              const IServiceSelectionCatalog& selectionCatalog);

    RuntimeArchitectureSnapshot loadSnapshot() const;

private:
    QVector<ResourceReference> filterVisibleResources(
        const QVector<ResourceReference>& resources,
        const ServiceSelectionSnapshot& selection) const;

    const IServiceDiscoveryProvider& discoveryProvider_;
    const IServiceBindingCatalog& bindingCatalog_;
    const IResourceCatalog& resourceCatalog_;
    const IServiceSelectionCatalog& selectionCatalog_;
};
