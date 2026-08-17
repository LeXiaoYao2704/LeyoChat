#pragma once

#include "architecture/IServiceSelectionCatalog.h"

class InMemoryServiceSelectionCatalog : public IServiceSelectionCatalog {
public:
    InMemoryServiceSelectionCatalog() = default;
    explicit InMemoryServiceSelectionCatalog(ServiceSelectionSnapshot currentSelection);

    ServiceSelectionSnapshot currentSelection() const override;
    void setCurrentSelection(ServiceSelectionSnapshot currentSelection);

private:
    ServiceSelectionSnapshot currentSelection_;
};
