#pragma once

#include "architecture/ServiceSelectionSnapshot.h"

class IServiceSelectionCatalog {
public:
    virtual ~IServiceSelectionCatalog() = default;

    virtual ServiceSelectionSnapshot currentSelection() const = 0;
};
