#pragma once

#include "architecture/IResourceCatalog.h"

class InMemoryResourceCatalog : public IResourceCatalog {
public:
    InMemoryResourceCatalog() = default;
    explicit InMemoryResourceCatalog(QVector<ResourceReference> resources);

    QVector<ResourceReference> listResources() const override;
    void setResources(QVector<ResourceReference> resources);

private:
    QVector<ResourceReference> resources_;
};
