#pragma once

#include <memory>
#include <optional>
#include <QVector>

#include <QString>

#include "integrations/AzureDevOpsNotificationContracts.h"
#include "integrations/AzureDevOpsSettings.h"
#include "integrations/LocalAzureDevOpsAdapter.h"

class AzureDevOpsBuildNotificationPoller {
public:
    explicit AzureDevOpsBuildNotificationPoller(
        AzureDevOpsConnectionSettings settings = {},
        std::shared_ptr<IAzureDevOpsApiTransport> transport = std::make_shared<NetworkAzureDevOpsApiTransport>());

    std::optional<AzureDevOpsNotificationEvent> pollLatestBuild(int lastSeenBuildId,
                                                                int* latestObservedBuildId,
                                                                QString* errorMessage) const;
    std::optional<AzureDevOpsNotificationEvent> pollLatestPullRequest(
        qint64 lastSeenUpdatedAtMs,
        qint64* latestObservedUpdatedAtMs,
        QString* errorMessage) const;
    std::optional<AzureDevOpsNotificationEvent> pollLatestAssignedWorkItem(
        qint64 lastSeenUpdatedAtMs,
        qint64* latestObservedUpdatedAtMs,
        QString* errorMessage) const;
    std::optional<AzureDevOpsNotificationEvent> pollLatestMentionComment(
        qint64 lastSeenUpdatedAtMs,
        qint64* latestObservedUpdatedAtMs,
        QString* errorMessage) const;
    QVector<AzureDevOpsNotificationEvent> pollTrackedBuilds(AzureDevOpsConnectionSettings* settings,
                                                            QString* errorMessage) const;
    QVector<AzureDevOpsNotificationEvent> pollTrackedNotifications(AzureDevOpsConnectionSettings* settings,
                                                                   QString* errorMessage) const;

private:
    AzureDevOpsConnectionSettings m_settings;
    std::shared_ptr<IAzureDevOpsApiTransport> m_transport;
};
