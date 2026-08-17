#include "ui/RuntimeArchitectureDialog.h"

#include "architecture/RuntimeArchitecturePresentation.h"
#include "ui/AppStyle.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <ElaText.h>
#include <QLineEdit>
#include <ElaListWidget.h>
#include <QListWidget>
#include <ElaPushButton.h>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <algorithm>

namespace {
enum ListRoles {
    KindRole = Qt::UserRole + 1,
    IdRole,
    ServiceIdRole,
    WorkspaceIdRole,
    ResourceIdRole,
};

QString serviceDisplayText(const ServiceRegistryEntry& entry)
{
    const QString name = entry.serviceName.trimmed().isEmpty() ? entry.serviceId : entry.serviceName;
    const QString org = entry.organizationName.trimmed().isEmpty()
                            ? QStringLiteral("\u672A\u77E5\u7EC4\u7EC7")
                            : entry.organizationName;
    const QString env = entry.environmentName.trimmed().isEmpty()
                            ? QStringLiteral("default")
                            : entry.environmentName;
    return QStringLiteral("%1 · %2 / %3").arg(name, org, env);
}

QString workspaceDisplayText(const WorkspaceServiceBindingSnapshot& binding)
{
    const QString name = binding.workspaceName.trimmed().isEmpty() ? binding.workspaceId : binding.workspaceName;
    return QStringLiteral("\u5DE5\u4F5C\u533A · %1").arg(name);
}

QString groupDisplayText(const GroupServiceBindingSnapshot& binding)
{
    const QString name = binding.groupName.trimmed().isEmpty() ? binding.groupId : binding.groupName;
    const QString serviceId = binding.binding.boundServiceId.trimmed();
    return serviceId.isEmpty()
               ? QStringLiteral("\u7FA4\u7EC4\u7ED1\u5B9A · %1").arg(name)
               : QStringLiteral("\u7FA4\u7EC4\u7ED1\u5B9A · %1 -> %2").arg(name, serviceId);
}
}

RuntimeArchitectureDialog::RuntimeArchitectureDialog(QWidget* parent)
    : ElaDialog(parent)
{
    setWindowTitle(QStringLiteral("\u6DF7\u5408\u67B6\u6784\u8BE6\u60C5"));
    resize(560, 700);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(12);

    auto* title = new ElaText(QStringLiteral("\u7B2C\u4E8C\u9636\u6BB5\u8FD0\u884C\u65F6\u5FEB\u7167"), this);
    title->setStyleSheet(QStringLiteral("font-size:18px; font-weight:700; color:%1;")
                             .arg(AppStyle::textPrimary()));
    m_selectionChip = new ElaText(this);
    m_selectionChip->setObjectName(QStringLiteral("runtimeDialogSelectionChip"));
    m_selectionChip->setStyleSheet(QStringLiteral(
        "QLabel {"
        "  background:%1;"
        "  color:%2;"
        "  border-radius:10px;"
        "  padding:3px 10px;"
        "  font-size:11px;"
        "  font-weight:600;"
        "}")
                                       .arg(AppStyle::hoverBg(), AppStyle::accent()));
    m_selectionChip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    m_summaryLabel = new ElaText(this);
    m_summaryLabel->setObjectName(QStringLiteral("runtimeDialogSummary"));
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet(QStringLiteral("font-size:13px; font-weight:600; color:%1;")
                                      .arg(AppStyle::textPrimary()));

    m_detailLabel = new ElaText(this);
    m_detailLabel->setObjectName(QStringLiteral("runtimeDialogDetail"));
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setStyleSheet(QStringLiteral("font-size:12px; color:%1;")
                                     .arg(AppStyle::textSecondary()));

    auto* serviceTitle = new ElaText(QStringLiteral("\u670D\u52A1\u53D1\u73B0"), this);
    serviceTitle->setStyleSheet(QStringLiteral("font-size:12px; color:%1; font-weight:700;")
                                    .arg(AppStyle::textMuted()));
    auto* serviceActions = new QHBoxLayout;
    auto* addServiceButton = new ElaPushButton(QStringLiteral("\u65B0\u589E\u670D\u52A1"), this);
    addServiceButton->setObjectName(QStringLiteral("runtimeDialogAddServiceButton"));
    auto* selectServiceButton = new ElaPushButton(QStringLiteral("\u8BBE\u4E3A\u5F53\u524D\u670D\u52A1"), this);
    selectServiceButton->setObjectName(QStringLiteral("runtimeDialogSelectServiceButton"));
    auto* removeServiceButton = new ElaPushButton(QStringLiteral("\u5220\u9664\u9009\u4E2D"), this);
    removeServiceButton->setObjectName(QStringLiteral("runtimeDialogRemoveServiceButton"));
    serviceActions->addWidget(addServiceButton);
    serviceActions->addWidget(selectServiceButton);
    serviceActions->addWidget(removeServiceButton);
    serviceActions->addStretch();

    m_serviceList = new ElaListWidget(this);
    m_serviceList->setObjectName(QStringLiteral("runtimeDialogServiceList"));

    auto* bindingTitle = new ElaText(QStringLiteral("\u7ED1\u5B9A\u6982\u89C8"), this);
    bindingTitle->setStyleSheet(QStringLiteral("font-size:12px; color:%1; font-weight:700;")
                                    .arg(AppStyle::textMuted()));
    auto* bindingActions = new QHBoxLayout;
    auto* addWorkspaceButton = new ElaPushButton(QStringLiteral("\u65B0\u589E\u5DE5\u4F5C\u533A"), this);
    addWorkspaceButton->setObjectName(QStringLiteral("runtimeDialogAddWorkspaceButton"));
    auto* addGroupButton = new ElaPushButton(QStringLiteral("\u65B0\u589E\u7FA4\u7EC4\u7ED1\u5B9A"), this);
    addGroupButton->setObjectName(QStringLiteral("runtimeDialogAddGroupButton"));
    auto* selectBindingButton = new ElaPushButton(QStringLiteral("\u8BBE\u4E3A\u5F53\u524D\u7ED1\u5B9A"), this);
    selectBindingButton->setObjectName(QStringLiteral("runtimeDialogSelectBindingButton"));
    auto* removeBindingButton = new ElaPushButton(QStringLiteral("\u5220\u9664\u9009\u4E2D"), this);
    removeBindingButton->setObjectName(QStringLiteral("runtimeDialogRemoveBindingButton"));
    bindingActions->addWidget(addWorkspaceButton);
    bindingActions->addWidget(addGroupButton);
    bindingActions->addWidget(selectBindingButton);
    bindingActions->addWidget(removeBindingButton);
    bindingActions->addStretch();

    m_bindingList = new ElaListWidget(this);
    m_bindingList->setObjectName(QStringLiteral("runtimeDialogBindingList"));

    auto* resourceTitle = new ElaText(QStringLiteral("\u53EF\u89C1\u8D44\u6E90"), this);
    resourceTitle->setStyleSheet(QStringLiteral("font-size:12px; color:%1; font-weight:700;")
                                     .arg(AppStyle::textMuted()));
    auto* resourceActions = new QHBoxLayout;
    auto* addResourceButton = new ElaPushButton(QStringLiteral("\u65B0\u589E\u8D44\u6E90"), this);
    addResourceButton->setObjectName(QStringLiteral("runtimeDialogAddResourceButton"));
    auto* selectResourceButton = new ElaPushButton(QStringLiteral("\u8BBE\u4E3A\u5F53\u524D\u8D44\u6E90"), this);
    selectResourceButton->setObjectName(QStringLiteral("runtimeDialogSelectResourceButton"));
    auto* removeResourceButton = new ElaPushButton(QStringLiteral("\u5220\u9664\u9009\u4E2D"), this);
    removeResourceButton->setObjectName(QStringLiteral("runtimeDialogRemoveResourceButton"));
    resourceActions->addWidget(addResourceButton);
    resourceActions->addWidget(selectResourceButton);
    resourceActions->addWidget(removeResourceButton);
    resourceActions->addStretch();
    m_resourceList = new ElaListWidget(this);
    m_resourceList->setObjectName(QStringLiteral("runtimeDialogResourceList"));

    const QString listStyle = QStringLiteral(
        "QListWidget {"
        "  background:%1;"
        "  border:1px solid %2;"
        "  border-radius:10px;"
        "  padding:4px;"
        "  color:%3;"
        "}"
        "QListWidget::item {"
        "  padding:8px;"
        "}")
                                  .arg(AppStyle::surfaceAlt(),
                                       AppStyle::border(),
                                       AppStyle::textPrimary());
    m_serviceList->setStyleSheet(listStyle);
    m_bindingList->setStyleSheet(listStyle);
    m_resourceList->setStyleSheet(listStyle);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);
    m_saveButton = buttons->button(QDialogButtonBox::Save);
    if (m_saveButton) {
        m_saveButton->setObjectName(QStringLiteral("runtimeDialogSaveButton"));
        m_saveButton->setText(QStringLiteral("\u4FDD\u5B58\u672C\u5730\u72B6\u6001"));
    }
    auto* closeButton = buttons->button(QDialogButtonBox::Close);
    if (closeButton) {
        closeButton->setText(QStringLiteral("\u5173\u95ED"));
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(addServiceButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::addServiceEntry);
    connect(removeServiceButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::removeSelectedServiceEntry);
    connect(selectServiceButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::setSelectionFromCurrentService);
    connect(addWorkspaceButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::addWorkspaceBinding);
    connect(addGroupButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::addGroupBinding);
    connect(removeBindingButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::removeSelectedBinding);
    connect(selectBindingButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::setSelectionFromCurrentBinding);
    connect(addResourceButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::addResourceEntry);
    connect(removeResourceButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::removeSelectedResource);
    connect(selectResourceButton, &QAbstractButton::clicked, this, &RuntimeArchitectureDialog::setSelectionFromCurrentResource);

    rootLayout->addWidget(title);
    rootLayout->addWidget(m_selectionChip, 0, Qt::AlignLeft);
    rootLayout->addWidget(m_summaryLabel);
    rootLayout->addWidget(m_detailLabel);
    rootLayout->addWidget(serviceTitle);
    rootLayout->addLayout(serviceActions);
    rootLayout->addWidget(m_serviceList);
    rootLayout->addWidget(bindingTitle);
    rootLayout->addLayout(bindingActions);
    rootLayout->addWidget(m_bindingList);
    rootLayout->addWidget(resourceTitle);
    rootLayout->addLayout(resourceActions);
    rootLayout->addWidget(m_resourceList, 1);
    rootLayout->addWidget(buttons);

    applySnapshot();
}

void RuntimeArchitectureDialog::setSnapshot(const RuntimeArchitectureSnapshot& snapshot)
{
    m_snapshot = snapshot;
    populateEditableStateFromSnapshot();
    applySnapshot();
}

void RuntimeArchitectureDialog::setEditableState(
    const QVector<ServiceRegistryEntry>& serviceRegistry,
    const QVector<WorkspaceServiceBindingSnapshot>& workspaceBindings,
    const QVector<GroupServiceBindingSnapshot>& groupBindings,
    const ServiceSelectionSnapshot& selection)
{
    m_serviceRegistry = serviceRegistry;
    m_workspaceBindings = workspaceBindings;
    m_groupBindings = groupBindings;
    m_selection = selection;
    applySnapshot();
}

QVector<ServiceRegistryEntry> RuntimeArchitectureDialog::editedServiceRegistry() const
{
    return m_serviceRegistry;
}

QVector<WorkspaceServiceBindingSnapshot> RuntimeArchitectureDialog::editedWorkspaceBindings() const
{
    return m_workspaceBindings;
}

QVector<GroupServiceBindingSnapshot> RuntimeArchitectureDialog::editedGroupBindings() const
{
    return m_groupBindings;
}

QVector<ResourceReference> RuntimeArchitectureDialog::editedResources() const
{
    return m_resources;
}

ServiceSelectionSnapshot RuntimeArchitectureDialog::editedSelection() const
{
    return m_selection;
}

void RuntimeArchitectureDialog::applySnapshot()
{
    RuntimeArchitectureSnapshot renderedSnapshot = m_snapshot;
    renderedSnapshot.discoveryResult.services.clear();
    for (const ServiceRegistryEntry& service : m_serviceRegistry) {
        renderedSnapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
            service.serviceId,
            service.serviceName,
            service.organizationName,
            service.environmentName,
            0,
            service.capabilities
        });
    }
    renderedSnapshot.discoveryResult.defaultServiceId = m_selection.serviceId;
    renderedSnapshot.discoveryResult.multipleServicesDetected =
        renderedSnapshot.discoveryResult.services.size() > 1;
    renderedSnapshot.serviceRegistry = m_serviceRegistry;
    renderedSnapshot.workspaceBindings = m_workspaceBindings;
    renderedSnapshot.groupBindings = m_groupBindings;
    renderedSnapshot.selection = m_selection;
    renderedSnapshot.visibleResources = m_resources;

    RuntimeArchitecturePresentation presentation =
        buildRuntimeArchitecturePresentation(renderedSnapshot);
    m_selectionChip->setText(presentation.panelBadge);
    m_summaryLabel->setText(presentation.panelSummary);
    m_detailLabel->setText(QStringLiteral("%1\n%2")
                               .arg(presentation.panelDetail,
                                    presentation.panelFootnote));

    m_serviceList->clear();
    for (const ServiceRegistryEntry& service : m_serviceRegistry) {
        auto* item = new QListWidgetItem(serviceDisplayText(service), m_serviceList);
        item->setData(IdRole, service.serviceId);
        item->setData(ServiceIdRole, service.serviceId);
    }

    m_bindingList->clear();
    for (const WorkspaceServiceBindingSnapshot& workspace : m_workspaceBindings) {
        auto* item = new QListWidgetItem(workspaceDisplayText(workspace), m_bindingList);
        item->setData(KindRole, QStringLiteral("workspace"));
        item->setData(IdRole, workspace.workspaceId);
        item->setData(WorkspaceIdRole, workspace.workspaceId);
    }
    for (const GroupServiceBindingSnapshot& group : m_groupBindings) {
        auto* item = new QListWidgetItem(groupDisplayText(group), m_bindingList);
        item->setData(KindRole, QStringLiteral("group"));
        item->setData(IdRole, group.groupId);
        item->setData(ServiceIdRole, group.binding.boundServiceId);
        item->setData(WorkspaceIdRole, group.primaryResource.workspaceId);
    }

    m_resourceList->clear();
    for (const auto& resource : m_resources) {
        auto* item = new QListWidgetItem(
            QStringLiteral("%1 · %2")
                .arg(resource.title.trimmed().isEmpty() ? resource.resourceId : resource.title,
                     resource.resourceKind.trimmed().isEmpty() ? QStringLiteral("resource")
                                                              : resource.resourceKind),
            m_resourceList);
        item->setData(ResourceIdRole, resource.resourceId);
        item->setData(ServiceIdRole, resource.serviceId);
        item->setData(WorkspaceIdRole, resource.workspaceId);
    }
}

void RuntimeArchitectureDialog::populateEditableStateFromSnapshot()
{
    m_serviceRegistry = m_snapshot.serviceRegistry;
    if (m_serviceRegistry.isEmpty()) {
        for (const auto& discovery : m_snapshot.discoveryResult.services) {
            m_serviceRegistry.push_back(ServiceRegistryEntry{
                discovery.serviceId,
                discovery.serviceName,
                discovery.organizationName,
                discovery.environmentName,
                QString(),
                0,
                false,
                discovery.capabilities
            });
        }
    }
    m_workspaceBindings = m_snapshot.workspaceBindings;
    m_groupBindings = m_snapshot.groupBindings;
    m_resources = m_snapshot.visibleResources;
    m_selection = m_snapshot.selection;
    rebuildSelection();
}

void RuntimeArchitectureDialog::rebuildSelection()
{
    auto findServiceName = [&](const QString& serviceId) {
        for (const ServiceRegistryEntry& entry : m_serviceRegistry) {
            if (entry.serviceId == serviceId) {
                return entry.serviceName.trimmed().isEmpty() ? entry.serviceId : entry.serviceName;
            }
        }
        return QString();
    };

    if (!m_selection.groupId.trimmed().isEmpty()) {
        for (const GroupServiceBindingSnapshot& group : m_groupBindings) {
            if (group.groupId == m_selection.groupId
                && group.binding.boundServiceId == m_selection.serviceId) {
                m_selection.serviceName = findServiceName(m_selection.serviceId);
                m_selection.bound = !m_selection.serviceId.trimmed().isEmpty();
                return;
            }
        }
    }

    for (const GroupServiceBindingSnapshot& group : m_groupBindings) {
        if (!group.binding.boundServiceId.trimmed().isEmpty()) {
            m_selection.workspaceId = group.primaryResource.workspaceId;
            m_selection.groupId = group.groupId;
            m_selection.serviceId = group.binding.boundServiceId;
            m_selection.serviceName = findServiceName(group.binding.boundServiceId);
            m_selection.selectionSource = QStringLiteral("group-binding");
            m_selection.selectedResource = group.primaryResource;
            m_selection.bound = true;
            return;
        }
    }

    if (!m_serviceRegistry.isEmpty()) {
        const ServiceRegistryEntry& first = m_serviceRegistry.front();
        m_selection.workspaceId.clear();
        m_selection.groupId.clear();
        m_selection.serviceId = first.serviceId;
        m_selection.serviceName = first.serviceName.trimmed().isEmpty() ? first.serviceId : first.serviceName;
        m_selection.selectionSource = QStringLiteral("registry");
        m_selection.selectedResource = {};
        m_selection.bound = false;
        return;
    }

    m_selection = {};
}

void RuntimeArchitectureDialog::addServiceEntry()
{
    bool ok = false;
    const QString serviceId = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u670D\u52A1"),
        QStringLiteral("\u670D\u52A1 ID"),
        QLineEdit::Normal,
        QString(),
        &ok).trimmed();
    if (!ok || serviceId.isEmpty()) {
        return;
    }

    const QString serviceName = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u670D\u52A1"),
        QStringLiteral("\u670D\u52A1\u540D\u79F0"),
        QLineEdit::Normal,
        serviceId,
        &ok).trimmed();
    if (!ok) {
        return;
    }

    const QString organizationName = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u670D\u52A1"),
        QStringLiteral("\u7EC4\u7EC7\u540D\u79F0"),
        QLineEdit::Normal,
        QStringLiteral("LeyoChat"),
        &ok).trimmed();
    if (!ok) {
        return;
    }

    const QString environmentName = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u670D\u52A1"),
        QStringLiteral("\u73AF\u5883\u6807\u8BC6"),
        QLineEdit::Normal,
        QStringLiteral("lan"),
        &ok).trimmed();
    if (!ok) {
        return;
    }

    for (ServiceRegistryEntry& entry : m_serviceRegistry) {
        if (entry.serviceId == serviceId) {
            entry.serviceName = serviceName;
            entry.organizationName = organizationName;
            entry.environmentName = environmentName;
            rebuildSelection();
            applySnapshot();
            return;
        }
    }

    m_serviceRegistry.push_back(ServiceRegistryEntry{
        serviceId,
        serviceName,
        organizationName,
        environmentName,
        QString(),
        0,
        false,
        {}
    });
    rebuildSelection();
    applySnapshot();
}

void RuntimeArchitectureDialog::removeSelectedServiceEntry()
{
    const QListWidgetItem* item = m_serviceList->currentItem();
    if (!item) {
        return;
    }

    const QString serviceId = item->data(IdRole).toString();
    for (int index = 0; index < m_serviceRegistry.size(); ++index) {
        if (m_serviceRegistry[index].serviceId == serviceId) {
            m_serviceRegistry.remove(index);
            break;
        }
    }

    for (GroupServiceBindingSnapshot& group : m_groupBindings) {
        if (group.binding.boundServiceId == serviceId) {
            group.binding.boundServiceId.clear();
            group.registryEntry = {};
            group.discoverySnapshot = {};
            group.primaryResource.serviceId.clear();
            group.enabled = false;
        }
    }

    rebuildSelection();
    applySnapshot();
}

void RuntimeArchitectureDialog::setSelectionFromCurrentService()
{
    const QListWidgetItem* item = m_serviceList->currentItem();
    if (!item) {
        return;
    }

    const QString serviceId = item->data(ServiceIdRole).toString();
    for (const ServiceRegistryEntry& entry : m_serviceRegistry) {
        if (entry.serviceId != serviceId) {
            continue;
        }

        m_selection.workspaceId.clear();
        m_selection.groupId.clear();
        m_selection.serviceId = entry.serviceId;
        m_selection.serviceName = entry.serviceName.trimmed().isEmpty() ? entry.serviceId : entry.serviceName;
        m_selection.selectionSource = QStringLiteral("registry");
        m_selection.registryEntry = entry;
        m_selection.discoverySnapshot = ServiceDiscoverySnapshot{
            entry.serviceId,
            entry.serviceName,
            entry.organizationName,
            entry.environmentName,
            0,
            entry.capabilities
        };
        m_selection.groupBinding = {};
        m_selection.selectedResource = {};
        m_selection.bound = false;
        applySnapshot();
        return;
    }
}

void RuntimeArchitectureDialog::addWorkspaceBinding()
{
    bool ok = false;
    const QString workspaceId = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u5DE5\u4F5C\u533A"),
        QStringLiteral("\u5DE5\u4F5C\u533A ID"),
        QLineEdit::Normal,
        QString(),
        &ok).trimmed();
    if (!ok || workspaceId.isEmpty()) {
        return;
    }

    const QString workspaceName = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u5DE5\u4F5C\u533A"),
        QStringLiteral("\u5DE5\u4F5C\u533A\u540D\u79F0"),
        QLineEdit::Normal,
        workspaceId,
        &ok).trimmed();
    if (!ok) {
        return;
    }

    for (WorkspaceServiceBindingSnapshot& binding : m_workspaceBindings) {
        if (binding.workspaceId == workspaceId) {
            binding.workspaceName = workspaceName;
            applySnapshot();
            return;
        }
    }

    m_workspaceBindings.push_back(WorkspaceServiceBindingSnapshot{workspaceId, workspaceName, {}});
    rebuildSelection();
    applySnapshot();
}

void RuntimeArchitectureDialog::addGroupBinding()
{
    bool ok = false;
    const QString groupId = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u7FA4\u7EC4\u7ED1\u5B9A"),
        QStringLiteral("\u7FA4 ID"),
        QLineEdit::Normal,
        QString(),
        &ok).trimmed();
    if (!ok || groupId.isEmpty()) {
        return;
    }

    const QString groupName = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u7FA4\u7EC4\u7ED1\u5B9A"),
        QStringLiteral("\u7FA4\u540D\u79F0"),
        QLineEdit::Normal,
        groupId,
        &ok).trimmed();
    if (!ok) {
        return;
    }

    QString workspaceId;
    if (!m_workspaceBindings.isEmpty()) {
        QStringList workspaceChoices;
        workspaceChoices.reserve(m_workspaceBindings.size());
        for (const WorkspaceServiceBindingSnapshot& binding : m_workspaceBindings) {
            workspaceChoices.push_back(
                QStringLiteral("%1|%2").arg(binding.workspaceId, binding.workspaceName));
        }
        const QString choice = QInputDialog::getItem(
            this,
            QStringLiteral("\u65B0\u589E\u7FA4\u7EC4\u7ED1\u5B9A"),
            QStringLiteral("\u6240\u5C5E\u5DE5\u4F5C\u533A"),
            workspaceChoices,
            0,
            false,
            &ok);
        if (!ok) {
            return;
        }
        workspaceId = choice.section(QLatin1Char('|'), 0, 0).trimmed();
    } else {
        workspaceId = QInputDialog::getText(
            this,
            QStringLiteral("\u65B0\u589E\u7FA4\u7EC4\u7ED1\u5B9A"),
            QStringLiteral("\u6240\u5C5E\u5DE5\u4F5C\u533A ID"),
            QLineEdit::Normal,
            QStringLiteral("workspace-default"),
            &ok).trimmed();
        if (!ok || workspaceId.isEmpty()) {
            return;
        }
    }

    QString serviceId;
    if (!m_serviceRegistry.isEmpty()) {
        QStringList serviceChoices;
        serviceChoices.reserve(m_serviceRegistry.size());
        for (const ServiceRegistryEntry& entry : m_serviceRegistry) {
            serviceChoices.push_back(
                QStringLiteral("%1|%2").arg(entry.serviceId,
                                            entry.serviceName.trimmed().isEmpty() ? entry.serviceId
                                                                                  : entry.serviceName));
        }
        const QString choice = QInputDialog::getItem(
            this,
            QStringLiteral("\u65B0\u589E\u7FA4\u7EC4\u7ED1\u5B9A"),
            QStringLiteral("\u7ED1\u5B9A\u670D\u52A1"),
            serviceChoices,
            0,
            false,
            &ok);
        if (!ok) {
            return;
        }
        serviceId = choice.section(QLatin1Char('|'), 0, 0).trimmed();
    }

    GroupServiceBindingSnapshot binding;
    binding.groupId = groupId;
    binding.groupName = groupName;
    binding.binding.boundServiceId = serviceId;
    binding.binding.sharedFilesEnabled = !serviceId.isEmpty();
    binding.primaryResource.serviceId = serviceId;
    binding.primaryResource.workspaceId = workspaceId;
    binding.primaryResource.resourceId = QStringLiteral("group:%1").arg(groupId);
    binding.primaryResource.resourceKind = QStringLiteral("shared_group");
    binding.primaryResource.title = groupName;
    binding.enabled = !serviceId.isEmpty();

    for (const ServiceRegistryEntry& entry : m_serviceRegistry) {
        if (entry.serviceId == serviceId) {
            binding.registryEntry = entry;
            binding.discoverySnapshot.serviceId = entry.serviceId;
            binding.discoverySnapshot.serviceName = entry.serviceName;
            binding.discoverySnapshot.organizationName = entry.organizationName;
            binding.discoverySnapshot.environmentName = entry.environmentName;
            break;
        }
    }

    bool replaced = false;
    for (GroupServiceBindingSnapshot& existing : m_groupBindings) {
        if (existing.groupId == groupId) {
            existing = binding;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        m_groupBindings.push_back(binding);
    }

    bool workspaceKnown = false;
    for (WorkspaceServiceBindingSnapshot& workspace : m_workspaceBindings) {
        if (workspace.workspaceId == workspaceId) {
            workspaceKnown = true;
            break;
        }
    }
    if (!workspaceKnown && !workspaceId.isEmpty()) {
        m_workspaceBindings.push_back(WorkspaceServiceBindingSnapshot{
            workspaceId,
            workspaceId,
            {}
        });
    }

    rebuildSelection();
    applySnapshot();
}

void RuntimeArchitectureDialog::removeSelectedBinding()
{
    const QListWidgetItem* item = m_bindingList->currentItem();
    if (!item) {
        return;
    }

    const QString kind = item->data(KindRole).toString();
    const QString id = item->data(IdRole).toString();
    if (kind == QStringLiteral("workspace")) {
        for (int index = 0; index < m_workspaceBindings.size(); ++index) {
            if (m_workspaceBindings[index].workspaceId == id) {
                m_workspaceBindings.remove(index);
                break;
            }
        }
        for (int index = m_groupBindings.size() - 1; index >= 0; --index) {
            if (m_groupBindings[index].primaryResource.workspaceId == id) {
                m_groupBindings.remove(index);
            }
        }
    } else if (kind == QStringLiteral("group")) {
        for (int index = 0; index < m_groupBindings.size(); ++index) {
            if (m_groupBindings[index].groupId == id) {
                m_groupBindings.remove(index);
                break;
            }
        }
    }

    rebuildSelection();
    applySnapshot();
}

void RuntimeArchitectureDialog::setSelectionFromCurrentBinding()
{
    const QListWidgetItem* item = m_bindingList->currentItem();
    if (!item) {
        return;
    }

    const QString kind = item->data(KindRole).toString();
    const QString id = item->data(IdRole).toString();
    if (kind == QStringLiteral("group")) {
        for (const GroupServiceBindingSnapshot& binding : m_groupBindings) {
            if (binding.groupId != id) {
                continue;
            }

            m_selection.workspaceId = binding.primaryResource.workspaceId;
            m_selection.groupId = binding.groupId;
            m_selection.serviceId = binding.binding.boundServiceId;
            m_selection.serviceName =
                binding.registryEntry.serviceName.trimmed().isEmpty()
                    ? binding.binding.boundServiceId
                    : binding.registryEntry.serviceName;
            m_selection.selectionSource = QStringLiteral("group-binding");
            m_selection.registryEntry = binding.registryEntry;
            m_selection.discoverySnapshot = binding.discoverySnapshot;
            m_selection.groupBinding = binding;
            m_selection.selectedResource = binding.primaryResource;
            m_selection.bound = !binding.binding.boundServiceId.trimmed().isEmpty();
            applySnapshot();
            return;
        }
        return;
    }

    if (kind != QStringLiteral("workspace")) {
        return;
    }

    const QString workspaceId = item->data(WorkspaceIdRole).toString();
    m_selection.workspaceId = workspaceId;
    m_selection.groupId.clear();
    m_selection.selectionSource = QStringLiteral("workspace-binding");
    m_selection.groupBinding = {};
    m_selection.selectedResource.workspaceId = workspaceId;
    m_selection.bound = false;
    m_selection.serviceId.clear();
    m_selection.serviceName.clear();
    m_selection.registryEntry = {};
    m_selection.discoverySnapshot = {};

    for (const GroupServiceBindingSnapshot& binding : m_groupBindings) {
        if (binding.primaryResource.workspaceId != workspaceId
            || binding.binding.boundServiceId.trimmed().isEmpty()) {
            continue;
        }

        m_selection.serviceId = binding.binding.boundServiceId;
        m_selection.serviceName =
            binding.registryEntry.serviceName.trimmed().isEmpty()
                ? binding.binding.boundServiceId
                : binding.registryEntry.serviceName;
        m_selection.registryEntry = binding.registryEntry;
        m_selection.discoverySnapshot = binding.discoverySnapshot;
        m_selection.bound = true;
        break;
    }
    applySnapshot();
}

void RuntimeArchitectureDialog::setSelectionFromCurrentResource()
{
    const QListWidgetItem* item = m_resourceList->currentItem();
    if (!item) {
        return;
    }

    const QString resourceId = item->data(ResourceIdRole).toString();
    const QString serviceId = item->data(ServiceIdRole).toString();
    const QString workspaceId = item->data(WorkspaceIdRole).toString();
    for (const ResourceReference& resource : m_resources) {
        if (resource.resourceId != resourceId) {
            continue;
        }

        m_selection.selectedResource = resource;
        m_selection.serviceId = serviceId;
        m_selection.workspaceId = workspaceId;
        m_selection.selectionSource = QStringLiteral("resource");
        m_selection.bound = !serviceId.trimmed().isEmpty();
        m_selection.groupId.clear();
        m_selection.groupBinding = {};
        for (const ServiceRegistryEntry& entry : m_serviceRegistry) {
            if (entry.serviceId != serviceId) {
                continue;
            }
            m_selection.serviceName =
                entry.serviceName.trimmed().isEmpty() ? entry.serviceId : entry.serviceName;
            m_selection.registryEntry = entry;
            m_selection.discoverySnapshot = ServiceDiscoverySnapshot{
                entry.serviceId,
                entry.serviceName,
                entry.organizationName,
                entry.environmentName,
                0,
                entry.capabilities
            };
            break;
        }
        applySnapshot();
        return;
    }
}

void RuntimeArchitectureDialog::addResourceEntry()
{
    bool ok = false;
    const QString resourceId = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u8D44\u6E90"),
        QStringLiteral("\u8D44\u6E90 ID"),
        QLineEdit::Normal,
        QString(),
        &ok).trimmed();
    if (!ok || resourceId.isEmpty()) {
        return;
    }

    const QString title = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u8D44\u6E90"),
        QStringLiteral("\u8D44\u6E90\u540D\u79F0"),
        QLineEdit::Normal,
        resourceId,
        &ok).trimmed();
    if (!ok) {
        return;
    }

    const QString resourceKind = QInputDialog::getText(
        this,
        QStringLiteral("\u65B0\u589E\u8D44\u6E90"),
        QStringLiteral("\u8D44\u6E90\u7C7B\u578B"),
        QLineEdit::Normal,
        QStringLiteral("shared_file"),
        &ok).trimmed();
    if (!ok) {
        return;
    }

    ResourceReference resource;
    resource.resourceId = resourceId;
    resource.title = title;
    resource.resourceKind = resourceKind;
    resource.serviceId = m_selection.serviceId;
    resource.workspaceId = m_selection.workspaceId;
    resource.origin = resource.serviceId.trimmed().isEmpty() ? ResourceOrigin::Local
                                                             : ResourceOrigin::Service;

    for (ResourceReference& existing : m_resources) {
        if (existing.resourceId != resource.resourceId) {
            continue;
        }
        existing = resource;
        applySnapshot();
        return;
    }

    m_resources.push_back(resource);
    applySnapshot();
}

void RuntimeArchitectureDialog::removeSelectedResource()
{
    const QListWidgetItem* item = m_resourceList->currentItem();
    if (!item) {
        return;
    }

    const QString resourceId = item->data(ResourceIdRole).toString();
    const auto it = std::remove_if(m_resources.begin(), m_resources.end(), [&](const ResourceReference& resource) {
        return resource.resourceId == resourceId;
    });
    if (it == m_resources.end()) {
        return;
    }

    m_resources.erase(it, m_resources.end());
    if (m_selection.selectedResource.resourceId == resourceId) {
        m_selection.selectedResource = {};
    }
    applySnapshot();
}
