#include <QApplication>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QtTest/QTest>

#include "architecture/ResourceReference.h"
#include "architecture/RuntimeArchitectureSnapshot.h"
#include "architecture/ServiceDiscoverySnapshot.h"
#include "ui/RuntimeArchitectureDialog.h"

class TestRuntimeArchitectureDialog : public QObject {
    Q_OBJECT

private slots:
    void showsEmptySnapshotState()
    {
        RuntimeArchitectureDialog dialog;

        auto* summary = dialog.findChild<QLabel*>(QStringLiteral("runtimeDialogSummary"));
        auto* serviceList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogServiceList"));
        auto* bindingList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogBindingList"));
        auto* resourceList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogResourceList"));
        QVERIFY(summary != nullptr);
        QVERIFY(serviceList != nullptr);
        QVERIFY(bindingList != nullptr);
        QVERIFY(resourceList != nullptr);
        QVERIFY(!summary->text().trimmed().isEmpty());
        QCOMPARE(serviceList->count(), 0);
        QCOMPARE(bindingList->count(), 0);
        QCOMPARE(resourceList->count(), 0);
    }

    void rendersSnapshotDetails()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("Demo Org"),
            QStringLiteral("lan"),
            1712400000000LL,
            {}
        });
        snapshot.discoveryResult.defaultServiceId = QStringLiteral("svc-001");
        snapshot.selection.serviceId = QStringLiteral("svc-001");
        snapshot.selection.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selection.bound = true;

        WorkspaceServiceBindingSnapshot workspace;
        workspace.workspaceId = QStringLiteral("ws-001");
        workspace.workspaceName = QStringLiteral("\u8BBE\u8BA1\u5DE5\u4F5C\u533A");
        snapshot.workspaceBindings.push_back(workspace);

        GroupServiceBindingSnapshot group;
        group.groupId = QStringLiteral("group-001");
        group.groupName = QStringLiteral("\u8BBE\u8BA1\u7FA4");
        group.enabled = true;
        group.binding.boundServiceId = QStringLiteral("svc-001");
        snapshot.groupBindings.push_back(group);

        snapshot.visibleResources.push_back(ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("file-001"),
            QStringLiteral("shared_file"),
            QStringLiteral("\u65B9\u6848\u6587\u6863"),
            QStringLiteral("v1"),
            QStringLiteral("\u5171\u4EAB\u8D44\u6E90"),
            ResourceOrigin::Service
        });

        RuntimeArchitectureDialog dialog;
        dialog.setSnapshot(snapshot);

        auto* selectionChip = dialog.findChild<QLabel*>(QStringLiteral("runtimeDialogSelectionChip"));
        auto* serviceList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogServiceList"));
        auto* bindingList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogBindingList"));
        auto* resourceList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogResourceList"));
        QVERIFY(selectionChip != nullptr);
        QVERIFY(serviceList != nullptr);
        QVERIFY(bindingList != nullptr);
        QVERIFY(resourceList != nullptr);
        QVERIFY(!selectionChip->text().trimmed().isEmpty());
        QCOMPARE(serviceList->count(), 1);
        QCOMPARE(bindingList->count(), 2);
        QCOMPARE(resourceList->count(), 1);
        QVERIFY(serviceList->item(0)->text().contains(QStringLiteral("LeyoChat Service")));
        QVERIFY(bindingList->item(0)->text().contains(QStringLiteral("\u8BBE\u8BA1\u5DE5\u4F5C\u533A")));
        QVERIFY(resourceList->item(0)->text().contains(QStringLiteral("\u65B9\u6848\u6587\u6863")));
    }

    void exposesEditableStateAndSaveAction()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-100"),
            QStringLiteral("Planner Service"),
            QStringLiteral("Demo Org"),
            QStringLiteral("lan"),
            1712400000000LL,
            {}
        });
        snapshot.serviceRegistry.push_back(ServiceRegistryEntry{
            QStringLiteral("svc-100"),
            QStringLiteral("Planner Service"),
            QStringLiteral("Demo Org"),
            QStringLiteral("lan"),
            QStringLiteral("192.0.2.10"),
            static_cast<quint16>(8443),
            true,
            {}
        });
        snapshot.workspaceBindings.push_back(WorkspaceServiceBindingSnapshot{
            QStringLiteral("ws-100"),
            QStringLiteral("\u89C4\u5212\u5DE5\u4F5C\u533A"),
            {}
        });
        snapshot.groupBindings.push_back(GroupServiceBindingSnapshot{
            QStringLiteral("group-100"),
            QStringLiteral("\u89C4\u5212\u7FA4"),
            ServiceBinding{QStringLiteral("svc-100"), true, false, false},
            {},
            ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712400000000LL, {}},
            ResourceReference{
                QStringLiteral("svc-100"),
                QStringLiteral("ws-100"),
                QStringLiteral("group-res-100"),
                QStringLiteral("shared_group"),
                QStringLiteral("\u89C4\u5212\u7FA4"),
                QStringLiteral("v1"),
                QStringLiteral("\u7FA4\u7ED1\u5B9A"),
                ResourceOrigin::Service
            },
            true
        });
        snapshot.selection.workspaceId = QStringLiteral("ws-100");
        snapshot.selection.groupId = QStringLiteral("group-100");
        snapshot.selection.serviceId = QStringLiteral("svc-100");
        snapshot.selection.serviceName = QStringLiteral("Planner Service");
        snapshot.selection.bound = true;

        RuntimeArchitectureDialog dialog;
        dialog.setSnapshot(snapshot);

        auto* saveButton = dialog.findChild<QPushButton*>(QStringLiteral("runtimeDialogSaveButton"));
        QVERIFY(saveButton != nullptr);
        QCOMPARE(dialog.editedServiceRegistry().size(), 1);
        QCOMPARE(dialog.editedWorkspaceBindings().size(), 1);
        QCOMPARE(dialog.editedGroupBindings().size(), 1);
        QCOMPARE(dialog.editedSelection().serviceId, QStringLiteral("svc-100"));
        QCOMPARE(dialog.editedSelection().groupId, QStringLiteral("group-100"));
    }

    void allowsSettingSelectionFromServiceBindingAndResource()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-a"),
            QStringLiteral("Alpha Service"),
            QStringLiteral("Org A"),
            QStringLiteral("lan"),
            1712400000000LL,
            {}
        });
        snapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-b"),
            QStringLiteral("Beta Service"),
            QStringLiteral("Org B"),
            QStringLiteral("lan"),
            1712400000001LL,
            {}
        });
        snapshot.serviceRegistry = {
            ServiceRegistryEntry{
                QStringLiteral("svc-a"),
                QStringLiteral("Alpha Service"),
                QStringLiteral("Org A"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.10"),
                static_cast<quint16>(8443),
                true,
                {}
            },
            ServiceRegistryEntry{
                QStringLiteral("svc-b"),
                QStringLiteral("Beta Service"),
                QStringLiteral("Org B"),
                QStringLiteral("lan"),
                QStringLiteral("192.0.2.11"),
                static_cast<quint16>(8444),
                true,
                {}
            }
        };
        snapshot.workspaceBindings = {
            WorkspaceServiceBindingSnapshot{
                QStringLiteral("ws-a"),
                QStringLiteral("\u7532\u5DE5\u4F5C\u533A"),
                {}
            }
        };
        snapshot.groupBindings = {
            GroupServiceBindingSnapshot{
                QStringLiteral("group-a"),
                QStringLiteral("\u7532\u7FA4"),
                ServiceBinding{QStringLiteral("svc-a"), true, false, false},
                {},
                ServiceDiscoverySnapshot{QString(), QString(), QString(), QString(), 1712400000000LL, {}},
                ResourceReference{
                    QStringLiteral("svc-a"),
                    QStringLiteral("ws-a"),
                    QStringLiteral("group-res-a"),
                    QStringLiteral("shared_group"),
                    QStringLiteral("\u7532\u7FA4"),
                    QStringLiteral("v1"),
                    QStringLiteral("\u7532\u7FA4\u7ED1\u5B9A"),
                    ResourceOrigin::Service
                },
                true
            }
        };
        snapshot.visibleResources = {
            ResourceReference{
                QStringLiteral("svc-a"),
                QStringLiteral("ws-a"),
                QStringLiteral("res-a"),
                QStringLiteral("shared_file"),
                QStringLiteral("\u7532\u8D44\u6E90"),
                QStringLiteral("v1"),
                QStringLiteral("\u7532\u8D44\u6E90"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QStringLiteral("svc-b"),
                QStringLiteral("ws-a"),
                QStringLiteral("res-b"),
                QStringLiteral("shared_file"),
                QStringLiteral("\u4E59\u8D44\u6E90"),
                QStringLiteral("v1"),
                QStringLiteral("\u4E59\u8D44\u6E90"),
                ResourceOrigin::Service
            }
        };

        RuntimeArchitectureDialog dialog;
        dialog.setSnapshot(snapshot);

        auto* serviceList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogServiceList"));
        auto* bindingList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogBindingList"));
        auto* resourceList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogResourceList"));
        auto* selectServiceButton =
            dialog.findChild<QPushButton*>(QStringLiteral("runtimeDialogSelectServiceButton"));
        auto* selectBindingButton =
            dialog.findChild<QPushButton*>(QStringLiteral("runtimeDialogSelectBindingButton"));
        auto* selectResourceButton =
            dialog.findChild<QPushButton*>(QStringLiteral("runtimeDialogSelectResourceButton"));
        QVERIFY(serviceList != nullptr);
        QVERIFY(bindingList != nullptr);
        QVERIFY(resourceList != nullptr);
        QVERIFY(selectServiceButton != nullptr);
        QVERIFY(selectBindingButton != nullptr);
        QVERIFY(selectResourceButton != nullptr);

        serviceList->setCurrentRow(1);
        QTest::mouseClick(selectServiceButton, Qt::LeftButton);
        QCOMPARE(dialog.editedSelection().serviceId, QStringLiteral("svc-b"));
        QCOMPARE(dialog.editedSelection().selectionSource, QStringLiteral("registry"));
        QCOMPARE(dialog.editedSelection().bound, false);

        bindingList->setCurrentRow(1);
        QTest::mouseClick(selectBindingButton, Qt::LeftButton);
        QCOMPARE(dialog.editedSelection().groupId, QStringLiteral("group-a"));
        QCOMPARE(dialog.editedSelection().serviceId, QStringLiteral("svc-a"));
        QCOMPARE(dialog.editedSelection().selectionSource, QStringLiteral("group-binding"));
        QCOMPARE(dialog.editedSelection().bound, true);

        resourceList->setCurrentRow(1);
        QTest::mouseClick(selectResourceButton, Qt::LeftButton);
        QCOMPARE(dialog.editedSelection().selectedResource.resourceId, QStringLiteral("res-b"));
    }

    void supportsEditingResourceCatalog()
    {
        RuntimeArchitectureSnapshot snapshot;
        snapshot.serviceRegistry.push_back(ServiceRegistryEntry{
            QStringLiteral("svc-a"),
            QStringLiteral("Service A"),
            QStringLiteral("Org A"),
            QStringLiteral("lan"),
            QString(),
            0,
            false,
            {}
        });
        snapshot.visibleResources = {
            ResourceReference{
                QStringLiteral("svc-a"),
                QStringLiteral("ws-a"),
                QStringLiteral("res-a"),
                QStringLiteral("shared_file"),
                QStringLiteral("Spec A"),
                QStringLiteral("v1"),
                QStringLiteral("Resource A"),
                ResourceOrigin::Service
            },
            ResourceReference{
                QStringLiteral(),
                QStringLiteral(),
                QStringLiteral("local-a"),
                QStringLiteral("draft"),
                QStringLiteral("Local A"),
                QStringLiteral("v1"),
                QStringLiteral("Local resource"),
                ResourceOrigin::Local
            }
        };

        RuntimeArchitectureDialog dialog;
        dialog.setSnapshot(snapshot);

        auto* resourceList = dialog.findChild<QListWidget*>(QStringLiteral("runtimeDialogResourceList"));
        auto* removeResourceButton =
            dialog.findChild<QPushButton*>(QStringLiteral("runtimeDialogRemoveResourceButton"));
        QVERIFY(resourceList != nullptr);
        QVERIFY(removeResourceButton != nullptr);
        QCOMPARE(resourceList->count(), 2);

        resourceList->setCurrentRow(1);
        QTest::mouseClick(removeResourceButton, Qt::LeftButton);

        QCOMPARE(resourceList->count(), 1);
        QCOMPARE(dialog.editedResources().size(), 1);
        QCOMPARE(dialog.editedResources().front().resourceId, QStringLiteral("res-a"));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestRuntimeArchitectureDialog tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestRuntimeArchitectureDialog.moc"
