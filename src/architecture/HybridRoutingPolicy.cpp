#include "architecture/HybridRoutingPolicy.h"

#include "architecture/RuntimeArchitectureQueryService.h"

HybridRoutingDecision HybridRoutingPolicy::decideGroupFileRouting(
    const RuntimeArchitectureSnapshot& snapshot,
    const QString& groupId)
{
    HybridRoutingDecision decision;
    decision.groupId = groupId.trimmed();

    RuntimeArchitectureQueryService query(snapshot);
    decision.hasBoundService = query.hasBoundServiceForGroup(decision.groupId);
    decision.sharedFilesEnabled = query.sharedFilesEnabledForGroup(decision.groupId);
    decision.serviceId = query.boundServiceIdForGroup(decision.groupId);
    decision.serviceName = query.serviceNameForGroup(decision.groupId);

    const ResourceReference primaryResource = query.primaryResourceForGroup(decision.groupId);
    decision.workspaceId = primaryResource.workspaceId.trimmed();
    decision.primaryResourceId = primaryResource.resourceId.trimmed();
    decision.primaryResourceTitle = primaryResource.title.trimmed();

    const ResourceReference selectedResource = query.selectedResourceForGroup(decision.groupId);
    decision.selectedResourceId = selectedResource.resourceId.trimmed();
    decision.selectedResourceTitle = selectedResource.title.trimmed();

    if (decision.hasBoundService && decision.sharedFilesEnabled) {
        decision.mode = HybridRouteMode::ServicePreferred;
    }

    return decision;
}
