#pragma once

#include <QString>
#include <QVector>

#include "architecture/GroupServiceBindingSnapshot.h"

struct WorkspaceServiceBindingSnapshot {
    QString workspaceId;
    QString workspaceName;
    QVector<GroupServiceBindingSnapshot> groupBindings;
};
