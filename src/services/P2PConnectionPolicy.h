#pragma once

#include "integrations/RemoteChatServiceSettings.h"

#include <QStringList>

enum class P2PConnectionTrigger {
    StartupKnownPeer,
    LanDiscovery,
    PeerDirectorySnapshot,
    ExplicitUserAction,
    ServiceFallback
};

class P2PConnectionPolicy {
public:
    static bool shouldStartPeerConnection(const RemoteChatServiceSettings& settings,
                                          P2PConnectionTrigger trigger);
    static bool shouldStartPeerConnection(const RemoteChatServiceSettings& settings,
                                          P2PConnectionTrigger trigger,
                                          const QStringList& peerCapabilities);
    static bool shouldPreflightLegacyPeerConnection(const RemoteChatServiceSettings& settings,
                                                    bool receiverServerCapable,
                                                    bool p2pAvailable,
                                                    bool hasPeerEndpoint);
};
