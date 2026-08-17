#include <QtTest/QTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <optional>

#include "integrations/RemoteChatServiceSettings.h"
#include "integrations/ServerMessageClient.h"
#include "services/ReliableGroupFileMessageSender.h"
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
        ack.serverMessageId = QStringLiteral("srv-group-file-1");
        ack.conversationId = draft.conversationId;
        ack.serverSeq = 40;
        ack.createdAtMs = 4000;
        return ack;
    }

    std::optional<ServerMessagePage> listMessages(
        const QString&,
        qint64,
        int,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("list not used by group file sender tests");
        }
        return std::nullopt;
    }

    bool acknowledgeDelivered(const QString&,
                              qint64,
                              QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("delivery ack not used by group file sender tests");
        }
        return false;
    }

    bool acknowledgeRead(const QString&,
                         qint64,
                         QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("read ack not used by group file sender tests");
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

QJsonObject localFileCard()
{
    return QJsonObject{
        {QStringLiteral("channel"), QStringLiteral("fileservice")},
        {QStringLiteral("file_id"), QStringLiteral("file-1")},
        {QStringLiteral("file_name"), QStringLiteral("spec.docx")},
        {QStringLiteral("file_size"), 42},
        {QStringLiteral("sender_file_path"), QStringLiteral("C:/tmp/spec.docx")},
        {QStringLiteral("uploader_name"), QStringLiteral("Local User")}
    };
}

QJsonObject broadcastFileCard()
{
    QJsonObject card = localFileCard();
    card.remove(QStringLiteral("sender_file_path"));
    return card;
}

std::vector<MessageEnvelope> groupFileEnvelopes(const QString& messageId,
                                                const QString& groupId,
                                                const QString& senderId,
                                                const QStringList& targetIds,
                                                const QString& body)
{
    const QByteArray cardBytes =
        QJsonDocument(broadcastFileCard()).toJson(QJsonDocument::Compact);
    const std::string cardStr(cardBytes.constData(),
                              static_cast<std::size_t>(cardBytes.size()));
    const std::string bodyStr = body.toStdString();

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(static_cast<std::size_t>(targetIds.size()));
    for (const QString& targetId : targetIds) {
        MessageEnvelope envelope;
        envelope.messageId = messageId.toStdString();
        envelope.type = MessageType::GroupMessage;
        envelope.senderId = senderId.toStdString();
        envelope.targetId = targetId.toStdString();
        envelope.conversationId = groupId.toStdString();
        envelope.messageSubtype = "group_file_card";
        envelope.body = bodyStr;
        envelope.payloadJson = cardStr;
        envelope.createdAtMs = 1234;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

ReliableGroupFileMessageSendRequest requestFor(RemoteChatTransportMode mode)
{
    ReliableGroupFileMessageSendRequest request;
    request.messageId = QStringLiteral("group-file-msg-1");
    request.createdAtMs = 1234;
    request.groupId = QStringLiteral("group:ops");
    request.groupTitle = QStringLiteral("Ops Room");
    request.body = QStringLiteral("spec.docx");
    request.fileId = QStringLiteral("file-1");
    request.localFilePath = QStringLiteral("C:/tmp/spec.docx");
    request.localFileCard = localFileCard();
    request.broadcastFileCard = broadcastFileCard();
    request.envelopes = groupFileEnvelopes(request.messageId,
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

class TestReliableGroupFileMessageSender : public QObject {
    Q_OBJECT

private slots:
    void serverPreferredReachableStoresFileCardAndSendsOneServiceDraft()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-file-service");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-service.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-file-msg-1"));
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        const ServerMessageDraft& draft = serverClient.drafts.front();
        QCOMPARE(draft.clientMessageId, result.messageId);
        QCOMPARE(draft.conversationId, QStringLiteral("group:ops"));
        QCOMPARE(draft.workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(draft.type, QStringLiteral("group_file_card"));
        QCOMPARE(draft.body, QStringLiteral("spec.docx"));
        QCOMPARE(draft.fileId, QStringLiteral("file-1"));
        QCOMPARE(draft.contentType,
                 QStringLiteral("application/vnd.leyochat.group-file-card+json"));
        QVERIFY(!draft.payload.contains(QStringLiteral("sender_file_path")));
        QCOMPARE(draft.payload.value(QStringLiteral("file_id")).toString(),
                 QStringLiteral("file-1"));
        QCOMPARE(draft.recipientIds,
                 QVector<QString>({QStringLiteral("peer-a"),
                                   QStringLiteral("peer-b")}));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(QString::fromStdWString(stored.conversationId),
                 QStringLiteral("group:ops"));
        QCOMPARE(QString::fromStdWString(stored.body),
                 QStringLiteral("spec.docx"));
        QCOMPARE(QString::fromStdWString(stored.messageType),
                 QStringLiteral("group_file_card"));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
        QCOMPARE(QString::fromStdWString(stored.localFilePath),
                 QStringLiteral("C:/tmp/spec.docx"));

        const QJsonObject storedCard = QJsonDocument::fromJson(
            QString::fromStdWString(stored.fileCardJson).toUtf8()).object();
        QCOMPARE(storedCard.value(QStringLiteral("sender_file_path")).toString(),
                 QStringLiteral("C:/tmp/spec.docx"));
        QCOMPARE(repository.loadLocalMessageIdForRemoteServerId(
                     QStringLiteral("srv-group-file-1")),
                 result.messageId);
    }

    void p2pFileCardSentThroughServiceKeepsSenderFilePathForOnDemandDownload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-file-p2p-card-service");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-p2p-card-service.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest&, QString*) {
                return true;
            });

        QJsonObject p2pCard{
            {QStringLiteral("channel"), QStringLiteral("p2p")},
            {QStringLiteral("file_name"), QStringLiteral("spec.docx")},
            {QStringLiteral("file_size"), 42},
            {QStringLiteral("sender_id"), QStringLiteral("local-a")},
            {QStringLiteral("sender_file_path"), QStringLiteral("C:/tmp/spec.docx")},
            {QStringLiteral("uploader_name"), QStringLiteral("Local User")}
        };

        ReliableGroupFileMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.messageId = QStringLiteral("group-file-p2p-card-service");
        request.body = QStringLiteral("[group file] spec.docx");
        request.fileId.clear();
        request.localFileCard = p2pCard;
        request.broadcastFileCard = p2pCard;
        request.envelopes = groupFileEnvelopes(request.messageId,
                                               request.groupId,
                                               QStringLiteral("local-a"),
                                               {QStringLiteral("new-a")},
                                               request.body);
        for (MessageEnvelope& envelope : request.envelopes) {
            envelope.payloadJson = QString::fromUtf8(
                QJsonDocument(p2pCard).toJson(QJsonDocument::Compact)).toStdString();
        }
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds.clear();

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(request);

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(serverClient.drafts.front()
                     .payload.value(QStringLiteral("sender_file_path")).toString(),
                 QStringLiteral("C:/tmp/spec.docx"));
    }

    void serverPreferredMixedRecipientsSplitsServiceAndP2PFileCards()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-file-mixed");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-mixed.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableGroupFileMessageP2PRequest> p2pRequests;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableGroupFileMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.messageId = QStringLiteral("group-file-msg-mixed");
        request.envelopes = groupFileEnvelopes(
            request.messageId,
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

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-file-msg-mixed"));
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

        const QString connectionName =
            QStringLiteral("reliable-group-file-mixed-p2p-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-mixed-p2p-fail.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableGroupFileMessageP2PRequest> p2pRequests;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest& p2pRequest, QString* errorMessage) {
                p2pRequests.push_back(p2pRequest);
                if (errorMessage) {
                    *errorMessage = QStringLiteral("legacy p2p file queue failed");
                }
                return false;
            });

        ReliableGroupFileMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.messageId = QStringLiteral("group-file-msg-mixed-p2p-fail");
        request.envelopes = groupFileEnvelopes(
            request.messageId,
            request.groupId,
            QStringLiteral("local-a"),
            {QStringLiteral("new-a"),
             QStringLiteral("legacy-a")},
            request.body);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(request);

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-file-msg-mixed-p2p-fail"));
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QVERIFY(result.errorMessage.contains(QStringLiteral("legacy p2p file queue failed")));
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(serverClient.drafts.size(), 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void serverPreferredMixedRecipientsServiceFailureFallsBackServerRecipientsToP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("reliable-group-file-mixed-service-fallback");
        DatabaseManager manager(
            dir.filePath(QStringLiteral("group-file-mixed-service-fallback.db")),
            connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        QVector<ReliableGroupFileMessageP2PRequest> p2pRequests;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        ReliableGroupFileMessageSendRequest request =
            requestFor(RemoteChatTransportMode::ServerPreferred);
        request.messageId = QStringLiteral("group-file-msg-mixed-service-fallback");
        request.envelopes = groupFileEnvelopes(
            request.messageId,
            request.groupId,
            QStringLiteral("local-a"),
            {QStringLiteral("new-a"),
             QStringLiteral("legacy-a")},
            request.body);
        request.serverRecipientIds = QVector<QString>{QStringLiteral("new-a")};
        request.p2pRecipientIds = QVector<QString>{QStringLiteral("legacy-a")};

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId,
                 QStringLiteral("group-file-msg-mixed-service-fallback"));
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
            QStringLiteral("reliable-group-file-ack-finalize-fail");
        DatabaseManager manager(
            dir.filePath(QStringLiteral("group-file-ack-finalize-fail.db")),
            connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QSqlQuery breakMapping(QSqlDatabase::database(connectionName, false));
        QVERIFY(breakMapping.exec(QStringLiteral("DROP TABLE remote_message_id_map")));

        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-file-msg-1"));
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void p2pOnlyUsesCallbackAndMarksSent()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-file-p2p-only");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-p2p.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        QVector<ReliableGroupFileMessageP2PRequest> p2pRequests;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                return true;
            });

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(requestFor(RemoteChatTransportMode::P2POnly));

        QVERIFY(result.success);
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 0);
        QCOMPARE(p2pRequests.size(), 1);
        QCOMPARE(p2pRequests.front().messageId, result.messageId);
        QVERIFY(!p2pRequests.front().acceptQueuedOnlyDelivery);
        QCOMPARE(p2pRequests.front().groupId, QStringLiteral("group:ops"));
        QCOMPARE(p2pRequests.front().envelopes.size(), static_cast<std::size_t>(2));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void serverPreferredServiceFailureFallsBackToP2PWithPendingLocalMessage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-file-fallback");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-fallback.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        bool sawPendingBeforeP2P = false;
        QVector<ReliableGroupFileMessageP2PRequest> p2pRequests;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest& p2pRequest, QString*) {
                p2pRequests.push_back(p2pRequest);
                ChatMessage stored;
                sawPendingBeforeP2P =
                    repository.findMessageById(p2pRequest.messageId, &stored)
                    && stored.deliveryState == MessageDeliveryState::Pending;
                return true;
            });

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(requestFor(RemoteChatTransportMode::ServerPreferred));

        QVERIFY(result.success);
        QVERIFY(sawPendingBeforeP2P);
        QCOMPARE(result.messageId, QStringLiteral("group-file-msg-1"));
        QCOMPARE(result.channelUsed, TransportChannel::P2P);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pRequests.size(), 1);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
    }

    void serverOnlyServiceFailureLeavesPendingAndSkipsP2P()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-file-server-only");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-server-only.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        serverClient.failSend = true;
        int p2pCalls = 0;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(requestFor(RemoteChatTransportMode::ServerOnly));

        QVERIFY(!result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-file-msg-1"));
        QVERIFY(!result.errorMessage.isEmpty());
        QCOMPARE(result.channelUsed, TransportChannel::MessageService);
        QCOMPARE(serverClient.drafts.size(), 1);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Pending);
    }

    void noRecipientsStoresLocalFileCardAndCompletesWithoutTransport()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("reliable-group-file-local-only");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-file-local-only.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient serverClient;
        int p2pCalls = 0;
        ReliableGroupFileMessageSender sender(
            QStringLiteral("local-a"),
            &repository,
            &serverClient,
            [&](const ReliableGroupFileMessageP2PRequest&, QString*) {
                ++p2pCalls;
                return true;
            });

        ReliableGroupFileMessageSendRequest request =
            requestFor(RemoteChatTransportMode::P2POnly);
        request.messageId = QStringLiteral("group-file-local-only");
        request.createdAtMs = 2222;
        request.envelopes.clear();
        request.p2pAvailable = false;
        request.serviceReachable = false;

        const ReliableGroupFileMessageSendResult result =
            sender.sendFileCard(request);

        QVERIFY(result.success);
        QCOMPARE(result.messageId, QStringLiteral("group-file-local-only"));
        QCOMPARE(result.channelUsed, TransportChannel::None);
        QCOMPARE(serverClient.drafts.size(), 0);
        QCOMPARE(p2pCalls, 0);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(result.messageId, &stored));
        QCOMPARE(QString::fromStdWString(stored.conversationId),
                 QStringLiteral("group:ops"));
        QCOMPARE(stored.createdAtMs, qint64(2222));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);
        QCOMPARE(QString::fromStdWString(stored.messageType),
                 QStringLiteral("group_file_card"));
        QVERIFY(!QString::fromStdWString(stored.fileCardJson).isEmpty());
    }
};

QTEST_MAIN(TestReliableGroupFileMessageSender)
#include "TestReliableGroupFileMessageSender.moc"
