#pragma once

#include <QString>
#include <QVector>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ResourceReference.h"
#include "architecture/RuntimeArchitectureSnapshot.h"
#include "architecture/ServiceSelectionSnapshot.h"

class RuntimeArchitectureQueryService {
public:
    explicit RuntimeArchitectureQueryService(const RuntimeArchitectureSnapshot& snapshot);

    const GroupServiceBindingSnapshot* findGroupBinding(const QString& groupId) const;

    bool hasBoundServiceForGroup(const QString& groupId) const;
    bool sharedFilesEnabledForGroup(const QString& groupId) const;

    QString boundServiceIdForGroup(const QString& groupId) const;
    QString serviceNameForGroup(const QString& groupId) const;

    ResourceReference primaryResourceForGroup(const QString& groupId) const;
    ResourceReference selectedResourceForGroup(const QString& groupId) const;
    QVector<ResourceReference> visibleResourcesForGroup(const QString& groupId) const;
    QVector<ResourceReference> sharedFileResourcesForGroup(const QString& groupId) const;
    ServiceSelectionSnapshot selectionForGroup(const QString& groupId) const;

private:
    const RuntimeArchitectureSnapshot& snapshot_;
};
