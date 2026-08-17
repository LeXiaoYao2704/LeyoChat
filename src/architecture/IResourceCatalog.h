#pragma once

#include <QVector>

#include "architecture/ResourceReference.h"

class IResourceCatalog {
public:
    virtual ~IResourceCatalog() = default;

    virtual QVector<ResourceReference> listResources() const = 0;
};
