#pragma once

#include <optional>

#include <QString>
#include <QVector>

#include "architecture/ResourceReference.h"
#include "domain/ResourceRefPayload.h"

struct DevOpsWorkItemResource {
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString organization;
    QString project;
    QString workItemType;
    QString title;
    QString state;
    QString assignedTo;
    QString webUrl;
    int numericId = 0;
};

struct DevOpsPullRequestResource {
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString organization;
    QString project;
    QString repository;
    QString title;
    QString status;
    QString author;
    QString webUrl;
    int pullRequestId = 0;
};

struct DevOpsBuildResource {
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString organization;
    QString project;
    QString definitionName;
    QString branchName;
    QString status;
    QString requestedBy;
    QString webUrl;
    int buildId = 0;
};

class IDevOpsAdapter {
public:
    virtual ~IDevOpsAdapter() = default;

    virtual QString adapterId() const = 0;
    virtual QString displayName() const = 0;

    virtual QVector<ResourceReference> visibleResourcesForWorkspace(const QString& workspaceId) const = 0;
    virtual std::optional<ResourceRefPayload> payloadForResource(const QString& resourceId) const = 0;
};

namespace DevOpsAdapterContracts {

ResourceReference makeWorkItemReference(const DevOpsWorkItemResource& resource);
ResourceReference makePullRequestReference(const DevOpsPullRequestResource& resource);
ResourceReference makeBuildReference(const DevOpsBuildResource& resource);

ResourceRefPayload makeWorkItemPayload(const DevOpsWorkItemResource& resource);
ResourceRefPayload makePullRequestPayload(const DevOpsPullRequestResource& resource);
ResourceRefPayload makeBuildPayload(const DevOpsBuildResource& resource);

}
