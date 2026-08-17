#include <QtTest/QTest>

#include <QDateTime>

#include <utility>

#include "app/RemotePresenceUiAdapter.h"

namespace {

ConversationSummary summary(const QString& conversationId)
{
    ConversationSummary s;
    s.conversationId = conversationId.toStdWString();
    s.title = conversationId.toStdWString();
    return s;
}

PeerEndpoint peer(const QString& clientId, bool connected)
{
    PeerEndpoint p;
    p.clientId = clientId.toStdString();
    p.displayName = clientId.toStdString();
    p.host = "192.0.2.10";
    p.port = 45454;
    p.isConnected = connected;
    p.presence = connected ? PeerPresenceStatus::Online
                           : PeerPresenceStatus::Offline;
    p.lastPresenceAtMs = connected ? 100 : 0;
    return p;
}

}  // namespace

class TestRemotePresenceUiAdapter : public QObject {
    Q_OBJECT

private slots:
    void serviceOnlineClientsMarkDirectConversationsOnline()
    {
        const std::vector<ConversationSummary> conversations = {
            summary(QStringLiteral("local-a|peer-a")),
            summary(QStringLiteral("local-a|peer-b")),
            summary(QStringLiteral("group-1"))
        };
        const QSet<QString> onlineClients = {QStringLiteral("peer-b")};

        const QSet<QString> onlineConversationIds =
            RemotePresenceUiAdapter::directConversationIdsForOnlineClients(
                QStringLiteral("local-a"),
                conversations,
                onlineClients);

        QVERIFY(!onlineConversationIds.contains(QStringLiteral("local-a|peer-a")));
        QVERIFY(onlineConversationIds.contains(QStringLiteral("local-a|peer-b")));
        QVERIFY(!onlineConversationIds.contains(QStringLiteral("group-1")));
    }

    void serviceOnlineClientsPromoteVisiblePeerEndpoint()
    {
        QVector<PeerEndpoint> peers = {
            peer(QStringLiteral("peer-a"), false),
            peer(QStringLiteral("peer-b"), false)
        };
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

        peers = RemotePresenceUiAdapter::applyOnlineClientsToPeers(
            std::move(peers),
            {QStringLiteral("peer-b")},
            nowMs);

        QVERIFY(!peers.at(0).isConnected);
        QCOMPARE(static_cast<int>(peers.at(0).presence),
                 static_cast<int>(PeerPresenceStatus::Offline));
        QVERIFY(peers.at(1).isConnected);
        QCOMPARE(static_cast<int>(peers.at(1).presence),
                 static_cast<int>(PeerPresenceStatus::Online));
        QVERIFY(peers.at(1).lastPresenceAtMs >= nowMs);
    }

    void recentUdpHeartbeatMarksDirectConversationOnline()
    {
        const std::vector<ConversationSummary> conversations = {
            summary(QStringLiteral("local-a|peer-a")),
            summary(QStringLiteral("local-a|peer-b"))
        };
        QVector<PeerEndpoint> peers = {
            peer(QStringLiteral("peer-a"), false),
            peer(QStringLiteral("peer-b"), false)
        };
        peers[1].presence = PeerPresenceStatus::Online;
        peers[1].lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch();

        const QSet<QString> onlineConversationIds =
            RemotePresenceUiAdapter::directConversationIdsForOnlinePeers(
                QStringLiteral("local-a"),
                conversations,
                peers,
                QDateTime::currentMSecsSinceEpoch());

        QVERIFY(!onlineConversationIds.contains(QStringLiteral("local-a|peer-a")));
        QVERIFY(onlineConversationIds.contains(QStringLiteral("local-a|peer-b")));
    }
};

QTEST_MAIN(TestRemotePresenceUiAdapter)
#include "TestRemotePresenceUiAdapter.moc"
