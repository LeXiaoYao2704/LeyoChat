#include <QtTest/QTest>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <optional>
#include <stdexcept>

#include "integrations/RemoteChatServiceSettings.h"
#include "integrations/ServerMessageClient.h"
#include "services/ReliableDirectMessageSender.h"
#include "services/ChatService.h"
#include "services/TransportPolicy.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

namespace {

class FakeServerMessageClient final : public IServerMessageClient {
public:
    mutable QVector<ServerMessageDraft> drafts;
    bool failSend = false;
    bool throwOnSend = false;
    QString errorToReturn = QStringLiteral("message service unavailable");

    std::optional<ServerMessageAck> sendMessage(
        const ServerMessageDraft& draft,
        QString* errorMessage = nullptr) const override
    {
        if (throwOnSend) {
            throw std::length_error("string too long");
        }
        drafts.push_back(draft);
        if (failSend) {
            if (errorMessage) {
                *errorMessage = errorToReturn;
            }
            return std::nullopt;
        }

        ServerMessageAck ack;
        ack.serverMessageId = QStringLiteral("srv-1");
        ack.conversationId = draft.conversationId;
        ack.serverSeq = 10;
        ack.createdAtMs = 2000;
        return ack;
    }

    std::optional<ServerMessagePage> listMessages(
        const QString&,
        qint64,
        int,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("list not used by sender tests");
        }
        return std::nullopt;
    }

    bool acknowledgeDelivered(const QString&,
                              qint64,
                              QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("delivery ack not used by sender tests");
        }
        return false;
    }

    bool acknowledgeRead(const QString&,
                         qint64,
                         QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("read ack not used by sender tests");
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

ReliableDirectMessageSendRequest requestFor(RemoteChatTransportMode mode)
{
    ReliableDirectMessageSendRequest request;
    request.settings = configuredSettings(mode);
    request.conversationId = QStringLiteral("conv-local-peer");
    request.targetId = QStringLiteral("peer-a");
    request.body = QStringLiteral("<p>Hello</p>");
    request.serviceReachable = true;
    request.receiverServerCapable = true;
    request.p2pAvailable = true;
    return request;
}

}  // namespace

class TestReliableDirectMessageSender : public QObject {
    Q_OBJECT

private slots:
    void retryBackoffGrowsExponentiallyAndIsBounded()
    {
        QCOMPARE(reliableDirectMessageRetryDelayMs(0), qint64(0));
        QCOMPARE(reliableDirectMessageRetryDelayMs(1), qint64(30000));
        QCOMPARE(reliableDirectMessageRetryDelayMs(2), qint64(60000));
        QCOMPARE(reliableDirectMessageRetryDelayMs(3), qint64(120000));
        QCOMPARE(reliableDirectMessageRetryDelayMs(4), qint64(240000));
        QCOMPARE(reliableDirectMessageRetryDelayMs(5), qint64(300000));
        QCOMPARE(reliableDirectMessageRetryDelayMs(100), qint64(300000));
    }

    void retryPersistedServerMessageKeepsIdAndMarksServerAcked()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString connectionName = QStringLiteral("reliable-retry-service");
        DatabaseManager manager(dir.filePath(QStringLiteral("retry-service.db")), connectionName);
        QVERIFY(manager.open());
        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        ReliableDirectMessageSender sender(QStringLiteral("local-a"), &repository, &serverClient,
                                           [](const ReliableDirectMessageP2PRequest&, QString*) {
                                               return false;
                                           });
        const QString messageId = ChatService::createOutgoingMessage(
            QStringLiteral("local-a"), &repository, QStringLiteral("conv-local-peer"),
            QStringLiteral("peer-a"), QStringLiteral("<p>Hello</p>"));
        QVERIFY(!messageId.isEmpty());

        const auto result = sender.retryText(
            messageId, requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QCOMPARE(result.messageId, messageId);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(serverClient.drafts.front().clientMessageId, messageId);
        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
        QCOMPARE(repository.loadMessages(
                     QStringLiteral("conv-local-peer").toStdWString()).size(),
                 static_cast<std::size_t>(1));
    }

    void retryPersistedP2PMessageKeepsIdAndMarksSent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString connectionName = QStringLiteral("reliable-retry-p2p");
        DatabaseManager manager(dir.filePath(QStringLiteral("retry-p2p.db")), connectionName);
        QVERIFY(manager.open());
        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableDirectMessageP2PRequest> requests;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"), &repository, &serverClient,
            [&](const ReliableDirectMessageP2PRequest& request, QString*) {
                requests.push_back(request);
                return true;
            });
        const QString messageId = ChatService::createOutgoingMessage(
            QStringLiteral("local-a"), &repository, QStringLiteral("conv-local-peer"),
            QStringLiteral("peer-a"), QStringLiteral("<p>Hello</p>"));
        QVERIFY(!messageId.isEmpty());
        auto request = requestFor(RemoteChatTransportMode::ServerPreferred);
        request.receiverServerCapable = false;

        const auto result = sender.retryText(messageId, request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId, messageId);
        QCOMPARE(requests.size(), 1);
        QCOMPARE(requests.front().messageId, messageId);
        ChatMessage stored;
        QVERIFY(repository.findMessageById(messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void receiptRequiredP2PMessageRemainsPendingUntilDurableAck()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString connectionName = QStringLiteral("reliable-p2p-receipt-gate");
        DatabaseManager manager(dir.filePath(QStringLiteral("receipt-gate.db")),
                                connectionName);
        QVERIFY(manager.open());
        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"), &repository, &serverClient,
            [](const ReliableDirectMessageP2PRequest&, QString*) {
                return true;
            });

        auto request = requestFor(RemoteChatTransportMode::ServerPreferred);
        request.receiverServerCapable = false;
        request.requireP2PDeliveryReceipt = true;

        const auto result = sender.sendText(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
        QCOMPARE(repository.loadPendingOutgoingMessages(
                     request.conversationId.toStdWString(),
                     QStringLiteral("local-a").toStdWString()).size(),
                 static_cast<std::size_t>(1));
    }

    void serverPreferredReachableUsesServiceAndMarksServerAcked()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-service");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-service.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableDirectMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        ReliableDirectMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.replyToMessageId = QStringLiteral("quoted-message");
        request.replyToSenderId = QStringLiteral("peer-a");
        request.replyToBody = QStringLiteral("quoted body");
        const ReliableDirectMessageSendResult result = sender.sendText(request);

        QVERIFY(result.success);
        QVERIFY(!result.messageId.isEmpty());
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.clientMessageId, result.messageId);
        QCOMPARE(draft.conversationId, QStringLiteral("conv-local-peer"));
        QCOMPARE(draft.workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(draft.type, QStringLiteral("chat_text"));
        QCOMPARE(draft.body, QStringLiteral("<p>Hello</p>"));
        QCOMPARE(draft.contentType, QStringLiteral("html"));
        QCOMPARE(draft.recipientIds, QVector<QString>{QStringLiteral("peer-a")});
        QCOMPARE(draft.replyToMessageId, QStringLiteral("quoted-message"));
        QCOMPARE(draft.payload.value(QStringLiteral("reply_to_sender_id")).toString(),
                 QStringLiteral("peer-a"));
        QCOMPARE(draft.payload.value(QStringLiteral("reply_to_body")).toString(),
                 QStringLiteral("quoted body"));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
        QCOMPARE(repository.loadLocalMessageIdForRemoteServerId(QStringLiteral("srv-1")),
                 result.messageId);
    }

    void healthyServerLegacyReceiverUsesP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-legacy");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-legacy.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableDirectMessageP2PRequest> p2pRequests;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableDirectMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableDirectMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.serviceReachable = true;
        request.receiverServerCapable = false;
        request.p2pAvailable = true;

        const ReliableDirectMessageSendResult result = sender.sendText(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 0);
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(p2pRequests.front().messageId, result.messageId);
        QCOMPARE(p2pRequests.front().targetId, QStringLiteral("peer-a"));
    }

    void healthyServerCapableReceiverUsesService()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-capable");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-capable.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableDirectMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        ReliableDirectMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.serviceReachable = true;
        request.receiverServerCapable = true;
        request.p2pAvailable = false;

        const ReliableDirectMessageSendResult result = sender.sendText(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);
    }

    void serverPreferredServiceFailureFallsBackToP2PAndMarksSent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-fallback");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-fallback.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        bool sawPendingBeforeP2P = false;
        QVector<ReliableDirectMessageP2PRequest> p2pRequests;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableDirectMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                ChatMessage stored;
                sawPendingBeforeP2P =
                    repository.findMessageById(p2pRequest.messageId, &stored)
                    && stored.deliveryState == MessageDeliveryState::Pending;
                return true;
            });

        const ReliableDirectMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QVERIFY(sawPendingBeforeP2P);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(p2pRequests.front().messageId, result.messageId);
        QCOMPARE(p2pRequests.front().conversationId,
                 QStringLiteral("conv-local-peer"));
        QCOMPARE(p2pRequests.front().targetId, QStringLiteral("peer-a"));
        QCOMPARE(p2pRequests.front().body, QStringLiteral("<p>Hello</p>"));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void acceptedServiceAckWithLocalFinalizeFailureDoesNotFallbackToP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-ack-finalize-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-ack-finalize-fail.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QSqlQuery breakMapping(QSqlDatabase::database(connectionName, false));
        QVERIFY(breakMapping.exec(QStringLiteral("DROP TABLE remote_message_id_map")));

        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableDirectMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableDirectMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(!result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void serverOnlyServiceFailureDoesNotInvokeP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-server-only");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-server-only.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        int p2pCalls = 0;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableDirectMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableDirectMessageSendResult result =
            sender.sendText(requestFor(RemoteChatTransportMode::ServerOnly));

        QVERIFY(!result.success);
        QVERIFY(!result.messageId.isEmpty());
        QVERIFY(!result.errorMessage.isEmpty());
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void serviceExceptionFallsBackToP2PWithoutEscapingQtSlot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-service-exception");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-service-exception.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.throwOnSend = true;
        QVector<ReliableDirectMessageP2PRequest> p2pRequests;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableDirectMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        bool threw = false;
        ReliableDirectMessageSendResult result;
        try {
            result = sender.sendText(requestFor(RemoteChatTransportMode::ServerPreferred));
        } catch (const std::exception&) {
            threw = true;
        }

        QVERIFY2(!threw, "message-service exceptions must not escape the Qt send slot");
        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(p2pRequests.size(), 1);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void p2pExceptionReturnsFailureWithoutEscapingQtSlot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-sender-p2p-exception");
        DatabaseManager manager(dir.filePath(QStringLiteral("sender-p2p-exception.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        ReliableDirectMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [](const ReliableDirectMessageP2PRequest&, QString*) -> bool {
                throw std::length_error("string too long");
            });

        ReliableDirectMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.receiverServerCapable = false;
        request.p2pAvailable = true;

        bool threw = false;
        ReliableDirectMessageSendResult result;
        try {
            result = sender.sendText(request);
        } catch (const std::exception&) {
            threw = true;
        }

        QVERIFY2(!threw, "P2P send exceptions must not escape the Qt send slot");
        QVERIFY(!result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QVERIFY(!result.messageId.isEmpty());
        QVERIFY(result.errorMessage.contains(QStringLiteral("p2p")));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }
};

QTEST_MAIN(TestReliableDirectMessageSender)
#include "TestReliableDirectMessageSender.moc"
