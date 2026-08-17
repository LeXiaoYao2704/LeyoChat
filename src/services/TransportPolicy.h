#pragma once

#include "integrations/RemoteChatServiceSettings.h"

#include <QString>

enum class TransportChannel {
    None,
    MessageService,
    P2P,
    Mixed
};

QString transportChannelName(TransportChannel channel);
QString transportModeName(RemoteChatTransportMode mode);

struct TransportPolicyInput {
    RemoteChatTransportMode mode = RemoteChatTransportMode::P2POnly;
    bool serviceConfigured = false;
    bool serviceReachable = false;
    bool receiverServerCapable = false;
    bool p2pAvailable = false;
    bool allowP2PFallback = true;
};

struct TransportDecision {
    TransportChannel primary = TransportChannel::None;
    bool mayFallbackToP2P = false;
};

class TransportPolicy {
public:
    static TransportDecision chooseDirectTextChannel(
        const TransportPolicyInput& input);
    static TransportDecision chooseGroupTextChannel(
        const TransportPolicyInput& input);
    static TransportDecision chooseGroupFileMessageChannel(
        const TransportPolicyInput& input);
};
