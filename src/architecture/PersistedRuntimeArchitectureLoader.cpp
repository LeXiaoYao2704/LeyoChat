#include "architecture/PersistedRuntimeArchitectureLoader.h"

#include <utility>

#include "architecture/DatabaseResourceCatalog.h"
#include "architecture/DatabaseServiceBindingCatalog.h"
#include "architecture/DatabaseServiceDiscoveryProvider.h"
#include "architecture/DatabaseServiceSelectionCatalog.h"
#include "architecture/RuntimeArchitectureFacade.h"
#include "storage/ServiceBindingRepository.h"
#include "storage/ServiceRegistryRepository.h"
#include "storage/ServiceResourceRepository.h"

PersistedRuntimeArchitectureLoader::PersistedRuntimeArchitectureLoader(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

RuntimeArchitectureSnapshot PersistedRuntimeArchitectureLoader::loadSnapshot() const
{
    ServiceRegistryRepository registryRepository(m_connectionName);
    ServiceBindingRepository bindingRepository(m_connectionName);
    ServiceResourceRepository resourceRepository(m_connectionName);

    DatabaseServiceDiscoveryProvider discoveryProvider(registryRepository);
    DatabaseServiceBindingCatalog bindingCatalog(
        bindingRepository, registryRepository, resourceRepository);
    DatabaseResourceCatalog resourceCatalog(resourceRepository);
    DatabaseServiceSelectionCatalog selectionCatalog(
        bindingRepository, registryRepository, resourceRepository);

    const RuntimeArchitectureFacade facade(
        discoveryProvider, bindingCatalog, resourceCatalog, selectionCatalog);
    return facade.loadSnapshot();
}
