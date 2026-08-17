#include "services/PeerDirectoryService.h"

#include <algorithm>
#include <tuple>

namespace {
void mergePeerFields(PeerEndpoint& existing, const PeerEndpoint& incoming) {
    if (!incoming.displayName.empty()) {
        existing.displayName = incoming.displayName;
    }
    if (!incoming.host.empty()) {
        existing.host = incoming.host;
    }
    if (incoming.port != 0) {
        existing.port = incoming.port;
    }
    if (incoming.presence != PeerPresenceStatus::Offline || incoming.isConnected) {
        existing.presence = incoming.presence;
    }
    if (incoming.lastPresenceAtMs > 0) {
        existing.lastPresenceAtMs = incoming.lastPresenceAtMs;
    }
}
}

PeerEndpoint* PeerDirectoryService::findMutablePeerByClientId(const std::string& clientId) {
    for (auto& peer : m_peers) {
        if (peer.clientId == clientId) {
            return &peer;
        }
    }
    return nullptr;
}

bool PeerDirectoryService::upsertDiscoveredPeer(const PeerEndpoint& peer) {
    if (peer.clientId.empty()) {
        return false;
    }

    if (PeerEndpoint* existing = findMutablePeerByClientId(peer.clientId)) {
        const bool wasConnected = existing->isConnected;
        if (wasConnected) {
            if (peer.lastPresenceAtMs > 0) {
                existing->lastPresenceAtMs = peer.lastPresenceAtMs;
            }
            if (!peer.displayName.empty() && existing->displayName != peer.displayName) {
                existing->displayName = peer.displayName;
                return true;
            }
            return false;
        }

        const auto before = std::make_tuple(existing->displayName,
                                            existing->host,
                                            existing->port,
                                            existing->presence);
        mergePeerFields(*existing, peer);
        existing->isConnected = false;
        if (peer.presence == PeerPresenceStatus::Offline) {
            existing->presence = PeerPresenceStatus::Offline;
        }
        const auto after = std::make_tuple(existing->displayName,
                                           existing->host,
                                           existing->port,
                                           existing->presence);
        return before != after;
    }

    PeerEndpoint discovered = peer;
    discovered.isConnected = false;
    m_peers.push_back(discovered);
    return true;
}

bool PeerDirectoryService::upsertConnectedPeer(const PeerEndpoint& peer) {
    if (peer.clientId.empty()) {
        return false;
    }

    if (PeerEndpoint* existing = findMutablePeerByClientId(peer.clientId)) {
        const auto before = std::make_tuple(existing->displayName,
                                            existing->host,
                                            existing->port,
                                            existing->isConnected,
                                            existing->presence);
        mergePeerFields(*existing, peer);
        existing->isConnected = true;
        if (existing->presence == PeerPresenceStatus::Offline) {
            existing->presence = PeerPresenceStatus::Online;
        }
        const auto after = std::make_tuple(existing->displayName,
                                           existing->host,
                                           existing->port,
                                           existing->isConnected,
                                           existing->presence);
        return before != after;
    }

    PeerEndpoint connected = peer;
    connected.isConnected = true;
    if (connected.presence == PeerPresenceStatus::Offline) {
        connected.presence = PeerPresenceStatus::Online;
    }
    m_peers.push_back(connected);
    return true;
}

bool PeerDirectoryService::touchPresence(const std::string& clientId, qint64 nowMs) {
    PeerEndpoint* existing = findMutablePeerByClientId(clientId);
    if (!existing) {
        return false;
    }
    existing->lastPresenceAtMs = nowMs;
    if (existing->isConnected && existing->presence != PeerPresenceStatus::Online) {
        existing->presence = PeerPresenceStatus::Online;
        return true;
    }
    return false;
}

bool PeerDirectoryService::markPeerDisconnected(const std::string& clientId) {
    if (PeerEndpoint* existing = findMutablePeerByClientId(clientId)) {
        const bool changed = existing->isConnected || existing->presence != PeerPresenceStatus::Offline;
        existing->isConnected = false;
        existing->presence = PeerPresenceStatus::Offline;
        return changed;
    }
    return false;
}

void PeerDirectoryService::removePeerByClientId(const std::string& clientId) {
    m_peers.erase(std::remove_if(m_peers.begin(), m_peers.end(),
                                 [&](const PeerEndpoint& peer) { return peer.clientId == clientId; }),
                  m_peers.end());
}

std::vector<PeerEndpoint> PeerDirectoryService::peers() const {
    return m_peers;
}

std::vector<PeerEndpoint> PeerDirectoryService::visiblePeers(const std::string& localClientId) const {
    std::vector<PeerEndpoint> visible;
    visible.reserve(m_peers.size());

    for (const auto& peer : m_peers) {
        if (peer.clientId == localClientId) {
            continue;
        }
        visible.push_back(peer);
    }

    std::sort(visible.begin(), visible.end(), [](const PeerEndpoint& left, const PeerEndpoint& right) {
        if (left.displayName != right.displayName) {
            return left.displayName < right.displayName;
        }
        return left.clientId < right.clientId;
    });
    return visible;
}

std::vector<PeerEndpoint> PeerDirectoryService::connectedPeers(const std::string& localClientId) const {
    std::vector<PeerEndpoint> connected;
    connected.reserve(m_peers.size());

    for (const auto& peer : m_peers) {
        if (peer.clientId == localClientId || !peer.isConnected) {
            continue;
        }
        connected.push_back(peer);
    }

    return connected;
}

std::optional<PeerEndpoint> PeerDirectoryService::findPeerByClientId(const std::string& clientId) const {
    for (const auto& peer : m_peers) {
        if (peer.clientId == clientId) {
            return peer;
        }
    }

    return std::nullopt;
}
