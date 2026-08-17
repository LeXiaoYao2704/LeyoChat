#include "architecture/InMemoryResourceCatalog.h"

#include <utility>

InMemoryResourceCatalog::InMemoryResourceCatalog(QVector<ResourceReference> resources)
    : resources_(std::move(resources))
{
}

QVector<ResourceReference> InMemoryResourceCatalog::listResources() const
{
    return resources_;
}

void InMemoryResourceCatalog::setResources(QVector<ResourceReference> resources)
{
    resources_ = std::move(resources);
}
