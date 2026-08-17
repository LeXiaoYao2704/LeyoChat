#include <QtTest/QTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <optional>

#include "integrations/RemoteChatServiceSettings.h"
#include "integrations/ServerMessageClient.h"
#include "services/ReliableGroupMessageSender.h"
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
        ack.serverMessageId = QStringLiteral("srv-group-1");
        ack.conversationId = draft.conversationId;
        ack.serverSeq = 30;
        ack.createdAtMs = 3000;
        return ack;
    }

    std::optional<ServerMessagePage> listMessages(
        const QString&,
        qint64,
        int,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("list not used by group sender tests");
        }
        return std::nullopt;
    }

    bool acknowledgeDelivered(const QString&,
                              qint64,
                              QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("delivery ack not used by group sender tests");
        }
        return false;
    }

    bool acknowledgeRead(const QString&,
                         qint64,
                         QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("read ack not used by group sender tests");
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

std::vector<MessageEnvelope> groupEnvelopes(const QString& messageId,
                                            const QString& groupId,
                                            const QString& senderId,
                                            const QStringList& targetIds,
                                            const QString& html)
{
    QJsonObject body;
    body.insert(QStringLiteral("group_id"), groupId);
    body.insert(QStringLiteral("message_kind"), QStringLiteral("text"));
    body.insert(QStringLiteral("content_type"), QStringLiteral("html"));
    body.insert(QStringLiteral("text"), html);
    const QByteArray bodyBytes =
        QJsonDocument(body).toJson(QJsonDocument::Compact);

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(static_cast<std::size_t>(targetIds.size()));
    for (const QString& targetId : targetIds) {
        MessageEnvelope envelope;
        envelope.messageId = messageId.toStdString();
        envelope.type = MessageType::GroupMessage;
        envelope.senderId = senderId.toStdString();
        envelope.targetId = targetId.toStdString();
        envelope.conversationId = groupId.toStdString();
        envelope.body = std::string(bodyBytes.constData(),
                                    static_cast<std::size_t>(bodyBytes.size()));
        envelope.createdAtMs = 1234;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

ReliableGroupMessageSendRequest requestFor(RemoteChatTransportMode mode)
{
    ReliableGroupMessageSendRequest request;
    request.groupId = QStringLiteral("group:ops");
    request.groupTitle = QStringLiteral("Ops Room");
    request.body = QStringLiteral("<p>Hello group</p>");
    request.envelopes = groupEnvelopes(QStringLiteral("group-msg-1"),
                                       request.groupId,
                                       QStringLiteral("local-a"),
                                       {QStringLiteral("peer-a"),
                                        QStringLiteral("peer-b")},
                                       request.body);
    request.settings = configuredSettings(mode);
    request.serviceReachable = true;
    request.p2pAvailable = true;
    return request;
}

}  // namespace

class TestReliableGroupMessageSender : public QObject {
    Q_OBJECT

private slots:
    void serverPreferredReachableUsesOneServiceDraftForAllRecipientsAndMarksServerAcked()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-service");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-service.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        ReliableGroupMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        for (MessageEnvelope& envelope : request.envelopes) {
            envelope.mentionedIds = {"peer-b", "__all__"};
            envelope.replyToMessageId = "quoted-group-message";
            envelope.replyToSenderId = "peer-b";
            envelope.replyToBody = "quoted group body";
        }
        const ReliableGroupMessageSendResult result = sender.sendText(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-msg-1"));
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.clientMessageId, result.messageId);
        QCOMPARE(draft.conversationId, QStringLiteral("group:ops"));
        QCOMPARE(draft.workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(draft.type, QStringLiteral("chat_text"));
        QCOMPARE(draft.body, QStringLiteral("<p>Hello group</p>"));
        QCOMPARE(draft.contentType, QStringLiteral("html"));
        QCOMPARE(draft.recipientIds,
                 QVector<QString>({QStringLiteral("peer-a"),
                                   QStringLiteral("peer-b")}));
        QCOMPARE(draft.replyToMessageId, QStringLiteral("quoted-group-message"));
        QCOMPARE(draft.payload.value(QStringLiteral("mentioned_ids")).toArray(),
                 QJsonArray({QStringLiteral("peer-b"), QStringLiteral("__all__")}));
        QCOMPARE(draft.payload.value(QStringLiteral("reply_to_sender_id")).toString(),
                 QStringLiteral("peer-b"));
        QCOMPARE(draft.payload.value(QStringLiteral("reply_to_body")).toString(),
                 QStringLiteral("quoted group body"));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(QString::fromStdWString(stored.conversationId),
                 QStringLiteral("group:ops"));
        QCOMPARE(QString::fromStdWString(stored.body),
                 QStringLiteral("<p>Hello group</p>"));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
        QCOMPARE(repository.loadLocalMessageIdForRemoteServerId(
                     QStringLiteral("srv-group-1")),
                 result.messageId);
    }

    void serverPreferredMixedRecipientsSplitsServiceAndP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-mixed");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-mixed.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableGroupMessageP2PRequest> p2pRequests;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableGroupMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.envelopes = groupEnvelopes(
            QStringLiteral("group-msg-mixed"),
            request.groupId,
            QStringLiteral("local-a"),
            {QStringLiteral("new-a"),
             QStringLiteral("new-b"),
             QStringLiteral("legacy-a")},
            request.body);
        request.serverRecipientIds = QVector<QString>{
            QStringLiteral("new-a"),
            QStringLiteral("new-b")
        };
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupMessageSendResult result = sender.sendText(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-msg-mixed"));
        QCOMPARE(result.channelUsed, TransportChannel::Mixed);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(serverClient.drafts.front().recipientIds,
                 QVector<QString>({QStringLiteral("new-a"),
                                   QStringLiteral("new-b")}));
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(p2pRequests.front().messageId, result.messageId);
        QVERIFY(p2pRequests.front().acceptQueuedOnlyDelivery);
        QCOMPARE(p2pRequests.front().envelopes.size(), static_cast<std::size_t>(1));
        QCOMPARE(QString::fromStdString(p2pRequests.front().envelopes.front().targetId),
                 QStringLiteral("legacy-a"));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
    }

    void serverPreferredMixedRecipientsP2PFailureDoesNotSendServiceBranch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-mixed-p2p-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-mixed-p2p-fail.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableGroupMessageP2PRequest> p2pRequests;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest& p2pRequest, QString* errorMessage) {
                p2pRequests.push_back(p2pRequest);
                if (errorMessage) {
                    *errorMessage = QStringLiteral("legacy p2p queue failed");
                }
                return false;
            });

        ReliableGroupMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.envelopes = groupEnvelopes(
            QStringLiteral("group-msg-mixed-p2p-fail"),
            request.groupId,
            QStringLiteral("local-a"),
            {QStringLiteral("new-a"),
             QStringLiteral("legacy-a")},
            request.body);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupMessageSendResult result = sender.sendText(request);

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-msg-mixed-p2p-fail"));
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QVERIFY(result.errorMessage.contains(QStringLiteral("legacy p2p queue failed")));
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(serverClient.drafts.size(), 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void p2pOnlyDoesNotAcceptQueuedOnlyDelivery()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-p2p-only-queued-flag");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-p2p-only-queued-flag.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableGroupMessageP2PRequest> p2pRequests;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        const ReliableGroupMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::P2POnly));

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(p2pRequests.size(), 1);
        QVERIFY(!p2pRequests.front().acceptQueuedOnlyDelivery);
    }

    void serverPreferredMixedRecipientsServiceFailureFallsBackServerRecipientsToP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("reliable-group-mixed-service-fallback");
        DatabaseManager manager(
            dir.filePath(QStringLiteral("group-mixed-service-fallback.db")),
            connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        QVector<ReliableGroupMessageP2PRequest> p2pRequests;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableGroupMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.envelopes = groupEnvelopes(
            QStringLiteral("group-msg-mixed-service-fallback"),
            request.groupId,
            QStringLiteral("local-a"),
            {QStringLiteral("new-a"),
             QStringLiteral("legacy-a")},
            request.body);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupMessageSendResult result = sender.sendText(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId,
                 QStringLiteral("group-msg-mixed-service-fallback"));
        QCOMPARE(result.channelUsed, TransportChannel::Mixed);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(serverClient.drafts.front().recipientIds,
                 QVector<QString>{QStringLiteral("new-a")});
        QCOMPARE(p2pRequests.size(), 2);
        QCOMPARE(QString::fromStdString(
                     p2pRequests.at(0).envelopes.front().targetId),
                 QStringLiteral("legacy-a"));
        QCOMPARE(QString::fromStdString(
                     p2pRequests.at(1).envelopes.front().targetId),
                 QStringLiteral("new-a"));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void acceptedServiceAckWithLocalFinalizeFailureDoesNotFallbackToP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("reliable-group-ack-finalize-fail");
        DatabaseManager manager(
            dir.filePath(QStringLiteral("group-ack-finalize-fail.db")),
            connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QSqlQuery breakMapping(QSqlDatabase::database(connectionName, false));
        QVERIFY(breakMapping.exec(QStringLiteral("DROP TABLE remote_message_id_map")));

        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableGroupMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-msg-1"));
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void p2pOnlyUsesCallbackAndDoesNotCallService()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-p2p-only");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-p2p.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableGroupMessageP2PRequest> p2pRequests;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        const ReliableGroupMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::P2POnly));

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 0);
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(p2pRequests.front().messageId, result.messageId);
        QCOMPARE(p2pRequests.front().groupId, QStringLiteral("group:ops"));
        QCOMPARE(p2pRequests.front().envelopes.size(), static_cast<std::size_t>(2));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void serverPreferredServiceFailureFallsBackToP2PWithSameMessageId()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-fallback");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-fallback.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        bool sawPendingBeforeP2P = false;
        QVector<ReliableGroupMessageP2PRequest> p2pRequests;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                ChatMessage stored;
                sawPendingBeforeP2P =
                    repository.findMessageById(p2pRequest.messageId, &stored)
                    && stored.deliveryState == MessageDeliveryState::Pending;
                return true;
            });

        const ReliableGroupMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QVERIFY(sawPendingBeforeP2P);
        QCOMPARE(result.messageId, QStringLiteral("group-msg-1"));
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(p2pRequests.front().messageId, result.messageId);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void serverOnlyServiceFailureLeavesPendingAndSkipsP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-server-only");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-server-only.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        int p2pCalls = 0;
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableGroupMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::ServerOnly));

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-msg-1"));
        QVERIFY(!result.errorMessage.isEmpty());
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void queuedOnlyP2PResultIsAcceptedButNotReportedAsFullyDelivered()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-queued-result");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-queued-result.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        ReliableGroupMessageP2PStatusCallback statusCallback =
            [](const ReliableGroupMessageP2PRequest& request, QString*) {
                GroupFanOutDeliveryResult delivery;
                delivery.attemptedCount = static_cast<int>(request.envelopes.size());
                delivery.queuedCount = delivery.attemptedCount;
                return delivery;
            };
        ReliableGroupMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            statusCallback);

        const ReliableGroupMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::P2POnly));

        QVERIFY(result.accepted);
        QVERIFY(!result.success);
        QCOMPARE(result.queuedRecipientCount, 2);
        QCOMPARE(result.writtenRecipientCount, 0);
        QCOMPARE(result.failedRecipientCount, 0);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }
};

QTEST_MAIN(TestReliableGroupMessageSender)
#include "TestReliableGroupMessageSender.moc"
