#pragma once

#include <ElaDialog.h>
#include <QVector>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/ResourceReference.h"
#include "architecture/RuntimeArchitectureSnapshot.h"
#include "architecture/ServiceRegistryEntry.h"
#include "architecture/ServiceSelectionSnapshot.h"
#include "architecture/WorkspaceServiceBindingSnapshot.h"

class ElaText;
class QListWidget;
class ElaListWidget;
class QPushButton;

class RuntimeArchitectureDialog : public ElaDialog {
    Q_OBJECT

public:
    explicit RuntimeArchitectureDialog(QWidget* parent = nullptr);

    void setSnapshot(const RuntimeArchitectureSnapshot& snapshot);
    void setEditableState(const QVector<ServiceRegistryEntry>& serviceRegistry,
                          const QVector<WorkspaceServiceBindingSnapshot>& workspaceBindings,
                          const QVector<GroupServiceBindingSnapshot>& groupBindings,
                          const ServiceSelectionSnapshot& selection);

    QVector<ServiceRegistryEntry> editedServiceRegistry() const;
    QVector<WorkspaceServiceBindingSnapshot> editedWorkspaceBindings() const;
    QVector<GroupServiceBindingSnapshot> editedGroupBindings() const;
    QVector<ResourceReference> editedResources() const;
    ServiceSelectionSnapshot editedSelection() const;

private:
    void applySnapshot();
    void populateEditableStateFromSnapshot();
    void rebuildSelection();
    void addServiceEntry();
    void removeSelectedServiceEntry();
    void setSelectionFromCurrentService();
    void addWorkspaceBinding();
    void addGroupBinding();
    void removeSelectedBinding();
    void setSelectionFromCurrentBinding();
    void addResourceEntry();
    void removeSelectedResource();
    void setSelectionFromCurrentResource();

    RuntimeArchitectureSnapshot m_snapshot;
    QVector<ServiceRegistryEntry> m_serviceRegistry;
    QVector<WorkspaceServiceBindingSnapshot> m_workspaceBindings;
    QVector<GroupServiceBindingSnapshot> m_groupBindings;
    QVector<ResourceReference> m_resources;
    ServiceSelectionSnapshot m_selection;
    ElaText* m_selectionChip = nullptr;
    ElaText* m_summaryLabel = nullptr;
    ElaText* m_detailLabel = nullptr;
    ElaListWidget* m_serviceList = nullptr;
    ElaListWidget* m_bindingList = nullptr;
    ElaListWidget* m_resourceList = nullptr;
    QPushButton* m_saveButton = nullptr;
};
