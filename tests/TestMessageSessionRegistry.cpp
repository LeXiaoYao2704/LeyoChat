#include <QtTest/QTest>

#include <QJsonObject>
#include <QVector>

#include "fileservice/MessageSessionRegistry.h"
#include "services/MessageRoutingCapabilities.h"

class TestMessageSessionRegistry : public QObject {
    Q_OBJECT

private slots:
    void touchCreatesAndRefreshesSession()
    {
        MessageSessionRegistry registry(1000);

        const MessageSessionSnapshot first =
            registry.touch(QStringLiteral("client-a"),
                           QStringLiteral("pc-a"),
                           QStringLiteral("ws-1"),
                           7,
                           1000);
        QVERIFY(!first.sessionId.isEmpty());
        QCOMPARE(first.clientId, QStringLiteral("client-a"));
        QCOMPARE(first.deviceId, QStringLiteral("pc-a"));
        QCOMPARE(first.workspaceId, QStringLiteral("ws-1"));
        QCOMPARE(first.connectedAtMs, qint64(1000));
        QCOMPARE(first.lastSeenAtMs, qint64(1000));
        QCOMPARE(first.lastEventId, qint64(7));

        const MessageSessionSnapshot refreshed =
            registry.touch(QStringLiteral("client-a"),
                           QStringLiteral("pc-a"),
                           QStringLiteral("ws-1"),
                           9,
                           1200);
        QCOMPARE(refreshed.sessionId, first.sessionId);
        QCOMPARE(refreshed.connectedAtMs, qint64(1000));
        QCOMPARE(refreshed.lastSeenAtMs, qint64(1200));
        QCOMPARE(refreshed.lastEventId, qint64(9));
    }

    void touchWithStatusReportsNewSessionOnlyOnce()
    {
        MessageSessionRegistry registry(1000);

        const MessageSessionTouchResult first =
            registry.touchWithStatus(QStringLiteral("client-a"),
                                     QStringLiteral("pc-a"),
                                     QStringLiteral("ws-1"),
                                     7,
                                     1000);
        QVERIFY(first.created);
        QVERIFY(!first.session.sessionId.isEmpty());
        QCOMPARE(first.session.lastEventId, qint64(7));

        const MessageSessionTouchResult refreshed =
            registry.touchWithStatus(QStringLiteral("client-a"),
                                     QStringLiteral("pc-a"),
                                     QStringLiteral("ws-1"),
                                     9,
                                     1200);
        QVERIFY(!refreshed.created);
        QCOMPARE(refreshed.session.sessionId, first.session.sessionId);
        QCOMPARE(refreshed.session.lastEventId, qint64(9));
    }

    void touchWithStatusStoresCapabilities()
    {
        MessageSessionRegistry registry(1000);

        const MessageSessionTouchResult result =
            registry.touchWithStatus(QStringLiteral("client-a"),
                                     QStringLiteral("pc-a"),
                                     QStringLiteral("ws-1"),
                                     7,
                                     QStringLiteral("0.2.0"),
                                     QStringList{
                                         MessageRoutingCapabilities::serverReceiveV1()
                                     },
                                     1000);

        QCOMPARE(result.session.appVersion, QStringLiteral("0.2.0"));
        QVERIFY(result.session.capabilities.contains(
            MessageRoutingCapabilities::serverReceiveV1()));
    }

    void cleanupExpiredSessionsRemovesStaleEntries()
    {
        MessageSessionRegistry registry(1000);

        registry.touch(QStringLiteral("client-a"),
                       QStringLiteral("pc-a"),
                       QStringLiteral("ws-1"),
                       0,
                       1000);
        registry.touch(QStringLiteral("client-b"),
                       QStringLiteral("pc-b"),
                       QStringLiteral("ws-1"),
                       0,
                       2500);

        QCOMPARE(registry.cleanupExpired(2601), 1);

        const QJsonObject metrics = registry.metricsJson(2601);
        QCOMPARE(metrics[QStringLiteral("onlineSessions")].toInt(), 1);
        QCOMPARE(metrics[QStringLiteral("onlineClients")].toInt(), 1);
        QCOMPARE(metrics[QStringLiteral("onlineDevices")].toInt(), 1);
        QCOMPARE(metrics[QStringLiteral("expiredSessions")].toInt(), 1);
    }

    void takeExpiredSessionsReturnsRemovedSnapshots()
    {
        MessageSessionRegistry registry(1000);

        const MessageSessionSnapshot stale =
            registry.touch(QStringLiteral("client-a"),
                           QStringLiteral("pc-a"),
                           QStringLiteral("ws-1"),
                           4,
                           1000);
        registry.touch(QStringLiteral("client-b"),
                       QStringLiteral("pc-b"),
                       QStringLiteral("ws-1"),
                       5,
                       2500);

        const QVector<MessageSessionSnapshot> expired =
            registry.takeExpiredSessions(2601);
        QCOMPARE(expired.size(), 1);
        QCOMPARE(expired.front().sessionId, stale.sessionId);
        QCOMPARE(expired.front().clientId, QStringLiteral("client-a"));
        QCOMPARE(expired.front().deviceId, QStringLiteral("pc-a"));
        QCOMPARE(expired.front().workspaceId, QStringLiteral("ws-1"));
        QCOMPARE(expired.front().lastEventId, qint64(4));

        const QJsonObject metrics = registry.metricsJson(2601);
        QCOMPARE(metrics[QStringLiteral("onlineSessions")].toInt(), 1);
        QCOMPARE(metrics[QStringLiteral("expiredSessions")].toInt(), 1);
    }

    void onlineSessionsReturnsOnlyActiveWorkspaceSessions()
    {
        MessageSessionRegistry registry(1000);

        registry.touch(QStringLiteral("client-a"),
                       QStringLiteral("pc-a"),
                       QStringLiteral("ws-1"),
                       4,
                       1000);
        const MessageSessionSnapshot active =
            registry.touch(QStringLiteral("client-b"),
                           QStringLiteral("pc-b"),
                           QStringLiteral("ws-1"),
                           5,
                           2500);
        registry.touch(QStringLiteral("client-c"),
                       QStringLiteral("pc-c"),
                       QStringLiteral("ws-2"),
                       6,
                       2500);

        const QVector<MessageSessionSnapshot> sessions =
            registry.onlineSessions(QStringLiteral("ws-1"), 2600);
        QCOMPARE(sessions.size(), 1);
        QCOMPARE(sessions.front().sessionId, active.sessionId);
        QCOMPARE(sessions.front().clientId, QStringLiteral("client-b"));
        QCOMPARE(sessions.front().deviceId, QStringLiteral("pc-b"));
        QCOMPARE(sessions.front().workspaceId, QStringLiteral("ws-1"));
    }

    void metricsCountsClientsDevicesAndExpiredSessions()
    {
        MessageSessionRegistry registry(1000);

        registry.touch(QStringLiteral("client-a"),
                       QStringLiteral("pc-a"),
                       QStringLiteral("ws-1"),
                       1,
                       1000);
        registry.touch(QStringLiteral("client-a"),
                       QStringLiteral("pc-b"),
                       QStringLiteral("ws-1"),
                       2,
                       1100);
        registry.touch(QStringLiteral("client-b"),
                       QStringLiteral("pc-c"),
                       QStringLiteral("ws-1"),
                       3,
                       1200);

        const QJsonObject metrics = registry.metricsJson(1300);
        QCOMPARE(metrics[QStringLiteral("onlineSessions")].toInt(), 3);
        QCOMPARE(metrics[QStringLiteral("onlineClients")].toInt(), 2);
        QCOMPARE(metrics[QStringLiteral("onlineDevices")].toInt(), 3);
        QCOMPARE(metrics[QStringLiteral("expiredSessions")].toInt(), 0);
    }
};

QTEST_MAIN(TestMessageSessionRegistry)
#include "TestMessageSessionRegistry.moc"
