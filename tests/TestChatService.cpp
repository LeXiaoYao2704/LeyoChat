#include <QtTest/QTest>
#include <QApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "architecture/ResourceReferenceMessage.h"
#include "domain/MessageMutation.h"
#include "services/ChatService.h"
#include "services/DirectConversationAddressing.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

class TestChatService : public QObject {
    Q_OBJECT

private slots:
    void storesIncomingDirectResourceReferencePreview()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-direct-resource-reference");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("chat-service.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const MessageEnvelope envelope = buildResourceReferenceEnvelope(
            QStringLiteral("resource-direct-001"),
            QStringLiteral("peer-a"),
            QStringLiteral("local-a"),
            QStringLiteral("peer-a|local-a"),
            ResourceReferenceMessagePayload{
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("res-001"),
                    QStringLiteral("shared_file"),
                    QStringLiteral("设计文档"),
                    QStringLiteral("v3"),
                    QStringLiteral("共享设计文档"),
                    ResourceOrigin::Service
                },
                QStringLiteral("共享设计文档")
            },
            1712800000000LL);

        QVERIFY(ChatService::storeIncomingEnvelope(QStringLiteral("local-a"), &repository, envelope));

        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(QStringLiteral("local-a"),
                                                                 QStringLiteral("peer-a"));
        const auto messages = ChatService::loadMessages(&repository, conversationId);
        QCOMPARE(messages.size(), std::size_t(1));
        QCOMPARE(QString::fromStdWString(messages.front().body),
                 QStringLiteral("[共享资源] 设计文档"));
    }

    void duplicateIncomingDirectMessage_isTreatedAsHandledForAckReplay()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-direct-duplicate");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("duplicate-direct.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);

        MessageEnvelope envelope;
        envelope.messageId = "dup-direct-001";
        envelope.type = MessageType::ChatText;
        envelope.senderId = "peer-a";
        envelope.targetId = "local-a";
        envelope.body = "hello";
        envelope.createdAtMs = 1712800002000LL;

        QVERIFY(ChatService::storeIncomingEnvelope(QStringLiteral("local-a"), &repository, envelope));
        QVERIFY(ChatService::storeIncomingEnvelope(QStringLiteral("local-a"), &repository, envelope));

        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(QStringLiteral("local-a"),
                                                                 QStringLiteral("peer-a"));
        const auto messages = ChatService::loadMessages(&repository, conversationId);
        QCOMPARE(messages.size(), std::size_t(1));
    }

    void receiptReceived_promotesPendingOutgoingMessageToReceived()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-receipt-received");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("receipt-received.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(QStringLiteral("local-a"),
                                                                 QStringLiteral("peer-a"));
        const QString messageId = ChatService::createOutgoingMessage(QStringLiteral("local-a"),
                                                                     &repository,
                                                                     conversationId,
                                                                     QStringLiteral("peer-a"),
                                                                     QStringLiteral("hello"));
        QVERIFY(!messageId.isEmpty());

        auto pending = repository.loadPendingOutgoingMessages(conversationId.toStdWString(),
                                                              QStringLiteral("local-a").toStdWString());
        QCOMPARE(pending.size(), std::size_t(1));

        MessageEnvelope receipt;
        receipt.messageId = messageId.toStdString();
        receipt.type = MessageType::ReceiptReceived;
        receipt.senderId = "peer-a";
        receipt.targetId = "local-a";
        receipt.conversationId = conversationId.toStdString();
        receipt.createdAtMs = 1712800003000LL;

        QVERIFY(ChatService::storeIncomingEnvelope(QStringLiteral("local-a"), &repository, receipt));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
        pending = repository.loadPendingOutgoingMessages(conversationId.toStdWString(),
                                                         QStringLiteral("local-a").toStdWString());
        QVERIFY(pending.empty());
    }

    void receiptReceived_beforeOutgoingMessageIsStored_isAppliedWhenMessageAppears()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-receipt-before-message");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("receipt-before-message.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString localClientId = QStringLiteral("local-a");
        const QString peerId = QStringLiteral("peer-a");
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, peerId);
        const QString messageId = QStringLiteral("late-outgoing-001");

        MessageEnvelope receipt;
        receipt.messageId = messageId.toStdString();
        receipt.type = MessageType::ReceiptReceived;
        receipt.senderId = peerId.toStdString();
        receipt.targetId = localClientId.toStdString();
        receipt.conversationId = conversationId.toStdString();
        receipt.createdAtMs = 1712800003050LL;

        QVERIFY(ChatService::storeIncomingEnvelope(localClientId, &repository, receipt));

        ChatMessage outgoing;
        outgoing.messageId = messageId.toStdWString();
        outgoing.conversationId = conversationId.toStdWString();
        outgoing.senderId = localClientId.toStdWString();
        outgoing.body = QStringLiteral("hello").toStdWString();
        outgoing.createdAtMs = 1712800003000LL;
        outgoing.deliveryState = MessageDeliveryState::Pending;
        QVERIFY(repository.appendMessage(outgoing));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
    }

    void receiptReceived_acceptsLegacyConversationIdOrderingForExistingMessage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-receipt-legacy-conversation");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("receipt-legacy-conversation.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString localClientId = QStringLiteral("local-a");
        const QString peerId = QStringLiteral("peer-a");
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(localClientId, peerId);
        const QString messageId = ChatService::createOutgoingMessage(localClientId,
                                                                     &repository,
                                                                     conversationId,
                                                                     peerId,
                                                                     QStringLiteral("hello"));
        QVERIFY(!messageId.isEmpty());

        MessageEnvelope receipt;
        receipt.messageId = messageId.toStdString();
        receipt.type = MessageType::ReceiptReceived;
        receipt.senderId = peerId.toStdString();
        receipt.targetId = localClientId.toStdString();
        receipt.conversationId = QStringLiteral("%1|%2").arg(peerId, localClientId).toStdString();
        receipt.createdAtMs = 1712800003060LL;

        QVERIFY(ChatService::storeIncomingEnvelope(localClientId, &repository, receipt));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
    }

    void markMessageServerAcked_promotesPendingOutgoingMessage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-server-acked");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("server-acked.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(QStringLiteral("local-a"),
                                                                 QStringLiteral("peer-a"));
        const QString messageId = ChatService::createOutgoingMessage(QStringLiteral("local-a"),
                                                                     &repository,
                                                                     conversationId,
                                                                     QStringLiteral("peer-a"),
                                                                     QStringLiteral("hello"));
        QVERIFY(!messageId.isEmpty());

        QVERIFY(ChatService::markMessageServerAcked(&repository, messageId));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
    }

    void receiptReceived_forDifferentTargetIsIgnored()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-receipt-wrong-target");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("receipt-wrong-target.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(QStringLiteral("local-a"),
                                                                 QStringLiteral("peer-a"));
        const QString messageId = ChatService::createOutgoingMessage(QStringLiteral("local-a"),
                                                                     &repository,
                                                                     conversationId,
                                                                     QStringLiteral("peer-a"),
                                                                     QStringLiteral("hello"));
        QVERIFY(!messageId.isEmpty());

        MessageEnvelope receipt;
        receipt.messageId = messageId.toStdString();
        receipt.type = MessageType::ReceiptReceived;
        receipt.senderId = "peer-a";
        receipt.targetId = "other-local";
        receipt.conversationId = conversationId.toStdString();
        receipt.createdAtMs = 1712800003100LL;

        QVERIFY(!ChatService::storeIncomingEnvelope(QStringLiteral("local-a"), &repository, receipt));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void receiptReceived_forGroupConversationDoesNotPromoteWholeMessage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-receipt-group");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("receipt-group.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString messageId = QStringLiteral("group-outgoing-001");
        ChatMessage outgoing;
        outgoing.messageId = messageId.toStdWString();
        outgoing.conversationId = QStringLiteral("group-001").toStdWString();
        outgoing.senderId = QStringLiteral("local-a").toStdWString();
        outgoing.body = QStringLiteral("hello group").toStdWString();
        outgoing.createdAtMs = 1712800003200LL;
        outgoing.deliveryState = MessageDeliveryState::Sent;
        QVERIFY(repository.appendMessage(outgoing));

        MessageEnvelope receipt;
        receipt.messageId = messageId.toStdString();
        receipt.type = MessageType::ReceiptReceived;
        receipt.senderId = "peer-a";
        receipt.targetId = "local-a";
        receipt.conversationId = "group-001";
        receipt.createdAtMs = 1712800003300LL;

        QVERIFY(!ChatService::storeIncomingEnvelope(QStringLiteral("local-a"), &repository, receipt));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void receiptRead_forGroupConversationRecordsReaderWithoutPromotingWholeMessage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-read-receipt-group");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("read-receipt-group.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QSqlQuery groupQuery(QSqlDatabase::database(connectionName, false));
        QVERIFY(groupQuery.exec(QStringLiteral(
            "INSERT INTO groups "
            "(group_id, group_name, owner_client_id, version, created_at_ms, updated_at_ms, is_active) "
            "VALUES ('group-001', 'group-read-test', 'local-a', 1, 1712800003300, 1712800003300, 1)")));

        const QString messageId = QStringLiteral("group-read-001");
        ChatMessage outgoing;
        outgoing.messageId = messageId.toStdWString();
        outgoing.conversationId = QStringLiteral("group-001").toStdWString();
        outgoing.senderId = QStringLiteral("local-a").toStdWString();
        outgoing.body = QStringLiteral("hello group").toStdWString();
        outgoing.createdAtMs = 1712800003400LL;
        outgoing.deliveryState = MessageDeliveryState::Sent;
        QVERIFY(repository.appendMessage(outgoing));

        MessageEnvelope receipt;
        receipt.messageId = messageId.toStdString();
        receipt.type = MessageType::ReceiptRead;
        receipt.senderId = "peer-a";
        receipt.targetId = "local-a";
        receipt.conversationId = "group-001";
        receipt.createdAtMs = 1712800003500LL;

        QVERIFY(ChatService::storeIncomingEnvelope(QStringLiteral("local-a"), &repository, receipt));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);

        const auto receipts = repository.loadReadReceiptsForMessage(messageId);
        QCOMPARE(receipts.size(), 1);
        QCOMPARE(receipts.front().first, QStringLiteral("peer-a"));
        QCOMPARE(receipts.front().second, qint64(1712800003500LL));
    }

    void storesIncomingGroupResourceReferencePreview()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-group-resource-reference");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("chat-service-group.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const MessageEnvelope envelope = buildResourceReferenceEnvelope(
            QStringLiteral("resource-group-001"),
            QStringLiteral("peer-a"),
            QStringLiteral("group-001"),
            QStringLiteral("group-001"),
            ResourceReferenceMessagePayload{
                ResourceReference{
                    QStringLiteral("svc-001"),
                    QStringLiteral("ws-001"),
                    QStringLiteral("res-001"),
                    QStringLiteral("shared_file"),
                    QStringLiteral("群文件索引"),
                    QStringLiteral("v1"),
                    QStringLiteral("共享群文件"),
                    ResourceOrigin::Service
                },
                QStringLiteral("共享群文件")
            },
            1712800001000LL);

        QVERIFY(ChatService::storeIncomingGroupEnvelope(&repository,
                                                        envelope,
                                                        QStringLiteral("group-001"),
                                                        QStringLiteral("研发讨论组")));

        const auto messages = ChatService::loadMessages(&repository, QStringLiteral("group-001"));
        QCOMPARE(messages.size(), std::size_t(1));
        QCOMPARE(QString::fromStdWString(messages.front().body),
                 QStringLiteral("[共享资源] 群文件索引"));
    }

    void persistOutgoingGroupFanOut_rejectsInvalidPendingAndRollsBack()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-group-fanout-invalid-pending");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("fanout-invalid-pending.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        MessageEnvelope selfEnvelope;
        selfEnvelope.messageId = "group-msg-atomic-001";
        selfEnvelope.type = MessageType::GroupMessage;
        selfEnvelope.senderId = "local-a";
        selfEnvelope.targetId = "peer-a";
        selfEnvelope.conversationId = "group-001";
        selfEnvelope.body = R"({"message_kind":"text","text":"hello group"})";
        selfEnvelope.createdAtMs = 1712800004000LL;

        std::vector<ChatService::PendingGroupFanOutEnvelope> pending;
        pending.push_back(ChatService::PendingGroupFanOutEnvelope{
            QStringLiteral("peer-a"), QByteArrayLiteral("payload-a"), 1712800004000LL});
        pending.push_back(ChatService::PendingGroupFanOutEnvelope{
            QString(), QByteArrayLiteral("payload-invalid"), 1712800004001LL});

        QVERIFY(!ChatService::persistOutgoingGroupFanOut(&repository,
                                                         QStringLiteral("group-001"),
                                                         QStringLiteral("研发群"),
                                                         pending,
                                                         selfEnvelope));

        QVERIFY(repository.loadPendingGroupEnvelopes(QStringLiteral("peer-a"), 10).empty());
        ChatMessage stored;
        QVERIFY(!repository.findMessageById(QStringLiteral("group-msg-atomic-001"), &stored));
    }

    void persistOutgoingGroupFanOut_rollsBackPendingWhenSelfStoreFails()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-group-fanout-invalid-self");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("fanout-invalid-self.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QSqlQuery triggerQuery(QSqlDatabase::database(connectionName, false));
        QVERIFY(triggerQuery.exec(QStringLiteral(R"(
            CREATE TRIGGER fail_group_fanout_self_insert
            BEFORE INSERT ON messages
            WHEN NEW.message_id = 'group-msg-atomic-rollback'
            BEGIN
                SELECT RAISE(ABORT, 'forced self message insert failure');
            END
        )")));

        MessageEnvelope invalidSelfEnvelope;
        invalidSelfEnvelope.messageId = "group-msg-atomic-rollback";
        invalidSelfEnvelope.type = MessageType::GroupMessage;
        invalidSelfEnvelope.senderId = "local-a";
        invalidSelfEnvelope.targetId = "peer-a";
        invalidSelfEnvelope.conversationId = "group-001";
        invalidSelfEnvelope.body = R"({"message_kind":"text","text":"hello group"})";
        invalidSelfEnvelope.createdAtMs = 1712800004100LL;

        std::vector<ChatService::PendingGroupFanOutEnvelope> pending;
        pending.push_back(ChatService::PendingGroupFanOutEnvelope{
            QStringLiteral("peer-a"), QByteArrayLiteral("payload-a"), 1712800004100LL});
        pending.push_back(ChatService::PendingGroupFanOutEnvelope{
            QStringLiteral("peer-b"), QByteArrayLiteral("payload-b"), 1712800004100LL});

        QVERIFY(!ChatService::persistOutgoingGroupFanOut(&repository,
                                                         QStringLiteral("group-001"),
                                                         QStringLiteral("研发群"),
                                                         pending,
                                                         invalidSelfEnvelope));

        QVERIFY(repository.loadPendingGroupEnvelopes(QStringLiteral("peer-a"), 10).empty());
        QVERIFY(repository.loadPendingGroupEnvelopes(QStringLiteral("peer-b"), 10).empty());
    }

    void persistPendingGroupFanOutOnly_persistsWithoutTouchingSelfMessage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-pending-only");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("pending-only.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        std::vector<ChatService::PendingGroupFanOutEnvelope> pending;
        pending.push_back(ChatService::PendingGroupFanOutEnvelope{
            QStringLiteral("peer-a"), QByteArrayLiteral("payload-a"), 1712800004200LL});
        pending.push_back(ChatService::PendingGroupFanOutEnvelope{
            QStringLiteral("peer-b"), QByteArrayLiteral("payload-b"), 1712800004201LL});

        QVERIFY(ChatService::persistPendingGroupFanOutOnly(&repository,
                                                           QStringLiteral("group-001"),
                                                           pending));

        QCOMPARE(repository.loadPendingGroupEnvelopes(QStringLiteral("peer-a"), 10).size(),
                 std::size_t(1));
        QCOMPARE(repository.loadPendingGroupEnvelopes(QStringLiteral("peer-b"), 10).size(),
                 std::size_t(1));

        ChatMessage stored;
        QVERIFY(!repository.findMessageById(QStringLiteral("payload-a"), &stored));
    }

    void buildEnvelope_restoresNudgeContentTypeForRetry()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-nudge-retry");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("nudge-retry.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(QStringLiteral("local-a"),
                                                                 QStringLiteral("peer-a"));
        const QString messageId =
            ChatService::createOutgoingMessage(QStringLiteral("local-a"),
                                               &repository,
                                               conversationId,
                                               QStringLiteral("peer-a"),
                                               QStringLiteral("【窗口抖动提醒】"));
        QVERIFY(!messageId.isEmpty());
        QVERIFY(repository.updateMessageFields(messageId,
                                               QStringLiteral("nudge"),
                                               QString()));

        MessageEnvelope envelope;
        QVERIFY(ChatService::buildEnvelope(QStringLiteral("local-a"),
                                           &repository,
                                           messageId,
                                           QStringLiteral("peer-a"),
                                           &envelope));

        QCOMPARE(envelope.type, MessageType::ChatText);
        QCOMPARE(envelope.contentType, std::string("nudge"));
    }

    void buildEnvelope_reportsWhyPersistedTextCannotBeRebuilt()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-envelope-error");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("envelope-error.db")),
                                connectionName);
        QVERIFY(manager.open());
        ConversationRepository repository(connectionName);

        MessageEnvelope envelope;
        QString errorMessage;
        QVERIFY(!ChatService::buildEnvelope(QStringLiteral("local-a"),
                                            &repository,
                                            QStringLiteral("missing-message"),
                                            QStringLiteral("peer-a"),
                                            &envelope,
                                            &errorMessage));
        QCOMPARE(errorMessage, QStringLiteral("persisted message record was not found"));

        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(
                QStringLiteral("local-a"), QStringLiteral("peer-a"));
        const QString messageId =
            ChatService::createOutgoingMessage(QStringLiteral("local-a"),
                                               &repository,
                                               conversationId,
                                               QStringLiteral("peer-a"),
                                               QStringLiteral("legacy attachment"));
        QVERIFY(!messageId.isEmpty());
        QVERIFY(repository.updateAttachmentMetadata(messageId,
                                                    QStringLiteral("old.log"),
                                                    QStringLiteral("C:/old.log")));

        errorMessage.clear();
        QVERIFY(!ChatService::buildEnvelope(QStringLiteral("local-a"),
                                            &repository,
                                            messageId,
                                            QStringLiteral("peer-a"),
                                            &envelope,
                                            &errorMessage));
        QCOMPARE(errorMessage,
                 QStringLiteral("attachment message cannot be rebuilt as direct text"));
    }

    void buildEnvelope_restoresForwardPackagePayloadForRetry()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString connectionName = QStringLiteral("chat-service-forward-retry");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("forward-retry.db")), connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        const QString conversationId =
            DirectConversationAddressing::conversationIdForPeers(QStringLiteral("local-a"),
                                                                 QStringLiteral("peer-a"));
        const QString messageId =
            ChatService::createOutgoingMessage(QStringLiteral("local-a"),
                                               &repository,
                                               conversationId,
                                               QStringLiteral("peer-a"),
                                               QStringLiteral("Merged forward"));
        QVERIFY(!messageId.isEmpty());
        const QString payload = QStringLiteral("{\"count\":2,\"title\":\"Forwarded messages\"}");
        QVERIFY(repository.updateMessageFields(messageId,
                                               QStringLiteral("forward_package"),
                                               payload));

        MessageEnvelope envelope;
        QVERIFY(ChatService::buildEnvelope(QStringLiteral("local-a"),
                                           &repository,
                                           messageId,
                                           QStringLiteral("peer-a"),
                                           &envelope));

        QCOMPARE(envelope.type, MessageType::ChatText);
        QCOMPARE(envelope.contentType, std::string("plain"));
        QCOMPARE(envelope.messageSubtype, std::string("forward_package"));
        QCOMPARE(QString::fromStdString(envelope.payloadJson), payload);
    }

    void duplicateMessageMutation_isTreatedAsHandledForAckReplay()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString conn = QStringLiteral("chat-svc-mutation-duplicate");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("mutation-duplicate.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        const QString groupId  = QStringLiteral("group-001");
        const QString senderId = QStringLiteral("peer-a");
        const QString origId   = QStringLiteral("orig-group-duplicate");

        QVERIFY(repository.upsertConversation(
            ConversationSummary{groupId.toStdWString(), L"研发群", L"hi", 1000000000000LL}));

        ChatMessage original;
        original.messageId      = origId.toStdWString();
        original.conversationId = groupId.toStdWString();
        original.senderId       = senderId.toStdWString();
        original.body           = L"group message";
        original.createdAtMs    = 1000000000000LL;
        original.deliveryState  = MessageDeliveryState::Received;
        QVERIFY(repository.appendMessage(original));

        const qint64 mutatedAt = 1000000001000LL;
        MessageEnvelope mutationEnvelope;
        mutationEnvelope.messageId      = "mutation-group-duplicate";
        mutationEnvelope.type           = MessageType::MessageMutation;
        mutationEnvelope.senderId       = senderId.toStdString();
        mutationEnvelope.targetId       = "local-a";
        mutationEnvelope.conversationId = groupId.toStdString();
        mutationEnvelope.messageSubtype = "recall";
        mutationEnvelope.payloadJson    = buildRecallPayloadJson(origId, mutatedAt);
        mutationEnvelope.createdAtMs    = mutatedAt;

        QVERIFY(ChatService::storeIncomingGroupEnvelope(&repository,
                                                        mutationEnvelope,
                                                        groupId,
                                                        QStringLiteral("研发群")));
        QVERIFY(ChatService::storeIncomingGroupEnvelope(&repository,
                                                        mutationEnvelope,
                                                        groupId,
                                                        QStringLiteral("研发群")));
    }

    // ── Message mutation tests ───────────────────────────────────────────

    void storeIncomingEnvelope_messageMutation_applyRecall()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString conn = QStringLiteral("chat-svc-mutation-direct");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("mutation-direct.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        // Store an original message directly
        const QString origId    = QStringLiteral("orig-direct-001");
        const QString senderId  = QStringLiteral("peer-a");
        const QString convId    = DirectConversationAddressing::conversationIdForPeers(
            QStringLiteral("local-a"), senderId);

        QVERIFY(repository.upsertConversation(
            ConversationSummary{convId.toStdWString(), L"peer-a", L"hello", 1000000000000LL}));

        ChatMessage original;
        original.messageId      = origId.toStdWString();
        original.conversationId = convId.toStdWString();
        original.senderId       = senderId.toStdWString();
        original.body           = L"hello";
        original.createdAtMs    = 1000000000000LL;
        original.deliveryState  = MessageDeliveryState::Received;
        QVERIFY(repository.appendMessage(original));

        // Build and deliver a recall mutation envelope
        const qint64 mutatedAt = 1000000001000LL;
        MessageEnvelope mutationEnvelope;
        mutationEnvelope.messageId      = "mutation-direct-001";
        mutationEnvelope.type           = MessageType::MessageMutation;
        mutationEnvelope.senderId       = senderId.toStdString();
        mutationEnvelope.targetId       = "local-a";
        mutationEnvelope.conversationId = convId.toStdString();
        mutationEnvelope.messageSubtype = "recall";
        mutationEnvelope.payloadJson    = buildRecallPayloadJson(origId, mutatedAt);
        mutationEnvelope.createdAtMs    = mutatedAt;

        QVERIFY(ChatService::storeIncomingEnvelope(QStringLiteral("local-a"),
                                                   &repository,
                                                   mutationEnvelope));

        ChatMessage state;
        QVERIFY(repository.findMessageMutationStateById(origId, &state));
        QVERIFY(state.isRecalled);
        QCOMPARE(state.recalledAtMs, mutatedAt);
    }

    void storeIncomingGroupEnvelope_messageMutation_applyRecall()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString conn = QStringLiteral("chat-svc-mutation-group");
        DatabaseManager manager(tempDir.filePath(QStringLiteral("mutation-group.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        const QString groupId  = QStringLiteral("group-001");
        const QString senderId = QStringLiteral("peer-a");
        const QString origId   = QStringLiteral("orig-group-001");

        QVERIFY(repository.upsertConversation(
            ConversationSummary{groupId.toStdWString(), L"研发群", L"hi", 1000000000000LL}));

        ChatMessage original;
        original.messageId      = origId.toStdWString();
        original.conversationId = groupId.toStdWString();
        original.senderId       = senderId.toStdWString();
        original.body           = L"group message";
        original.createdAtMs    = 1000000000000LL;
        original.deliveryState  = MessageDeliveryState::Received;
        QVERIFY(repository.appendMessage(original));

        // Build and deliver a recall mutation envelope for the group
        const qint64 mutatedAt = 1000000001000LL;
        MessageEnvelope mutationEnvelope;
        mutationEnvelope.messageId      = "mutation-group-001";
        mutationEnvelope.type           = MessageType::MessageMutation;
        mutationEnvelope.senderId       = senderId.toStdString();
        mutationEnvelope.targetId       = "local-a";
        mutationEnvelope.conversationId = groupId.toStdString();
        mutationEnvelope.messageSubtype = "recall";
        mutationEnvelope.payloadJson    = buildRecallPayloadJson(origId, mutatedAt);
        mutationEnvelope.createdAtMs    = mutatedAt;

        QVERIFY(ChatService::storeIncomingGroupEnvelope(&repository,
                                                        mutationEnvelope,
                                                        groupId,
                                                        QStringLiteral("研发群")));

        ChatMessage state;
        QVERIFY(repository.findMessageMutationStateById(origId, &state));
        QVERIFY(state.isRecalled);
        QCOMPARE(state.recalledAtMs, mutatedAt);
    }
};

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    TestChatService tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "TestChatService.moc"
