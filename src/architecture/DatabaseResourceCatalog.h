#pragma once

#include "architecture/IResourceCatalog.h"

class ServiceResourceRepository;

class DatabaseResourceCatalog : public IResourceCatalog {
public:
    explicit DatabaseResourceCatalog(const ServiceResourceRepository& repository);

    QVector<ResourceReference> listResources() const override;

private:
    const ServiceResourceRepository& repository_;
};
