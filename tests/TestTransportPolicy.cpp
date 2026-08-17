#include <QtTest/QTest>

#include "services/TransportPolicy.h"

class TestTransportPolicy : public QObject {
    Q_OBJECT

private slots:
    void channelNameIsStableForRouteLogs()
    {
        QCOMPARE(transportChannelName(TransportChannel::None),
                 QStringLiteral("None"));
        QCOMPARE(transportChannelName(TransportChannel::MessageService),
                 QStringLiteral("MessageService"));
        QCOMPARE(transportChannelName(TransportChannel::P2P),
                 QStringLiteral("P2P"));
        QCOMPARE(transportChannelName(TransportChannel::Mixed),
                 QStringLiteral("Mixed"));
    }

    void transportModeNameIsStableForRouteLogs()
    {
        QCOMPARE(transportModeName(RemoteChatTransportMode::P2POnly),
                 QStringLiteral("P2POnly"));
        QCOMPARE(transportModeName(RemoteChatTransportMode::ServerPreferred),
                 QStringLiteral("ServerPreferred"));
        QCOMPARE(transportModeName(RemoteChatTransportMode::ServerOnly),
                 QStringLiteral("ServerOnly"));
    }

    void p2pOnlyUsesP2PWhenAvailable()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::P2POnly;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.receiverServerCapable = true;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseDirectTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::P2P);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void serverPreferredUsesServiceWhenReachable()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerPreferred;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.receiverServerCapable = true;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseDirectTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::MessageService);
        QVERIFY(decision.mayFallbackToP2P);
    }

    void serverPreferredHealthyButReceiverLegacyUsesP2P()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerPreferred;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.receiverServerCapable = false;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseDirectTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::P2P);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void serverPreferredHealthyAndReceiverCapableUsesService()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerPreferred;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.receiverServerCapable = true;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseDirectTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::MessageService);
        QVERIFY(decision.mayFallbackToP2P);
    }

    void serverPreferredFallsBackToP2PWhenServiceUnavailable()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerPreferred;
        input.serviceConfigured = true;
        input.serviceReachable = false;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseDirectTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::P2P);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void serverOnlyNeverFallsBackToP2P()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerOnly;
        input.serviceConfigured = true;
        input.serviceReachable = false;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseDirectTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::None);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void unavailableChannelsReturnNone()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerPreferred;
        input.serviceConfigured = false;
        input.serviceReachable = false;
        input.p2pAvailable = false;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseDirectTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::None);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void groupP2POnlyUsesP2PWhenAvailable()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::P2POnly;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.receiverServerCapable = true;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseGroupTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::P2P);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void groupServerPreferredUsesServiceWhenReachable()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerPreferred;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.receiverServerCapable = true;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseGroupTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::MessageService);
        QVERIFY(decision.mayFallbackToP2P);
    }

    void groupServerOnlyUnavailableReturnsNone()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerOnly;
        input.serviceConfigured = true;
        input.serviceReachable = false;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseGroupTextChannel(input);

        QCOMPARE(decision.primary, TransportChannel::None);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void groupFileP2POnlyUsesP2PWhenAvailable()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::P2POnly;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseGroupFileMessageChannel(input);

        QCOMPARE(decision.primary, TransportChannel::P2P);
        QVERIFY(!decision.mayFallbackToP2P);
    }

    void groupFileServerPreferredUsesServiceWhenReachable()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerPreferred;
        input.serviceConfigured = true;
        input.serviceReachable = true;
        input.receiverServerCapable = true;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseGroupFileMessageChannel(input);

        QCOMPARE(decision.primary, TransportChannel::MessageService);
        QVERIFY(decision.mayFallbackToP2P);
    }

    void groupFileServerOnlyUnavailableReturnsNone()
    {
        TransportPolicyInput input;
        input.mode = RemoteChatTransportMode::ServerOnly;
        input.serviceConfigured = true;
        input.serviceReachable = false;
        input.p2pAvailable = true;
        input.allowP2PFallback = true;

        const TransportDecision decision =
            TransportPolicy::chooseGroupFileMessageChannel(input);

        QCOMPARE(decision.primary, TransportChannel::None);
        QVERIFY(!decision.mayFallbackToP2P);
    }
};

QTEST_MAIN(TestTransportPolicy)
#include "TestTransportPolicy.moc"
