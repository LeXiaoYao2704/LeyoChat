#include "services/P2PConnectionPolicy.h"

namespace {

bool isAutomaticDiscoveryTrigger(P2PConnectionTrigger trigger)
{
    switch (trigger) {
    case P2PConnectionTrigger::StartupKnownPeer:
    case P2PConnectionTrigger::LanDiscovery:
    case P2PConnectionTrigger::PeerDirectorySnapshot:
        return true;
    case P2PConnectionTrigger::ExplicitUserAction:
    case P2PConnectionTrigger::ServiceFallback:
        return false;
    }
    return false;
}

}  // namespace

bool P2PConnectionPolicy::shouldStartPeerConnection(
    const RemoteChatServiceSettings& settings,
    P2PConnectionTrigger trigger)
{
    if (isAutomaticDiscoveryTrigger(trigger)) {
        return settings.mode == RemoteChatTransportMode::P2POnly
            && settings.allowAutomaticPeerConnections;
    }

    switch (trigger) {
    case P2PConnectionTrigger::ExplicitUserAction:
        return settings.mode != RemoteChatTransportMode::ServerOnly;
    case P2PConnectionTrigger::ServiceFallback:
        return settings.mode != RemoteChatTransportMode::ServerOnly
            && settings.allowP2PFallback;
    case P2PConnectionTrigger::StartupKnownPeer:
    case P2PConnectionTrigger::LanDiscovery:
    case P2PConnectionTrigger::PeerDirectorySnapshot:
        return false;
    }

    return false;
}

bool P2PConnectionPolicy::shouldStartPeerConnection(
    const RemoteChatServiceSettings& settings,
    P2PConnectionTrigger trigger,
    const QStringList& peerCapabilities)
{
    Q_UNUSED(peerCapabilities);
    return shouldStartPeerConnection(settings, trigger);
}

bool P2PConnectionPolicy::shouldPreflightLegacyPeerConnection(
    const RemoteChatServiceSettings& settings,
    bool receiverServerCapable,
    bool p2pAvailable,
    bool hasPeerEndpoint)
{
    if (receiverServerCapable || p2pAvailable || !hasPeerEndpoint) {
        return false;
    }
    return shouldStartPeerConnection(settings, P2PConnectionTrigger::ServiceFallback);
}
