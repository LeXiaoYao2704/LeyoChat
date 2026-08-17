#include <QtTest>

#include <QTemporaryDir>

#include "architecture/GroupServiceBindingSnapshot.h"
#include "architecture/PersistedRuntimeArchitectureLoader.h"
#include "architecture/RuntimeArchitectureQueryService.h"
#include "domain/ChatMessage.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "services/SharedFileResourceSync.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/ServiceBindingRepository.h"
#include "storage/ServiceResourceRepository.h"

namespace {
ChatMessage makeResourceRefMessage(const std::wstring& messageId,
                                   const QString& serviceId,
                                   const QString& workspaceId,
                                   const QString& resourceId,
                                   const QString& kind,
                                   const QString& title)
{
    ChatMessage message;
    message.messageId = messageId;
    message.conversationId = L"group-1";
    message.senderId = L"user-a";
    message.body = L"[共享资源]";
    message.createdAtMs = 1000;
    message.deliveryState = MessageDeliveryState::Received;
    message.messageType = L"resource_ref";
    message.payloadJson = QStringLiteral(
                              "{\"service_id\":\"%1\",\"workspace_id\":\"%2\","
                              "\"resource_id\":\"%3\",\"kind\":\"%4\","
                              "\"title\":\"%5\",\"snapshot_version\":\"v2\"}")
                              .arg(serviceId, workspaceId, resourceId, kind, title)
                              .toStdWString();
    return message;
}

void seedGroupBinding(ServiceBindingRepository& repository,
                      const QString& groupId,
                      const QString& workspaceId,
                      const QString& serviceId)
{
    GroupServiceBindingSnapshot binding;
    binding.groupId = groupId;
    binding.groupName = QStringLiteral("设计群");
    binding.binding.boundServiceId = serviceId;
    binding.binding.sharedFilesEnabled = true;
    binding.primaryResource.serviceId = serviceId;
    binding.primaryResource.workspaceId = workspaceId;
    binding.primaryResource.resourceId = QStringLiteral("group-root");
    binding.primaryResource.resourceKind = QStringLiteral("shared_group");
    binding.enabled = true;
    QVERIFY(repository.replaceGroupBindings({binding}));
}
}

class TestSharedFileResourceSync : public QObject {
    Q_OBJECT

private slots:
    void replay_populatesVisibleSharedFilesWhenBindingMatches();
    void replay_skipsMismatchedServiceOrWorkspace();
};

void TestSharedFileResourceSync::replay_populatesVisibleSharedFilesWhenBindingMatches()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = QStringLiteral("shared-file-replay-pass");
    DatabaseManager db(dir.filePath(QStringLiteral("chat.db")), conn);
    QVERIFY(db.open());

    ConversationRepository conversations(conn);
    ServiceBindingRepository bindings(conn);
    ServiceResourceRepository resources(conn);
    PersistedRuntimeArchitectureLoader loader(conn);

    QVERIFY(conversations.upsertConversation(
        ConversationSummary{L"group-1", L"设计群", L"", 1000}));
    QVERIFY(conversations.appendMessage(
        makeResourceRefMessage(L"msg-1",
                               QStringLiteral("remote-file-service"),
                               QStringLiteral("ws-1"),
                               QStringLiteral("file-1"),
                               QStringLiteral("shared_file"),
                               QStringLiteral("方案文档"))));

    seedGroupBinding(bindings, QStringLiteral("group-1"),
                     QStringLiteral("ws-1"),
                     QStringLiteral("remote-file-service"));

    GroupFileServiceConfig config;
    config.groupId = QStringLiteral("group-1");
    config.enabled = true;
    config.workspaceId = QStringLiteral("ws-1");

    QCOMPARE(SharedFileResourceSync::replaySharedFileResourcesForConversation(
                 QStringLiteral("group-1"),
                 QStringLiteral("remote-file-service"),
                 config,
                 conversations,
                 resources),
             1);

    const RuntimeArchitectureSnapshot snapshot = loader.loadSnapshot();
    RuntimeArchitectureQueryService query(snapshot);
    const auto sharedFiles = query.sharedFileResourcesForGroup(QStringLiteral("group-1"));
    QCOMPARE(sharedFiles.size(), 1);
    QCOMPARE(sharedFiles.front().resourceId, QStringLiteral("file-1"));
    QCOMPARE(sharedFiles.front().title, QStringLiteral("方案文档"));
}

void TestSharedFileResourceSync::replay_skipsMismatchedServiceOrWorkspace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = QStringLiteral("shared-file-replay-skip");
    DatabaseManager db(dir.filePath(QStringLiteral("chat.db")), conn);
    QVERIFY(db.open());

    ConversationRepository conversations(conn);
    ServiceBindingRepository bindings(conn);
    ServiceResourceRepository resources(conn);
    PersistedRuntimeArchitectureLoader loader(conn);

    QVERIFY(conversations.upsertConversation(
        ConversationSummary{L"group-1", L"设计群", L"", 1000}));
    QVERIFY(conversations.appendMessage(
        makeResourceRefMessage(L"msg-1",
                               QStringLiteral("foreign-service"),
                               QStringLiteral("ws-1"),
                               QStringLiteral("file-1"),
                               QStringLiteral("shared_file"),
                               QStringLiteral("外部文件"))));
    QVERIFY(conversations.appendMessage(
        makeResourceRefMessage(L"msg-2",
                               QStringLiteral("remote-file-service"),
                               QStringLiteral("ws-2"),
                               QStringLiteral("file-2"),
                               QStringLiteral("shared_file"),
                               QStringLiteral("错误工作区"))));

    seedGroupBinding(bindings, QStringLiteral("group-1"),
                     QStringLiteral("ws-1"),
                     QStringLiteral("remote-file-service"));

    GroupFileServiceConfig config;
    config.groupId = QStringLiteral("group-1");
    config.enabled = true;
    config.workspaceId = QStringLiteral("ws-1");

    QCOMPARE(SharedFileResourceSync::replaySharedFileResourcesForConversation(
                 QStringLiteral("group-1"),
                 QStringLiteral("remote-file-service"),
                 config,
                 conversations,
                 resources),
             0);

    const RuntimeArchitectureSnapshot snapshot = loader.loadSnapshot();
    RuntimeArchitectureQueryService query(snapshot);
    QVERIFY(query.sharedFileResourcesForGroup(QStringLiteral("group-1")).isEmpty());
}

QTEST_MAIN(TestSharedFileResourceSync)
#include "TestSharedFileResourceSync.moc"
