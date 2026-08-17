#include <QtTest/QTest>

#include <QDateTime>

#include "integrations/RemoteChatServiceSettings.h"
#include "services/P2PConnectionPolicy.h"

class TestP2PConnectionPolicy : public QObject {
    Q_OBJECT

private slots:
    void automaticDiscoveryTriggersAreDirectoryOnlyByDefault()
    {
        RemoteChatServiceSettings settings;
        settings.mode = RemoteChatTransportMode::P2POnly;
        settings.allowAutomaticPeerConnections = false;

        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::StartupKnownPeer));
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::PeerDirectorySnapshot));
    }

    void automaticDiscoveryOptInOnlyAppliesToP2POnlyMode()
    {
        RemoteChatServiceSettings settings;
        settings.allowAutomaticPeerConnections = true;

        settings.mode = RemoteChatTransportMode::P2POnly;
        QVERIFY(P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));

        settings.mode = RemoteChatTransportMode::ServerPreferred;
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));

        settings.mode = RemoteChatTransportMode::ServerOnly;
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));
    }

    void serverPreferredFailedHealthDoesNotStartDiscoveryFullMesh()
    {
        RemoteChatServiceSettings settings;
        settings.mode = RemoteChatTransportMode::ServerPreferred;
        settings.allowP2PFallback = true;
        settings.allowAutomaticPeerConnections = false;

        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        settings.lastHealthCheckAtMs = nowMs - 1000;
        settings.lastHealthSuccessAtMs = 0;
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::StartupKnownPeer));
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::PeerDirectorySnapshot));

        settings.lastHealthSuccessAtMs = nowMs - 1000;
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));

        settings.lastHealthSuccessAtMs = 0;
        settings.allowP2PFallback = false;
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery));
    }

    void serverPreferredHealthyLanDiscoveryDoesNotAutoConnectLegacyPeer()
    {
        RemoteChatServiceSettings settings;
        settings.mode = RemoteChatTransportMode::ServerPreferred;
        settings.allowP2PFallback = true;
        settings.allowAutomaticPeerConnections = false;
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        settings.lastHealthCheckAtMs = nowMs - 1000;
        settings.lastHealthSuccessAtMs = nowMs - 1000;

        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::LanDiscovery, {}));
    }

    void serverOnlyDisablesServiceFallbackP2PConnection()
    {
        RemoteChatServiceSettings settings;
        settings.mode = RemoteChatTransportMode::ServerPreferred;
        settings.allowP2PFallback = true;
        QVERIFY(P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::ServiceFallback));

        settings.mode = RemoteChatTransportMode::ServerOnly;
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::ServiceFallback));

        settings.mode = RemoteChatTransportMode::ServerPreferred;
        settings.allowP2PFallback = false;
        QVERIFY(!P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::ServiceFallback));
    }

    void legacyPreflightRequiresFallbackTargetEndpointAndNoConnectedP2P()
    {
        RemoteChatServiceSettings settings;
        settings.mode = RemoteChatTransportMode::ServerPreferred;
        settings.allowP2PFallback = true;

        QVERIFY(P2PConnectionPolicy::shouldPreflightLegacyPeerConnection(
            settings,
            false,
            false,
            true));

        QVERIFY(!P2PConnectionPolicy::shouldPreflightLegacyPeerConnection(
            settings,
            true,
            false,
            true));
        QVERIFY(!P2PConnectionPolicy::shouldPreflightLegacyPeerConnection(
            settings,
            false,
            true,
            true));
        QVERIFY(!P2PConnectionPolicy::shouldPreflightLegacyPeerConnection(
            settings,
            false,
            false,
            false));

        settings.mode = RemoteChatTransportMode::ServerOnly;
        QVERIFY(!P2PConnectionPolicy::shouldPreflightLegacyPeerConnection(
            settings,
            false,
            false,
            true));
    }

    void explicitUserActionCanStillStartP2PConnection()
    {
        RemoteChatServiceSettings settings;

        settings.mode = RemoteChatTransportMode::P2POnly;
        QVERIFY(P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::ExplicitUserAction));

        settings.mode = RemoteChatTransportMode::ServerPreferred;
        QVERIFY(P2PConnectionPolicy::shouldStartPeerConnection(
            settings, P2PConnectionTrigger::ExplicitUserAction));
    }
};

QTEST_MAIN(TestP2PConnectionPolicy)
#include "TestP2PConnectionPolicy.moc"
