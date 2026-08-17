#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVector>

struct AzureDevOpsOrganizationInfo {
    QString organizationId;
    QString organizationName;
    QString organizationUrl;
};

struct AzureDevOpsProjectInfo {
    QString projectId;
    QString projectName;
    QString state;
};

struct AzureDevOpsProjectEntry {
    QString projectId;
    QString projectName;
    QString state;
};

struct AzureDevOpsOrganizationEntry {
    QString organizationId;
    QString organizationName;
    QString organizationUrl;
    QVector<AzureDevOpsProjectEntry> projects;
};

struct AzureDevOpsNotificationTarget {
    QString organization;
    QString project;
    bool enabled = true;
    int lastNotifiedBuildId = 0;
    qint64 lastNotifiedPullRequestUpdatedAtMs = 0;
    qint64 lastNotifiedAssignedWorkItemUpdatedAtMs = 0;
    qint64 lastNotifiedMentionCommentAtMs = 0;
    QString lastNotifiedBuildResult;
    qint64 lastPollAttemptAtMs = 0;
    qint64 lastPollSuccessAtMs = 0;
    QString lastPollErrorMessage;
    QString lastPollErrorCategory;
    int consecutivePollFailures = 0;
};

struct AzureDevOpsConnectionSettings {
    bool enabled = false;
    QString baseUrl;
    QString organization;
    QString project;
    QString personalAccessToken;
    QString currentUserId;
    QString currentUserDisplayName;
    QString currentUserUniqueName;
    QString currentUserEmail;
    QVector<AzureDevOpsOrganizationEntry> organizations;
    QVector<AzureDevOpsNotificationTarget> notificationTargets;
    bool notificationsEnabled = false;
    QString notificationConversationId;
    QString notificationConversationTitle;
    int notificationPollIntervalMinutes = 3;
    int lastNotifiedBuildId = 0;
    qint64 lastPollAttemptAtMs = 0;
    qint64 lastPollSuccessAtMs = 0;
    QString lastPollErrorMessage;
    QString lastPollErrorCategory;
    int consecutivePollFailures = 0;

    bool hasCredentialConfiguration() const;
    bool hasCurrentUserIdentity() const;
    bool hasProjectSelection() const;
    bool hasRequiredConfiguration() const;
    bool hasNotificationConfiguration() const;
    QStringList knownOrganizationNames() const;
    QStringList knownProjectsForOrganization(const QString& organizationName) const;
    QStringList currentUserIdentityTokens() const;
    void rememberOrganizations(const QVector<AzureDevOpsOrganizationInfo>& discoveredOrganizations);
    void rememberProjects(const QString& organizationName,
                          const QVector<AzureDevOpsProjectInfo>& discoveredProjects);
    QVector<AzureDevOpsNotificationTarget> enabledNotificationTargets() const;
    void rememberNotificationTarget(const QString& organizationName, const QString& projectName);
    bool isDefaultNotificationTarget(const QString& organizationName, const QString& projectName) const;
    void setNotificationTargetEnabled(const QString& organizationName,
                                      const QString& projectName,
                                      bool enabledValue);
    void setDefaultNotificationTarget(const QString& organizationName, const QString& projectName);
    void normalizeSelection();
};

namespace AzureDevOpsSettingsStore {

AzureDevOpsConnectionSettings load(QSettings* settings = nullptr);
void save(const AzureDevOpsConnectionSettings& config, QSettings* settings = nullptr);
AzureDevOpsConnectionSettings mergePollState(const AzureDevOpsConnectionSettings& latestSettings,
                                             const AzureDevOpsConnectionSettings& polledSettings);
QString formatPollHealthSummary(const AzureDevOpsConnectionSettings& config, int nextPollMinutes);

}  // namespace AzureDevOpsSettingsStore
