// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增60行/修改0行/删除0行; 总行数60行
// @AI-LastModified: 2026-04-16 09:07:23

#include <QtTest/QTest>

#include "services/PeerDirectoryService.h"

class TestPeerDirectoryService : public QObject {
    Q_OBJECT

private slots:
    void discoveredHeartbeatMarksPeerPresentWithoutTcp()
    {
        PeerDirectoryService service;

        PeerEndpoint discovered;
        discovered.clientId = "peer-udp";
        discovered.displayName = "Udp Peer";
        discovered.host = "192.0.2.30";
        discovered.port = 45454;
        discovered.isConnected = false;
        discovered.presence = PeerPresenceStatus::Online;
        discovered.lastPresenceAtMs = 3000;

        QVERIFY(service.upsertDiscoveredPeer(discovered));

        const auto visible = service.visiblePeers("local-user");
        QCOMPARE(visible.size(), 1);
        QVERIFY(!visible.front().isConnected);
        QCOMPARE(visible.front().presence, PeerPresenceStatus::Online);
        QCOMPARE(visible.front().lastPresenceAtMs, 3000LL);
    }

    void discoveredPeerDoesNotDowngradeConnectedPeer()
    {
        PeerDirectoryService service;

        PeerEndpoint connected;
        connected.clientId = "peer-1";
        connected.displayName = "Alice";
        connected.host = "192.0.2.20";
        connected.port = 45454;
        connected.isConnected = true;
        connected.presence = PeerPresenceStatus::Online;
        connected.lastPresenceAtMs = 1000;

        QVERIFY(service.upsertConnectedPeer(connected));

        PeerEndpoint discovered = connected;
        discovered.isConnected = false;
        discovered.presence = PeerPresenceStatus::Offline;
        discovered.lastPresenceAtMs = 2000;

        QVERIFY(!service.upsertDiscoveredPeer(discovered));

        const auto visible = service.visiblePeers("local-user");
        QCOMPARE(visible.size(), 1);
        QVERIFY(visible.front().isConnected);
        QCOMPARE(visible.front().presence, PeerPresenceStatus::Online);
        QCOMPARE(visible.front().lastPresenceAtMs, 2000LL);
    }

    void duplicateConnectedPeerHeartbeatDoesNotCountAsUiChange()
    {
        PeerDirectoryService service;

        PeerEndpoint peer;
        peer.clientId = "peer-1";
        peer.displayName = "Alice";
        peer.host = "192.0.2.20";
        peer.port = 45454;
        peer.isConnected = true;
        peer.presence = PeerPresenceStatus::Online;
        peer.lastPresenceAtMs = 1000;

        QVERIFY(service.upsertConnectedPeer(peer));

        PeerEndpoint heartbeat = peer;
        heartbeat.lastPresenceAtMs = 2000;

        QVERIFY(!service.upsertConnectedPeer(heartbeat));

        const auto visible = service.visiblePeers("local-user");
        QCOMPARE(visible.size(), 1);
        QCOMPARE(visible.front().displayName, std::string("Alice"));
        QCOMPARE(visible.front().host, std::string("192.0.2.20"));
        QCOMPARE(visible.front().port, static_cast<quint16>(45454));
        QVERIFY(visible.front().isConnected);
        QCOMPARE(visible.front().presence, PeerPresenceStatus::Online);
        QCOMPARE(visible.front().lastPresenceAtMs, 2000LL);
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TestPeerDirectoryService tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestPeerDirectoryService.moc"
