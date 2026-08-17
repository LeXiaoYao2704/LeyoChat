#include "architecture/DatabaseResourceCatalog.h"

#include "storage/ServiceResourceRepository.h"

DatabaseResourceCatalog::DatabaseResourceCatalog(const ServiceResourceRepository& repository)
    : repository_(repository)
{
}

QVector<ResourceReference> DatabaseResourceCatalog::listResources() const
{
    return repository_.loadResources();
}
