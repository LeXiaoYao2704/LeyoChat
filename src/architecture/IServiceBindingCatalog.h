#pragma once

#include <QVector>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ServiceRegistryEntry.h"
#include "architecture/WorkspaceServiceBindingSnapshot.h"

class IServiceBindingCatalog {
public:
    virtual ~IServiceBindingCatalog() = default;

    virtual QVector<ServiceRegistryEntry> listServiceRegistry() const = 0;
    virtual QVector<WorkspaceServiceBindingSnapshot> listWorkspaceBindings() const = 0;
    virtual QVector<GroupServiceBindingSnapshot> listGroupBindings() const = 0;
};
