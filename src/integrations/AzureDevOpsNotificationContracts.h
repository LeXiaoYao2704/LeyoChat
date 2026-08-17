#pragma once

#include <QString>

#include "architecture/ResourceReference.h"
#include "domain/ResourceRefPayload.h"

enum class AzureDevOpsNotificationKind {
    WorkItemAssignedToMe,
    WorkItemMentioned,
    WorkItemCommented,
    WorkItemAttentionNeeded,
    WorkItemUpdated,
    PullRequestReviewRequested,
    PullRequestUpdated,
    BuildCompleted,
    BuildRecovered
};

struct AzureDevOpsNotificationEvent {
    AzureDevOpsNotificationKind kind = AzureDevOpsNotificationKind::WorkItemUpdated;
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString title;
    QString summary;
    QString status;
    QString webUrl;
    QString actor;
};

namespace AzureDevOpsNotificationContracts {

ResourceReference makeNotificationReference(const AzureDevOpsNotificationEvent& event);
ResourceRefPayload makeNotificationPayload(const AzureDevOpsNotificationEvent& event);

}
