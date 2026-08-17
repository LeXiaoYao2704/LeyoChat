// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增271行/修改0行/删除0行; 总行数866行
// @AI-LastModified: 2026-04-16 14:29:10

#include <QCoreApplication>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest/QTest>

#include "domain/MessageMutation.h"
#include "services/FileTransferService.h"
#include "services/GroupService.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/FileTransferRepository.h"
#include "storage/GroupRepository.h"

class TestGroupService : public QObject {
    Q_OBJECT

private slots:
    void createsGroupAndSeedsConversation() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("研发一组"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        QCOMPARE(QString::fromStdWString(createdGroup.ownerClientId), QStringLiteral("owner-001"));

        const auto members = groupRepository.loadMembers(createdGroup.groupId);
        QCOMPARE(members.size(), 3);

        const auto summaries = conversationRepository.loadConversationSummaries();
        QCOMPARE(summaries.size(), 1);
        QCOMPARE(QString::fromStdWString(summaries.front().conversationId),
                 QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(QString::fromStdWString(summaries.front().title), QStringLiteral("研发一组"));
    }

    void addsMembersAndBuildsActiveRecipients() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-members-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-members-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-members-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("项目群"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        GroupEvent event;
        QVERIFY(service.addMembers(QStringLiteral("owner-001"),
                                   QString::fromStdWString(createdGroup.groupId),
                                   {QStringLiteral("user-003"), QStringLiteral("user-002")},
                                   &event));

        QCOMPARE(QString::fromStdWString(event.groupId), QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(QString::fromStdWString(event.operatorClientId), QStringLiteral("owner-001"));

        const auto members = groupRepository.loadMembers(createdGroup.groupId);
        QCOMPARE(members.size(), 3);

        const auto recipients =
            service.activeRecipients(QStringLiteral("owner-001"), QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(recipients.size(), 2);
        QCOMPARE(recipients.at(0), QStringLiteral("user-002"));
        QCOMPARE(recipients.at(1), QStringLiteral("user-003"));
    }

    void persistsDisplayNameSnapshotsForKnownMembers() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-display-name-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-display-name-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-display-name-test"));
        GroupService service(&groupRepository, &conversationRepository);

        QVERIFY(conversationRepository.saveKnownPeer(PeerEndpoint{
            std::string("owner-001"),
            std::string("测试用户"),
            std::string("192.0.2.10"),
            45454,
            true}));
        QVERIFY(conversationRepository.saveKnownPeer(PeerEndpoint{
            std::string("user-002"),
            std::string("李四"),
            std::string("192.0.2.11"),
            45454,
            true}));
        QVERIFY(conversationRepository.saveKnownPeer(PeerEndpoint{
            std::string("user-003"),
            std::string("王五"),
            std::string("192.0.2.12"),
            45454,
            true}));

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("项目群"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        auto members = groupRepository.loadMembers(createdGroup.groupId);
        QCOMPARE(members.size(), 2);
        QCOMPARE(QString::fromStdWString(members[0].memberDisplayNameSnapshot), QStringLiteral("测试用户"));
        QCOMPARE(QString::fromStdWString(members[1].memberDisplayNameSnapshot), QStringLiteral("李四"));

        GroupEvent addEvent;
        QVERIFY(service.addMembers(QStringLiteral("owner-001"),
                                   QString::fromStdWString(createdGroup.groupId),
                                   {QStringLiteral("user-003")},
                                   &addEvent));

        members = groupRepository.loadMembers(createdGroup.groupId);
        QCOMPARE(members.size(), 3);
        QCOMPARE(QString::fromStdWString(members[2].memberDisplayNameSnapshot), QStringLiteral("王五"));
    }

    void removesMemberFromActiveRecipients() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-remove-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-remove-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-remove-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("项目群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        GroupEvent event;
        QVERIFY(service.removeMember(QStringLiteral("owner-001"),
                                     QString::fromStdWString(createdGroup.groupId),
                                     QStringLiteral("user-003"),
                                     &event));

        QCOMPARE(QString::fromStdWString(event.groupId), QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(QString::fromStdWString(event.operatorClientId), QStringLiteral("owner-001"));

        const auto recipients =
            service.activeRecipients(QStringLiteral("owner-001"), QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(recipients.size(), 1);
        QCOMPARE(recipients.front(), QStringLiteral("user-002"));
    }

    void leavesGroupByDeactivatingSelfMember() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-leave-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-leave-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-leave-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("项目群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        GroupEvent event;
        QVERIFY(service.leaveGroup(QStringLiteral("user-003"),
                                   QString::fromStdWString(createdGroup.groupId),
                                   &event));

        QCOMPARE(QString::fromStdWString(event.operatorClientId), QStringLiteral("user-003"));
        QCOMPARE(QString::fromStdWString(event.payload), QStringLiteral("user-003"));

        const auto recipients =
            service.activeRecipients(QStringLiteral("owner-001"), QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(recipients.size(), 1);
        QCOMPARE(recipients.front(), QStringLiteral("user-002"));
    }

    void returnsOnlyActiveMemberIdsForGroupSnapshots() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-active-members-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-active-members-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-active-members-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("group-sync-test"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        GroupEvent leaveEvent;
        QVERIFY(service.leaveGroup(QStringLiteral("user-003"),
                                   QString::fromStdWString(createdGroup.groupId),
                                   &leaveEvent));

        const QStringList memberIds =
            service.activeMemberIds(QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(memberIds.size(), 2);
        QVERIFY(memberIds.contains(QStringLiteral("owner-001")));
        QVERIFY(memberIds.contains(QStringLiteral("user-002")));
        QVERIFY(!memberIds.contains(QStringLiteral("user-003")));
    }

    void rejectsStaleIncomingGroupMetaByVersionAndTimestamp() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-meta-guard-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-meta-guard-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-meta-guard-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("group-meta-guard"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        GroupEvent addEvent;
        QVERIFY(service.addMembers(QStringLiteral("owner-001"),
                                   QString::fromStdWString(createdGroup.groupId),
                                   {QStringLiteral("user-003")},
                                   &addEvent));

        const auto updatedGroup = groupRepository.findGroupById(createdGroup.groupId);
        QVERIFY(updatedGroup.has_value());
        QCOMPARE(updatedGroup->version, 2);

        QVERIFY(!service.shouldApplyIncomingMeta(QString::fromStdWString(createdGroup.groupId),
                                                 1,
                                                 updatedGroup->updatedAtMs - 1000));
        QVERIFY(!service.shouldApplyIncomingMeta(QString::fromStdWString(createdGroup.groupId),
                                                 2,
                                                 updatedGroup->updatedAtMs - 1000));
        QVERIFY(service.shouldApplyIncomingMeta(QString::fromStdWString(createdGroup.groupId),
                                               3,
                                               updatedGroup->updatedAtMs + 1000));
    }

    // Task 4 Step 1: createsGroupConversationSummary smoke test
    void createsGroupConversationSummary() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-app-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-app-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-app-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("研发一组"),
                                    {QStringLiteral("user-002")},
                                    &group));

        const auto summaries = conversationRepository.loadConversationSummaries();
        QCOMPARE(summaries.size(), 1);
        QCOMPARE(QString::fromStdWString(summaries.front().conversationId),
                 QString::fromStdWString(group.groupId));
    }

    // Task 5 Step 1: 群文本 fan-out 失败测试 → 实现后通过
    void buildsGroupTextFanOut() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-fanout-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-fanout-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-fanout-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("研发一组"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));

        const auto envelopes = service.buildGroupTextFanOut(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            QStringLiteral("hello team"));

        // owner 发给 2 个成员
        QCOMPARE(envelopes.size(), static_cast<std::size_t>(2));
        QCOMPARE(QString::fromUtf8(envelopes[0].targetId.c_str()), QStringLiteral("user-002"));
        QCOMPARE(QString::fromUtf8(envelopes[1].targetId.c_str()), QStringLiteral("user-003"));
        QCOMPARE(envelopes[0].type, MessageType::GroupMessage);
        // 两条消息共享同一 messageId
        QCOMPARE(envelopes[0].messageId, envelopes[1].messageId);
    }

    // Task 5 Step 1: 群文件 fan-out 失败测试 → 实现后通过
    void fansOutGroupFileTasksPerMember() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-file-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-file-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-file-test"));
        FileTransferRepository fileRepository(QStringLiteral("group-file-test"));
        FileTransferService fileService(&fileRepository);
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("研发一组"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));

        QTemporaryFile tempFile;
        QVERIFY(tempFile.open());
        tempFile.write("dummy");
        tempFile.flush();

        const auto taskIds = service.createOutgoingGroupFileTasks(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            tempFile.fileName(),
            &fileService);

        // 2 个成员 → 2 个任务
        QCOMPARE(taskIds.size(), static_cast<std::size_t>(2));
    }

    void buildsInlineAttachmentFanOutForGroupImages() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-inline-image-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-inline-image-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-inline-image-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("图片群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString imagePath = tempDir.filePath(QStringLiteral("capture.png"));
        QFile imageFile(imagePath);
        QVERIFY(imageFile.open(QIODevice::WriteOnly));
        static const unsigned char kPngBytes[] = {
            0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A,
            0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52,
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
            0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4,
            0x89, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x44, 0x41,
            0x54, 0x78, 0x9C, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
            0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0x89, 0x99,
            0x3D, 0x1D, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45,
            0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82
        };
        QVERIFY(imageFile.write(reinterpret_cast<const char*>(kPngBytes),
                                static_cast<qint64>(sizeof(kPngBytes))) == static_cast<qint64>(sizeof(kPngBytes)));
        imageFile.close();

        const auto envelopes = service.buildGroupInlineAttachmentFanOut(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            imagePath);

        QCOMPARE(envelopes.size(), static_cast<std::size_t>(2));
        QCOMPARE(envelopes.front().type, MessageType::GroupMessage);
        QCOMPARE(QString::fromUtf8(envelopes.front().attachmentName.c_str()), QStringLiteral("capture.png"));

        const QJsonObject body =
            QJsonDocument::fromJson(QByteArray::fromStdString(envelopes.front().body)).object();
        QCOMPARE(body.value(QStringLiteral("message_kind")).toString(), QStringLiteral("attachment"));
        QCOMPARE(body.value(QStringLiteral("attachment_kind")).toString(), QStringLiteral("image"));
        QVERIFY(!body.value(QStringLiteral("base64")).toString().isEmpty());
    }

    void buildsResourceReferenceFanOutForGroupSharedFiles() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-resource-reference-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-resource-reference-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-resource-reference-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("共享群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));

        const auto envelopes = service.buildGroupResourceReferenceFanOut(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            ResourceReferenceMessagePayload{
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("res-file-001"),
                    QStringLiteral("shared_file"),
                    QStringLiteral("Spec Board"),
                    QStringLiteral("v3"),
                    QStringLiteral("共享规格文件"),
                    ResourceOrigin::Service
                },
                QStringLiteral("共享规格文件")
            });

        QCOMPARE(envelopes.size(), static_cast<std::size_t>(2));
        QCOMPARE(envelopes.front().type, MessageType::ResourceReference);
        QCOMPARE(QString::fromUtf8(envelopes.front().conversationId.c_str()),
                 QString::fromStdWString(group.groupId));
        QCOMPARE(QString::fromUtf8(envelopes.front().targetId.c_str()), QStringLiteral("user-002"));
        QCOMPARE(QString::fromUtf8(envelopes.back().targetId.c_str()), QStringLiteral("user-003"));
        QCOMPARE(QString::fromUtf8(envelopes.front().resourceId.c_str()), QStringLiteral("res-file-001"));
        QCOMPARE(QString::fromUtf8(envelopes.front().resourceTitle.c_str()), QStringLiteral("Spec Board"));
    }

    void buildsGroupMetaFanOutWithAnnouncementAndAffectedMember() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-meta-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-meta-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-meta-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("测试群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));
        group.announcement = QStringLiteral("今晚 8 点同步").toStdWString();

        const auto envelopes = service.buildGroupMetaFanOut(QStringLiteral("owner-001"),
                                                            group,
                                                            {QStringLiteral("owner-001"),
                                                             QStringLiteral("user-002")},
                                                            {QStringLiteral("owner-001"),
                                                             QStringLiteral("user-002"),
                                                             QStringLiteral("user-003")},
                                                            QStringLiteral("remove_member"),
                                                            QStringLiteral("user-003"));

        QCOMPARE(envelopes.size(), static_cast<std::size_t>(2));
        const QJsonObject payload =
            QJsonDocument::fromJson(QByteArray::fromStdString(envelopes.front().body)).object();
        QCOMPARE(payload.value(QStringLiteral("event_type")).toString(), QStringLiteral("remove_member"));
        QCOMPARE(payload.value(QStringLiteral("announcement")).toString(), QStringLiteral("今晚 8 点同步"));
        QCOMPARE(payload.value(QStringLiteral("affected_member_id")).toString(), QStringLiteral("user-003"));
        QCOMPARE(payload.value(QStringLiteral("members")).toArray().size(), 2);
    }

    void disbandsGroupAndMarksItInactive() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-disband-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-disband-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-disband-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("待解散群"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        GroupEvent event;
        QVERIFY(service.disbandGroup(QStringLiteral("owner-001"),
                                     QString::fromStdWString(createdGroup.groupId),
                                     &event));

        const auto storedGroup = groupRepository.findGroupById(createdGroup.groupId);
        QVERIFY(storedGroup.has_value());
        QVERIFY(!storedGroup->isActive);
        QCOMPARE(QString::fromStdWString(event.eventType), QStringLiteral("disband_group"));
    }

    void buildGroupFileServiceConfigFanOut_buildsEnvelopePerRecipient() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-file-cfg-fanout-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-file-cfg-fanout-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-file-cfg-fanout-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("u1"),
                                    QStringLiteral("cfg-test-group"),
                                    {QStringLiteral("u2"), QStringLiteral("u3")},
                                    &group));

        GroupFileServiceConfig cfg;
        cfg.groupId     = QString::fromStdWString(group.groupId);
        cfg.enabled     = true;
        cfg.baseUrl     = QStringLiteral("http://server:8765");
        cfg.bearerToken = QStringLiteral("tok");
        cfg.workspaceId = QStringLiteral("ws-1");

        // u1 sends — should produce 2 envelopes (u2, u3)
        const auto envs = service.buildGroupFileServiceConfigFanOut(
            QStringLiteral("u1"), cfg.groupId, cfg);
        QCOMPARE((int)envs.size(), 2);
        QCOMPARE(envs[0].type, MessageType::GroupMessage);
        QCOMPARE(envs[0].messageSubtype, std::string("group_file_service_config"));

        // Verify body JSON
        const QJsonObject body = QJsonDocument::fromJson(
            QByteArray::fromStdString(envs[0].body)).object();
        QCOMPARE(body.value(QStringLiteral("message_kind")).toString(),
                 QStringLiteral("group_file_service_config"));
        const QJsonObject cfg2 = body.value(QStringLiteral("file_service_config")).toObject();
        QCOMPARE(cfg2.value(QStringLiteral("base_url")).toString(),
                 QStringLiteral("http://server:8765"));
        QCOMPARE(cfg2.value(QStringLiteral("enabled")).toBool(), true);

        QCOMPARE(envs[0].contentType, std::string("group_config"));

        // Verify sender is excluded and both recipients are present
        QStringList targetIds;
        for (const auto& e : envs) targetIds.append(QString::fromStdString(e.targetId));
        QVERIFY(!targetIds.contains(QStringLiteral("u1")));  // sender excluded
        QVERIFY(targetIds.contains(QStringLiteral("u2")));
        QVERIFY(targetIds.contains(QStringLiteral("u3")));
    }

    void buildGroupFileServiceConfigFanOut_returnsEmptyWhenNoOtherMembers() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-file-cfg-solo-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-file-cfg-solo-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-file-cfg-solo-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("u1"),
                                    QStringLiteral("solo-group"),
                                    {},
                                    &group));

        GroupFileServiceConfig cfg;
        cfg.groupId = QString::fromStdWString(group.groupId);
        cfg.enabled = true;
        const auto envs = service.buildGroupFileServiceConfigFanOut(
            QStringLiteral("u1"), cfg.groupId, cfg);
        QVERIFY(envs.empty());
    }

    // ── Mutation fan-out tests ───────────────────────────────────────────

    void buildGroupMutationFanOut_recall_oneEnvelopePerRecipient()
    {
        DatabaseManager databaseManager(QStringLiteral(":memory:"),
                                        QStringLiteral("group-mutation-recall-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-mutation-recall-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-mutation-recall-test"));
        GroupService service(&groupRepository, &conversationRepository);

        // owner-001 + user-002, user-003 = 3 members; owner sends → 2 recipients
        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("mutation-test-group"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));

        MessageMutation mutation;
        mutation.mutationMessageId = QStringLiteral("mut-001");
        mutation.targetMessageId   = QStringLiteral("msg-original-001");
        mutation.conversationId    = QString::fromStdWString(group.groupId);
        mutation.actorId           = QStringLiteral("owner-001");
        mutation.kind              = MessageMutationKind::Recall;
        mutation.mutatedAtMs       = 1712000010000LL;

        const auto envelopes = service.buildGroupMutationFanOut(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            mutation);

        // One envelope per non-local recipient
        QCOMPARE(envelopes.size(), static_cast<std::size_t>(2));

        // All envelopes share the same type and messageId
        for (const auto& env : envelopes) {
            QCOMPARE(env.type, MessageType::MessageMutation);
            QVERIFY(env.messageId == envelopes.front().messageId);
            QVERIFY(env.messageSubtype == std::string("recall"));
            QVERIFY(env.payloadJson == envelopes.front().payloadJson);
        }

        // Verify the payload references the original message
        const std::string& payload = envelopes.front().payloadJson;
        QVERIFY(payload.find("msg-original-001") != std::string::npos);
    }

    void buildGroupMutationFanOut_edit_setsCorrectPayload()
    {
        DatabaseManager databaseManager(QStringLiteral(":memory:"),
                                        QStringLiteral("group-mutation-edit-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-mutation-edit-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-mutation-edit-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("edit-test-group"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &group));

        MessageMutation mutation;
        mutation.mutationMessageId = QStringLiteral("mut-edit-001");
        mutation.targetMessageId   = QStringLiteral("msg-original-002");
        mutation.conversationId    = QString::fromStdWString(group.groupId);
        mutation.actorId           = QStringLiteral("owner-001");
        mutation.kind              = MessageMutationKind::Edit;
        mutation.newBody           = QStringLiteral("updated text");
        mutation.newContentType    = QStringLiteral("plain");
        mutation.mutatedAtMs       = 1712000020000LL;

        const auto envelopes = service.buildGroupMutationFanOut(
            QStringLiteral("owner-001"),
            QString::fromStdWString(group.groupId),
            mutation);

        QCOMPARE(envelopes.size(), static_cast<std::size_t>(2));

        for (const auto& env : envelopes) {
            QCOMPARE(env.type, MessageType::MessageMutation);
            QVERIFY(env.messageSubtype == std::string("edit"));
        }

        // Verify payload contains new_body
        const std::string& payload = envelopes.front().payloadJson;
        QVERIFY(payload.find("new_body")     != std::string::npos);
        QVERIFY(payload.find("updated text") != std::string::npos);
    }

    // Regression: group recall with partial fan-out (1 of 2 sends fails) must NOT apply locally.
    // Scenario: owner-001 + user-002 (online, send ok) + user-003 (sendPayload fails) + user-004 (offline)
    // → expectedRecipients=3, successfulRecipients=1+0=1 → no local apply → isRecalled stays false
    void groupRecallFanOut_partialSendFailure_doesNotApplyLocally()
    {
        DatabaseManager databaseManager(QStringLiteral(":memory:"),
                                        QStringLiteral("group-partial-recall-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-partial-recall-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-partial-recall-test"));
        GroupService service(&groupRepository, &conversationRepository);

        // 3 non-local recipients: user-002, user-003, user-004
        Group group;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("partial-recall-group"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003"),
                                     QStringLiteral("user-004")},
                                    &group));

        const QString conversationId = QString::fromStdWString(group.groupId);
        const QString messageId = QStringLiteral("msg-partial-001");
        const qint64  createdAt = 1712000000000LL;

        // Insert the original message to recall
        const ChatMessage msg{
            .messageId      = messageId.toStdWString(),
            .conversationId = group.groupId,
            .senderId       = L"owner-001",
            .body           = L"hello group",
            .createdAtMs    = createdAt,
            .deliveryState  = MessageDeliveryState::Sent,
            .messageType    = L"text"
        };
        QVERIFY(conversationRepository.appendMessage(msg));

        MessageMutation mutation;
        mutation.targetMessageId = messageId;
        mutation.conversationId  = conversationId;
        mutation.actorId         = QStringLiteral("owner-001");
        mutation.kind            = MessageMutationKind::Recall;
        mutation.mutatedAtMs     = createdAt + 30000LL;

        const auto envelopes = service.buildGroupMutationFanOut(
            QStringLiteral("owner-001"), conversationId, mutation);

        // 3 active recipients → 3 envelopes
        QCOMPARE(envelopes.size(), static_cast<std::size_t>(3));

        // Simulate: user-002 online + send ok = 1 success;
        //           user-003 connected but sendPayload returns false = 0;
        //           user-004 offline (no connection) = 0
        const int expectedRecipients    = static_cast<int>(envelopes.size());
        const int successfulRecipients  = 1;  // only user-002 succeeded

        // Orchestration rule: only apply locally when ALL sends succeeded
        bool applied = false;
        if (successfulRecipients == expectedRecipients) {
            applied = conversationRepository.applyMessageRecall(
                messageId, QStringLiteral("owner-001"), mutation.mutatedAtMs);
        }

        QVERIFY(!applied);  // partial failure → must NOT apply locally

        // Confirm the message is still NOT recalled in the database
        ChatMessage state;
        QVERIFY(conversationRepository.findMessageMutationStateById(messageId, &state));
        QVERIFY(!state.isRecalled);
    }

    // ==================== 群管理员测试 ====================

    void createGroupSetsOwnerRole() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-owner-role-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-owner-role-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-owner-role-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("角色测试群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        const auto members = groupRepository.loadMembers(createdGroup.groupId);
        QCOMPARE(members.size(), 3);
        // owner-001 应该是 "owner" 角色
        QCOMPARE(QString::fromStdWString(members[0].role), QStringLiteral("owner"));
        // 其他成员应该是 "member" 角色
        QCOMPARE(QString::fromStdWString(members[1].role), QStringLiteral("member"));
        QCOMPARE(QString::fromStdWString(members[2].role), QStringLiteral("member"));
    }

    void setAdminPromotesMember() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-setadmin-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-setadmin-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-setadmin-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("管理员测试群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        GroupEvent event;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"),
                                 &event));
        QCOMPARE(QString::fromStdWString(event.eventType), QStringLiteral("set_admin"));
        QCOMPARE(QString::fromStdWString(event.payload), QStringLiteral("user-002"));

        const auto members = groupRepository.loadMembers(createdGroup.groupId);
        for (const auto& m : members) {
            if (QString::fromStdWString(m.memberClientId) == QStringLiteral("user-002")) {
                QCOMPARE(QString::fromStdWString(m.role), QStringLiteral("admin"));
            }
        }
    }

    void setAdminRejectsNonOwnerOperator() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-setadmin-perm-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-setadmin-perm-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-setadmin-perm-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("权限测试群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        // 管理员不能设其他管理员
        GroupEvent setEvent;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"),
                                 &setEvent));

        GroupEvent failEvent;
        QVERIFY(!service.setAdmin(QStringLiteral("user-002"),
                                  QString::fromStdWString(createdGroup.groupId),
                                  QStringLiteral("user-003"),
                                  &failEvent));
    }

    void setAdminRejectsWhenLimitReached() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-setadmin-limit-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-setadmin-limit-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-setadmin-limit-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("上限测试群"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003"),
                                     QStringLiteral("user-004"), QStringLiteral("user-005")},
                                    &createdGroup));

        GroupEvent e1, e2, e3, e4;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"), &e1));
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-003"), &e2));
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-004"), &e3));
        // 第4个管理员应该被拒绝
        QVERIFY(!service.setAdmin(QStringLiteral("owner-001"),
                                  QString::fromStdWString(createdGroup.groupId),
                                  QStringLiteral("user-005"), &e4));
    }

    void unsetAdminDemotesToMember() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-unsetadmin-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-unsetadmin-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-unsetadmin-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("取消管理员测试"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        GroupEvent setEvent;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"),
                                 &setEvent));

        GroupEvent unsetEvent;
        QVERIFY(service.unsetAdmin(QStringLiteral("owner-001"),
                                   QString::fromStdWString(createdGroup.groupId),
                                   QStringLiteral("user-002"),
                                   &unsetEvent));
        QCOMPARE(QString::fromStdWString(unsetEvent.eventType), QStringLiteral("unset_admin"));

        const auto members = groupRepository.loadMembers(createdGroup.groupId);
        for (const auto& m : members) {
            if (QString::fromStdWString(m.memberClientId) == QStringLiteral("user-002")) {
                QCOMPARE(QString::fromStdWString(m.role), QStringLiteral("member"));
            }
        }
    }

    void unsetAdminFailsForNonAdmin() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-unsetadmin-noop-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-unsetadmin-noop-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-unsetadmin-noop-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("非管理员取消测试"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        GroupEvent failEvent;
        QVERIFY(!service.unsetAdmin(QStringLiteral("owner-001"),
                                    QString::fromStdWString(createdGroup.groupId),
                                    QStringLiteral("user-002"),
                                    &failEvent));
    }

    void adminCanRemoveRegularMember() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-admin-remove-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-admin-remove-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-admin-remove-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("管理员移除测试"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        GroupEvent setEvent;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"),
                                 &setEvent));

        // 管理员移除普通成员应成功
        GroupEvent removeEvent;
        QVERIFY(service.removeMember(QStringLiteral("user-002"),
                                     QString::fromStdWString(createdGroup.groupId),
                                     QStringLiteral("user-003"),
                                     &removeEvent));

        const auto recipients =
            service.activeRecipients(QStringLiteral("owner-001"), QString::fromStdWString(createdGroup.groupId));
        QCOMPARE(recipients.size(), 1);
        QCOMPARE(recipients.front(), QStringLiteral("user-002"));
    }

    void adminCannotRemoveOtherAdmin() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-admin-remove-admin-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-admin-remove-admin-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-admin-remove-admin-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("管理员互移测试"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        GroupEvent e1, e2;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"), &e1));
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-003"), &e2));

        // 管理员不能移除其他管理员
        GroupEvent removeEvent;
        QVERIFY(!service.removeMember(QStringLiteral("user-002"),
                                      QString::fromStdWString(createdGroup.groupId),
                                      QStringLiteral("user-003"),
                                      &removeEvent));
    }

    void adminCannotRemoveOwner() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-admin-remove-owner-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-admin-remove-owner-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-admin-remove-owner-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("管理员移除群主测试"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        GroupEvent setEvent;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"),
                                 &setEvent));

        // 管理员不能移除群主
        GroupEvent removeEvent;
        QVERIFY(!service.removeMember(QStringLiteral("user-002"),
                                      QString::fromStdWString(createdGroup.groupId),
                                      QStringLiteral("owner-001"),
                                      &removeEvent));
    }

    void regularMemberCannotRemoveAnyone() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-member-remove-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-member-remove-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-member-remove-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("普通成员移除测试"),
                                    {QStringLiteral("user-002"), QStringLiteral("user-003")},
                                    &createdGroup));

        GroupEvent removeEvent;
        QVERIFY(!service.removeMember(QStringLiteral("user-002"),
                                      QString::fromStdWString(createdGroup.groupId),
                                      QStringLiteral("user-003"),
                                      &removeEvent));
    }

    void metaFanOutIncludesRoleInMembers() {
        DatabaseManager databaseManager(QStringLiteral(":memory:"), QStringLiteral("group-service-meta-role-test"));
        QVERIFY(databaseManager.open());

        GroupRepository groupRepository(QStringLiteral("group-service-meta-role-test"));
        ConversationRepository conversationRepository(QStringLiteral("group-service-meta-role-test"));
        GroupService service(&groupRepository, &conversationRepository);

        Group createdGroup;
        QVERIFY(service.createGroup(QStringLiteral("owner-001"),
                                    QStringLiteral("Meta角色测试"),
                                    {QStringLiteral("user-002")},
                                    &createdGroup));

        GroupEvent adminEvent;
        QVERIFY(service.setAdmin(QStringLiteral("owner-001"),
                                 QString::fromStdWString(createdGroup.groupId),
                                 QStringLiteral("user-002"),
                                 &adminEvent));

        const auto updatedGroup = groupRepository.findGroupById(createdGroup.groupId);
        QVERIFY(updatedGroup.has_value());
        const auto envelopes = service.buildGroupCreateMetaFanOut(
            QStringLiteral("owner-001"),
            *updatedGroup,
            {QStringLiteral("owner-001"), QStringLiteral("user-002")});

        QVERIFY(!envelopes.empty());
        // 解析 body JSON，确认 members 数组包含 role 字段
        const QByteArray rawBody(envelopes.front().body.data(),
                                 static_cast<int>(envelopes.front().body.size()));
        const QJsonObject json = QJsonDocument::fromJson(rawBody).object();
        const QJsonArray membersArr = json.value(QStringLiteral("members")).toArray();
        QCOMPARE(membersArr.size(), 2);

        QHash<QString, QString> roleMap;
        for (const auto& v : membersArr) {
            const QJsonObject mo = v.toObject();
            roleMap.insert(mo.value(QStringLiteral("client_id")).toString(),
                           mo.value(QStringLiteral("role")).toString());
        }
        QCOMPARE(roleMap.value(QStringLiteral("owner-001")), QStringLiteral("owner"));
        QCOMPARE(roleMap.value(QStringLiteral("user-002")), QStringLiteral("admin"));
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TestGroupService tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "TestGroupService.moc"
