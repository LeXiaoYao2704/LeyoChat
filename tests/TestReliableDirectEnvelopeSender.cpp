#include <QtTest/QTest>

#include <QJsonDocument>
#include <QJsonObject>

#include <optional>

#include "integrations/RemoteChatServiceSettings.h"
#include "integrations/ServerMessageClient.h"
#include "services/ReliableDirectEnvelopeSender.h"
#include "services/TransportPolicy.h"

namespace {

class FakeServerMessageClient final : public IServerMessageClient {
public:
    mutable QVector<ServerMessageDraft> drafts;
    bool failSend = false;
    QString errorToReturn = QStringLiteral("message service unavailable");
    QString ackConversationIdOverride;

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
        ack.serverMessageId = QStringLiteral("srv-direct-envelope-%1").arg(drafts.size());
        ack.conversationId = ackConversationIdOverride.trimmed().isEmpty()
            ? draft.conversationId
            : ackConversationIdOverride.trimmed();
        ack.serverSeq = 30 + drafts.size();
        ack.createdAtMs = 3000 + drafts.size();
        return ack;
    }

    std::optional<ServerMessagePage> listMessages(
        const QString&,
        qint64,
        int,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("list not used by direct envelope tests");
        }
        return std::nullopt;
    }

    bool acknowledgeDelivered(const QString&,
                              qint64,
                              QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("delivery ack not used by direct envelope tests");
        }
        return false;
    }

    bool acknowledgeRead(const QString&,
                         qint64,
                         QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("read ack not used by direct envelope tests");
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

MessageEnvelope directMutationEnvelope()
{
    MessageEnvelope envelope;
    envelope.messageId = "direct-mut-1";
    envelope.type = MessageType::MessageMutation;
    envelope.senderId = "local-a";
    envelope.targetId = "peer-a";
    envelope.conversationId = "conv-local-peer";
    envelope.messageSubtype = "recall";
    envelope.payloadJson = compactJsonString(QJsonObject{
        {QStringLiteral("target_message_id"), QStringLiteral("direct-msg-1")},
        {QStringLiteral("mutation_kind"), QStringLiteral("recall")},
        {QStringLiteral("mutated_at_ms"), 12345}
    });
    envelope.createdAtMs = 12345;
    return envelope;
}

MessageEnvelope directNudgeEnvelope()
{
    MessageEnvelope envelope;
    envelope.messageId = "direct-nudge-1";
    envelope.type = MessageType::ChatText;
    envelope.senderId = "local-a";
    envelope.targetId = "peer-a";
    envelope.conversationId = "conv-local-peer";
    envelope.body = "【窗口抖动提醒】";
    envelope.contentType = "nudge";
    envelope.createdAtMs = 23456;
    return envelope;
}

MessageEnvelope directForwardPackageEnvelope()
{
    MessageEnvelope envelope;
    envelope.messageId = "direct-forward-1";
    envelope.type = MessageType::ChatText;
    envelope.senderId = "local-a";
    envelope.targetId = "peer-a";
    envelope.conversationId = "conv-local-peer";
    envelope.body = "Merged forward";
    envelope.contentType = "plain";
    envelope.messageSubtype = "forward_package";
    envelope.payloadJson = compactJsonString(QJsonObject{
        {QStringLiteral("title"), QStringLiteral("Forwarded messages")},
        {QStringLiteral("count"), 2}
    });
    envelope.createdAtMs = 24567;
    return envelope;
}

MessageEnvelope directReactionEnvelope()
{
    MessageEnvelope envelope;
    envelope.messageId = "direct-reaction-1";
    envelope.type = MessageType::MessageReaction;
    envelope.senderId = "local-a";
    envelope.targetId = "peer-a";
    envelope.conversationId = "conv-local-peer";
    envelope.payloadJson = compactJsonString(QJsonObject{
        {QStringLiteral("targetMessageId"), QStringLiteral("direct-msg-1")},
        {QStringLiteral("emoji"), QStringLiteral(":ok:")}
    });
    envelope.createdAtMs = 34567;
    return envelope;
}

ReliableDirectEnvelopeSendRequest requestFor(const MessageEnvelope& envelope,
                                             RemoteChatTransportMode mode)
{
    ReliableDirectEnvelopeSendRequest request;
    request.envelope = envelope;
    request.settings = configuredSettings(mode);
    request.serviceReachable = true;
    request.receiverServerCapable = true;
    request.p2pAvailable = true;
    return request;
}

}  // namespace

class TestReliableDirectEnvelopeSender : public QObject {
    Q_OBJECT

private slots:
    void serverPreferredMutationUsesMessageServiceDraft()
    {
        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableDirectEnvelopeSender sender(
            QStringLiteral("local-a"),
            &serverClient,
            [&](const ReliableDirectEnvelopeP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableDirectEnvelopeSendResult result =
            sender.send(requestFor(directMutationEnvelope(),
                                   RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("direct-mut-1"));
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(p2pCalls, 0);
        QCOMPARE(serverClient.drafts.size(), 1);
        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.type, QStringLiteral("message_mutation"));
        QCOMPARE(draft.body, QString());
        QCOMPARE(draft.contentType, QStringLiteral("mutation"));
        QCOMPARE(draft.recipientIds, QVector<QString>{QStringLiteral("peer-a")});
        QCOMPARE(draft.payload.value(QStringLiteral("target_message_id")).toString(),
                 QStringLiteral("direct-msg-1"));
    }

    void serverPreferredNudgePreservesNudgeContentType()
    {
        FakeServerMessageClient serverClient;
        ReliableDirectEnvelopeSender sender(
            QStringLiteral("local-a"),
            &serverClient,
            [&](const ReliableDirectEnvelopeP2PRequest&, QString*) {
                return true;
            });

        const ReliableDirectEnvelopeSendResult result =
            sender.send(requestFor(directNudgeEnvelope(),
                                   RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(result.serverMessageId, QStringLiteral("srv-direct-envelope-1"));
        QCOMPARE(serverClient.drafts.size(), 1);
        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.type, QStringLiteral("chat_text"));
        QCOMPARE(draft.body, QStringLiteral("【窗口抖动提醒】"));
        QCOMPARE(draft.contentType, QStringLiteral("nudge"));
    }

    void serverPreferredForwardPackagePreservesServiceTypeAndPayload()
    {
        FakeServerMessageClient serverClient;
        ReliableDirectEnvelopeSender sender(
            QStringLiteral("local-a"),
            &serverClient,
            [&](const ReliableDirectEnvelopeP2PRequest&, QString*) {
                return true;
            });

        const ReliableDirectEnvelopeSendResult result =
            sender.send(requestFor(directForwardPackageEnvelope(),
                                   RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.type, QStringLiteral("forward_package"));
        QCOMPARE(draft.body, QStringLiteral("Merged forward"));
        QCOMPARE(draft.contentType, QStringLiteral("plain"));
        QCOMPARE(draft.payload.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Forwarded messages"));
        QCOMPARE(draft.payload.value(QStringLiteral("count")).toInt(), 2);
    }

    void serverPreferredReactionUsesMessageReactionDraft()
    {
        FakeServerMessageClient serverClient;
        ReliableDirectEnvelopeSender sender(
            QStringLiteral("local-a"),
            &serverClient,
            [&](const ReliableDirectEnvelopeP2PRequest&, QString*) {
                return true;
            });

        const ReliableDirectEnvelopeSendResult result =
            sender.send(requestFor(directReactionEnvelope(),
                                   RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.type, QStringLiteral("message_reaction"));
        QCOMPARE(draft.body, QString());
        QCOMPARE(draft.contentType, QStringLiteral("reaction"));
        QCOMPARE(draft.payload.value(QStringLiteral("targetMessageId")).toString(),
                 QStringLiteral("direct-msg-1"));
        QCOMPARE(draft.recipientIds, QVector<QString>{QStringLiteral("peer-a")});
    }

    void legacyReceiverUsesP2PEnvelope()
    {
        FakeServerMessageClient serverClient;
        QVector<ReliableDirectEnvelopeP2PRequest> p2pRequests;
        ReliableDirectEnvelopeSender sender(
            QStringLiteral("local-a"),
            &serverClient,
            [&](const ReliableDirectEnvelopeP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableDirectEnvelopeSendRequest request =
            requestFor(directMutationEnvelope(), RemoteChatTransportMode::ServerPreferred);
        request.receiverServerCapable = false;

        const ReliableDirectEnvelopeSendResult result = sender.send(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 0);
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(p2pRequests.front().messageId, QStringLiteral("direct-mut-1"));
        QCOMPARE(p2pRequests.front().envelope.type, MessageType::MessageMutation);
    }

    void serviceFailureFallsBackToP2P()
    {
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        QVector<ReliableDirectEnvelopeP2PRequest> p2pRequests;
        ReliableDirectEnvelopeSender sender(
            QStringLiteral("local-a"),
            &serverClient,
            [&](const ReliableDirectEnvelopeP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        const ReliableDirectEnvelopeSendResult result =
            sender.send(requestFor(directMutationEnvelope(),
                                   RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pRequests.size(), 1);
    }

    void acceptedServiceAckMismatchDoesNotFallbackToP2P()
    {
        FakeServerMessageClient serverClient;
        serverClient.ackConversationIdOverride = QStringLiteral("other-conv");
        int p2pCalls = 0;
        ReliableDirectEnvelopeSender sender(
            QStringLiteral("local-a"),
            &serverClient,
            [&](const ReliableDirectEnvelopeP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableDirectEnvelopeSendResult result =
            sender.send(requestFor(directMutationEnvelope(),
                                   RemoteChatTransportMode::ServerPreferred));

        QVERIFY(!result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);
    }
};

QTEST_MAIN(TestReliableDirectEnvelopeSender)
#include "TestReliableDirectEnvelopeSender.moc"
