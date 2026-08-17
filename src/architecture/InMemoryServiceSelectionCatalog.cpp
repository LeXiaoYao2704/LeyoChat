#include "architecture/InMemoryServiceSelectionCatalog.h"

#include <utility>

InMemoryServiceSelectionCatalog::InMemoryServiceSelectionCatalog(ServiceSelectionSnapshot currentSelection)
    : currentSelection_(std::move(currentSelection))
{
}

ServiceSelectionSnapshot InMemoryServiceSelectionCatalog::currentSelection() const
{
    return currentSelection_;
}

void InMemoryServiceSelectionCatalog::setCurrentSelection(ServiceSelectionSnapshot currentSelection)
{
    currentSelection_ = std::move(currentSelection);
}
