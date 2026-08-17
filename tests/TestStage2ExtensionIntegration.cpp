#include <QtTest>

#include "architecture/PersistedRuntimeArchitectureLoader.h"
#include "architecture/RuntimeArchitectureQueryService.h"
#include "architecture/ServiceCapability.h"
#include "architecture/ServiceRegistryEntry.h"
#include "architecture/WorkspaceServiceBindingSnapshot.h"
#include "integrations/DevOpsAdapterContracts.h"
#include "integrations/OutlookAdapterContracts.h"
#include "integrations/SharedFileResourceContracts.h"
#include "services/GroupService.h"
#include "services/ResourceRefRouter.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/GroupRepository.h"
#include "storage/ServiceBindingRepository.h"
#include "storage/ServiceRegistryRepository.h"
#include "storage/ServiceResourceRepository.h"

class TestStage2ExtensionIntegration : public QObject {
    Q_OBJECT

private slots:
    void persistedSnapshot_roundTripsRegistryBindingsSelectionAndResources();
    void sharedFileReferenceFanOut_usesSharedFilePreviewLabel();
    void adapterPayloads_flowThroughResourceReferenceEnvelope();
};

void TestStage2ExtensionIntegration::persistedSnapshot_roundTripsRegistryBindingsSelectionAndResources()
{
    const QString connectionName = QStringLiteral("stage2-extension-integration");
    DatabaseManager databaseManager(QStringLiteral(":memory:"), connectionName);
    QVERIFY(databaseManager.open());

    ServiceRegistryRepository registryRepository(connectionName);
    ServiceBindingRepository bindingRepository(connectionName);
    ServiceResourceRepository resourceRepository(connectionName);

    ServiceRegistryEntry registry;
    registry.serviceId = QStringLiteral("svc-stage2");
    registry.serviceName = QStringLiteral("LeyoChat Service");
    registry.organizationName = QStringLiteral("LeyoChat Contributors");
    registry.environmentName = QStringLiteral("lan");
    registry.host = QStringLiteral("192.0.2.10");
    registry.port = 8443;
    registry.tlsEnabled = true;
    registry.capabilities.push_back(ServiceCapability{
        QStringLiteral("shared_files"),
        QStringLiteral("Shared Files"),
        QStringLiteral("1.0"),
        true,
    });
    QVERIFY(registryRepository.replaceRegistry({registry}, registry.serviceId, 1712810000000LL));

    WorkspaceServiceBindingSnapshot workspaceBinding;
    workspaceBinding.workspaceId = QStringLiteral("workspace-alpha");
    workspaceBinding.workspaceName = QStringLiteral("Alpha");

    GroupServiceBindingSnapshot groupBinding;
    groupBinding.groupId = QStringLiteral("group-001");
    groupBinding.groupName = QStringLiteral("阶段二交流群");
    groupBinding.binding.boundServiceId = registry.serviceId;
    groupBinding.binding.sharedFilesEnabled = true;
    groupBinding.enabled = true;
    groupBinding.registryEntry = registry;
    groupBinding.primaryResource = SharedFileResourceContracts::makeReference(SharedFileResource{
        registry.serviceId,
        workspaceBinding.workspaceId,
        QStringLiteral("shared-file-001"),
        QStringLiteral("Beta 安装包"),
        QStringLiteral("张小乐"),
        QStringLiteral("v0.1.3"),
        QStringLiteral("群共享文件"),
        QStringLiteral("shared://download/001"),
        QStringLiteral("shared://open/001"),
        4 * 1024 * 1024,
    });
    groupBinding.primaryResource.workspaceId = workspaceBinding.workspaceId;

    QVERIFY(bindingRepository.replaceWorkspaceBindings({workspaceBinding}));
    QVERIFY(bindingRepository.replaceGroupBindings({groupBinding}));

    ServiceSelectionSnapshot selection;
    selection.workspaceId = workspaceBinding.workspaceId;
    selection.groupId = groupBinding.groupId;
    selection.serviceId = registry.serviceId;
    selection.serviceName = registry.serviceName;
    selection.selectionSource = QStringLiteral("integration-test");
    selection.selectedResource = groupBinding.primaryResource;
    selection.bound = true;
    QVERIFY(bindingRepository.saveCurrentSelection(selection));

    const ResourceReference devopsResource = DevOpsAdapterContracts::makeWorkItemReference(DevOpsWorkItemResource{
        registry.serviceId,
        workspaceBinding.workspaceId,
        QStringLiteral("wi-2048"),
        QStringLiteral("LeyoChat Contributors"),
        QStringLiteral("LeyoChat"),
        QStringLiteral("Bug"),
        QStringLiteral("修复群文件卡住"),
        QStringLiteral("进行中"),
        QStringLiteral("侯晓刚"),
        QStringLiteral("https://devops.example/workitems/2048"),
        2048,
    });
    const ResourceReference sharedFileResource = groupBinding.primaryResource;
    QVERIFY(resourceRepository.replaceResources({sharedFileResource, devopsResource}));

    PersistedRuntimeArchitectureLoader loader(connectionName);
    const RuntimeArchitectureSnapshot snapshot = loader.loadSnapshot();
    RuntimeArchitectureQueryService query(snapshot);

    QCOMPARE(query.serviceNameForGroup(groupBinding.groupId), registry.serviceName);
    QCOMPARE(query.sharedFileResourcesForGroup(groupBinding.groupId).size(), 1);
    QCOMPARE(query.selectedResourceForGroup(groupBinding.groupId).resourceId, QStringLiteral("shared-file-001"));
    QCOMPARE(query.visibleResourcesForGroup(groupBinding.groupId).size(), 2);
}

void TestStage2ExtensionIntegration::sharedFileReferenceFanOut_usesSharedFilePreviewLabel()
{
    const QString connectionName = QStringLiteral("stage2-shared-file-fanout");
    DatabaseManager databaseManager(QStringLiteral(":memory:"), connectionName);
    QVERIFY(databaseManager.open());

    GroupRepository groupRepository(connectionName);
    ConversationRepository conversationRepository(connectionName);
    GroupService groupService(&groupRepository, &conversationRepository);

    Group group;
    QVERIFY(groupService.createGroup(QStringLiteral("owner-001"),
                                     QStringLiteral("共享文件组"),
                                     {QStringLiteral("user-002")},
                                     &group));

    const SharedFileResource resource{
        QStringLiteral("svc-stage2"),
        QStringLiteral("workspace-alpha"),
        QStringLiteral("shared-file-001"),
        QStringLiteral("Beta 安装包"),
        QStringLiteral("张小乐"),
        QStringLiteral("v0.1.3"),
        QStringLiteral("群共享文件"),
        QStringLiteral("shared://download/001"),
        QStringLiteral("shared://open/001"),
        4 * 1024 * 1024,
    };

    const auto envelopes = groupService.buildGroupSharedFileReferenceFanOut(
        QStringLiteral("owner-001"),
        QString::fromStdWString(group.groupId),
        resource);

    QCOMPARE(envelopes.size(), static_cast<std::size_t>(1));
    QCOMPARE(ResourceRefRouter::previewLabel(envelopes.front()), QStringLiteral("[共享文件] Beta 安装包"));
}

void TestStage2ExtensionIntegration::adapterPayloads_flowThroughResourceReferenceEnvelope()
{
    const auto devopsPayload = DevOpsAdapterContracts::makeBuildPayload(DevOpsBuildResource{
        QStringLiteral("svc-stage2"),
        QStringLiteral("workspace-alpha"),
        QStringLiteral("build-31"),
        QStringLiteral("LeyoChat Contributors"),
        QStringLiteral("LeyoChat"),
        QStringLiteral("Nightly Build"),
        QStringLiteral("refs/heads/main"),
        QStringLiteral("已完成"),
        QStringLiteral("乔志晓"),
        QStringLiteral("https://devops.example/builds/31"),
        31,
    });
    MessageEnvelope envelope = buildResourceReferenceEnvelope(QStringLiteral("msg-1"),
                                                              QStringLiteral("sender-1"),
                                                              QStringLiteral("target-1"),
                                                              QStringLiteral("conv-1"),
                                                              devopsPayload,
                                                              1712811000000LL);
    QCOMPARE(ResourceRefRouter::previewLabel(envelope), QStringLiteral("[DevOps 构建] Nightly Build"));

    const auto mailPayload = OutlookAdapterContracts::makeMailPayload(OutlookMailResource{
        QStringLiteral("svc-stage2"),
        QStringLiteral("workspace-alpha"),
        QStringLiteral("mail-1"),
        QStringLiteral("user@example.com"),
        QStringLiteral("阶段二排期"),
        QStringLiteral("张大乐"),
        QStringLiteral("今天 18:00"),
        QStringLiteral("https://outlook.example/mail/1"),
    });
    envelope = buildResourceReferenceEnvelope(QStringLiteral("msg-2"),
                                              QStringLiteral("sender-1"),
                                              QStringLiteral("target-1"),
                                              QStringLiteral("conv-1"),
                                              mailPayload,
                                              1712811000001LL);
    QCOMPARE(ResourceRefRouter::previewLabel(envelope), QStringLiteral("[Outlook 邮件] 阶段二排期"));
}

QTEST_MAIN(TestStage2ExtensionIntegration)
#include "TestStage2ExtensionIntegration.moc"
