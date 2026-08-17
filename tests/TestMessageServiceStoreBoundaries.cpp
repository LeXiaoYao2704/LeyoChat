#include <QtTest>

#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>

#include "fileservice/MessageServiceDatabase.h"
#include "fileservice/MessageServiceStoreBoundaries.h"

namespace {
QString uniqueConn()
{
    return QStringLiteral("test-store-boundaries-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

class TestMessageServiceStoreBoundaries : public QObject {
    Q_OBJECT

private slots:
    void sqliteMessageEventStore_appendsAndListsDurableEvents();
    void sessionPresenceStore_tracksTouchesAndExpiry();
    void rateLimitStore_keepsSingleNodeBehaviorBehindInterface();
};

void TestMessageServiceStoreBoundaries::
    sqliteMessageEventStore_appendsAndListsDurableEvents()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    MessageServiceDatabase db(dir.filePath(QStringLiteral("messages.db")), conn);
    QVERIFY(db.open());

    SqliteMessageEventStore store(&db);
    StoredMessage message;
    message.serverMessageId = QStringLiteral("srv-1");
    message.clientMessageId = QStringLiteral("local-1");
    message.conversationId = QStringLiteral("conv-1");
    message.workspaceId = QStringLiteral("ws-1");
    message.senderId = QStringLiteral("client-1");
    message.serverSeq = 1;
    message.type = QStringLiteral("chat_text");
    message.body = QStringLiteral("hello");
    message.createdAtMs = 1000;

    const StoredMessageEvent created = store.appendMessageCreatedEvent(message);
    QVERIFY(created.eventId > 0);
    QCOMPARE(created.eventType, QStringLiteral("message.created"));

    const StoredMessageEvent online =
        store.appendSessionStatusEvent(QStringLiteral("ws-1"),
                                       QStringLiteral("session.online"),
                                       QStringLiteral("session-1"),
                                       QStringLiteral("client-1"),
                                       QStringLiteral("device-1"),
                                       1000,
                                       1200,
                                       created.eventId);
    QVERIFY(online.eventId > created.eventId);

    const QVector<StoredMessageEvent> events =
        store.listMessageEventsAfter(QStringLiteral("ws-1"), 0, 10);
    QCOMPARE(events.size(), 2);
    QCOMPARE(events.at(0).eventType, QStringLiteral("message.created"));
    QCOMPARE(events.at(1).eventType, QStringLiteral("session.online"));
}

void TestMessageServiceStoreBoundaries::
    sessionPresenceStore_tracksTouchesAndExpiry()
{
    InMemorySessionPresenceStore store(1000);

    const MessageSessionTouchResult first =
        store.touchSession(QStringLiteral("client-1"),
                           QStringLiteral("device-1"),
                           QStringLiteral("ws-1"),
                           9,
                           1000);
    QVERIFY(first.created);
    QCOMPARE(first.session.lastEventId, qint64(9));

    const MessageSessionTouchResult refreshed =
        store.touchSession(QStringLiteral("client-1"),
                           QStringLiteral("device-1"),
                           QStringLiteral("ws-1"),
                           11,
                           1200);
    QVERIFY(!refreshed.created);
    QCOMPARE(refreshed.session.sessionId, first.session.sessionId);
    QCOMPARE(refreshed.session.lastEventId, qint64(11));

    const QVector<MessageSessionSnapshot> expired =
        store.takeExpiredSessions(2201);
    QCOMPARE(expired.size(), 1);
    QCOMPARE(expired.front().sessionId, first.session.sessionId);
}

void TestMessageServiceStoreBoundaries::
    rateLimitStore_keepsSingleNodeBehaviorBehindInterface()
{
    InMemoryRateLimitStore store;
    store.setRateLimit(1, 60000);

    const MessageServiceRateLimitDecision first =
        store.accept(QStringLiteral("client-1"),
                     MessageServiceOperation::PostMessage);
    QVERIFY(first.allowed);

    const MessageServiceRateLimitDecision second =
        store.accept(QStringLiteral("client-1"),
                     MessageServiceOperation::PostMessage);
    QVERIFY(!second.allowed);
    QVERIFY(second.retryAfterMs > 0);

    const QJsonObject metrics = store.metricsJson();
    QCOMPARE(metrics[QStringLiteral("counters")]
                 .toObject()[QStringLiteral("accepted")]
                 .toObject()[QStringLiteral("post_message")]
                 .toInt(),
             1);
    QCOMPARE(metrics[QStringLiteral("counters")]
                 .toObject()[QStringLiteral("rejected")]
                 .toObject()[QStringLiteral("post_message")]
                 .toInt(),
             1);
}

QTEST_MAIN(TestMessageServiceStoreBoundaries)
#include "TestMessageServiceStoreBoundaries.moc"
