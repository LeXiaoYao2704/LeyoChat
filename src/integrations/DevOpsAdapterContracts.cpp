#include "integrations/DevOpsAdapterContracts.h"

namespace {

ResourceReference baseReference(const QString& serviceId,
                                const QString& workspaceId,
                                const QString& resourceId,
                                const QString& kind,
                                const QString& title,
                                const QString& version,
                                const QString& summary)
{
    ResourceReference reference;
    reference.serviceId = serviceId.trimmed();
    reference.workspaceId = workspaceId.trimmed();
    reference.resourceId = resourceId.trimmed();
    reference.resourceKind = kind;
    reference.title = title.trimmed();
    reference.version = version.trimmed();
    reference.summary = summary.trimmed();
    reference.origin = ResourceOrigin::Service;
    return reference;
}

ResourceRefPayload basePayload(const QString& serviceId,
                               const QString& workspaceId,
                               const QString& resourceId,
                               const QString& kind,
                               const QString& title,
                               const QString& subtitle,
                               const QString& status)
{
    ResourceRefPayload payload;
    payload.serviceId = serviceId.trimmed();
    payload.workspaceId = workspaceId.trimmed();
    payload.origin = QStringLiteral("service");
    payload.kind = kind;
    payload.resourceId = resourceId.trimmed();
    payload.title = title.trimmed();
    payload.subtitle = subtitle.trimmed();
    payload.status = status.trimmed();
    payload.snapshotVersion = QStringLiteral("v1");
    return payload;
}

QString joinedSummary(std::initializer_list<QString> parts)
{
    QStringList filtered;
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            filtered.push_back(trimmed);
        }
    }
    return filtered.join(QStringLiteral(" · "));
}

void appendPrimaryLink(ResourceRefPayload& payload, const QString& label, const QString& target)
{
    if (target.trimmed().isEmpty()) {
        return;
    }
    payload.actions.push_back(ResourceRefAction{
        QStringLiteral("open"),
        label,
        target.trimmed(),
        true,
    });
}

}

namespace DevOpsAdapterContracts {

ResourceReference makeWorkItemReference(const DevOpsWorkItemResource& resource)
{
    const QString title = resource.title.trimmed().isEmpty()
        ? QStringLiteral("Work Item %1").arg(resource.numericId > 0 ? QString::number(resource.numericId)
                                                                    : resource.resourceId.trimmed())
        : resource.title.trimmed();
    return baseReference(resource.serviceId,
                         resource.workspaceId,
                         resource.resourceId,
                         QStringLiteral("devops_work_item"),
                         title,
                         resource.state,
                         joinedSummary({resource.organization, resource.project, resource.assignedTo}));
}

ResourceReference makePullRequestReference(const DevOpsPullRequestResource& resource)
{
    const QString title = resource.title.trimmed().isEmpty()
        ? QStringLiteral("Pull Request %1").arg(resource.pullRequestId > 0 ? QString::number(resource.pullRequestId)
                                                                           : resource.resourceId.trimmed())
        : resource.title.trimmed();
    return baseReference(resource.serviceId,
                         resource.workspaceId,
                         resource.resourceId,
                         QStringLiteral("devops_pull_request"),
                         title,
                         resource.status,
                         joinedSummary({resource.project, resource.repository, resource.author}));
}

ResourceReference makeBuildReference(const DevOpsBuildResource& resource)
{
    const QString title = resource.definitionName.trimmed().isEmpty()
        ? QStringLiteral("Build %1").arg(resource.buildId > 0 ? QString::number(resource.buildId)
                                                              : resource.resourceId.trimmed())
        : resource.definitionName.trimmed();
    return baseReference(resource.serviceId,
                         resource.workspaceId,
                         resource.resourceId,
                         QStringLiteral("devops_build"),
                         title,
                         resource.status,
                         joinedSummary({resource.project, resource.branchName, resource.requestedBy}));
}

ResourceRefPayload makeWorkItemPayload(const DevOpsWorkItemResource& resource)
{
    const ResourceReference reference = makeWorkItemReference(resource);
    ResourceRefPayload payload = basePayload(reference.serviceId,
                                             reference.workspaceId,
                                             reference.resourceId,
                                             reference.resourceKind,
                                             reference.title,
                                             joinedSummary({resource.workItemType,
                                                            resource.project,
                                                            resource.assignedTo}),
                                             resource.state);
    appendPrimaryLink(payload, QStringLiteral("打开工作项"), resource.webUrl);
    return payload;
}

ResourceRefPayload makePullRequestPayload(const DevOpsPullRequestResource& resource)
{
    const ResourceReference reference = makePullRequestReference(resource);
    ResourceRefPayload payload = basePayload(reference.serviceId,
                                             reference.workspaceId,
                                             reference.resourceId,
                                             reference.resourceKind,
                                             reference.title,
                                             joinedSummary({resource.repository, resource.author}),
                                             resource.status);
    appendPrimaryLink(payload, QStringLiteral("打开合并请求"), resource.webUrl);
    return payload;
}

ResourceRefPayload makeBuildPayload(const DevOpsBuildResource& resource)
{
    const ResourceReference reference = makeBuildReference(resource);
    ResourceRefPayload payload = basePayload(reference.serviceId,
                                             reference.workspaceId,
                                             reference.resourceId,
                                             reference.resourceKind,
                                             reference.title,
                                             joinedSummary({resource.project, resource.branchName}),
                                             resource.status);
    appendPrimaryLink(payload, QStringLiteral("打开构建"), resource.webUrl);
    return payload;
}

}
