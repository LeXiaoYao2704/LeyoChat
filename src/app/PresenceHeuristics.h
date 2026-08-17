#pragma once

#include "domain/PeerEndpoint.h"

#include <QtGlobal>

struct PresenceHeuristicInputs {
    bool workstationLocked = false;
    qint64 idleMilliseconds = 0;
    qint64 awayThresholdMilliseconds = 10 * 60 * 1000;
};

inline PeerPresenceStatus determineLocalPresence(const PresenceHeuristicInputs& inputs)
{
    if (inputs.workstationLocked) {
        return PeerPresenceStatus::Away;
    }

    const bool idleAway =
        inputs.awayThresholdMilliseconds > 0
        && inputs.idleMilliseconds >= inputs.awayThresholdMilliseconds;

    if (idleAway) {
        return PeerPresenceStatus::Away;
    }

    return PeerPresenceStatus::Online;
}
