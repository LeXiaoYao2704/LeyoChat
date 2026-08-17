#include <QtTest/QTest>

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include <optional>

#include "integrations/ServerMessageClient.h"
#include "services/MessageRoutingCapabilities.h"
#include "services/RemoteMessageEventConsumer.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

namespace {

class FakeServerMessageClient final : public IServerMessageClient {
public:
    struct EventCall {
        QString workspaceId;
        QString deviceId;
        qint64 afterEventId = -1;
        int limit = 0;
    };

    struct ListCall {
        QString conversationId;
        qint64 afterSeq = -1;
        int limit = 0;
    };

    struct HeartbeatCall {
        QString workspaceId;
        QString deviceId;
        qint64 lastEventId = -1;
        QString appVersion;
        QStringList capabilities;
    };

    struct DeliveryAckCall {
        QString serverMessageId;
        qint64 receivedSeq = -1;
    };

    struct ReadAckCall {
        QString serverMessageId;
        qint64 readSeq = -1;
    };

    ServerMessageEventPage eventPage;
    QVector<ServerMessageSessionSnapshot> onlineSessions;
    QString eventError;
    QHash<QString, ServerMessagePage> pagesByConversation;
    QSet<QString> failingConversations;
    bool failReadAck = false;
    mutable QVector<EventCall> eventCalls;
    mutable QVector<ListCall> listCalls;
    mutable QVector<DeliveryAckCall> deliveryAckCalls;
    mutable QVector<ReadAckCall> readAckCalls;
    mutable QVector<HeartbeatCall> heartbeatCalls;
    mutable QVector<QString> onlineSessionCalls;

    std::optional<ServerMessageAck> sendMessage(
        const ServerMessageDraft&,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("send not used by event consumer tests");
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
        if (failingConversations.contains(conversationId)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("sync failed for %1").arg(conversationId);
            }
            return std::nullopt;
        }
        return pagesByConversation.value(conversationId);
    }

    bool acknowledgeDelivered(const QString& serverMessageId,
                              qint64 receivedSeq,
                              QString* errorMessage = nullptr) const override
    {
        Q_UNUSED(errorMessage);
        deliveryAckCalls.push_back({serverMessageId, receivedSeq});
        return true;
    }

    bool acknowledgeRead(const QString& serverMessageId,
                         qint64 readSeq,
                         QString* errorMessage = nullptr) const override
    {
        readAckCalls.push_back({serverMessageId, readSeq});
        if (failReadAck) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("read ack failed");
            }
            return false;
        }
        Q_UNUSED(errorMessage);
        return true;
    }

    std::optional<ServerMessageEventPage> listEvents(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 afterEventId,
        int limit,
        QString* errorMessage = nullptr) const override
    {
        eventCalls.push_back({workspaceId, deviceId, afterEventId, limit});
        if (!eventError.isEmpty()) {
            if (errorMessage) {
                *errorMessage = eventError;
            }
            return std::nullopt;
        }
        return eventPage;
    }

    std::optional<ServerMessageSessionAck> sendSessionHeartbeat(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 lastEventId,
        QString* errorMessage = nullptr) const override
    {
        Q_UNUSED(errorMessage);
        heartbeatCalls.push_back({workspaceId, deviceId, lastEventId});
        ServerMessageSessionAck ack;
        ack.ok = true;
        ack.sessionId = QStringLiteral("sess-1");
        ack.clientId = QStringLiteral("local-a");
        ack.deviceId = deviceId;
        ack.workspaceId = workspaceId;
        ack.lastEventId = lastEventId;
        return ack;
    }

    std::optional<ServerMessageSessionAck> sendSessionHeartbeat(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 lastEventId,
        const QString& appVersion,
        const QStringList& capabilities,
        QString* errorMessage = nullptr) const override
    {
        Q_UNUSED(errorMessage);
        heartbeatCalls.push_back({workspaceId,
                                  deviceId,
                                  lastEventId,
                                  appVersion,
                                  capabilities});
        ServerMessageSessionAck ack;
        ack.ok = true;
        ack.sessionId = QStringLiteral("sess-1");
        ack.clientId = QStringLiteral("local-a");
        ack.deviceId = deviceId;
        ack.workspaceId = workspaceId;
        ack.lastEventId = lastEventId;
        return ack;
    }

    std::optional<QVector<ServerMessageSessionSnapshot>> listOnlineSessions(
        const QString& workspaceId,
        QString* errorMessage = nullptr) const override
    {
        Q_UNUSED(errorMessage);
        onlineSessionCalls.push_back(workspaceId);
        return onlineSessions;
    }
};

ServerMessageEvent messageCreatedEvent(qint64 eventId, const QString& conversationId)
{
    ServerMessageEvent event;
    event.eventId = eventId;
    event.type = QStringLiteral("message.created");
    event.workspaceId = QStringLiteral("ws-main");
    event.conversationId = conversationId;
    event.data = QJsonObject{
        {QStringLiteral("eventId"), eventId},
        {QStringLiteral("type"), QStringLiteral("message.created")},
        {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
        {QStringLiteral("conversationId"), conversationId},
        {QStringLiteral("serverMessageId"), QStringLiteral("srv-%1").arg(eventId)}
    };
    return event;
}

ServerMessageEvent messageStateEvent(qint64 eventId,
                                     const QString& eventType,
                                     const QString& conversationId,
                                     const QString& serverMessageId,
                                     const QString& recipientId,
                                     qint64 stateSeq)
{
    ServerMessageEvent event;
    event.eventId = eventId;
    event.type = eventType;
    event.workspaceId = QStringLiteral("ws-main");
    event.conversationId = conversationId;
    event.data = QJsonObject{
        {QStringLiteral("eventId"), eventId},
        {QStringLiteral("type"), eventType},
        {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
        {QStringLiteral("conversationId"), conversationId},
        {QStringLiteral("serverMessageId"), serverMessageId},
        {QStringLiteral("recipientId"), recipientId},
        {QStringLiteral("serverSeq"), stateSeq},
        {QStringLiteral("createdAtMs"), 5000 + eventId}
    };
    if (eventType == QStringLiteral("message.delivered")) {
        event.data[QStringLiteral("receivedSeq")] = stateSeq;
    } else if (eventType == QStringLiteral("message.read")) {
        event.data[QStringLiteral("readSeq")] = stateSeq;
    }
    return event;
}

ServerMessageEvent sessionStateEvent(qint64 eventId,
                                     const QString& eventType,
                                     const QString& clientId,
                                     const QString& deviceId,
                                     const QString& sessionId,
                                     qint64 lastSeenAtMs)
{
    ServerMessageEvent event;
    event.eventId = eventId;
    event.type = eventType;
    event.workspaceId = QStringLiteral("ws-main");
    event.conversationId = QStringLiteral("__sessions__");
    event.data = QJsonObject{
        {QStringLiteral("eventId"), eventId},
        {QStringLiteral("type"), eventType},
        {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
        {QStringLiteral("conversationId"), QStringLiteral("__sessions__")},
        {QStringLiteral("sessionId"), sessionId},
        {QStringLiteral("clientId"), clientId},
        {QStringLiteral("deviceId"), deviceId},
        {QStringLiteral("connectedAtMs"), qint64(1000)},
        {QStringLiteral("lastSeenAtMs"), lastSeenAtMs},
        {QStringLiteral("lastEventId"), eventId - 1}
    };
    return event;
}

ServerMessageRecord textRecord(const QString& conversationId,
                               const QString& clientMessageId,
                               const QString& senderId,
                               qint64 serverSeq)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("%1-srv-%2")
                                 .arg(conversationId)
                                 .arg(serverSeq);
    record.clientMessageId = clientMessageId;
    record.conversationId = conversationId;
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("chat_text");
    record.body = QStringLiteral("hello %1").arg(serverSeq);
    record.payload = QJsonObject{{QStringLiteral("source"),
                                  QStringLiteral("event-consumer-test")}};
    record.contentType = QStringLiteral("html");
    record.createdAtMs = 1000 + serverSeq;
    return record;
}

ServerMessagePage pageWithRecord(const ServerMessageRecord& record)
{
    ServerMessagePage page;
    page.messages.push_back(record);
    page.nextAfterSeq = record.serverSeq;
    return page;
}

}  // namespace

class TestRemoteMessageEventConsumer : public QObject {
    Q_OBJECT

private slots:
    void consumeOnce_fetchesEventsAndSyncsConversations()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-basic");
        DatabaseManager manager(dir.filePath(QStringLiteral("basic.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        FakeServerMessageClient client;
        client.eventPage.events.push_back(
            messageCreatedEvent(5, QStringLiteral("conv-service")));
        client.eventPage.nextAfterEventId = 5;
        client.pagesByConversation.insert(
            QStringLiteral("conv-service"),
            pageWithRecord(textRecord(QStringLiteral("conv-service"),
                                      QStringLiteral("peer-msg-5"),
                                      QStringLiteral("peer-a"),
                                      5)));

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client,
                                            50,
                                            25);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 1);
        QCOMPARE(result.conversationsTriggered, 1);
        QCOMPARE(result.conversationsSynced, 1);
        QCOMPARE(result.conversationsFailed, 0);
        QCOMPARE(result.previousEventId, qint64(0));
        QCOMPARE(result.nextEventId, qint64(5));
        QCOMPARE(result.triggeredConversationIds,
                 QStringList({QStringLiteral("conv-service")}));
        QCOMPARE(result.newIncomingConversationIds,
                 QStringList({QStringLiteral("conv-service")}));
        QCOMPARE(result.newIncomingNotifications.size(), 1);
        QCOMPARE(result.newIncomingNotifications.front().conversationId,
                 QStringLiteral("conv-service"));

        QCOMPARE(client.eventCalls.size(), 1);
        QCOMPARE(client.eventCalls.front().afterEventId, qint64(0));
        QCOMPARE(client.eventCalls.front().limit, 50);
        QCOMPARE(client.listCalls.size(), 1);
        QCOMPARE(client.listCalls.front().conversationId,
                 QStringLiteral("conv-service"));
        QCOMPARE(client.listCalls.front().limit, 25);
        QCOMPARE(client.heartbeatCalls.size(), 1);
        QCOMPARE(client.heartbeatCalls.front().lastEventId, qint64(5));

        QCOMPARE(repository.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                         QStringLiteral("pc-a")),
                 qint64(5));
        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("peer-msg-5"), &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
        QCOMPARE(repository.loadLocalMessageIdForRemoteServerId(
                     QStringLiteral("conv-service-srv-5")),
                 QStringLiteral("peer-msg-5"));
    }

    void consumeOnce_deduplicatesConversationIds()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-dedupe");
        DatabaseManager manager(dir.filePath(QStringLiteral("dedupe.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        FakeServerMessageClient client;
        client.eventPage.events.push_back(
            messageCreatedEvent(7, QStringLiteral("conv-service")));
        client.eventPage.events.push_back(
            messageCreatedEvent(8, QStringLiteral("conv-service")));
        client.eventPage.nextAfterEventId = 8;
        client.pagesByConversation.insert(
            QStringLiteral("conv-service"),
            pageWithRecord(textRecord(QStringLiteral("conv-service"),
                                      QStringLiteral("peer-msg-8"),
                                      QStringLiteral("peer-a"),
                                      8)));

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 2);
        QCOMPARE(result.conversationsTriggered, 1);
        QCOMPARE(result.conversationsSynced, 1);
        QCOMPARE(client.listCalls.size(), 1);
        QCOMPARE(result.nextEventId, qint64(8));
        QCOMPARE(repository.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                         QStringLiteral("pc-a")),
                 qint64(8));
    }

    void consumeOnce_appliesDeliveredEventToMappedLocalMessage()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-delivered");
        DatabaseManager manager(dir.filePath(QStringLiteral("delivered.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        ChatMessage local;
        local.messageId = L"local-msg-1";
        local.conversationId = L"conv-service";
        local.senderId = L"local-a";
        local.body = L"outgoing";
        local.createdAtMs = 1000;
        local.deliveryState = MessageDeliveryState::ServerAcked;
        QVERIFY(repository.appendMessage(local));
        QVERIFY(repository.saveRemoteMessageIdMapping(QStringLiteral("srv-1"),
                                                      QStringLiteral("local-msg-1")));

        FakeServerMessageClient client;
        client.eventPage.events.push_back(
            messageStateEvent(21,
                              QStringLiteral("message.delivered"),
                              QStringLiteral("conv-service"),
                              QStringLiteral("srv-1"),
                              QStringLiteral("peer-a"),
                              1));
        client.eventPage.nextAfterEventId = 21;

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 1);
        QCOMPARE(result.conversationsTriggered, 0);
        QCOMPARE(result.nextEventId, qint64(21));
        QCOMPARE(client.listCalls.size(), 0);
        QCOMPARE(client.heartbeatCalls.size(), 1);
        QCOMPARE(client.heartbeatCalls.front().lastEventId, qint64(21));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("local-msg-1"), &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Received);
    }

    void consumeOnce_appliesReadEventToMappedLocalMessage()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-read");
        DatabaseManager manager(dir.filePath(QStringLiteral("read.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        ChatMessage local;
        local.messageId = L"local-msg-2";
        local.conversationId = L"conv-service";
        local.senderId = L"local-a";
        local.body = L"outgoing";
        local.createdAtMs = 1000;
        local.deliveryState = MessageDeliveryState::Received;
        QVERIFY(repository.appendMessage(local));
        QVERIFY(repository.saveRemoteMessageIdMapping(QStringLiteral("srv-2"),
                                                      QStringLiteral("local-msg-2")));

        FakeServerMessageClient client;
        client.eventPage.events.push_back(
            messageStateEvent(22,
                              QStringLiteral("message.read"),
                              QStringLiteral("conv-service"),
                              QStringLiteral("srv-2"),
                              QStringLiteral("peer-a"),
                              1));
        client.eventPage.nextAfterEventId = 22;

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 1);
        QCOMPARE(result.conversationsTriggered, 0);
        QCOMPARE(result.nextEventId, qint64(22));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("local-msg-2"), &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Read);

        const auto receipts =
            repository.loadReadReceiptsForMessage(QStringLiteral("local-msg-2"));
        QCOMPARE(receipts.size(), 1);
        QCOMPARE(receipts.front().first, QStringLiteral("peer-a"));
        QCOMPARE(receipts.front().second, qint64(5022));
    }

    void consumeOnce_recordsGroupReadEventWithoutPromotingWholeMessage()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-group-read");
        DatabaseManager manager(dir.filePath(QStringLiteral("group-read.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);
        QSqlQuery groupQuery(QSqlDatabase::database(conn, false));
        QVERIFY(groupQuery.exec(QStringLiteral(
            "INSERT INTO groups "
            "(group_id, group_name, owner_client_id, version, created_at_ms, updated_at_ms, is_active) "
            "VALUES ('group-service', 'group-read-test', 'local-a', 1, 5000, 5000, 1)")));

        ChatMessage local;
        local.messageId = L"local-group-msg-1";
        local.conversationId = L"group-service";
        local.senderId = L"local-a";
        local.body = L"group outgoing";
        local.createdAtMs = 1000;
        local.deliveryState = MessageDeliveryState::Sent;
        QVERIFY(repository.appendMessage(local));
        QVERIFY(repository.saveRemoteMessageIdMapping(QStringLiteral("srv-group-1"),
                                                      QStringLiteral("local-group-msg-1")));

        FakeServerMessageClient client;
        client.eventPage.events.push_back(
            messageStateEvent(23,
                              QStringLiteral("message.read"),
                              QStringLiteral("group-service"),
                              QStringLiteral("srv-group-1"),
                              QStringLiteral("peer-a"),
                              1));
        client.eventPage.nextAfterEventId = 23;

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 1);
        QCOMPARE(result.nextEventId, qint64(23));

        ChatMessage stored;
        QVERIFY(repository.findMessageById(QStringLiteral("local-group-msg-1"), &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::Sent);

        const auto receipts =
            repository.loadReadReceiptsForMessage(QStringLiteral("local-group-msg-1"));
        QCOMPARE(receipts.size(), 1);
        QCOMPARE(receipts.front().first, QStringLiteral("peer-a"));
        QCOMPARE(receipts.front().second, qint64(5023));
    }

    void consumeOnce_appliesSessionOnlineAndOfflineEvents()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-session");
        DatabaseManager manager(dir.filePath(QStringLiteral("session.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        FakeServerMessageClient client;
        client.eventPage.events.push_back(
            sessionStateEvent(31,
                              QStringLiteral("session.online"),
                              QStringLiteral("peer-a"),
                              QStringLiteral("pc-peer"),
                              QStringLiteral("sess-1"),
                              3100));
        client.eventPage.events.push_back(
            sessionStateEvent(32,
                              QStringLiteral("session.offline"),
                              QStringLiteral("peer-a"),
                              QStringLiteral("pc-peer"),
                              QStringLiteral("sess-1"),
                              3200));
        client.eventPage.nextAfterEventId = 32;

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 2);
        QCOMPARE(result.conversationsTriggered, 0);
        QCOMPARE(result.conversationsSynced, 0);
        QCOMPARE(result.nextEventId, qint64(32));
        QCOMPARE(client.listCalls.size(), 0);
        QCOMPARE(client.heartbeatCalls.size(), 1);
        QCOMPARE(client.heartbeatCalls.front().lastEventId, qint64(32));

        const auto presence =
            repository.loadRemoteSessionPresence(QStringLiteral("ws-main"),
                                                 QStringLiteral("peer-a"),
                                                 QStringLiteral("pc-peer"));
        QVERIFY(presence.has_value());
        QCOMPARE(presence->sessionId, QStringLiteral("sess-1"));
        QVERIFY(!presence->online);
        QCOMPARE(presence->lastSeenAtMs, qint64(3200));
        QCOMPARE(presence->lastEventId, qint64(31));
        QVERIFY(!repository.loadOnlineRemoteSessionClientIds(QStringLiteral("ws-main"))
                     .contains(QStringLiteral("peer-a")));
    }

    void consumeOnce_syncsOnlineSessionSnapshot()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-session-snapshot");
        DatabaseManager manager(dir.filePath(QStringLiteral("session-snapshot.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        ConversationRepository::RemoteSessionPresence stale;
        stale.workspaceId = QStringLiteral("ws-main");
        stale.clientId = QStringLiteral("peer-stale");
        stale.deviceId = QStringLiteral("pc-stale");
        stale.sessionId = QStringLiteral("sess-stale");
        stale.online = true;
        QVERIFY(repository.saveRemoteSessionPresence(stale));

        ServerMessageSessionSnapshot online;
        online.sessionId = QStringLiteral("sess-live");
        online.clientId = QStringLiteral("peer-live");
        online.deviceId = QStringLiteral("pc-live");
        online.workspaceId = QStringLiteral("ws-main");
        online.connectedAtMs = 1000;
        online.lastSeenAtMs = 1200;
        online.lastEventId = 8;

        FakeServerMessageClient client;
        client.eventPage.nextAfterEventId = 8;
        client.onlineSessions.push_back(online);

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 0);
        QCOMPARE(result.sessionsSynced, 1);
        QCOMPARE(client.onlineSessionCalls,
                 QVector<QString>{QStringLiteral("ws-main")});

        const QSet<QString> onlineClients =
            repository.loadOnlineRemoteSessionClientIds(QStringLiteral("ws-main"));
        QVERIFY(onlineClients.contains(QStringLiteral("peer-live")));
        QVERIFY(!onlineClients.contains(QStringLiteral("peer-stale")));
        const auto presence =
            repository.loadRemoteSessionPresence(QStringLiteral("ws-main"),
                                                 QStringLiteral("peer-live"),
                                                 QStringLiteral("pc-live"));
        QVERIFY(presence.has_value());
        QVERIFY(presence->online);
        QCOMPARE(presence->sessionId, QStringLiteral("sess-live"));
        QCOMPARE(presence->lastEventId, qint64(8));
    }

    void consumeOnce_advancesCursorAndHeartbeatsWhenOneConversationFails()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-sync-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-fail.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);
        QVERIFY(repository.saveRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                        QStringLiteral("pc-a"),
                                                        4));

        FakeServerMessageClient client;
        client.eventPage.events.push_back(
            messageCreatedEvent(5, QStringLiteral("conv-failing")));
        client.eventPage.events.push_back(
            messageCreatedEvent(6, QStringLiteral("conv-working")));
        client.eventPage.nextAfterEventId = 6;
        client.failingConversations.insert(QStringLiteral("conv-failing"));
        client.pagesByConversation.insert(
            QStringLiteral("conv-working"),
            pageWithRecord(textRecord(QStringLiteral("conv-working"),
                                      QStringLiteral("msg-working"),
                                      QStringLiteral("peer-working"),
                                      1)));

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(!result.success);
        QCOMPARE(result.eventsSeen, 2);
        QCOMPARE(result.conversationsTriggered, 2);
        QCOMPARE(result.conversationsSynced, 1);
        QCOMPARE(result.conversationsFailed, 1);
        QCOMPARE(result.previousEventId, qint64(4));
        QCOMPARE(result.nextEventId, qint64(6));
        QCOMPARE(repository.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                         QStringLiteral("pc-a")),
                 qint64(6));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("conv-working")),
                 qint64(1));
        QCOMPARE(client.heartbeatCalls.size(), 1);
        QCOMPARE(client.heartbeatCalls.front().lastEventId, qint64(6));
        QVERIFY(result.errorMessage.contains(QStringLiteral("sync failed")));
    }

    void consumeOnce_heartbeatsWhenNoEvents()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-empty");
        DatabaseManager manager(dir.filePath(QStringLiteral("empty.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);
        QVERIFY(repository.saveRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                        QStringLiteral("pc-a"),
                                                        12));

        FakeServerMessageClient client;
        client.eventPage.nextAfterEventId = 12;

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.eventsSeen, 0);
        QCOMPARE(result.conversationsTriggered, 0);
        QCOMPARE(result.conversationsSynced, 0);
        QCOMPARE(result.previousEventId, qint64(12));
        QCOMPARE(result.nextEventId, qint64(12));
        QCOMPARE(client.listCalls.size(), 0);
        QCOMPARE(client.heartbeatCalls.size(), 1);
        QCOMPARE(client.heartbeatCalls.front().lastEventId, qint64(12));
    }

    void consumeOnce_flushesPendingReadAcksOnSuccessfulPoll()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-read-ack-flush");
        DatabaseManager manager(dir.filePath(QStringLiteral("read-ack-flush.db")),
                                conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);
        QVERIFY(repository.enqueuePendingRemoteReadAck(
            QStringLiteral("srv-read-1"),
            QStringLiteral("conv-service"),
            42));

        FakeServerMessageClient client;
        client.eventPage.nextAfterEventId = 0;

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(result.pendingReadAcksAttempted, 1);
        QCOMPARE(result.pendingReadAcksAcknowledged, 1);
        QCOMPARE(client.readAckCalls.size(), 1);
        QCOMPARE(client.readAckCalls.front().serverMessageId,
                 QStringLiteral("srv-read-1"));
        QCOMPARE(client.readAckCalls.front().readSeq, qint64(42));
        QVERIFY(repository.loadPendingRemoteReadAcks().empty());
    }

    void consumeOnce_sendsCapabilityHeartbeat()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-capability-heartbeat");
        DatabaseManager manager(dir.filePath(QStringLiteral("capability.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);

        FakeServerMessageClient client;
        client.eventPage.nextAfterEventId = 0;

        RemoteMessageEventConsumer consumer(
            QStringLiteral("local-a"),
            QStringLiteral("ws-main"),
            QStringLiteral("pc-a"),
            &repository,
            &client,
            100,
            100,
            QStringLiteral("0.2.0"),
            QStringList{MessageRoutingCapabilities::serverReceiveV1()});
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(result.success);
        QCOMPARE(client.heartbeatCalls.size(), 1);
        QCOMPARE(client.heartbeatCalls.front().appVersion, QStringLiteral("0.2.0"));
        QVERIFY(client.heartbeatCalls.front().capabilities.contains(
            MessageRoutingCapabilities::serverReceiveV1()));
    }

    void consumeOnce_preservesFallbackWhenEventFetchFails()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("remote-event-consume-fetch-fail");
        DatabaseManager manager(dir.filePath(QStringLiteral("fetch-fail.db")), conn);
        QVERIFY(manager.open());
        ConversationRepository repository(conn);
        QVERIFY(repository.saveRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                        QStringLiteral("pc-a"),
                                                        20));

        FakeServerMessageClient client;
        client.eventError = QStringLiteral("event stream unavailable");

        RemoteMessageEventConsumer consumer(QStringLiteral("local-a"),
                                            QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            &repository,
                                            &client);
        const RemoteMessageEventConsumerResult result = consumer.consumeOnce();

        QVERIFY(!result.success);
        QCOMPARE(result.previousEventId, qint64(20));
        QCOMPARE(result.nextEventId, qint64(20));
        QCOMPARE(client.listCalls.size(), 0);
        QCOMPARE(client.heartbeatCalls.size(), 0);
        QCOMPARE(repository.loadRemoteMessageEventCursor(QStringLiteral("ws-main"),
                                                         QStringLiteral("pc-a")),
                 qint64(20));
        QVERIFY(result.errorMessage.contains(QStringLiteral("event stream unavailable")));
    }
};

QTEST_MAIN(TestRemoteMessageEventConsumer)
#include "TestRemoteMessageEventConsumer.moc"
