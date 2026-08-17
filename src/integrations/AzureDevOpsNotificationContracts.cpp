#include "integrations/AzureDevOpsNotificationContracts.h"

namespace {

QString eventKindResourceKind(AzureDevOpsNotificationKind kind)
{
    switch (kind) {
    case AzureDevOpsNotificationKind::WorkItemAssignedToMe:
    case AzureDevOpsNotificationKind::WorkItemMentioned:
    case AzureDevOpsNotificationKind::WorkItemCommented:
    case AzureDevOpsNotificationKind::WorkItemAttentionNeeded:
    case AzureDevOpsNotificationKind::WorkItemUpdated:
        return QStringLiteral("devops_work_item");
    case AzureDevOpsNotificationKind::PullRequestReviewRequested:
    case AzureDevOpsNotificationKind::PullRequestUpdated:
        return QStringLiteral("devops_pull_request");
    case AzureDevOpsNotificationKind::BuildCompleted:
    case AzureDevOpsNotificationKind::BuildRecovered:
        return QStringLiteral("devops_build");
    default:
        return QStringLiteral("devops_event");
    }
}

QString eventLabel(AzureDevOpsNotificationKind kind)
{
    switch (kind) {
    case AzureDevOpsNotificationKind::WorkItemAssignedToMe:
        return QStringLiteral("工作项已分配");
    case AzureDevOpsNotificationKind::WorkItemMentioned:
        return QStringLiteral("工作项提及");
    case AzureDevOpsNotificationKind::WorkItemCommented:
        return QStringLiteral("工作项评论");
    case AzureDevOpsNotificationKind::WorkItemAttentionNeeded:
        return QStringLiteral("工作项待关注");
    case AzureDevOpsNotificationKind::WorkItemUpdated:
        return QStringLiteral("工作项更新");
    case AzureDevOpsNotificationKind::PullRequestReviewRequested:
        return QStringLiteral("PR 审核请求");
    case AzureDevOpsNotificationKind::PullRequestUpdated:
        return QStringLiteral("PR 更新");
    case AzureDevOpsNotificationKind::BuildCompleted:
        return QStringLiteral("构建通知");
    case AzureDevOpsNotificationKind::BuildRecovered:
        return QStringLiteral("构建恢复");
    default:
        return QStringLiteral("DevOps 通知");
    }
}

void appendOpenAction(ResourceRefPayload& payload, const QString& target)
{
    if (target.trimmed().isEmpty()) {
        return;
    }

    payload.actions.push_back(ResourceRefAction{
        QStringLiteral("open"),
        QStringLiteral("打开详情"),
        target.trimmed(),
        true,
    });
}

}  // namespace

namespace AzureDevOpsNotificationContracts {

ResourceReference makeNotificationReference(const AzureDevOpsNotificationEvent& event)
{
    ResourceReference reference;
    reference.serviceId = event.serviceId.trimmed();
    reference.workspaceId = event.workspaceId.trimmed();
    reference.resourceId = event.resourceId.trimmed();
    reference.resourceKind = eventKindResourceKind(event.kind);
    reference.title =
        event.title.trimmed().isEmpty() ? eventLabel(event.kind) : event.title.trimmed();
    reference.version = event.status.trimmed();
    reference.summary =
        event.summary.trimmed().isEmpty() ? event.actor.trimmed() : event.summary.trimmed();
    reference.origin = ResourceOrigin::Service;
    return reference;
}

ResourceRefPayload makeNotificationPayload(const AzureDevOpsNotificationEvent& event)
{
    const ResourceReference reference = makeNotificationReference(event);

    ResourceRefPayload payload;
    payload.serviceId = reference.serviceId;
    payload.workspaceId = reference.workspaceId;
    payload.origin = QStringLiteral("service");
    payload.kind = reference.resourceKind;
    payload.resourceId = reference.resourceId;
    payload.title = reference.title;
    payload.subtitle = reference.summary;
    payload.status = reference.version;
    payload.snapshotVersion = QStringLiteral("event-v1");
    appendOpenAction(payload, event.webUrl);
    return payload;
}

}  // namespace AzureDevOpsNotificationContracts
