#include "services/TransportPolicy.h"

QString transportChannelName(TransportChannel channel)
{
    switch (channel) {
    case TransportChannel::None:
        return QStringLiteral("None");
    case TransportChannel::MessageService:
        return QStringLiteral("MessageService");
    case TransportChannel::P2P:
        return QStringLiteral("P2P");
    case TransportChannel::Mixed:
        return QStringLiteral("Mixed");
    }

    return QStringLiteral("None");
}

QString transportModeName(RemoteChatTransportMode mode)
{
    switch (mode) {
    case RemoteChatTransportMode::P2POnly:
        return QStringLiteral("P2POnly");
    case RemoteChatTransportMode::ServerPreferred:
        return QStringLiteral("ServerPreferred");
    case RemoteChatTransportMode::ServerOnly:
        return QStringLiteral("ServerOnly");
    }

    return QStringLiteral("P2POnly");
}

TransportDecision TransportPolicy::chooseDirectTextChannel(
    const TransportPolicyInput& input)
{
    switch (input.mode) {
    case RemoteChatTransportMode::P2POnly:
        return TransportDecision{
            input.p2pAvailable ? TransportChannel::P2P : TransportChannel::None,
            false
        };

    case RemoteChatTransportMode::ServerPreferred:
        if (input.serviceConfigured && input.serviceReachable
            && input.receiverServerCapable) {
            return TransportDecision{
                TransportChannel::MessageService,
                input.allowP2PFallback && input.p2pAvailable
            };
        }
        if (input.allowP2PFallback && input.p2pAvailable) {
            return TransportDecision{TransportChannel::P2P, false};
        }
        return TransportDecision{TransportChannel::None, false};

    case RemoteChatTransportMode::ServerOnly:
        if (input.serviceConfigured && input.serviceReachable
            && input.receiverServerCapable) {
            return TransportDecision{TransportChannel::MessageService, false};
        }
        return TransportDecision{TransportChannel::None, false};
    }

    return TransportDecision{TransportChannel::None, false};
}

TransportDecision TransportPolicy::chooseGroupTextChannel(
    const TransportPolicyInput& input)
{
    return chooseDirectTextChannel(input);
}

TransportDecision TransportPolicy::chooseGroupFileMessageChannel(
    const TransportPolicyInput& input)
{
    return chooseDirectTextChannel(input);
}
