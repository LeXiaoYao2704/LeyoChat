#pragma once

#include <algorithm>

#include "domain/PeerEndpoint.h"

namespace PeerPresenceEvaluator {

constexpr qint64 kPresenceAwayThresholdMs = 90 * 1000;
constexpr qint64 kPresenceOfflineThresholdMs = 5 * 60 * 1000;
constexpr qint64 kDisconnectGraceMs = 3 * 1000;

inline qint64 presenceAgeMs(const PeerEndpoint& endpoint, qint64 nowMs)
{
    if (endpoint.lastPresenceAtMs <= 0) {
        return 0;
    }
    return std::max<qint64>(0, nowMs - endpoint.lastPresenceAtMs);
}

inline PeerPresenceStatus effectivePresence(const PeerEndpoint& endpoint, qint64 nowMs)
{
    const qint64 ageMs = presenceAgeMs(endpoint, nowMs);

    if (!endpoint.isConnected) {
        if (endpoint.presence == PeerPresenceStatus::Offline) {
            if (endpoint.lastPresenceAtMs > 0 && ageMs <= kDisconnectGraceMs) {
                return PeerPresenceStatus::Away;
            }
            return PeerPresenceStatus::Offline;
        }
        if (endpoint.lastPresenceAtMs <= 0) {
            return PeerPresenceStatus::Offline;
        }
        if (ageMs >= kPresenceOfflineThresholdMs) {
            return PeerPresenceStatus::Offline;
        }
        if (endpoint.presence == PeerPresenceStatus::Away
            || ageMs >= kPresenceAwayThresholdMs) {
            return PeerPresenceStatus::Away;
        }
        return PeerPresenceStatus::Online;
    }

    if (endpoint.lastPresenceAtMs > 0 && ageMs >= kPresenceOfflineThresholdMs) {
        return PeerPresenceStatus::Offline;
    }
    if (endpoint.presence == PeerPresenceStatus::Offline) {
        return PeerPresenceStatus::Offline;
    }
    if (endpoint.presence == PeerPresenceStatus::Away
        || (endpoint.lastPresenceAtMs > 0 && ageMs >= kPresenceAwayThresholdMs)) {
        return PeerPresenceStatus::Away;
    }
    return PeerPresenceStatus::Online;
}

inline bool isOnlineOrAway(const PeerEndpoint& endpoint, qint64 nowMs)
{
    return effectivePresence(endpoint, nowMs) != PeerPresenceStatus::Offline;
}

}  // namespace PeerPresenceEvaluator
