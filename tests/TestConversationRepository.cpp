#include <QtTest/QTest>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "domain/ChatMessage.h"
#include "domain/MessageEnvelope.h"
#include "network/MessageCodec.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

class TestConversationRepository : public QObject {
    Q_OBJECT

private slots:
    void noMessages_returnsEmptySet()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("unread-empty");
        DatabaseManager mgr(dir.filePath(QStringLiteral("empty.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.loadConversationsWithUnreadMessages(
                    QStringLiteral("local-user")).isEmpty());
    }

    void receivedMessageFromPeer_marksConversationUnread()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("unread-received");
        DatabaseManager mgr(dir.filePath(QStringLiteral("rcv.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-1", L"Alice", L"hi", 1000}));
        const ChatMessage msg{
            .messageId      = L"msg-1",
            .conversationId = L"conv-1",
            .senderId       = L"peer-a",
            .body           = L"hello",
            .createdAtMs    = 1000,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));

        const auto result = repo.loadConversationsWithUnreadMessages(
            QStringLiteral("local-user"));
        QCOMPARE(static_cast<int>(result.size()), 1);
        QVERIFY(result.contains(QStringLiteral("conv-1")));
    }

    void ownMessage_doesNotMarkUnread()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("unread-own");
        DatabaseManager mgr(dir.filePath(QStringLiteral("own.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-2", L"Bob", L"hey", 2000}));
        const ChatMessage msg{
            .messageId      = L"msg-2",
            .conversationId = L"conv-2",
            .senderId       = L"local-user",
            .body           = L"sent by me",
            .createdAtMs    = 2000,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));

        QVERIFY(repo.loadConversationsWithUnreadMessages(
                    QStringLiteral("local-user")).isEmpty());
    }

    void readMessage_doesNotMarkUnread()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("unread-read");
        DatabaseManager mgr(dir.filePath(QStringLiteral("read.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-3", L"Carol", L"ok", 3000}));
        const ChatMessage msg{
            .messageId      = L"msg-3",
            .conversationId = L"conv-3",
            .senderId       = L"peer-b",
            .body           = L"already read",
            .createdAtMs    = 3000,
            .deliveryState  = MessageDeliveryState::Read
        };
        QVERIFY(repo.appendMessage(msg));

        QVERIFY(repo.loadConversationsWithUnreadMessages(
                    QStringLiteral("local-user")).isEmpty());
    }

    void manuallyUnread_alwaysIncluded()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("unread-manual");
        DatabaseManager mgr(dir.filePath(QStringLiteral("manual.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-4", L"Dan", L"", 4000}));
        QVERIFY(repo.setConversationFlag(
            QStringLiteral("conv-4"), ConversationFlag::ManuallyUnread, true));

        const auto result = repo.loadConversationsWithUnreadMessages(
            QStringLiteral("local-user"));
        QCOMPARE(static_cast<int>(result.size()), 1);
        QVERIFY(result.contains(QStringLiteral("conv-4")));
    }

    void consumeConversationUnread_marksPeerMessagesReadAndClearsManualFlag()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("unread-consume");
        DatabaseManager mgr(dir.filePath(QStringLiteral("consume.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-5", L"Eve", L"", 5000}));
        QVERIFY(repo.setConversationFlag(
            QStringLiteral("conv-5"), ConversationFlag::ManuallyUnread, true));

        ChatMessage received;
        received.messageId = L"msg-5a";
        received.conversationId = L"conv-5";
        received.senderId = L"peer-a";
        received.body = L"hello";
        received.createdAtMs = 5001;
        received.deliveryState = MessageDeliveryState::Received;
        QVERIFY(repo.appendMessage(received));

        ChatMessage ownSent;
        ownSent.messageId = L"msg-5b";
        ownSent.conversationId = L"conv-5";
        ownSent.senderId = L"local-user";
        ownSent.body = L"reply";
        ownSent.createdAtMs = 5002;
        ownSent.deliveryState = MessageDeliveryState::Sent;
        QVERIFY(repo.appendMessage(ownSent));

        QVERIFY(repo.consumeConversationUnread(QStringLiteral("conv-5"),
                                              QStringLiteral("local-user"),
                                              false));

        QVERIFY(repo.loadConversationsWithUnreadMessages(
                    QStringLiteral("local-user")).isEmpty());

        ChatMessage storedReceived;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-5a"), &storedReceived));
        QCOMPARE(storedReceived.deliveryState, MessageDeliveryState::Read);

        ChatMessage storedOwnSent;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-5b"), &storedOwnSent));
        QCOMPARE(storedOwnSent.deliveryState, MessageDeliveryState::Sent);
    }

    void mixedConversations_correctSet()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("unread-mixed");
        DatabaseManager mgr(dir.filePath(QStringLiteral("mixed.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        // conv-a: received msg from peer -> unread
        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-a", L"Alice", L"hi", 1000}));
        const ChatMessage mA{
            .messageId      = L"ma",
            .conversationId = L"conv-a",
            .senderId       = L"peer-a",
            .body           = L"hi",
            .createdAtMs    = 1000,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(mA));

        // conv-b: only read messages -> not unread
        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-b", L"Bob", L"ok", 2000}));
        const ChatMessage mB{
            .messageId      = L"mb",
            .conversationId = L"conv-b",
            .senderId       = L"peer-b",
            .body           = L"ok",
            .createdAtMs    = 2000,
            .deliveryState  = MessageDeliveryState::Read
        };
        QVERIFY(repo.appendMessage(mB));

        // conv-c: manually unread, no messages -> unread
        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-c", L"Carol", L"", 3000}));
        QVERIFY(repo.setConversationFlag(
            QStringLiteral("conv-c"), ConversationFlag::ManuallyUnread, true));

        const auto result = repo.loadConversationsWithUnreadMessages(
            QStringLiteral("local-user"));
        QCOMPARE(static_cast<int>(result.size()), 2);
        QVERIFY(result.contains(QStringLiteral("conv-a")));
        QVERIFY(result.contains(QStringLiteral("conv-c")));
        QVERIFY(!result.contains(QStringLiteral("conv-b")));
    }

    void pendingAndSentMessages_doNotMarkUnread()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        DatabaseManager mgr(dir.filePath(QStringLiteral("not-received.db")),
                            QStringLiteral("unread-notrcv"));
        QVERIFY(mgr.open());
        ConversationRepository repo(QStringLiteral("unread-notrcv"));

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-x", L"Eve", L"hi", 1000}));

        // pending 消息来自对端 -> 不应标未读
        ChatMessage pending;
        pending.messageId = L"mpending"; pending.conversationId = L"conv-x";
        pending.senderId = L"peer-e"; pending.body = L"pending";
        pending.createdAtMs = 1000;
        pending.deliveryState = MessageDeliveryState::Pending;
        QVERIFY(repo.appendMessage(pending));

        // sent 消息来自对端 -> 不应标未读
        ChatMessage sent;
        sent.messageId = L"msent"; sent.conversationId = L"conv-x";
        sent.senderId = L"peer-e"; sent.body = L"sent";
        sent.createdAtMs = 2000;
        sent.deliveryState = MessageDeliveryState::Sent;
        QVERIFY(repo.appendMessage(sent));

        QVERIFY(repo.loadConversationsWithUnreadMessages(
                    QStringLiteral("local-user")).isEmpty());
    }

    void serverAckedMessage_roundTripsDeliveryState()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-server-acked-state");
        DatabaseManager mgr(dir.filePath(QStringLiteral("server-acked.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        ChatMessage message;
        message.messageId = L"msg-server-acked";
        message.conversationId = L"conv-service";
        message.senderId = L"local-user";
        message.body = L"sent through service";
        message.createdAtMs = 1000;
        message.deliveryState = MessageDeliveryState::ServerAcked;

        QVERIFY(repo.appendMessage(message));

        QSqlQuery raw(QSqlDatabase::database(conn, false));
        raw.prepare(QStringLiteral(
            "SELECT delivery_state FROM messages WHERE message_id = ?"));
        raw.addBindValue(QStringLiteral("msg-server-acked"));
        QVERIFY(raw.exec());
        QVERIFY(raw.next());
        QCOMPARE(raw.value(0).toString(), QStringLiteral("server_acked"));

        ChatMessage stored;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-server-acked"), &stored));
        QCOMPARE(stored.deliveryState, MessageDeliveryState::ServerAcked);
    }

    void remoteChatCursor_saveLoadAndClamp()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-chat-cursor");
        DatabaseManager mgr(dir.filePath(QStringLiteral("remote-cursor.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QCOMPARE(repo.loadRemoteChatCursor(QStringLiteral("conv-service")), qint64(0));
        QVERIFY(repo.saveRemoteChatCursor(QStringLiteral("conv-service"), 42));
        QCOMPARE(repo.loadRemoteChatCursor(QStringLiteral("conv-service")), qint64(42));

        QVERIFY(repo.saveRemoteChatCursor(QStringLiteral("conv-service"), -10));
        QCOMPARE(repo.loadRemoteChatCursor(QStringLiteral("conv-service")), qint64(0));
        QVERIFY(!repo.saveRemoteChatCursor(QString(), 1));
    }

    void remoteChatDeviceCursors_areIndependent()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-chat-device-cursor");
        DatabaseManager mgr(dir.filePath(QStringLiteral("remote-device-cursor.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QCOMPARE(repo.loadRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                 QStringLiteral("pc-a")),
                 qint64(0));
        QVERIFY(repo.saveRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                QStringLiteral("pc-a"),
                                                7));
        QCOMPARE(repo.loadRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                 QStringLiteral("pc-a")),
                 qint64(7));
        QCOMPARE(repo.loadRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                 QStringLiteral("pc-b")),
                 qint64(0));

        QVERIFY(repo.saveRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                QStringLiteral("pc-b"),
                                                9));
        QCOMPARE(repo.loadRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                 QStringLiteral("pc-a")),
                 qint64(7));
        QCOMPARE(repo.loadRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                 QStringLiteral("pc-b")),
                 qint64(9));

        QVERIFY(repo.saveRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                QStringLiteral("pc-a"),
                                                -3));
        QCOMPARE(repo.loadRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                 QStringLiteral("pc-a")),
                 qint64(0));
        QVERIFY(!repo.saveRemoteChatDeviceCursor(QString(), QStringLiteral("pc-a"), 1));
        QVERIFY(!repo.saveRemoteChatDeviceCursor(QStringLiteral("conv-service"),
                                                QString(),
                                                1));
    }

    void remoteMessageIdMapping_saveLoadAndRejectsEmpty()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-message-id-map");
        DatabaseManager mgr(dir.filePath(QStringLiteral("remote-id-map.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QCOMPARE(repo.loadLocalMessageIdForRemoteServerId(QStringLiteral("srv-1")),
                 QString());
        QVERIFY(repo.saveRemoteMessageIdMapping(QStringLiteral("srv-1"),
                                                QStringLiteral("local-1")));
        QCOMPARE(repo.loadLocalMessageIdForRemoteServerId(QStringLiteral("srv-1")),
                 QStringLiteral("local-1"));

        QVERIFY(repo.saveRemoteMessageIdMapping(QStringLiteral("srv-1"),
                                                QStringLiteral("local-1-retry")));
        QCOMPARE(repo.loadLocalMessageIdForRemoteServerId(QStringLiteral("srv-1")),
                 QStringLiteral("local-1-retry"));
        QVERIFY(!repo.saveRemoteMessageIdMapping(QString(), QStringLiteral("local-2")));
        QVERIFY(!repo.saveRemoteMessageIdMapping(QStringLiteral("srv-2"), QString()));
    }

    void pendingRemoteReadAck_queueDeduplicatesAndDeletes()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-pending-remote-read-ack");
        DatabaseManager mgr(dir.filePath(QStringLiteral("pending-read-ack.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(!repo.enqueuePendingRemoteReadAck(QString(), QStringLiteral("conv-a"), 1));
        QVERIFY(!repo.enqueuePendingRemoteReadAck(QStringLiteral("srv-1"), QString(), 1));

        QVERIFY(repo.enqueuePendingRemoteReadAck(QStringLiteral("srv-1"),
                                                QStringLiteral("conv-a"),
                                                12));
        QVERIFY(repo.enqueuePendingRemoteReadAck(QStringLiteral("srv-1"),
                                                QStringLiteral("conv-a"),
                                                18));
        QVERIFY(repo.enqueuePendingRemoteReadAck(QStringLiteral("srv-2"),
                                                QStringLiteral("conv-a"),
                                                -5));

        const auto pending = repo.loadPendingRemoteReadAcks(10);
        QCOMPARE(static_cast<int>(pending.size()), 2);
        QCOMPARE(pending.at(0).serverMessageId, QStringLiteral("srv-1"));
        QCOMPARE(pending.at(0).conversationId, QStringLiteral("conv-a"));
        QCOMPARE(pending.at(0).readSeq, qint64(18));
        QCOMPARE(pending.at(1).serverMessageId, QStringLiteral("srv-2"));
        QCOMPARE(pending.at(1).readSeq, qint64(0));

        QVERIFY(repo.deletePendingRemoteReadAck(QStringLiteral("srv-1")));
        const auto remaining = repo.loadPendingRemoteReadAcks(10);
        QCOMPARE(static_cast<int>(remaining.size()), 1);
        QCOMPARE(remaining.at(0).serverMessageId, QStringLiteral("srv-2"));
        QVERIFY(!repo.deletePendingRemoteReadAck(QString()));
    }

    void remoteSessionPresence_saveLoadAndListsOnlineClients()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-session-presence");
        DatabaseManager mgr(dir.filePath(QStringLiteral("remote-session.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        ConversationRepository::RemoteSessionPresence presence;
        presence.workspaceId = QStringLiteral("ws-main");
        presence.clientId = QStringLiteral("peer-a");
        presence.deviceId = QStringLiteral("pc-peer");
        presence.sessionId = QStringLiteral("sess-1");
        presence.online = true;
        presence.connectedAtMs = 1000;
        presence.lastSeenAtMs = 1200;
        presence.lastEventId = 7;

        QVERIFY(repo.saveRemoteSessionPresence(presence));
        const auto loaded = repo.loadRemoteSessionPresence(QStringLiteral("ws-main"),
                                                           QStringLiteral("peer-a"),
                                                           QStringLiteral("pc-peer"));
        QVERIFY(loaded.has_value());
        QCOMPARE(loaded->sessionId, QStringLiteral("sess-1"));
        QVERIFY(loaded->online);
        QCOMPARE(loaded->connectedAtMs, qint64(1000));
        QCOMPARE(loaded->lastSeenAtMs, qint64(1200));
        QCOMPARE(loaded->lastEventId, qint64(7));
        QVERIFY(repo.loadOnlineRemoteSessionClientIds(QStringLiteral("ws-main"))
                    .contains(QStringLiteral("peer-a")));

        presence.online = false;
        presence.lastSeenAtMs = 2000;
        presence.lastEventId = 8;
        QVERIFY(repo.saveRemoteSessionPresence(presence));
        const auto offline = repo.loadRemoteSessionPresence(QStringLiteral("ws-main"),
                                                            QStringLiteral("peer-a"),
                                                            QStringLiteral("pc-peer"));
        QVERIFY(offline.has_value());
        QVERIFY(!offline->online);
        QCOMPARE(offline->lastSeenAtMs, qint64(2000));
        QVERIFY(!repo.loadOnlineRemoteSessionClientIds(QStringLiteral("ws-main"))
                     .contains(QStringLiteral("peer-a")));

        presence.workspaceId.clear();
        QVERIFY(!repo.saveRemoteSessionPresence(presence));
    }

    void remoteSessionPresence_replaceWorkspaceSnapshotMarksMissingOffline()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-remote-session-snapshot");
        DatabaseManager mgr(dir.filePath(QStringLiteral("remote-session-snapshot.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        ConversationRepository::RemoteSessionPresence stale;
        stale.workspaceId = QStringLiteral("ws-main");
        stale.clientId = QStringLiteral("peer-stale");
        stale.deviceId = QStringLiteral("pc-stale");
        stale.sessionId = QStringLiteral("sess-stale");
        stale.online = true;
        stale.connectedAtMs = 900;
        stale.lastSeenAtMs = 1000;
        QVERIFY(repo.saveRemoteSessionPresence(stale));

        ConversationRepository::RemoteSessionPresence active;
        active.workspaceId = QStringLiteral("ws-main");
        active.clientId = QStringLiteral("peer-active");
        active.deviceId = QStringLiteral("pc-active");
        active.sessionId = QStringLiteral("sess-active");
        active.online = true;
        active.connectedAtMs = 1100;
        active.lastSeenAtMs = 1200;
        active.lastEventId = 7;

        QVERIFY(repo.replaceRemoteSessionPresenceForWorkspace(
            QStringLiteral("ws-main"), QVector{active}));

        const QSet<QString> online =
            repo.loadOnlineRemoteSessionClientIds(QStringLiteral("ws-main"));
        QVERIFY(online.contains(QStringLiteral("peer-active")));
        QVERIFY(!online.contains(QStringLiteral("peer-stale")));

        const auto staleLoaded =
            repo.loadRemoteSessionPresence(QStringLiteral("ws-main"),
                                           QStringLiteral("peer-stale"),
                                           QStringLiteral("pc-stale"));
        QVERIFY(staleLoaded.has_value());
        QVERIFY(!staleLoaded->online);
    }

    void loadResourceRefMessages_returnsOnlyResourceRefType()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("load-resource-ref");
        DatabaseManager mgr(dir.filePath(QStringLiteral("resref.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-1", L"Group A", L"hi", 1000}));

        const ChatMessage textMsg{
            .messageId      = L"msg-text",
            .conversationId = L"conv-1",
            .senderId       = L"user-a",
            .body           = L"hello",
            .createdAtMs    = 1000,
            .deliveryState  = MessageDeliveryState::Received,
            .messageType    = L"text"
        };
        QVERIFY(repo.appendMessage(textMsg));

        const ChatMessage refMsg{
            .messageId      = L"msg-ref",
            .conversationId = L"conv-1",
            .senderId       = L"user-a",
            .body           = L"[共享资源] design.pdf",
            .createdAtMs    = 2000,
            .deliveryState  = MessageDeliveryState::Received,
            .messageType    = L"resource_ref",
            .payloadJson    = L"{\"service_id\":\"remote-file-service\",\"workspace_id\":\"group-a\","
                              L"\"resource_id\":\"file-001\",\"kind\":\"shared_file\","
                              L"\"title\":\"design.pdf\",\"snapshot_version\":\"v1\"}"
        };
        QVERIFY(repo.appendMessage(refMsg));

        const auto results = repo.loadResourceRefMessages(L"conv-1");
        QCOMPARE(static_cast<int>(results.size()), 1);
        QCOMPARE(results.front().messageId, L"msg-ref");
        QCOMPARE(results.front().messageType, L"resource_ref");
        QCOMPARE(results.front().payloadJson, refMsg.payloadJson);
    }

    void loadLatestMessageIdForConversation_prefersEffectiveSortTimestamp()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("latest-message-id");
        DatabaseManager mgr(dir.filePath(QStringLiteral("latest-message.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-latest", L"Perf", L"", 0}));

        ChatMessage sent;
        sent.messageId = L"msg-sent";
        sent.conversationId = L"conv-latest";
        sent.senderId = L"local-user";
        sent.body = L"local";
        sent.createdAtMs = 2000;
        sent.deliveryState = MessageDeliveryState::Sent;
        QVERIFY(repo.appendMessage(sent, 0));

        ChatMessage receivedOlder;
        receivedOlder.messageId = L"msg-recv-old";
        receivedOlder.conversationId = L"conv-latest";
        receivedOlder.senderId = L"peer-a";
        receivedOlder.body = L"older";
        receivedOlder.createdAtMs = 1000;
        receivedOlder.deliveryState = MessageDeliveryState::Received;
        QVERIFY(repo.appendMessage(receivedOlder, 1500));

        ChatMessage receivedLatest;
        receivedLatest.messageId = L"msg-recv-new";
        receivedLatest.conversationId = L"conv-latest";
        receivedLatest.senderId = L"peer-b";
        receivedLatest.body = L"newest";
        receivedLatest.createdAtMs = 1200;
        receivedLatest.deliveryState = MessageDeliveryState::Received;
        QVERIFY(repo.appendMessage(receivedLatest, 2600));

        QCOMPARE(repo.loadLatestMessageIdForConversation(QStringLiteral("conv-latest")),
                 QStringLiteral("msg-recv-new"));
    }

    void applyMessageRecall_setsIsRecalled()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-recall-basic");
        DatabaseManager mgr(dir.filePath(QStringLiteral("recall-basic.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-r1", L"Alice", L"hi", 1000}));

        const ChatMessage msg{
            .messageId      = L"msg-r1",
            .conversationId = L"conv-r1",
            .senderId       = L"peer-a",
            .body           = L"original body",
            .createdAtMs    = 1000000000000LL,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));

        const qint64 recalledAt = 1000000001000LL;
        QVERIFY(repo.applyMessageRecall(QStringLiteral("msg-r1"),
                                        QStringLiteral("peer-a"),
                                        recalledAt));

        ChatMessage state;
        QVERIFY(repo.findMessageMutationStateById(QStringLiteral("msg-r1"), &state));
        QVERIFY(state.isRecalled);
        QCOMPARE(state.recalledAtMs, recalledAt);
    }

    void applyMessageEdit_updatesBody()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-edit-basic");
        DatabaseManager mgr(dir.filePath(QStringLiteral("edit-basic.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-e1", L"Bob", L"hello", 2000}));

        const ChatMessage msg{
            .messageId      = L"msg-e1",
            .conversationId = L"conv-e1",
            .senderId       = L"peer-b",
            .body           = L"original",
            .createdAtMs    = 1000000000000LL,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));

        const qint64 editedAt = 1000000001000LL;
        QVERIFY(repo.applyMessageEdit(QStringLiteral("msg-e1"),
                                      QStringLiteral("peer-b"),
                                      editedAt,
                                      QStringLiteral("edited body")));

        ChatMessage updated;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-e1"), &updated));
        QCOMPARE(QString::fromStdWString(updated.body), QStringLiteral("edited body"));
        QCOMPARE(updated.editedAtMs, editedAt);
    }

    void applyMessageEdit_ignoredIfRecalled()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-edit-recalled");
        DatabaseManager mgr(dir.filePath(QStringLiteral("edit-recalled.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-er", L"Carol", L"hi", 3000}));

        const ChatMessage msg{
            .messageId      = L"msg-er",
            .conversationId = L"conv-er",
            .senderId       = L"peer-c",
            .body           = L"original body",
            .createdAtMs    = 1000000000000LL,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));
        QVERIFY(repo.applyMessageRecall(QStringLiteral("msg-er"),
                                        QStringLiteral("peer-c"),
                                        1000000001000LL));

        const bool editResult = repo.applyMessageEdit(
            QStringLiteral("msg-er"),
            QStringLiteral("peer-c"),
            1000000002000LL,
            QStringLiteral("should not be set"));
        QVERIFY(!editResult);
    }

    void applyMessageRecall_ignoredIfStale()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-recall-stale");
        DatabaseManager mgr(dir.filePath(QStringLiteral("recall-stale.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-rs", L"Dan", L"hey", 4000}));

        const ChatMessage msg{
            .messageId      = L"msg-rs",
            .conversationId = L"conv-rs",
            .senderId       = L"peer-d",
            .body           = L"test",
            .createdAtMs    = 1000000000000LL,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));
        QVERIFY(repo.applyMessageRecall(QStringLiteral("msg-rs"),
                                        QStringLiteral("peer-d"),
                                        1000000002000LL));

        const bool staleResult = repo.applyMessageRecall(
            QStringLiteral("msg-rs"),
            QStringLiteral("peer-d"),
            1000000001000LL);
        QVERIFY(!staleResult);
    }

    void applyMessageEdit_ignoredIfStale()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-edit-stale");
        DatabaseManager mgr(dir.filePath(QStringLiteral("edit-stale.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-es", L"Eve", L"test", 5000}));

        const ChatMessage msg{
            .messageId      = L"msg-es",
            .conversationId = L"conv-es",
            .senderId       = L"peer-e",
            .body           = L"original",
            .createdAtMs    = 1000000000000LL,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));
        QVERIFY(repo.applyMessageEdit(QStringLiteral("msg-es"),
                                      QStringLiteral("peer-e"),
                                      1000000002000LL,
                                      QStringLiteral("first edit")));

        const bool staleResult = repo.applyMessageEdit(
            QStringLiteral("msg-es"),
            QStringLiteral("peer-e"),
            1000000001000LL,
            QStringLiteral("second edit stale"));
        QVERIFY(!staleResult);
    }

    void refreshConversationPreview_usesRecalledPlaceholder()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-preview-recalled");
        DatabaseManager mgr(dir.filePath(QStringLiteral("preview-recalled.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-pr", L"Frank", L"original preview", 6000}));

        const ChatMessage msg{
            .messageId      = L"msg-pr",
            .conversationId = L"conv-pr",
            .senderId       = L"peer-f",
            .body           = L"latest body",
            .createdAtMs    = 1000000000000LL,
            .deliveryState  = MessageDeliveryState::Received
        };
        QVERIFY(repo.appendMessage(msg));
        QVERIFY(repo.applyMessageRecall(QStringLiteral("msg-pr"),
                                        QStringLiteral("peer-f"),
                                        1000000001000LL));

        QVERIFY(repo.refreshConversationPreviewFromLatestVisibleMessage(
            QStringLiteral("conv-pr")));

        const auto summaries = repo.loadConversationSummaries();
        QCOMPARE(static_cast<int>(summaries.size()), 1);
        QCOMPARE(QString::fromStdWString(summaries.front().lastMessagePreview),
                 QStringLiteral("消息已撤回"));
    }
    void deletePendingGroupEnvelopeForTargetMessage_removesOnlyMatchingTargetMessage()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("repo-pending-group-ack");
        DatabaseManager mgr(dir.filePath(QStringLiteral("pending-group-ack.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        auto encodeGroupEnvelope = [](const char* messageId, const char* targetId) {
            MessageEnvelope envelope;
            envelope.messageId = messageId;
            envelope.type = MessageType::GroupMessage;
            envelope.senderId = "local-a";
            envelope.targetId = targetId;
            envelope.conversationId = "group-001";
            envelope.body = R"({"group_id":"group-001","message_kind":"text","text":"hello"})";
            envelope.createdAtMs = 1712800000000LL;
            return QByteArray::fromStdString(MessageCodec::encode(envelope));
        };

        QVERIFY(repo.enqueuePendingGroupEnvelope(QStringLiteral("peer-a"),
                                                 QStringLiteral("group-001"),
                                                 encodeGroupEnvelope("group-msg-001", "peer-a"),
                                                 1712800000000LL));
        QVERIFY(repo.enqueuePendingGroupEnvelope(QStringLiteral("peer-a"),
                                                 QStringLiteral("group-001"),
                                                 encodeGroupEnvelope("group-msg-002", "peer-a"),
                                                 1712800000001LL));
        QVERIFY(repo.enqueuePendingGroupEnvelope(QStringLiteral("peer-b"),
                                                 QStringLiteral("group-001"),
                                                 encodeGroupEnvelope("group-msg-001", "peer-b"),
                                                 1712800000002LL));

        QVERIFY(repo.deletePendingGroupEnvelopeForTargetMessage(QStringLiteral("peer-a"),
                                                                QStringLiteral("group-msg-001")));

        const auto peerA = repo.loadPendingGroupEnvelopes(QStringLiteral("peer-a"), 10);
        QCOMPARE(peerA.size(), std::size_t(1));
        const auto remaining =
            MessageCodec::decode(std::string_view(peerA.front().envelopeBlob.constData(),
                                                  static_cast<std::size_t>(peerA.front().envelopeBlob.size())));
        QVERIFY(remaining.has_value());
        QCOMPARE(QString::fromStdString(remaining->messageId), QStringLiteral("group-msg-002"));

        const auto peerB = repo.loadPendingGroupEnvelopes(QStringLiteral("peer-b"), 10);
        QCOMPARE(peerB.size(), std::size_t(1));
    }

    void pendingGroupEnvelopePaginationDoesNotStrandRowsAfterFirstBatch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString conn = QStringLiteral("pending-group-pagination");
        DatabaseManager mgr(dir.filePath(QStringLiteral("pending-group-pagination.db")), conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        for (int index = 0; index < 205; ++index) {
            MessageEnvelope envelope;
            envelope.messageId = QStringLiteral("group-msg-%1").arg(index).toStdString();
            envelope.type = MessageType::GroupMessage;
            envelope.senderId = "local-a";
            envelope.targetId = "legacy-peer";
            envelope.conversationId = "group-a";
            envelope.body = R"({"message_kind":"text","text":"hello"})";
            envelope.createdAtMs = 1000 + index;
            QVERIFY(repo.enqueuePendingGroupEnvelope(
                QStringLiteral("legacy-peer"),
                QStringLiteral("group-a"),
                QByteArray::fromStdString(MessageCodec::encode(envelope)),
                envelope.createdAtMs));
        }

        const auto firstPage = repo.loadPendingGroupEnvelopesAfterId(
            QStringLiteral("legacy-peer"), 0, 200);
        QCOMPARE(firstPage.size(), static_cast<std::size_t>(200));
        const auto secondPage = repo.loadPendingGroupEnvelopesAfterId(
            QStringLiteral("legacy-peer"), firstPage.back().id, 200);
        QCOMPARE(secondPage.size(), static_cast<std::size_t>(5));
        QVERIFY(secondPage.front().id > firstPage.back().id);

        const auto allPending = repo.loadAllPendingGroupEnvelopes();
        QCOMPARE(allPending.size(), static_cast<std::size_t>(205));
    }
};

QTEST_MAIN(TestConversationRepository)
#include "TestConversationRepository.moc"
