#pragma once

#include <QString>

#include "architecture/RuntimeArchitectureSnapshot.h"

enum class HybridRouteMode {
    P2POnly,
    ServicePreferred,
    ServiceRequired
};

struct HybridRoutingDecision {
    HybridRouteMode mode = HybridRouteMode::P2POnly;
    bool hasBoundService = false;
    bool sharedFilesEnabled = false;
    QString groupId;
    QString workspaceId;
    QString serviceId;
    QString serviceName;
    QString primaryResourceId;
    QString primaryResourceTitle;
    QString selectedResourceId;
    QString selectedResourceTitle;
};

class HybridRoutingPolicy {
public:
    static HybridRoutingDecision decideGroupFileRouting(
        const RuntimeArchitectureSnapshot& snapshot,
        const QString& groupId);
};
