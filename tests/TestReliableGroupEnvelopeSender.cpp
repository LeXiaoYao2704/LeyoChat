#include <QtTest/QTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <optional>

#include "integrations/RemoteChatServiceSettings.h"
#include "integrations/ServerMessageClient.h"
#include "network/MessageCodec.h"
#include "services/ReliableGroupEnvelopeSender.h"
#include "services/TransportPolicy.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

namespace {

class FakeServerMessageClient final : public IServerMessageClient {
public:
    mutable QVector<ServerMessageDraft> drafts;
    bool failSend = false;
    QString errorToReturn = QStringLiteral("message service unavailable");

    std::optional<ServerMessageAck> sendMessage(
        const ServerMessageDraft& draft,
        QString* errorMessage = nullptr) const override
    {
        drafts.push_back(draft);
        if (failSend) {
            if (errorMessage) {
                *errorMessage = errorToReturn;
            }
            return std::nullopt;
        }

        ServerMessageAck ack;
        ack.serverMessageId = QStringLiteral("srv-envelope-%1").arg(drafts.size());
        ack.conversationId = draft.conversationId;
        ack.serverSeq = 80 + drafts.size();
        ack.createdAtMs = 8000 + drafts.size();
        return ack;
    }

    std::optional<ServerMessagePage> listMessages(
        const QString&,
        qint64,
        int,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("list not used by envelope sender tests");
        }
        return std::nullopt;
    }

    bool acknowledgeDelivered(const QString&,
                              qint64,
                              QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("delivery ack not used by envelope sender tests");
        }
        return false;
    }

    bool acknowledgeRead(const QString&,
                         qint64,
                         QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("read ack not used by envelope sender tests");
        }
        return false;
    }
};

RemoteChatServiceSettings configuredSettings(RemoteChatTransportMode mode)
{
    RemoteChatServiceSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("http://chat.local:8765");
    settings.bearerToken = QStringLiteral("token");
    settings.workspaceId = QStringLiteral("ws-main");
    settings.mode = mode;
    settings.allowP2PFallback = true;
    return settings;
}

std::string compactJsonString(const QJsonObject& object)
{
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

std::vector<MessageEnvelope> stickerEnvelopes(const QString& messageId,
                                              const QString& groupId,
                                              const QString& senderId,
                                              const QStringList& targetIds)
{
    const QJsonObject body{
        {QStringLiteral("group_id"), groupId},
        {QStringLiteral("message_kind"), QStringLiteral("text")},
        {QStringLiteral("content_type"), QStringLiteral("plain")},
        {QStringLiteral("text"), QStringLiteral("[sticker]")}
    };
    const QJsonObject payload{
        {QStringLiteral("pack_id"), QStringLiteral("pack-a")},
        {QStringLiteral("sticker_id"), QStringLiteral("wave")},
        {QStringLiteral("gif_base64"), QStringLiteral("R0lGODlhAQABAAAAACw=")}
    };

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(static_cast<std::size_t>(targetIds.size()));
    for (const QString& targetId : targetIds) {
        MessageEnvelope envelope;
        envelope.messageId = messageId.toStdString();
        envelope.type = MessageType::GroupMessage;
        envelope.senderId = senderId.toStdString();
        envelope.targetId = targetId.toStdString();
        envelope.conversationId = groupId.toStdString();
        envelope.body = compactJsonString(body);
        envelope.contentType = "plain";
        envelope.messageSubtype = "sticker";
        envelope.payloadJson = compactJsonString(payload);
        envelope.createdAtMs = 12345;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

std::vector<MessageEnvelope> mutationEnvelopes(const QString& messageId,
                                               const QString& groupId,
                                               const QString& senderId,
                                               const QStringList& targetIds)
{
    const QJsonObject payload{
        {QStringLiteral("target_message_id"), QStringLiteral("msg-original")},
        {QStringLiteral("mutation_kind"), QStringLiteral("recall")},
        {QStringLiteral("mutated_at_ms"), 45678}
    };

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(static_cast<std::size_t>(targetIds.size()));
    for (const QString& targetId : targetIds) {
        MessageEnvelope envelope;
        envelope.messageId = messageId.toStdString();
        envelope.type = MessageType::MessageMutation;
        envelope.senderId = senderId.toStdString();
        envelope.targetId = targetId.toStdString();
        envelope.conversationId = groupId.toStdString();
        envelope.messageSubtype = "recall";
        envelope.payloadJson = compactJsonString(payload);
        envelope.createdAtMs = 45678;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

std::vector<MessageEnvelope> reactionEnvelopes(const QString& messageId,
                                               const QString& groupId,
                                               const QString& senderId,
                                               const QStringList& targetIds)
{
    const QJsonObject payload{
        {QStringLiteral("targetMessageId"), QStringLiteral("group-msg-1")},
        {QStringLiteral("emoji"), QStringLiteral(":ok:")}
    };

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(static_cast<std::size_t>(targetIds.size()));
    for (const QString& targetId : targetIds) {
        MessageEnvelope envelope;
        envelope.messageId = messageId.toStdString();
        envelope.type = MessageType::MessageReaction;
        envelope.senderId = senderId.toStdString();
        envelope.targetId = targetId.toStdString();
        envelope.conversationId = groupId.toStdString();
        envelope.payloadJson = compactJsonString(payload);
        envelope.createdAtMs = 56789;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

std::vector<MessageEnvelope> pinEnvelopes(const QString& messageId,
                                          const QString& groupId,
                                          const QString& senderId,
                                          const QStringList& targetIds)
{
    const QJsonObject payload{
        {QStringLiteral("group_id"), groupId},
        {QStringLiteral("message_id"), QStringLiteral("group-msg-1")},
        {QStringLiteral("pinned_body"), QStringLiteral("important")},
        {QStringLiteral("author_name"), QStringLiteral("Peer A")},
        {QStringLiteral("pinner_name"), QStringLiteral("Local A")},
        {QStringLiteral("action"), QStringLiteral("pin")}
    };

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(static_cast<std::size_t>(targetIds.size()));
    for (const QString& targetId : targetIds) {
        MessageEnvelope envelope;
        envelope.messageId = messageId.toStdString();
        envelope.type = MessageType::PinMessage;
        envelope.senderId = senderId.toStdString();
        envelope.targetId = targetId.toStdString();
        envelope.conversationId = groupId.toStdString();
        envelope.payloadJson = compactJsonString(payload);
        envelope.createdAtMs = 67890;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

ReliableGroupEnvelopeSendRequest stickerRequest(RemoteChatTransportMode mode)
{
    ReliableGroupEnvelopeSendRequest request;
    request.groupId = QStringLiteral("group:ops");
    request.groupTitle = QStringLiteral("Ops Room");
    request.envelopes = stickerEnvelopes(QStringLiteral("group-sticker-1"),
                                         request.groupId,
                                         QStringLiteral("local-a"),
                                         {QStringLiteral("new-a"),
                                          QStringLiteral("legacy-a")});
    request.settings = configuredSettings(mode);
    request.serviceReachable = true;
    request.p2pAvailable = true;
    return request;
}

ChatMessage localStickerMessage(const QString& messageId)
{
    ChatMessage message;
    message.messageId = messageId.toStdWString();
    message.conversationId = QStringLiteral("group:ops").toStdWString();
    message.senderId = QStringLiteral("local-a").toStdWString();
    message.body = QStringLiteral("[sticker]").toStdWString();
    message.createdAtMs = 12345;
    message.deliveryState = MessageDeliveryState::Pending;
    message.messageType = QStringLiteral("sticker").toStdWString();
    message.payloadJson =
        QStringLiteral(R"({"pack_id":"pack-a","sticker_id":"wave"})").toStdWString();
    return message;
}

}  // namespace

class TestReliableGroupEnvelopeSender : public QObject {
    Q_OBJECT

private slots:
    void mixedStickerSendsServiceAndP2PBranchesAndMarksServerAcked()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-envelope-mixed");
        DatabaseManager manager(dir.filePath(QStringLiteral("envelope-mixed.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(localStickerMessage(
            QStringLiteral("group-sticker-1"))));

        FakeServerMessageClient serverClient;
        QVector<ReliableGroupEnvelopeP2PRequest> p2pRequests;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableGroupEnvelopeSendRequest request =
            stickerRequest(RemoteChatTransportMode::ServerPreferred);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-sticker-1"));
        QCOMPARE(result.channelUsed, TransportChannel::Mixed);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pRequests.size(), 1);
        QVERIFY(p2pRequests.front().acceptQueuedOnlyDelivery);

        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.clientMessageId, result.messageId);
        QCOMPARE(draft.conversationId, QStringLiteral("group:ops"));
        QCOMPARE(draft.workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(draft.type, QStringLiteral("sticker"));
        QCOMPARE(draft.body, QStringLiteral("[sticker]"));
        QCOMPARE(draft.contentType, QStringLiteral("plain"));
        QCOMPARE(draft.payload.value(QStringLiteral("pack_id")).toString(),
                 QStringLiteral("pack-a"));
        QCOMPARE(draft.payload.value(QStringLiteral("sticker_id")).toString(),
                 QStringLiteral("wave"));
        QCOMPARE(draft.recipientIds, QVector<QString>{QStringLiteral("new-a")});

        QCOMPARE(p2pRequests.front().envelopes.size(), static_cast<std::size_t>(1));
        QCOMPARE(QString::fromStdString(p2pRequests.front().envelopes.front().targetId),
                 QStringLiteral("legacy-a"));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
        QCOMPARE(repository.loadLocalMessageIdForRemoteServerId(
                     QStringLiteral("srv-envelope-1")),
                 result.messageId);
    }

    void serviceBranchClearsPendingFanOutForServerRecipientsOnly()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-envelope-clear-pending");
        DatabaseManager manager(dir.filePath(QStringLiteral("envelope-clear-pending.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(localStickerMessage(
            QStringLiteral("group-sticker-1"))));

        ReliableGroupEnvelopeSendRequest request =
            stickerRequest(RemoteChatTransportMode::ServerPreferred);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        for (const MessageEnvelope& envelope : request.envelopes) {
            QVERIFY(repository.enqueuePendingGroupEnvelope(
                QString::fromStdString(envelope.targetId),
                request.groupId,
                QByteArray::fromStdString(MessageCodec::encode(envelope)),
                envelope.createdAtMs));
        }

        FakeServerMessageClient serverClient;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest&, QString*) {
                return true;
            });

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(result.success);
        QVERIFY(repository.loadPendingGroupEnvelopes(QStringLiteral("new-a"), 10).empty());
        QCOMPARE(repository.loadPendingGroupEnvelopes(QStringLiteral("legacy-a"), 10).size(),
                 std::size_t(1));
    }

    void mixedStickerP2PFailureDoesNotSendServiceBranch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-envelope-p2p-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("envelope-p2p-fail.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(localStickerMessage(
            QStringLiteral("group-sticker-1"))));

        FakeServerMessageClient serverClient;
        QVector<ReliableGroupEnvelopeP2PRequest> p2pRequests;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest& p2pRequest,
                QString* errorMessage) {
                p2pRequests.push_back(p2pRequest);
                if (errorMessage) {
                    *errorMessage = QStringLiteral("legacy p2p queue failed");
                }
                return false;
            });

        ReliableGroupEnvelopeSendRequest request =
            stickerRequest(RemoteChatTransportMode::ServerPreferred);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-sticker-1"));
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QVERIFY(result.errorMessage.contains(QStringLiteral("legacy p2p queue failed")));
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(serverClient.drafts.size(), 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void mixedStickerServiceFailureFallsBackServerRecipientsToP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("reliable-envelope-service-fallback");
        DatabaseManager manager(
            dir.filePath(QStringLiteral("envelope-service-fallback.db")),
            connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(localStickerMessage(
            QStringLiteral("group-sticker-1"))));

        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        QVector<ReliableGroupEnvelopeP2PRequest> p2pRequests;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableGroupEnvelopeSendRequest request =
            stickerRequest(RemoteChatTransportMode::ServerPreferred);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-sticker-1"));
        QCOMPARE(result.channelUsed, TransportChannel::Mixed);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(serverClient.drafts.front().recipientIds,
                 QVector<QString>{QStringLiteral("new-a")});
        QCOMPARE(p2pRequests.size(), 2);
        QCOMPARE(QString::fromStdString(
                     p2pRequests.at(0).envelopes.front().targetId),
                 QStringLiteral("legacy-a"));
        QVERIFY(p2pRequests.at(0).acceptQueuedOnlyDelivery);
        QCOMPARE(QString::fromStdString(
                     p2pRequests.at(1).envelopes.front().targetId),
                 QStringLiteral("new-a"));
        QVERIFY(!p2pRequests.at(1).acceptQueuedOnlyDelivery);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void acceptedServiceAckWithLocalFinalizeFailureDoesNotFallbackToP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("reliable-envelope-ack-finalize-fail");
        DatabaseManager manager(
            dir.filePath(QStringLiteral("envelope-ack-finalize-fail.db")),
            connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(localStickerMessage(
            QStringLiteral("group-sticker-1"))));
        QSqlQuery breakMapping(QSqlDatabase::database(connectionName, false));
        QVERIFY(breakMapping.exec(QStringLiteral("DROP TABLE remote_message_id_map")));

        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        ReliableGroupEnvelopeSendRequest request =
            stickerRequest(RemoteChatTransportMode::ServerPreferred);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds.clear();

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-sticker-1"));
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void mutationDraftUsesMessageMutationTypeWithoutLocalMessageRow()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-envelope-mutation");
        DatabaseManager manager(dir.filePath(QStringLiteral("envelope-mutation.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        ReliableGroupEnvelopeSendRequest request;
        request.groupId = QStringLiteral("group:ops");
        request.groupTitle = QStringLiteral("Ops Room");
        request.envelopes = mutationEnvelopes(QStringLiteral("mutation-1"),
                                              request.groupId,
                                              QStringLiteral("local-a"),
                                              {QStringLiteral("new-a")});
        request.settings = configuredSettings(RemoteChatTransportMode::ServerPreferred);
        request.serviceReachable = true;
        request.p2pAvailable = true;
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(p2pCalls, 0);
        QCOMPARE(serverClient.drafts.size(), 1);
        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.type, QStringLiteral("message_mutation"));
        QCOMPARE(draft.body, QString());
        QCOMPARE(draft.payload.value(QStringLiteral("target_message_id")).toString(),
                 QStringLiteral("msg-original"));
        QCOMPARE(draft.payload.value(QStringLiteral("mutation_kind")).toString(),
                 QStringLiteral("recall"));
    }

    void reactionDraftUsesMessageReactionType()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-envelope-reaction");
        DatabaseManager manager(dir.filePath(QStringLiteral("envelope-reaction.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest&, QString*) {
                return true;
            });

        ReliableGroupEnvelopeSendRequest request;
        request.groupId = QStringLiteral("group:ops");
        request.groupTitle = QStringLiteral("Ops Room");
        request.envelopes = reactionEnvelopes(QStringLiteral("reaction-1"),
                                              request.groupId,
                                              QStringLiteral("local-a"),
                                              {QStringLiteral("new-a")});
        request.settings = configuredSettings(RemoteChatTransportMode::ServerPreferred);
        request.serviceReachable = true;
        request.p2pAvailable = true;
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.type, QStringLiteral("message_reaction"));
        QCOMPARE(draft.body, QString());
        QCOMPARE(draft.contentType, QStringLiteral("reaction"));
        QCOMPARE(draft.payload.value(QStringLiteral("targetMessageId")).toString(),
                 QStringLiteral("group-msg-1"));
        QCOMPARE(draft.recipientIds, QVector<QString>{QStringLiteral("new-a")});
    }

    void pinDraftUsesPinMessageType()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-envelope-pin");
        DatabaseManager manager(dir.filePath(QStringLiteral("envelope-pin.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        ReliableGroupEnvelopeSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupEnvelopeP2PRequest&, QString*) {
                return true;
            });

        ReliableGroupEnvelopeSendRequest request;
        request.groupId = QStringLiteral("group:ops");
        request.groupTitle = QStringLiteral("Ops Room");
        request.envelopes = pinEnvelopes(QStringLiteral("pin-1"),
                                         request.groupId,
                                         QStringLiteral("local-a"),
                                         {QStringLiteral("new-a")});
        request.settings = configuredSettings(RemoteChatTransportMode::ServerPreferred);
        request.serviceReachable = true;
        request.p2pAvailable = true;
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};

        const ReliableGroupEnvelopeSendResult result = sender.send(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.type, QStringLiteral("pin_message"));
        QCOMPARE(draft.body, QString());
        QCOMPARE(draft.contentType, QStringLiteral("pin"));
        QCOMPARE(draft.payload.value(QStringLiteral("message_id")).toString(),
                 QStringLiteral("group-msg-1"));
        QCOMPARE(draft.recipientIds, QVector<QString>{QStringLiteral("new-a")});
    }
};

QTEST_MAIN(TestReliableGroupEnvelopeSender)
#include "TestReliableGroupEnvelopeSender.moc"
