#pragma once

#include <optional>
#include <vector>

#include "domain/PeerEndpoint.h"

class PeerDirectoryService {
public:
    bool upsertDiscoveredPeer(const PeerEndpoint& peer);
    bool upsertConnectedPeer(const PeerEndpoint& peer);
    bool touchPresence(const std::string& clientId, qint64 nowMs);
    bool markPeerDisconnected(const std::string& clientId);
    void removePeerByClientId(const std::string& clientId);
    std::vector<PeerEndpoint> peers() const;
    std::vector<PeerEndpoint> visiblePeers(const std::string& localClientId) const;
    std::vector<PeerEndpoint> connectedPeers(const std::string& localClientId) const;
    std::optional<PeerEndpoint> findPeerByClientId(const std::string& clientId) const;

private:
    PeerEndpoint* findMutablePeerByClientId(const std::string& clientId);
    std::vector<PeerEndpoint> m_peers;
};
