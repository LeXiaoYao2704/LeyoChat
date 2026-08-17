#include <QtTest>

#include "app/PeerPresentationHelpers.h"

#include <QDateTime>

class TestPeerPresentationHelpers : public QObject {
    Q_OBJECT

private slots:
    void endpointKey_combinesHostAndPort();
    void normalizeHost_stripsIpv4MappedPrefix();
    void displayNameForPeer_prefersTrimmedDisplayName();
    void presenceTextForPeer_reportsConnectedAwayAndOfflineStates();
    void presenceTextForPeer_reportsUdpHeartbeatAsOnlineWithoutTcp();
};

void TestPeerPresentationHelpers::endpointKey_combinesHostAndPort()
{
    QCOMPARE(endpointKey(QStringLiteral("127.0.0.1"), 45454),
             QStringLiteral("127.0.0.1:45454"));
}

void TestPeerPresentationHelpers::normalizeHost_stripsIpv4MappedPrefix()
{
    QCOMPARE(normalizeHost(QStringLiteral("::ffff:192.0.2.10")),
             QStringLiteral("192.0.2.10"));
    QCOMPARE(normalizeHost(QStringLiteral("fe80::1")), QStringLiteral("fe80::1"));
}

void TestPeerPresentationHelpers::displayNameForPeer_prefersTrimmedDisplayName()
{
    PeerEndpoint peer;
    peer.clientId = "client-a";
    peer.displayName = "  Alice  ";
    QCOMPARE(displayNameForPeer(peer), QStringLiteral("Alice"));

    peer.displayName.clear();
    QCOMPARE(displayNameForPeer(peer), QStringLiteral("client-a"));
}

void TestPeerPresentationHelpers::presenceTextForPeer_reportsConnectedAwayAndOfflineStates()
{
    PeerEndpoint peer;
    peer.isConnected = true;
    peer.presence = PeerPresenceStatus::Online;
    peer.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch();
    QCOMPARE(presenceTextForPeer(peer), QStringLiteral("\u5728\u7EBF"));

    peer.presence = PeerPresenceStatus::Away;
    QCOMPARE(presenceTextForPeer(peer), QStringLiteral("\u79BB\u5F00"));

    peer.isConnected = false;
    peer.lastPresenceAtMs = 0;
    QCOMPARE(presenceTextForPeer(peer), QStringLiteral("\u79BB\u7EBF"));
}

void TestPeerPresentationHelpers::presenceTextForPeer_reportsUdpHeartbeatAsOnlineWithoutTcp()
{
    PeerEndpoint peer;
    peer.isConnected = false;
    peer.presence = PeerPresenceStatus::Online;
    peer.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch();

    QCOMPARE(presenceTextForPeer(peer), QStringLiteral("\u5728\u7EBF"));
}

QTEST_MAIN(TestPeerPresentationHelpers)
#include "TestPeerPresentationHelpers.moc"
