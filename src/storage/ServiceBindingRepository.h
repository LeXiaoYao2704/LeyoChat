#pragma once

#include <QString>
#include <QVector>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ServiceSelectionSnapshot.h"
#include "architecture/WorkspaceServiceBindingSnapshot.h"

class ServiceBindingRepository {
public:
    explicit ServiceBindingRepository(QString connectionName);

    bool replaceWorkspaceBindings(const QVector<WorkspaceServiceBindingSnapshot>& bindings) const;
    bool replaceGroupBindings(const QVector<GroupServiceBindingSnapshot>& bindings) const;
    bool saveCurrentSelection(const ServiceSelectionSnapshot& selection) const;

    QVector<WorkspaceServiceBindingSnapshot> loadWorkspaceBindings() const;
    QVector<GroupServiceBindingSnapshot> loadGroupBindings() const;
    ServiceSelectionSnapshot loadCurrentSelection() const;

private:
    QString m_connectionName;
};
