#include <QtTest/QTest>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QTemporaryDir>

#include <algorithm>
#include <functional>
#include <optional>

#include "domain/MessageMutation.h"
#include "integrations/ServerMessageClient.h"
#include "services/MessageSyncService.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

namespace {

class FakeServerMessageClient final : public IServerMessageClient {
public:
    struct ListCall {
        QString conversationId;
        qint64 afterSeq = -1;
        int limit = 0;
    };

    struct DeliveryAckCall {
        QString serverMessageId;
        qint64 receivedSeq = -1;
    };

    struct ReadAckCall {
        QString serverMessageId;
        qint64 readSeq = -1;
    };

    ServerMessagePage page;
    QString errorToReturn;
    bool deliveryAckResult = true;
    QString deliveryAckError;
    bool readAckResult = true;
    QString readAckError;
    QSet<QString> missingDeliveryAckIds;
    QSet<QString> missingReadAckIds;
    std::function<void(const QString&, qint64)> onDeliveryAck;
    mutable QVector<ListCall> listCalls;
    mutable QVector<DeliveryAckCall> deliveryAckCalls;
    mutable QVector<ReadAckCall> readAckCalls;

    std::optional<ServerMessageAck> sendMessage(
        const ServerMessageDraft&,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("send not used by sync tests");
        }
        return std::nullopt;
    }

    std::optional<ServerMessagePage> listMessages(
        const QString& conversationId,
        qint64 afterSeq,
        int limit,
        QString* errorMessage = nullptr) const override
    {
        listCalls.push_back({conversationId, afterSeq, limit});
        if (!errorToReturn.isEmpty()) {
            if (errorMessage) {
                *errorMessage = errorToReturn;
            }
            return std::nullopt;
        }
        return page;
    }

    bool acknowledgeDelivered(const QString& serverMessageId,
                              qint64 receivedSeq,
                              QString* errorMessage = nullptr) const override
    {
        deliveryAckCalls.push_back({serverMessageId, receivedSeq});
        if (onDeliveryAck) {
            onDeliveryAck(serverMessageId, receivedSeq);
        }
        if (!deliveryAckResult && errorMessage) {
            *errorMessage = deliveryAckError.trimmed().isEmpty()
                ? QStringLiteral("delivery ack failed")
                : deliveryAckError;
        }
        return deliveryAckResult;
    }

    bool acknowledgeRead(const QString& serverMessageId,
                         qint64 readSeq,
                         QString* errorMessage = nullptr) const override
    {
        readAckCalls.push_back({serverMessageId, readSeq});
        if (!readAckResult && errorMessage) {
            *errorMessage = readAckError.trimmed().isEmpty()
                ? QStringLiteral("read ack failed")
                : readAckError;
        }
        return readAckResult;
    }

    ServerAckAttemptResult acknowledgeDeliveredResult(
        const QString& serverMessageId,
        qint64 receivedSeq) const override
    {
        if (missingDeliveryAckIds.contains(serverMessageId)) {
            deliveryAckCalls.push_back({serverMessageId, receivedSeq});
            return ServerAckAttemptResult{
                ServerAckOutcome::MessageNotFound,
                404,
                QStringLiteral("message not found")};
        }
        return IServerMessageClient::acknowledgeDeliveredResult(
            serverMessageId, receivedSeq);
    }

    ServerAckAttemptResult acknowledgeReadResult(
        const QString& serverMessageId,
        qint64 readSeq) const override
    {
        if (missingReadAckIds.contains(serverMessageId)) {
            readAckCalls.push_back({serverMessageId, readSeq});
            return ServerAckAttemptResult{
                ServerAckOutcome::MessageNotFound,
                404,
                QStringLiteral("message not found")};
        }
        return IServerMessageClient::acknowledgeReadResult(serverMessageId,
                                                           readSeq);
    }
};

ServerMessageRecord textRecord(const QString& clientMessageId,
                               const QString& senderId,
                               qint64 serverSeq)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("srv-%1").arg(serverSeq);
    record.clientMessageId = clientMessageId;
    record.conversationId = QStringLiteral("conv-service");
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("chat_text");
    record.body = QStringLiteral("hello from service %1").arg(serverSeq);
    record.payload = QJsonObject{
        {QStringLiteral("html"), QStringLiteral("<b>hello</b>")}
    };
    record.contentType = QStringLiteral("html");
    record.createdAtMs = 1000 + serverSeq;
    return record;
}

ServerMessageRecord groupTextRecord(const QString& clientMessageId,
                                    const QString& groupId,
                                    const QString& senderId,
                                    qint64 serverSeq)
{
    ServerMessageRecord record =
        textRecord(clientMessageId, senderId, serverSeq);
    record.conversationId = groupId;
    record.body = QStringLiteral("<p>group service message %1</p>").arg(serverSeq);
    return record;
}

ServerMessageRecord groupFileCardRecord(const QString& clientMessageId,
                                        const QString& groupId,
                                        const QString& senderId,
                                        qint64 serverSeq)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("srv-file-%1").arg(serverSeq);
    record.clientMessageId = clientMessageId;
    record.conversationId = groupId;
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("group_file_card");
    record.body = QStringLiteral("spec.docx");
    record.payload = QJsonObject{
        {QStringLiteral("channel"), QStringLiteral("fileservice")},
        {QStringLiteral("file_id"), QStringLiteral("file-1")},
        {QStringLiteral("file_name"), QStringLiteral("spec.docx")},
        {QStringLiteral("file_size"), 42}
    };
    record.fileId = QStringLiteral("file-1");
    record.contentType =
        QStringLiteral("application/vnd.leyochat.group-file-card+json");
    record.createdAtMs = 2000 + serverSeq;
    return record;
}

ServerMessageRecord mutationRecord(const QString& mutationMessageId,
                                   const QString& groupId,
                                   const QString& senderId,
                                   qint64 serverSeq,
                                   const std::string& payloadJson)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("srv-mutation-%1").arg(serverSeq);
    record.clientMessageId = mutationMessageId;
    record.conversationId = groupId;
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("message_mutation");
    record.payload = QJsonDocument::fromJson(
        QByteArray::fromStdString(payloadJson)).object();
    record.createdAtMs = 3000 + serverSeq;
    return record;
}

ServerMessageRecord reactionRecord(const QString& reactionMessageId,
                                   const QString& groupId,
                                   const QString& senderId,
                                   qint64 serverSeq,
                                   const QString& targetMessageId,
                                   const QString& emoji)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("srv-reaction-%1").arg(serverSeq);
    record.clientMessageId = reactionMessageId;
    record.conversationId = groupId;
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("message_reaction");
    record.payload = QJsonObject{
        {QStringLiteral("targetMessageId"), targetMessageId},
        {QStringLiteral("emoji"), emoji}
    };
    record.createdAtMs = 4000 + serverSeq;
    return record;
}

ServerMessageRecord pinRecord(const QString& pinMessageId,
                              const QString& groupId,
                              const QString& senderId,
                              qint64 serverSeq,
                              const QString& targetMessageId,
                              const QString& action)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("srv-pin-%1").arg(serverSeq);
    record.clientMessageId = pinMessageId;
    record.conversationId = groupId;
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("pin_message");
    record.payload = QJsonObject{
        {QStringLiteral("group_id"), groupId},
        {QStringLiteral("message_id"), targetMessageId},
        {QStringLiteral("pinned_body"), QStringLiteral("important body")},
        {QStringLiteral("author_name"), QStringLiteral("Peer A")},
        {QStringLiteral("pinner_name"), QStringLiteral("Peer B")},
        {QStringLiteral("action"), action}
    };
    record.createdAtMs = 5000 + serverSeq;
    return record;
}

ServerMessageRecord nudgeRecord(const QString& clientMessageId,
                                const QString& senderId,
                                qint64 serverSeq)
{
    ServerMessageRecord record = textRecord(clientMessageId, senderId, serverSeq);
    record.body = QStringLiteral("[nudge]");
    record.contentType = QStringLiteral("nudge");
    return record;
}

ServerMessageRecord stickerRecord(const QString& clientMessageId,
                                  const QString& senderId,
                                  qint64 serverSeq,
                                  const QByteArray& gifData)
{
    ServerMessageRecord record = textRecord(clientMessageId, senderId, serverSeq);
    record.type = QStringLiteral("sticker");
    record.body = QStringLiteral("[sticker]");
    record.contentType = QStringLiteral("plain");
    record.payload = QJsonObject{
        {QStringLiteral("pack_id"), QStringLiteral("pack-a")},
        {QStringLiteral("sticker_id"), QStringLiteral("wave")},
        {QStringLiteral("gif_base64"), QString::fromLatin1(gifData.toBase64())}
    };
    return record;
}

ServerMessageRecord forwardPackageRecord(const QString& clientMessageId,
                                         const QString& conversationId,
                                         const QString& senderId,
                                         qint64 serverSeq)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("srv-forward-%1").arg(serverSeq);
    record.clientMessageId = clientMessageId;
    record.conversationId = conversationId;
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("forward_package");
    record.body = QStringLiteral("Merged forward");
    record.payload = QJsonObject{
        {QStringLiteral("title"), QStringLiteral("Forwarded messages")},
        {QStringLiteral("count"), 2}
    };
    record.contentType = QStringLiteral("plain");
    record.createdAtMs = 6000 + serverSeq;
    return record;
}

ChatMessage localMessage(const QString& messageId,
                         const QString& senderId,
                         const QString& body)
{
    ChatMessage message;
    message.messageId = messageId.toStdWString();
    message.conversationId = L"conv-service";
    message.senderId = senderId.toStdWString();
    message.body = body.toStdWString();
    message.createdAtMs = 900;
    message.deliveryState = MessageDeliveryState::ServerAcked;
    return message;
}

ChatMessage storedGroupTextMessage(const QString& messageId,
                                   const QString& groupId,
                                   const QString& senderId,
                                   const QString& body,
                                   qint64 createdAtMs)
{
    ChatMessage message;
    message.messageId = messageId.toStdWString();
    message.conversationId = groupId.toStdWString();
    message.senderId = senderId.toStdWString();
    message.body = body.toStdWString();
    message.createdAtMs = createdAtMs;
    message.deliveryState = MessageDeliveryState::Received;
    message.messageType = L"text";
    return message;
}

ChatMessage localSystemMessage(const QString& messageId,
                               const QString& groupId,
                               const QString& senderId,
                               const QString& body,
                               qint64 createdAtMs)
{
    ChatMessage message;
    message.messageId = messageId.toStdWString();
    message.conversationId = groupId.toStdWString();
    message.senderId = senderId.toStdWString();
    message.body = body.toStdWString();
    message.createdAtMs = createdAtMs;
    message.deliveryState = MessageDeliveryState::Read;
    message.messageType = L"system";
    return message;
}

int systemMessageCount(ConversationRepository* repository,
                       const QString& conversationId)
{
    const auto messages = repository->loadMessages(conversationId.toStdWString());
    return static_cast<int>(std::count_if(
        messages.cbegin(),
        messages.cend(),
        [](const ChatMessage& message) {
            return QString::fromStdWString(message.messageType)
                == QStringLiteral("system");
        }));
}

}  // namespace

class TestMessageSyncService : public QObject {
    Q_OBJECT

private slots:
    void syncFromEmptyCursorStoresPeerMessageAsReceived()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-peer");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-peer.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(textRecord(QStringLiteral("peer-msg-1"),
                                                  QStringLiteral("peer-a"),
                                                  1));
        client.page.nextAfterSeq = 1;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);
        QCOMPARE(result.skippedDuplicateCount, 0);
        QCOMPARE(result.newIncomingConversationIds,
                 QStringList({QStringLiteral("conv-service")}));
        QCOMPARE(result.newIncomingNotifications.size(), 1);
        QCOMPARE(result.newIncomingNotifications.front().conversationId,
                 QStringLiteral("conv-service"));
        QCOMPARE(result.newIncomingNotifications.front().senderId,
                 QStringLiteral("peer-a"));
        QCOMPARE(result.newIncomingNotifications.front().messageId,
                 QStringLiteral("peer-msg-1"));
        QCOMPARE(result.newIncomingNotifications.front().messageType,
                 QStringLiteral("text"));
        QCOMPARE(result.previousCursor, qint64(0));
        QCOMPARE(result.nextCursor, qint64(1));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("conv-service")),
                 qint64(1));

        QCOMPARE(client.listCalls.size(), 1);
        QCOMPARE(client.listCalls.front().conversationId,
                 QStringLiteral("conv-service"));
        QCOMPARE(client.listCalls.front().afterSeq, qint64(0));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("peer-msg-1"), &stored));
        QCOMPARE(QString::fromStdWString(stored.conversationId),
                 QStringLiteral("conv-service"));
        QCOMPARE(QString::fromStdWString(stored.senderId), QStringLiteral("peer-a"));
        QCOMPARE(QString::fromStdWString(stored.body),
                 QStringLiteral("hello from service 1"));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
        QCOMPARE(QString::fromStdWString(stored.messageType), QStringLiteral("text"));
        const QJsonObject payload = QJsonDocument::fromJson(
            QString::fromStdWString(stored.payloadJson).toUtf8()).object();
        QCOMPARE(payload.value(QStringLiteral("html")).toString(),
                 QStringLiteral("<b>hello</b>"));
    }

    void syncPeerMessageAcknowledgesDeliveryAfterPersisting()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-delivery-ack");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-ack.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(textRecord(QStringLiteral("peer-msg-ack"),
                                                  QStringLiteral("peer-a"),
                                                  3));
        client.page.nextAfterSeq = 3;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("conv-service")),
                 qint64(3));
        QCOMPARE(client.deliveryAckCalls.size(), 1);
        QCOMPARE(client.deliveryAckCalls.front().serverMessageId,
                 QStringLiteral("srv-3"));
        QCOMPARE(client.deliveryAckCalls.front().receivedSeq, qint64(3));
    }

    void serviceMessageRestoresMentionsAndCompleteReplyContext()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-rich-context");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-rich-context.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"group:ops", L"Ops Room", L"old preview", 900}));

        FakeServerMessageClient client;
        ServerMessageRecord record = groupTextRecord(QStringLiteral("group-rich-1"),
                                                     QStringLiteral("group:ops"),
                                                     QStringLiteral("peer-a"),
                                                     2);
        record.replyToMessageId = QStringLiteral("quoted-group-message");
        record.payload.insert(QStringLiteral("mentioned_ids"),
                              QJsonArray({QStringLiteral("local-a"),
                                          QStringLiteral("peer-b")}));
        record.payload.insert(QStringLiteral("reply_to_sender_id"),
                              QStringLiteral("peer-b"));
        record.payload.insert(QStringLiteral("reply_to_body"),
                              QStringLiteral("quoted group body"));
        client.page.messages.push_back(record);
        client.page.nextAfterSeq = 2;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("group-rich-1"), &stored));
        QCOMPARE(QString::fromStdWString(stored.replyToMessageId),
                 QStringLiteral("quoted-group-message"));
        QCOMPARE(QString::fromStdWString(stored.replyToSenderId),
                 QStringLiteral("peer-b"));
        QCOMPARE(QString::fromStdWString(stored.replyToBody),
                 QStringLiteral("quoted group body"));
        QCOMPARE(QJsonDocument::fromJson(
                     QString::fromStdWString(stored.mentionedIds).toUtf8()).array(),
                 QJsonArray({QStringLiteral("local-a"), QStringLiteral("peer-b")}));

        const auto summaries = repository.loadConversationSummaries();
        const auto summaryIt = std::find_if(
            summaries.cbegin(), summaries.cend(), [](const ConversationSummary& summary) {
                return QString::fromStdWString(summary.conversationId)
                    == QStringLiteral("group:ops");
            });
        QVERIFY(summaryIt != summaries.cend());
        QVERIFY(summaryIt->hasMentionMe);
        QCOMPARE(result.newIncomingNotifications.size(), 1);
        QCOMPARE(result.newIncomingNotifications.front().mentionedIds,
                 QStringList({QStringLiteral("local-a"),
                              QStringLiteral("peer-b")}));
    }

    void serviceStickerCachesBinaryAndDoesNotPersistBase64()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-sticker-cache");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-sticker-cache.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        const QByteArray gifData("GIF89a-test-sticker");
        client.page.messages.push_back(stickerRecord(
            QStringLiteral("sticker-msg-1"), QStringLiteral("peer-a"), 3, gifData));
        client.page.nextAfterSeq = 3;

        QString cachedPackId;
        QString cachedStickerId;
        QByteArray cachedData;
        MessageSyncService service(
            QStringLiteral("local-a"), &repository, &client, 100,
            [&](const QString& packId,
                const QString& stickerId,
                const QByteArray& data) {
                cachedPackId = packId;
                cachedStickerId = stickerId;
                cachedData = data;
                return true;
            });
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(cachedPackId, QStringLiteral("pack-a"));
        QCOMPARE(cachedStickerId, QStringLiteral("wave"));
        QCOMPARE(cachedData, gifData);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("sticker-msg-1"), &stored));
        QCOMPARE(QString::fromStdWString(stored.messageType), QStringLiteral("sticker"));
        const QJsonObject storedPayload = QJsonDocument::fromJson(
            QString::fromStdWString(stored.payloadJson).toUtf8()).object();
        QCOMPARE(storedPayload.value(QStringLiteral("pack_id")).toString(),
                 QStringLiteral("pack-a"));
        QCOMPARE(storedPayload.value(QStringLiteral("sticker_id")).toString(),
                 QStringLiteral("wave"));
        QVERIFY(!storedPayload.contains(QStringLiteral("gif_base64")));
    }

    void newlyStoredSelfAuthoredMessageDoesNotRequestNotification()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-new-self");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-new-self.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(textRecord(QStringLiteral("self-msg-new"),
                                                  QStringLiteral("local-a"),
                                                  2));
        client.page.nextAfterSeq = 2;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);
        QVERIFY(result.newIncomingConversationIds.isEmpty());
    }

    void syncPeerMessageAcknowledgesDeliveryOnlyAfterCursorIsDurable()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath(QStringLiteral("sync-ack-order.db"));
        const QString connectionName =
            QStringLiteral("message-sync-delivery-ack-after-cursor");
        DatabaseManager manager(dbPath, connectionName);
        QVERIFY(manager.open());

        const QString readConnectionName =
            QStringLiteral("message-sync-delivery-ack-after-cursor-reader");
        DatabaseManager readManager(dbPath, readConnectionName);
        QVERIFY(readManager.open());

        ConversationRepository repository(connectionName);
        ConversationRepository readRepository(readConnectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(textRecord(QStringLiteral("peer-msg-ack-order"),
                                                  QStringLiteral("peer-a"),
                                                  5));
        client.page.nextAfterSeq = 5;
        qint64 cursorSeenByAck = -1;
        client.onDeliveryAck = [&](const QString&, qint64) {
            cursorSeenByAck =
                readRepository.loadRemoteChatCursor(QStringLiteral("conv-service"));
        };

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(client.deliveryAckCalls.size(), 1);
        QCOMPARE(cursorSeenByAck, qint64(5));
    }

    void syncPeerMessageKeepsPendingDeliveryAckWhenServerAckFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-delivery-ack-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-ack-fail.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.saveRemoteChatCursor(QStringLiteral("conv-service"), 2));

        FakeServerMessageClient client;
        client.deliveryAckResult = false;
        client.deliveryAckError = QStringLiteral("server refused delivery ack");
        client.page.messages.push_back(textRecord(QStringLiteral("peer-msg-ack-fail"),
                                                  QStringLiteral("peer-a"),
                                                  4));
        client.page.nextAfterSeq = 4;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(result.previousCursor, qint64(2));
        QCOMPARE(result.nextCursor, qint64(4));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("conv-service")),
                 qint64(4));
        QCOMPARE(client.deliveryAckCalls.size(), 1);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("peer-msg-ack-fail"),
                                           &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
        const auto pendingDeliveryAcks =
            repository.loadPendingRemoteDeliveryAcks(10);
        QCOMPARE(pendingDeliveryAcks.size(), size_t(1));
        QCOMPARE(pendingDeliveryAcks.front().serverMessageId,
                 QStringLiteral("srv-4"));
        QCOMPARE(pendingDeliveryAcks.front().receivedSeq, qint64(4));
    }

    void duplicateSelfAuthoredMessageIsNotOverwritten()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-duplicate-self");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-duplicate.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(localMessage(QStringLiteral("self-msg-1"),
                                                      QStringLiteral("local-a"),
                                                      QStringLiteral("local original"))));

        FakeServerMessageClient client;
        ServerMessageRecord echo =
            textRecord(QStringLiteral("self-msg-1"), QStringLiteral("local-a"), 7);
        echo.body = QStringLiteral("server echo should not replace local body");
        client.page.messages.push_back(echo);
        client.page.nextAfterSeq = 7;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 0);
        QCOMPARE(result.skippedDuplicateCount, 1);
        QVERIFY(result.newIncomingConversationIds.isEmpty());
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("conv-service")),
                 qint64(7));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("self-msg-1"), &stored));
        QCOMPARE(QString::fromStdWString(stored.senderId),
                 QStringLiteral("local-a"));
        QCOMPARE(QString::fromStdWString(stored.body),
                 QStringLiteral("local original"));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
    }

    void cursorDoesNotAdvanceWhenLocalPersistenceFails()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-persist-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-fail.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.saveRemoteChatCursor(QStringLiteral("conv-service"), 4));

        QSqlQuery breakPersistence(QSqlDatabase::database(connectionName, false));
        QVERIFY(breakPersistence.exec(QStringLiteral("DROP TABLE messages")));

        FakeServerMessageClient client;
        client.page.messages.push_back(textRecord(QStringLiteral("peer-msg-5"),
                                                  QStringLiteral("peer-a"),
                                                  5));
        client.page.nextAfterSeq = 5;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(!result.success);
        QVERIFY(!result.errorMessage.isEmpty());
        QCOMPARE(result.previousCursor, qint64(4));
        QCOMPARE(result.nextCursor, qint64(4));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("conv-service")),
                 qint64(4));
    }

    void syncingKnownGroupMessagePreservesExistingGroupTitle()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-group-title");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-group.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"group:ops",
            L"Ops Room",
            L"old preview",
            900
        }));

        FakeServerMessageClient client;
        client.page.messages.push_back(groupTextRecord(
            QStringLiteral("group-msg-1"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            11));
        client.page.nextAfterSeq = 11;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);
        QCOMPARE(result.newIncomingConversationIds,
                 QStringList({QStringLiteral("group:ops")}));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(11));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("group-msg-1"), &stored));
        QCOMPARE(QString::fromStdWString(stored.conversationId),
                 QStringLiteral("group:ops"));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);

        const auto summaries = repository.loadConversationSummaries();
        auto found = std::find_if(
            summaries.cbegin(),
            summaries.cend(),
            [](const ConversationSummary& summary) {
                return summary.conversationId == L"group:ops";
            });
        QVERIFY(found != summaries.cend());
        QCOMPARE(QString::fromStdWString(found->title), QStringLiteral("Ops Room"));
        QCOMPARE(QString::fromStdWString(found->lastMessagePreview),
                 QStringLiteral("<p>group service message 11</p>"));
    }

    void groupFileCardSyncPersistsRenderableFileCardJson()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-group-file");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-group-file.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"group:ops",
            L"Ops Room",
            L"old preview",
            900
        }));

        FakeServerMessageClient client;
        client.page.messages.push_back(groupFileCardRecord(
            QStringLiteral("group-file-msg-1"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            12));
        client.page.nextAfterSeq = 12;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(12));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("group-file-msg-1"),
                                           &stored));
        QCOMPARE(QString::fromStdWString(stored.conversationId),
                 QStringLiteral("group:ops"));
        QCOMPARE(QString::fromStdWString(stored.senderId),
                 QStringLiteral("peer-a"));
        QCOMPARE(QString::fromStdWString(stored.body),
                 QStringLiteral("spec.docx"));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
        QCOMPARE(QString::fromStdWString(stored.messageType),
                 QStringLiteral("group_file_card"));
        QVERIFY(QString::fromStdWString(stored.payloadJson).isEmpty());

        const QJsonObject fileCard = QJsonDocument::fromJson(
            QString::fromStdWString(stored.fileCardJson).toUtf8()).object();
        QCOMPARE(fileCard.value(QStringLiteral("channel")).toString(),
                 QStringLiteral("fileservice"));
        QCOMPARE(fileCard.value(QStringLiteral("file_id")).toString(),
                 QStringLiteral("file-1"));
        QCOMPARE(fileCard.value(QStringLiteral("file_name")).toString(),
                 QStringLiteral("spec.docx"));
        QCOMPARE(fileCard.value(QStringLiteral("file_size")).toInt(), 42);
        QCOMPARE(result.newIncomingNotifications.size(), 1);
        QCOMPARE(result.newIncomingNotifications.front().messageType,
                 QStringLiteral("group_file_card"));
        QCOMPARE(result.newIncomingNotifications.front().payload.value(
                     QStringLiteral("file_id")).toString(),
                 QStringLiteral("file-1"));
    }

    void groupRecallMutationFromServiceAppliesToExistingMessage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-group-recall");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-group-recall.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"group:ops",
            L"Ops Room",
            L"old preview",
            900
        }));
        QVERIFY(repository.appendMessage(storedGroupTextMessage(
            QStringLiteral("group-msg-recall"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            QStringLiteral("recall me"),
            1000)));

        FakeServerMessageClient client;
        client.page.messages.push_back(mutationRecord(
            QStringLiteral("group-mut-recall-1"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            13,
            buildRecallPayloadJson(QStringLiteral("group-msg-recall"), 1500)));
        client.page.nextAfterSeq = 13;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(13));

        ChatMessage state;
        QVERIFY(repository.findMessageMutationStateById(QStringLiteral("group-msg-recall"),
                                                        &state));
        QVERIFY(state.isRecalled);
        QCOMPARE(state.lastMutationAtMs, qint64(1500));
        QCOMPARE(QString::fromStdWString(state.lastEditorId), QStringLiteral("peer-a"));

        ChatMessage mutationRow;
        QVERIFY(!repository.findMessageById(QStringLiteral("group-mut-recall-1"),
                                            &mutationRow));
    }

    void groupEditMutationFromServiceAppliesToExistingMessage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-group-edit");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-group-edit.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.upsertConversation(ConversationSummary{
            L"group:ops",
            L"Ops Room",
            L"old preview",
            900
        }));
        QVERIFY(repository.appendMessage(storedGroupTextMessage(
            QStringLiteral("group-msg-edit"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            QStringLiteral("old body"),
            1000)));

        FakeServerMessageClient client;
        client.page.messages.push_back(mutationRecord(
            QStringLiteral("group-mut-edit-1"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            14,
            buildEditPayloadJson(QStringLiteral("group-msg-edit"),
                                 QStringLiteral("<p>new body</p>"),
                                 QStringLiteral("html"),
                                 1600)));
        client.page.nextAfterSeq = 14;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(14));

        ChatMessage state;
        QVERIFY(repository.findMessageMutationStateById(QStringLiteral("group-msg-edit"),
                                                        &state));
        QCOMPARE(QString::fromStdWString(state.body), QStringLiteral("<p>new body</p>"));
        QCOMPARE(state.lastMutationAtMs, qint64(1600));
        QCOMPARE(QString::fromStdWString(state.lastEditorId), QStringLiteral("peer-a"));

        ChatMessage mutationRow;
        QVERIFY(!repository.findMessageById(QStringLiteral("group-mut-edit-1"),
                                            &mutationRow));
    }

    void reactionFromServiceAppliesToExistingMessage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-reaction");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-reaction.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(storedGroupTextMessage(
            QStringLiteral("group-msg-reaction"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            QStringLiteral("react to me"),
            1000)));

        FakeServerMessageClient client;
        client.page.messages.push_back(reactionRecord(
            QStringLiteral("group-reaction-1"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-b"),
            15,
            QStringLiteral("group-msg-reaction"),
            QStringLiteral(":ok:")));
        client.page.nextAfterSeq = 15;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(15));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("group-msg-reaction"),
                                           &stored));
        const QJsonObject reactions = QJsonDocument::fromJson(
            QString::fromStdWString(stored.reactionsJson).toUtf8()).object();
        QCOMPARE(reactions.value(QStringLiteral(":ok:")).toArray().first().toString(),
                 QStringLiteral("peer-b"));
    }

    void missingReactionTargetDoesNotBlockFollowingServiceMessages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-missing-reaction");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-missing-reaction.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(reactionRecord(
            QStringLiteral("group-reaction-missing"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-b"),
            19,
            QStringLiteral("missing-target"),
            QStringLiteral(":ok:")));
        client.page.messages.push_back(groupTextRecord(
            QStringLiteral("group-msg-after-reaction"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            20));
        client.page.nextAfterSeq = 20;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);
        QCOMPARE(result.skippedDuplicateCount, 1);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(20));
        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("group-msg-after-reaction"),
                                           &stored));
    }

    void missingMutationTargetDoesNotBlockFollowingServiceMessages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-missing-mutation");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-missing-mutation.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(mutationRecord(
            QStringLiteral("group-mut-missing"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            21,
            buildRecallPayloadJson(QStringLiteral("missing-target"), 2100)));
        client.page.messages.push_back(groupTextRecord(
            QStringLiteral("group-msg-after-mutation"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            22));
        client.page.nextAfterSeq = 22;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);
        QCOMPARE(result.skippedDuplicateCount, 1);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(22));
        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("group-msg-after-mutation"),
                                           &stored));
    }

    void pinAndUnpinFromServiceUpdatePinnedMessageState()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-pin");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-pin.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(storedGroupTextMessage(
            QStringLiteral("group-msg-pin"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            QStringLiteral("important body"),
            1000)));

        FakeServerMessageClient client;
        client.page.messages.push_back(pinRecord(
            QStringLiteral("group-pin-1"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-b"),
            16,
            QStringLiteral("group-msg-pin"),
            QStringLiteral("pin")));
        client.page.nextAfterSeq = 16;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        MessageSyncResult result = service.syncConversation(QStringLiteral("group:ops"));
        QVERIFY(result.success);

        std::vector<ConversationRepository::PinnedMessageInfo> pins =
            repository.loadPinnedMessages(QStringLiteral("group:ops"));
        QCOMPARE(pins.size(), std::size_t(1));
        QCOMPARE(pins.front().messageId, QStringLiteral("group-msg-pin"));
        QCOMPARE(pins.front().pinnerId, QStringLiteral("peer-b"));
        QCOMPARE(pins.front().pinnerName, QStringLiteral("Peer B"));

        const auto messagesAfterPin =
            repository.loadMessages(QStringLiteral("group:ops").toStdWString());
        QVERIFY(std::any_of(messagesAfterPin.cbegin(),
                            messagesAfterPin.cend(),
                            [](const ChatMessage& message) {
                                return QString::fromStdWString(message.messageType)
                                           == QStringLiteral("system")
                                    && QString::fromStdWString(message.body)
                                           .contains(QStringLiteral("置顶"));
                            }));

        client.page.messages.clear();
        client.page.messages.push_back(pinRecord(
            QStringLiteral("group-unpin-1"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-b"),
            17,
            QStringLiteral("group-msg-pin"),
            QStringLiteral("unpin")));
        client.page.nextAfterSeq = 17;

        result = service.syncConversation(QStringLiteral("group:ops"));
        QVERIFY(result.success);
        QVERIFY(repository.loadPinnedMessages(QStringLiteral("group:ops")).empty());
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(17));

        const auto messagesAfterUnpin =
            repository.loadMessages(QStringLiteral("group:ops").toStdWString());
        QVERIFY(std::any_of(messagesAfterUnpin.cbegin(),
                            messagesAfterUnpin.cend(),
                            [](const ChatMessage& message) {
                                return QString::fromStdWString(message.messageType)
                                           == QStringLiteral("system")
                                    && QString::fromStdWString(message.body)
                                           .contains(QStringLiteral("取消"));
                            }));
    }

    void pinAndUnpinEchoDoNotDuplicateExistingLocalSystemMessages()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-pin-echo");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-pin-echo.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.appendMessage(storedGroupTextMessage(
            QStringLiteral("group-msg-pin-echo"),
            QStringLiteral("group:ops"),
            QStringLiteral("peer-a"),
            QStringLiteral("important body"),
            1000)));
        QVERIFY(repository.appendMessage(localSystemMessage(
            QStringLiteral("system-pin-group-pin-echo"),
            QStringLiteral("group:ops"),
            QStringLiteral("local-a"),
            QStringLiteral("Local pin marker"),
            1100)));
        QVERIFY(repository.appendMessage(localSystemMessage(
            QStringLiteral("system-pin-group-unpin-echo"),
            QStringLiteral("group:ops"),
            QStringLiteral("local-a"),
            QStringLiteral("Local unpin marker"),
            1200)));
        QCOMPARE(systemMessageCount(&repository, QStringLiteral("group:ops")), 2);

        FakeServerMessageClient client;
        client.page.messages.push_back(pinRecord(
            QStringLiteral("group-pin-echo"),
            QStringLiteral("group:ops"),
            QStringLiteral("local-a"),
            23,
            QStringLiteral("group-msg-pin-echo"),
            QStringLiteral("pin")));
        client.page.messages.push_back(pinRecord(
            QStringLiteral("group-unpin-echo"),
            QStringLiteral("group:ops"),
            QStringLiteral("local-a"),
            24,
            QStringLiteral("group-msg-pin-echo"),
            QStringLiteral("unpin")));
        client.page.nextAfterSeq = 24;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("group:ops"));

        QVERIFY(result.success);
        QCOMPARE(systemMessageCount(&repository, QStringLiteral("group:ops")), 2);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("group:ops")),
                 qint64(24));
        QVERIFY(repository.loadPinnedMessages(QStringLiteral("group:ops")).empty());
    }

    void nudgeFromServiceKeepsNudgeMessageType()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-nudge");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-nudge.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(nudgeRecord(QStringLiteral("nudge-msg-1"),
                                                   QStringLiteral("peer-a"),
                                                   18));
        client.page.nextAfterSeq = 18;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("nudge-msg-1"), &stored));
        QCOMPARE(QString::fromStdWString(stored.messageType),
                 QStringLiteral("nudge"));
        QCOMPARE(QString::fromStdWString(stored.body), QStringLiteral("[nudge]"));
        QCOMPARE(result.newIncomingNotifications.size(), 1);
        QCOMPARE(result.newIncomingNotifications.front().messageType,
                 QStringLiteral("nudge"));
    }

    void forwardPackageFromServicePersistsRenderablePayload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-forward-package");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-forward-package.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.page.messages.push_back(forwardPackageRecord(
            QStringLiteral("forward-msg-1"),
            QStringLiteral("conv-service"),
            QStringLiteral("peer-a"),
            25));
        client.page.nextAfterSeq = 25;

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const MessageSyncResult result =
            service.syncConversation(QStringLiteral("conv-service"));

        QVERIFY(result.success);
        QCOMPARE(result.storedCount, 1);

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("forward-msg-1"),
                                           &stored));
        QCOMPARE(QString::fromStdWString(stored.messageType),
                 QStringLiteral("forward_package"));
        QCOMPARE(QString::fromStdWString(stored.body),
                 QStringLiteral("Merged forward"));
        const QJsonObject payload = QJsonDocument::fromJson(
            QString::fromStdWString(stored.payloadJson).toUtf8()).object();
        QCOMPARE(payload.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Forwarded messages"));
        QCOMPARE(payload.value(QStringLiteral("count")).toInt(), 2);
    }

    void flushPendingReadAcksPostsAndDeletesQueuedAcks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-read-ack");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-read-ack.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.enqueuePendingRemoteReadAck(
            QStringLiteral("srv-1"),
            QStringLiteral("conv-service"),
            42));
        QVERIFY(repository.enqueuePendingRemoteReadAck(
            QStringLiteral("srv-2"),
            QStringLiteral("conv-service"),
            43));

        FakeServerMessageClient client;
        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const PendingReadAckFlushResult result = service.flushPendingReadAcks();

        QVERIFY(result.success);
        QCOMPARE(result.attemptedCount, 2);
        QCOMPARE(result.acknowledgedCount, 2);
        QCOMPARE(client.readAckCalls.size(), 2);
        QCOMPARE(client.readAckCalls.at(0).serverMessageId, QStringLiteral("srv-1"));
        QCOMPARE(client.readAckCalls.at(0).readSeq, qint64(42));
        QCOMPARE(client.readAckCalls.at(1).serverMessageId, QStringLiteral("srv-2"));
        QCOMPARE(client.readAckCalls.at(1).readSeq, qint64(43));
        QVERIFY(repository.loadPendingRemoteReadAcks().empty());
    }

    void flushPendingReadAcksDiscardsMissingAndContinues()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName = QStringLiteral("message-sync-read-ack-404");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-read-ack-404.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.enqueuePendingRemoteReadAck(
            QStringLiteral("missing"), QStringLiteral("conv-service"), 41));
        QVERIFY(repository.enqueuePendingRemoteReadAck(
            QStringLiteral("srv-2"), QStringLiteral("conv-service"), 42));

        FakeServerMessageClient client;
        client.missingReadAckIds.insert(QStringLiteral("missing"));
        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const PendingReadAckFlushResult result = service.flushPendingReadAcks();

        QVERIFY(result.success);
        QCOMPARE(result.attemptedCount, 2);
        QCOMPARE(result.acknowledgedCount, 1);
        QCOMPARE(result.discardedTerminalCount, 1);
        QVERIFY(repository.loadPendingRemoteReadAcks().empty());
    }

    void flushPendingReadAcksKeepsQueueWhenServerRejects()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("message-sync-read-ack-reject");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-read-ack.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.enqueuePendingRemoteReadAck(
            QStringLiteral("srv-1"),
            QStringLiteral("conv-service"),
            42));

        FakeServerMessageClient client;
        client.readAckResult = false;
        client.readAckError = QStringLiteral("temporary failure");

        MessageSyncService service(QStringLiteral("local-a"), &repository, &client);
        const PendingReadAckFlushResult result = service.flushPendingReadAcks();

        QVERIFY(!result.success);
        QCOMPARE(result.attemptedCount, 1);
        QCOMPARE(result.acknowledgedCount, 0);
        QCOMPARE(client.readAckCalls.size(), 1);
        QCOMPARE(repository.loadPendingRemoteReadAcks().size(), size_t(1));
    }
};

QTEST_MAIN(TestMessageSyncService)
#include "TestMessageSyncService.moc"
