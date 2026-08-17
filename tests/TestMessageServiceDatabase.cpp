#include <QtTest>

#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>

#include "MessageServiceDatabase.h"
#include "services/MessageRoutingCapabilities.h"

namespace {
QString uniqueConn()
{
    return QStringLiteral("test-message-db-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool tableExists(const QString& connectionName, const QString& tableName)
{
    QSqlQuery query(QSqlDatabase::database(connectionName));
    query.prepare(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?"));
    query.addBindValue(tableName);
    return query.exec() && query.next();
}

int countRows(const QString& connectionName, const QString& tableName)
{
    QSqlQuery query(QSqlDatabase::database(connectionName));
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(tableName))
        || !query.next()) {
        return -1;
    }
    return query.value(0).toInt();
}

StoreMessageRequest makeRequest(const QString& clientMessageId,
                                const QString& senderId = QStringLiteral("client-1"),
                                const QString& conversationId = QStringLiteral("conv-1"))
{
    StoreMessageRequest request;
    request.clientMessageId = clientMessageId;
    request.conversationId = conversationId;
    request.workspaceId = QStringLiteral("ws-1");
    request.senderId = senderId;
    request.type = QStringLiteral("chat_text");
    request.body = QStringLiteral("hello %1").arg(clientMessageId);
    request.payloadJson = QStringLiteral("{\"format\":\"plain\"}");
    request.contentType = QStringLiteral("text/plain");
    request.recipientIds = {QStringLiteral("client-2"), QStringLiteral("client-3")};
    request.createdAtMs = 1000;
    return request;
}
}

class TestMessageServiceDatabase : public QObject {
    Q_OBJECT

private slots:
    void open_createsMessageTablesAndMigrationRow();
    void storeMessage_assignsConversationSeqAndDeliveryRows();
    void storeMessage_appendsDurableMessageCreatedEvent();
    void listMessageEventsAfter_filtersByWorkspaceAndOrdersByEventId();
    void listMessageEventsAfterForClient_filtersForeignConversations();
    void storeMessage_duplicateClientMessageIdReturnsOriginalAck();
    void storeMessage_duplicateMatchingPayloadAddsMissingRecipients();
    void listMessagesAfterSeq_ordersByServerSeqAndRespectsLimit();
    void markDeliveredAndRead_updatesDeliveryAndCursor();
    void markDelivered_appendsDurableDeliveredEvent();
    void markRead_appendsDurableReadEvent();
    void markDeliveredAndRead_retriesDoNotDuplicateDurableEvents();
    void workspaceAndAuditRecords_areStoredWithoutMessageBody();
    void conversationMembership_allowsSenderAndRecipientsOnly();
    void clientCapabilities_upsertAndQueryProfiles();
};

void TestMessageServiceDatabase::open_createsMessageTablesAndMigrationRow()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    QVERIFY(tableExists(conn, QStringLiteral("service_migrations")));
    QVERIFY(tableExists(conn, QStringLiteral("messages")));
    QVERIFY(tableExists(conn, QStringLiteral("message_deliveries")));
    QVERIFY(tableExists(conn, QStringLiteral("conversation_members")));
    QVERIFY(tableExists(conn, QStringLiteral("conversation_cursors")));
    QVERIFY(tableExists(conn, QStringLiteral("message_events")));
    QVERIFY(tableExists(conn, QStringLiteral("message_workspaces")));
    QVERIFY(tableExists(conn, QStringLiteral("message_audit_events")));
    QVERIFY(tableExists(conn, QStringLiteral("message_client_capabilities")));

    QSqlQuery migration(QSqlDatabase::database(conn));
    QVERIFY(migration.exec(QStringLiteral(
        "SELECT version FROM service_migrations WHERE module = 'message_service'")));
    QVERIFY(migration.next());
    QCOMPARE(migration.value(0).toInt(), 4);
}

void TestMessageServiceDatabase::storeMessage_assignsConversationSeqAndDeliveryRows()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    const auto first = db.storeMessage(makeRequest(QStringLiteral("local-1")));
    QVERIFY2(first.ok, qPrintable(first.error));
    QVERIFY(!first.duplicate);
    QCOMPARE(first.message.serverSeq, qint64(1));
    QCOMPARE(first.message.senderId, QStringLiteral("client-1"));
    QCOMPARE(first.message.body, QStringLiteral("hello local-1"));
    QCOMPARE(first.message.payloadJson, QStringLiteral("{\"format\":\"plain\"}"));

    const auto second = db.storeMessage(makeRequest(QStringLiteral("local-2")));
    QVERIFY2(second.ok, qPrintable(second.error));
    QCOMPARE(second.message.serverSeq, qint64(2));

    QCOMPARE(countRows(conn, QStringLiteral("messages")), 2);
    QCOMPARE(countRows(conn, QStringLiteral("message_deliveries")), 4);

    const auto deliveries = db.listDeliveries(first.message.serverMessageId);
    QCOMPARE(deliveries.size(), 2);
    QSet<QString> recipients;
    for (const auto& delivery : deliveries) {
        QCOMPARE(delivery.state, QStringLiteral("pending"));
        recipients.insert(delivery.recipientId);
    }
    QCOMPARE(recipients, QSet<QString>({QStringLiteral("client-2"),
                                        QStringLiteral("client-3")}));
}

void TestMessageServiceDatabase::storeMessage_appendsDurableMessageCreatedEvent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    const auto stored = db.storeMessage(makeRequest(QStringLiteral("local-1")));
    QVERIFY2(stored.ok, qPrintable(stored.error));

    const QVector<StoredMessageEvent> events =
        db.listMessageEventsAfter(QStringLiteral("ws-1"), 0, 10);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.front().eventId, qint64(1));
    QCOMPARE(events.front().workspaceId, QStringLiteral("ws-1"));
    QCOMPARE(events.front().conversationId, QStringLiteral("conv-1"));
    QCOMPARE(events.front().eventType, QStringLiteral("message.created"));
    QVERIFY(events.front().createdAtMs > 0);

    const QJsonObject payload =
        QJsonDocument::fromJson(events.front().payloadJson.toUtf8()).object();
    QCOMPARE(payload[QStringLiteral("eventId")].toInteger(), qint64(1));
    QCOMPARE(payload[QStringLiteral("type")].toString(),
             QStringLiteral("message.created"));
    QCOMPARE(payload[QStringLiteral("serverMessageId")].toString(),
             stored.message.serverMessageId);
    QCOMPARE(payload[QStringLiteral("conversationId")].toString(),
             QStringLiteral("conv-1"));
    QCOMPARE(payload[QStringLiteral("workspaceId")].toString(),
             QStringLiteral("ws-1"));
    QCOMPARE(payload[QStringLiteral("serverSeq")].toInteger(), qint64(1));
}

void TestMessageServiceDatabase::listMessageEventsAfter_filtersByWorkspaceAndOrdersByEventId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    QVERIFY(db.storeMessage(makeRequest(QStringLiteral("local-1"))).ok);
    QVERIFY(db.storeMessage(makeRequest(QStringLiteral("local-2"))).ok);
    auto other = makeRequest(QStringLiteral("local-other"),
                             QStringLiteral("client-4"),
                             QStringLiteral("conv-2"));
    other.workspaceId = QStringLiteral("ws-2");
    QVERIFY(db.storeMessage(other).ok);
    QVERIFY(db.storeMessage(makeRequest(QStringLiteral("local-3"))).ok);

    const QVector<StoredMessageEvent> ws1AfterFirst =
        db.listMessageEventsAfter(QStringLiteral("ws-1"), 1, 10);
    QCOMPARE(ws1AfterFirst.size(), 2);
    QCOMPARE(ws1AfterFirst.at(0).eventId, qint64(2));
    QCOMPARE(ws1AfterFirst.at(1).eventId, qint64(4));
    QCOMPARE(ws1AfterFirst.at(0).workspaceId, QStringLiteral("ws-1"));
    QCOMPARE(ws1AfterFirst.at(1).workspaceId, QStringLiteral("ws-1"));

    const QVector<StoredMessageEvent> ws2 =
        db.listMessageEventsAfter(QStringLiteral("ws-2"), 0, 10);
    QCOMPARE(ws2.size(), 1);
    QCOMPARE(ws2.front().eventId, qint64(3));

    const QVector<StoredMessageEvent> limited =
        db.listMessageEventsAfter(QStringLiteral("ws-1"), 0, 1);
    QCOMPARE(limited.size(), 1);
    QCOMPARE(limited.front().eventId, qint64(1));
}

void TestMessageServiceDatabase::listMessageEventsAfterForClient_filtersForeignConversations()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    auto visible = makeRequest(QStringLiteral("visible-1"),
                               QStringLiteral("client-1"),
                               QStringLiteral("conv-visible"));
    visible.recipientIds = {QStringLiteral("client-2")};
    QVERIFY(db.storeMessage(visible).ok);

    auto foreign = makeRequest(QStringLiteral("foreign-1"),
                               QStringLiteral("client-4"),
                               QStringLiteral("conv-foreign"));
    foreign.recipientIds = {QStringLiteral("client-5")};
    QVERIFY(db.storeMessage(foreign).ok);

    QVERIFY(db.appendSessionStatusEvent(QStringLiteral("ws-1"),
                                        QStringLiteral("session.online"),
                                        QStringLiteral("session-1"),
                                        QStringLiteral("client-4"),
                                        QStringLiteral("device-4"),
                                        100,
                                        200,
                                        2).eventId > 0);

    const QVector<StoredMessageEvent> client2Events =
        db.listMessageEventsAfterForClient(QStringLiteral("ws-1"),
                                           QStringLiteral("client-2"),
                                           0,
                                           10);
    QCOMPARE(client2Events.size(), 2);
    QCOMPARE(client2Events.at(0).conversationId, QStringLiteral("conv-visible"));
    QCOMPARE(client2Events.at(0).eventType, QStringLiteral("message.created"));
    QCOMPARE(client2Events.at(1).conversationId, QStringLiteral("__sessions__"));
    QCOMPARE(client2Events.at(1).eventType, QStringLiteral("session.online"));

    const QVector<StoredMessageEvent> unrelatedClientEvents =
        db.listMessageEventsAfterForClient(QStringLiteral("ws-1"),
                                           QStringLiteral("client-unrelated"),
                                           0,
                                           10);
    QCOMPARE(unrelatedClientEvents.size(), 1);
    QCOMPARE(unrelatedClientEvents.front().conversationId,
             QStringLiteral("__sessions__"));
}

void TestMessageServiceDatabase::storeMessage_duplicateClientMessageIdReturnsOriginalAck()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    auto request = makeRequest(QStringLiteral("local-1"));
    const auto first = db.storeMessage(request);
    QVERIFY2(first.ok, qPrintable(first.error));
    QCOMPARE(first.message.serverSeq, qint64(1));

    request.body = QStringLiteral("retry body should not overwrite");
    request.recipientIds = {QStringLiteral("client-4")};
    const auto duplicate = db.storeMessage(request);
    QVERIFY2(duplicate.ok, qPrintable(duplicate.error));
    QVERIFY(duplicate.duplicate);
    QCOMPARE(duplicate.message.serverMessageId, first.message.serverMessageId);
    QCOMPARE(duplicate.message.serverSeq, first.message.serverSeq);
    QCOMPARE(duplicate.message.body, QStringLiteral("hello local-1"));
    QCOMPARE(countRows(conn, QStringLiteral("messages")), 1);
    QCOMPARE(countRows(conn, QStringLiteral("message_deliveries")), 2);
    QCOMPARE(countRows(conn, QStringLiteral("message_events")), 1);
}

void TestMessageServiceDatabase::storeMessage_duplicateMatchingPayloadAddsMissingRecipients()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    auto request = makeRequest(QStringLiteral("mixed-upgrade-1"));
    request.recipientIds = {QStringLiteral("new-client-a")};
    const auto first = db.storeMessage(request);
    QVERIFY2(first.ok, qPrintable(first.error));

    request.recipientIds = {
        QStringLiteral("new-client-a"),
        QStringLiteral("upgraded-legacy-client")
    };
    const auto duplicate = db.storeMessage(request);

    QVERIFY2(duplicate.ok, qPrintable(duplicate.error));
    QVERIFY(duplicate.duplicate);
    QCOMPARE(duplicate.message.serverMessageId, first.message.serverMessageId);
    QCOMPARE(countRows(conn, QStringLiteral("messages")), 1);
    QCOMPARE(countRows(conn, QStringLiteral("message_deliveries")), 2);
    QCOMPARE(countRows(conn, QStringLiteral("message_events")), 2);

    const auto deliveries = db.listDeliveries(first.message.serverMessageId);
    QSet<QString> recipients;
    for (const auto& delivery : deliveries) {
        recipients.insert(delivery.recipientId);
    }
    QCOMPARE(recipients,
             QSet<QString>({QStringLiteral("new-client-a"),
                            QStringLiteral("upgraded-legacy-client")}));
}

void TestMessageServiceDatabase::listMessagesAfterSeq_ordersByServerSeqAndRespectsLimit()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    QVERIFY(db.storeMessage(makeRequest(QStringLiteral("local-1"))).ok);
    QVERIFY(db.storeMessage(makeRequest(QStringLiteral("local-2"))).ok);
    QVERIFY(db.storeMessage(makeRequest(QStringLiteral("local-3"))).ok);
    QVERIFY(db.storeMessage(makeRequest(QStringLiteral("other-1"),
                                        QStringLiteral("client-4"),
                                        QStringLiteral("conv-2"))).ok);

    const auto afterFirst = db.listMessagesAfterSeq(QStringLiteral("conv-1"), 1, 10);
    QCOMPARE(afterFirst.size(), 2);
    QCOMPARE(afterFirst.at(0).clientMessageId, QStringLiteral("local-2"));
    QCOMPARE(afterFirst.at(0).serverSeq, qint64(2));
    QCOMPARE(afterFirst.at(1).clientMessageId, QStringLiteral("local-3"));
    QCOMPARE(afterFirst.at(1).serverSeq, qint64(3));

    const auto limited = db.listMessagesAfterSeq(QStringLiteral("conv-1"), 0, 2);
    QCOMPARE(limited.size(), 2);
    QCOMPARE(limited.at(0).serverSeq, qint64(1));
    QCOMPARE(limited.at(1).serverSeq, qint64(2));
}

void TestMessageServiceDatabase::markDeliveredAndRead_updatesDeliveryAndCursor()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    const auto stored = db.storeMessage(makeRequest(QStringLiteral("local-1")));
    QVERIFY2(stored.ok, qPrintable(stored.error));

    QVERIFY(db.markDelivered(stored.message.serverMessageId,
                             QStringLiteral("client-2"),
                             stored.message.serverSeq));

    auto deliveries = db.listDeliveries(stored.message.serverMessageId);
    auto delivered = std::find_if(deliveries.cbegin(), deliveries.cend(), [](const auto& d) {
        return d.recipientId == QStringLiteral("client-2");
    });
    QVERIFY(delivered != deliveries.cend());
    QCOMPARE(delivered->state, QStringLiteral("delivered"));
    QVERIFY(delivered->deliveredAtMs > 0);

    QVERIFY(db.markRead(stored.message.serverMessageId,
                        QStringLiteral("client-2"),
                        stored.message.serverSeq));

    deliveries = db.listDeliveries(stored.message.serverMessageId);
    delivered = std::find_if(deliveries.cbegin(), deliveries.cend(), [](const auto& d) {
        return d.recipientId == QStringLiteral("client-2");
    });
    QVERIFY(delivered != deliveries.cend());
    QCOMPARE(delivered->state, QStringLiteral("read"));
    QVERIFY(delivered->readAtMs > 0);

    QSqlQuery cursor(QSqlDatabase::database(conn));
    QVERIFY(cursor.exec(QStringLiteral(
        "SELECT last_received_seq, last_read_seq FROM conversation_cursors "
        "WHERE conversation_id = 'conv-1' AND client_id = 'client-2'")));
    QVERIFY(cursor.next());
    QCOMPARE(cursor.value(0).toLongLong(), qint64(1));
    QCOMPARE(cursor.value(1).toLongLong(), qint64(1));

    QVERIFY(!db.markDelivered(stored.message.serverMessageId,
                              QStringLiteral("not-a-recipient"),
                              stored.message.serverSeq));
}

void TestMessageServiceDatabase::markDelivered_appendsDurableDeliveredEvent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    const auto stored = db.storeMessage(makeRequest(QStringLiteral("local-1")));
    QVERIFY2(stored.ok, qPrintable(stored.error));

    QVERIFY(db.markDelivered(stored.message.serverMessageId,
                             QStringLiteral("client-2"),
                             stored.message.serverSeq));

    const QVector<StoredMessageEvent> events =
        db.listMessageEventsAfter(QStringLiteral("ws-1"), 1, 10);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.front().eventId, qint64(2));
    QCOMPARE(events.front().workspaceId, QStringLiteral("ws-1"));
    QCOMPARE(events.front().conversationId, QStringLiteral("conv-1"));
    QCOMPARE(events.front().eventType, QStringLiteral("message.delivered"));

    const QJsonObject payload =
        QJsonDocument::fromJson(events.front().payloadJson.toUtf8()).object();
    QCOMPARE(payload[QStringLiteral("eventId")].toInteger(), qint64(2));
    QCOMPARE(payload[QStringLiteral("type")].toString(),
             QStringLiteral("message.delivered"));
    QCOMPARE(payload[QStringLiteral("serverMessageId")].toString(),
             stored.message.serverMessageId);
    QCOMPARE(payload[QStringLiteral("conversationId")].toString(),
             QStringLiteral("conv-1"));
    QCOMPARE(payload[QStringLiteral("workspaceId")].toString(),
             QStringLiteral("ws-1"));
    QCOMPARE(payload[QStringLiteral("recipientId")].toString(),
             QStringLiteral("client-2"));
    QCOMPARE(payload[QStringLiteral("serverSeq")].toInteger(), qint64(1));
    QCOMPARE(payload[QStringLiteral("receivedSeq")].toInteger(), qint64(1));
}

void TestMessageServiceDatabase::markRead_appendsDurableReadEvent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    const auto stored = db.storeMessage(makeRequest(QStringLiteral("local-1")));
    QVERIFY2(stored.ok, qPrintable(stored.error));

    QVERIFY(db.markRead(stored.message.serverMessageId,
                        QStringLiteral("client-2"),
                        stored.message.serverSeq));

    const QVector<StoredMessageEvent> events =
        db.listMessageEventsAfter(QStringLiteral("ws-1"), 1, 10);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.front().eventId, qint64(2));
    QCOMPARE(events.front().workspaceId, QStringLiteral("ws-1"));
    QCOMPARE(events.front().conversationId, QStringLiteral("conv-1"));
    QCOMPARE(events.front().eventType, QStringLiteral("message.read"));

    const QJsonObject payload =
        QJsonDocument::fromJson(events.front().payloadJson.toUtf8()).object();
    QCOMPARE(payload[QStringLiteral("eventId")].toInteger(), qint64(2));
    QCOMPARE(payload[QStringLiteral("type")].toString(),
             QStringLiteral("message.read"));
    QCOMPARE(payload[QStringLiteral("serverMessageId")].toString(),
             stored.message.serverMessageId);
    QCOMPARE(payload[QStringLiteral("conversationId")].toString(),
             QStringLiteral("conv-1"));
    QCOMPARE(payload[QStringLiteral("workspaceId")].toString(),
             QStringLiteral("ws-1"));
    QCOMPARE(payload[QStringLiteral("recipientId")].toString(),
             QStringLiteral("client-2"));
    QCOMPARE(payload[QStringLiteral("serverSeq")].toInteger(), qint64(1));
    QCOMPARE(payload[QStringLiteral("readSeq")].toInteger(), qint64(1));
}

void TestMessageServiceDatabase::markDeliveredAndRead_retriesDoNotDuplicateDurableEvents()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    const auto stored = db.storeMessage(makeRequest(QStringLiteral("local-1")));
    QVERIFY2(stored.ok, qPrintable(stored.error));

    QVERIFY(db.markDelivered(stored.message.serverMessageId,
                             QStringLiteral("client-2"),
                             stored.message.serverSeq));
    QVERIFY(db.markDelivered(stored.message.serverMessageId,
                             QStringLiteral("client-2"),
                             stored.message.serverSeq));
    QVERIFY(db.markRead(stored.message.serverMessageId,
                        QStringLiteral("client-2"),
                        stored.message.serverSeq));
    QVERIFY(db.markRead(stored.message.serverMessageId,
                        QStringLiteral("client-2"),
                        stored.message.serverSeq));

    const QVector<StoredMessageEvent> events =
        db.listMessageEventsAfter(QStringLiteral("ws-1"), 1, 10);
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0).eventType, QStringLiteral("message.delivered"));
    QCOMPARE(events.at(1).eventType, QStringLiteral("message.read"));
}

void TestMessageServiceDatabase::workspaceAndAuditRecords_areStoredWithoutMessageBody()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    MessageWorkspaceRecord workspace;
    workspace.workspaceId = QStringLiteral("ws-admin");
    workspace.displayName = QStringLiteral("Admin Workspace");
    workspace.createdById = QStringLiteral("admin-1");
    workspace.enabled = true;

    QVERIFY(db.upsertWorkspace(workspace));
    auto workspaces = db.listWorkspaces();
    QCOMPARE(workspaces.size(), 1);
    QCOMPARE(workspaces.front().workspaceId, QStringLiteral("ws-admin"));
    QCOMPARE(workspaces.front().displayName, QStringLiteral("Admin Workspace"));
    QVERIFY(workspaces.front().enabled);

    workspace.displayName = QStringLiteral("Renamed Workspace");
    workspace.enabled = false;
    QVERIFY(db.upsertWorkspace(workspace));
    workspaces = db.listWorkspaces();
    QCOMPARE(workspaces.size(), 1);
    QCOMPARE(workspaces.front().displayName, QStringLiteral("Renamed Workspace"));
    QVERIFY(!workspaces.front().enabled);

    QJsonObject metadata;
    metadata[QStringLiteral("workspaceId")] = QStringLiteral("ws-admin");
    metadata[QStringLiteral("body")] = QStringLiteral("secret message text");
    metadata[QStringLiteral("messageBody")] = QStringLiteral("secret message body");
    metadata[QStringLiteral("payloadJson")] = QStringLiteral("{\"body\":\"secret\"}");

    const auto audit = db.appendAuditEvent(QStringLiteral("ws-admin"),
                                           QStringLiteral("admin-1"),
                                           QStringLiteral("workspace.upsert"),
                                           QStringLiteral("success"),
                                           metadata);
    QVERIFY(audit.auditId > 0);

    const auto audits = db.listAuditEvents(QStringLiteral("ws-admin"), 10);
    QCOMPARE(audits.size(), 1);
    QCOMPARE(audits.front().action, QStringLiteral("workspace.upsert"));

    const auto sanitized = QJsonDocument::fromJson(
                               audits.front().metadataJson.toUtf8()).object();
    QCOMPARE(sanitized[QStringLiteral("workspaceId")].toString(),
             QStringLiteral("ws-admin"));
    QVERIFY(!sanitized.contains(QStringLiteral("body")));
    QVERIFY(!sanitized.contains(QStringLiteral("messageBody")));
    QVERIFY(!sanitized.contains(QStringLiteral("payloadJson")));
}

void TestMessageServiceDatabase::conversationMembership_allowsSenderAndRecipientsOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    auto request = makeRequest(QStringLiteral("local-1"));
    request.recipientIds = {
        QStringLiteral("client-2"),
        QStringLiteral("client-2"),
        QStringLiteral("client-3"),
        QStringLiteral("client-1"),
        QString()
    };
    const auto stored = db.storeMessage(request);
    QVERIFY2(stored.ok, qPrintable(stored.error));

    QVERIFY(db.isConversationMember(QStringLiteral("conv-1"), QStringLiteral("client-1")));
    QVERIFY(db.isConversationMember(QStringLiteral("conv-1"), QStringLiteral("client-2")));
    QVERIFY(db.isConversationMember(QStringLiteral("conv-1"), QStringLiteral("client-3")));
    QVERIFY(!db.isConversationMember(QStringLiteral("conv-1"), QStringLiteral("client-4")));

    const auto deliveries = db.listDeliveries(stored.message.serverMessageId);
    QCOMPARE(deliveries.size(), 2);
}

void TestMessageServiceDatabase::clientCapabilities_upsertAndQueryProfiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    QVERIFY(db.upsertClientCapabilities(
        QStringLiteral("ws-1"),
        QStringLiteral("client-a"),
        QStringLiteral("0.2.0"),
        QStringList{MessageRoutingCapabilities::serverReceiveV1()},
        1000));

    const QVector<MessageClientCapabilityProfile> profiles =
        db.loadClientCapabilities(QStringLiteral("ws-1"),
                                  QStringList{QStringLiteral("client-a"),
                                              QStringLiteral("legacy")});

    QCOMPARE(profiles.size(), 2);
    QCOMPARE(profiles.at(0).clientId, QStringLiteral("client-a"));
    QCOMPARE(profiles.at(0).appVersion, QStringLiteral("0.2.0"));
    QCOMPARE(profiles.at(0).updatedAtMs, qint64(1000));
    QVERIFY(profiles.at(0).supports(
        MessageRoutingCapabilities::serverReceiveV1()));

    QCOMPARE(profiles.at(1).clientId, QStringLiteral("legacy"));
    QCOMPARE(profiles.at(1).updatedAtMs, qint64(0));
    QVERIFY(!profiles.at(1).supports(
        MessageRoutingCapabilities::serverReceiveV1()));
}

QTEST_MAIN(TestMessageServiceDatabase)
#include "TestMessageServiceDatabase.moc"
