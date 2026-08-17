#include <QtTest/QTest>

#include "integrations/SharedFileResourceContracts.h"
#include "services/GroupService.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/GroupRepository.h"

class TestGroupResourceRefFanOut : public QObject {
    Q_OBJECT

private slots:
    void fansOutStructuredResourceRefsToAllRecipients()
    {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-resource-ref-fanout-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-resource-ref-fanout-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-resource-ref-fanout-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("架构扩展群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));

        ResourceRefPayload payload;
        payload.serviceId = QStringLiteral("svc-001");
        payload.workspaceId = QStringLiteral("ws-001");
        payload.origin = QStringLiteral("service");
        payload.kind = QStringLiteral("shared_file");
        payload.resourceId = QStringLiteral("res-file-001");
        payload.title = QStringLiteral("Spec Board");
        payload.subtitle = QStringLiteral("共享规格文件");
        payload.status = QStringLiteral("ready");
        payload.snapshotVersion = QStringLiteral("v3");
        payload.updatedAtMs = 1712800000000LL;

        const auto envelopes = service.buildGroupResourceReferenceFanOut(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            payload);

        QCOMPARE(envelopes.size(), static_cast<std::size_t>(2));
        QCOMPARE(envelopes.front().type, MessageType::ResourceReference);
        QCOMPARE(QString::fromUtf8(envelopes.front().messageSubtype.c_str()), QStringLiteral("resource_ref"));
        QCOMPARE(QString::fromUtf8(envelopes.front().resourceId.c_str()), payload.resourceId);
        QCOMPARE(QString::fromUtf8(envelopes.front().resourceTitle.c_str()), payload.title);
        QVERIFY(!QString::fromUtf8(envelopes.front().payloadJson.c_str()).trimmed().isEmpty());
        QCOMPARE(QString::fromUtf8(envelopes.front().targetId.c_str()), QStringLiteral("user-002"));
        QCOMPARE(QString::fromUtf8(envelopes.back().targetId.c_str()), QStringLiteral("user-003"));
    }

    void fansOutSharedFileResourceBuilderPayload()
    {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-shared-file-fanout-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-shared-file-fanout-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-shared-file-fanout-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("共享文件群"),
                                    {QStringLiteral("user-002")},
                                    &group));

        const SharedFileResource resource{
            QStringLiteral("svc-share"),
            QStringLiteral("workspace-share"),
            QStringLiteral("shared-file-001"),
            QStringLiteral("Beta 包"),
            QStringLiteral("张小乐"),
            QStringLiteral("v0.1.3"),
            QStringLiteral("阶段二共享文件"),
            QStringLiteral("shared://download/001"),
            QStringLiteral("shared://open/001"),
            1024 * 1024,
        };

        const auto envelopes = service.buildGroupSharedFileReferenceFanOut(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            resource);

        QCOMPARE(envelopes.size(), static_cast<std::size_t>(1));
        QCOMPARE(QString::fromUtf8(envelopes.front().messageSubtype.c_str()), QStringLiteral("resource_ref"));
        QCOMPARE(QString::fromUtf8(envelopes.front().resourceKind.c_str()), QStringLiteral("shared_file"));
        QVERIFY(QString::fromUtf8(envelopes.front().payloadJson.c_str()).contains(QStringLiteral("download")));
    }
};

QTEST_MAIN(TestGroupResourceRefFanOut)
#include "TestGroupResourceRefFanOut.moc"
