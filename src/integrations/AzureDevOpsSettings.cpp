// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增9行/修改0行/删除0行; 总行数752行
// @AI-LastModified: 2026-04-16 11:01:00

#include "integrations/AzureDevOpsSettings.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "app/AppSettings.h"

namespace {

constexpr auto kAzureDevOpsGroup = "integrations/azureDevOps";
constexpr auto kEnabledKey = "enabled";
constexpr auto kBaseUrlKey = "baseUrl";
constexpr auto kOrganizationKey = "organization";
constexpr auto kProjectKey = "project";
constexpr auto kPatTokenKey = "personalAccessToken";
constexpr auto kCurrentUserIdKey = "currentUserId";
constexpr auto kCurrentUserDisplayNameKey = "currentUserDisplayName";
constexpr auto kCurrentUserUniqueNameKey = "currentUserUniqueName";
constexpr auto kCurrentUserEmailKey = "currentUserEmail";
constexpr auto kOrganizationsJsonKey = "organizationsCatalogJson";
constexpr auto kNotificationTargetsJsonKey = "notificationTargetsJson";
constexpr auto kNotificationsEnabledKey = "notificationsEnabled";
constexpr auto kNotificationConversationIdKey = "notificationConversationId";
constexpr auto kNotificationConversationTitleKey = "notificationConversationTitle";
constexpr auto kNotificationPollIntervalMinutesKey = "notificationPollIntervalMinutes";
constexpr auto kLastNotifiedBuildIdKey = "lastNotifiedBuildId";
constexpr auto kLastPollAttemptAtMsKey = "lastPollAttemptAtMs";
constexpr auto kLastPollSuccessAtMsKey = "lastPollSuccessAtMs";
constexpr auto kLastPollErrorMessageKey = "lastPollErrorMessage";
constexpr auto kLastPollErrorCategoryKey = "lastPollErrorCategory";
constexpr auto kConsecutivePollFailuresKey = "consecutivePollFailures";

QString trimmedOrEmpty(const QString& value)
{
    return value.trimmed();
}

QString chooseFirstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return {};
}

AzureDevOpsProjectEntry projectEntryFromInfo(const AzureDevOpsProjectInfo& info)
{
    AzureDevOpsProjectEntry entry;
    entry.projectId = trimmedOrEmpty(info.projectId);
    entry.projectName = trimmedOrEmpty(info.projectName);
    entry.state = trimmedOrEmpty(info.state);
    return entry;
}

QJsonObject toJson(const AzureDevOpsProjectEntry& entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("projectId"), entry.projectId.trimmed());
    object.insert(QStringLiteral("projectName"), entry.projectName.trimmed());
    object.insert(QStringLiteral("state"), entry.state.trimmed());
    return object;
}

QJsonObject toJson(const AzureDevOpsOrganizationEntry& entry)
{
    QJsonObject object;
    object.insert(QStringLiteral("organizationId"), entry.organizationId.trimmed());
    object.insert(QStringLiteral("organizationName"), entry.organizationName.trimmed());
    object.insert(QStringLiteral("organizationUrl"), entry.organizationUrl.trimmed());
    QJsonArray projects;
    for (const AzureDevOpsProjectEntry& project : entry.projects) {
        if (project.projectName.trimmed().isEmpty()) {
            continue;
        }
        projects.push_back(toJson(project));
    }
    object.insert(QStringLiteral("projects"), projects);
    return object;
}

QJsonObject toJson(const AzureDevOpsNotificationTarget& target)
{
    QJsonObject object;
    object.insert(QStringLiteral("organization"), target.organization.trimmed());
    object.insert(QStringLiteral("project"), target.project.trimmed());
    object.insert(QStringLiteral("enabled"), target.enabled);
    object.insert(QStringLiteral("lastNotifiedBuildId"), qMax(0, target.lastNotifiedBuildId));
    object.insert(QStringLiteral("lastNotifiedPullRequestUpdatedAtMs"),
                  static_cast<double>(qMax<qint64>(0, target.lastNotifiedPullRequestUpdatedAtMs)));
    object.insert(QStringLiteral("lastNotifiedAssignedWorkItemUpdatedAtMs"),
                  static_cast<double>(qMax<qint64>(0, target.lastNotifiedAssignedWorkItemUpdatedAtMs)));
    object.insert(QStringLiteral("lastNotifiedMentionCommentAtMs"),
                  static_cast<double>(qMax<qint64>(0, target.lastNotifiedMentionCommentAtMs)));
    object.insert(QStringLiteral("lastNotifiedBuildResult"),
                  target.lastNotifiedBuildResult.trimmed());
    object.insert(QStringLiteral("lastPollAttemptAtMs"),
                  static_cast<double>(qMax<qint64>(0, target.lastPollAttemptAtMs)));
    object.insert(QStringLiteral("lastPollSuccessAtMs"),
                  static_cast<double>(qMax<qint64>(0, target.lastPollSuccessAtMs)));
    object.insert(QStringLiteral("lastPollErrorMessage"),
                  target.lastPollErrorMessage.trimmed());
    object.insert(QStringLiteral("lastPollErrorCategory"),
                  target.lastPollErrorCategory.trimmed());
    object.insert(QStringLiteral("consecutivePollFailures"),
                  qMax(0, target.consecutivePollFailures));
    return object;
}

AzureDevOpsProjectEntry projectEntryFromJson(const QJsonObject& object)
{
    AzureDevOpsProjectEntry entry;
    entry.projectId = object.value(QStringLiteral("projectId")).toString().trimmed();
    entry.projectName = object.value(QStringLiteral("projectName")).toString().trimmed();
    entry.state = object.value(QStringLiteral("state")).toString().trimmed();
    return entry;
}

AzureDevOpsOrganizationEntry organizationEntryFromJson(const QJsonObject& object)
{
    AzureDevOpsOrganizationEntry entry;
    entry.organizationId = object.value(QStringLiteral("organizationId")).toString().trimmed();
    entry.organizationName = object.value(QStringLiteral("organizationName")).toString().trimmed();
    entry.organizationUrl = object.value(QStringLiteral("organizationUrl")).toString().trimmed();
    for (const QJsonValue& projectValue : object.value(QStringLiteral("projects")).toArray()) {
        if (!projectValue.isObject()) {
            continue;
        }
        AzureDevOpsProjectEntry projectEntry = projectEntryFromJson(projectValue.toObject());
        if (!projectEntry.projectName.trimmed().isEmpty()) {
            entry.projects.push_back(projectEntry);
        }
    }
    return entry;
}

AzureDevOpsNotificationTarget notificationTargetFromJson(const QJsonObject& object)
{
    AzureDevOpsNotificationTarget target;
    target.organization = object.value(QStringLiteral("organization")).toString().trimmed();
    target.project = object.value(QStringLiteral("project")).toString().trimmed();
    target.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    target.lastNotifiedBuildId =
        qMax(0, object.value(QStringLiteral("lastNotifiedBuildId")).toInt());
    target.lastNotifiedPullRequestUpdatedAtMs = qMax<qint64>(
        0,
        static_cast<qint64>(object.value(QStringLiteral("lastNotifiedPullRequestUpdatedAtMs")).toDouble()));
    target.lastNotifiedAssignedWorkItemUpdatedAtMs = qMax<qint64>(
        0,
        static_cast<qint64>(object.value(QStringLiteral("lastNotifiedAssignedWorkItemUpdatedAtMs")).toDouble()));
    target.lastNotifiedMentionCommentAtMs = qMax<qint64>(
        0,
        static_cast<qint64>(object.value(QStringLiteral("lastNotifiedMentionCommentAtMs")).toDouble()));
    target.lastNotifiedBuildResult =
        object.value(QStringLiteral("lastNotifiedBuildResult")).toString().trimmed();
    target.lastPollAttemptAtMs = qMax<qint64>(
        0,
        static_cast<qint64>(object.value(QStringLiteral("lastPollAttemptAtMs")).toDouble()));
    target.lastPollSuccessAtMs = qMax<qint64>(
        0,
        static_cast<qint64>(object.value(QStringLiteral("lastPollSuccessAtMs")).toDouble()));
    target.lastPollErrorMessage =
        object.value(QStringLiteral("lastPollErrorMessage")).toString().trimmed();
    target.lastPollErrorCategory =
        object.value(QStringLiteral("lastPollErrorCategory")).toString().trimmed();
    target.consecutivePollFailures =
        qMax(0, object.value(QStringLiteral("consecutivePollFailures")).toInt());
    return target;
}

QVector<AzureDevOpsOrganizationEntry> organizationsFromJson(const QString& json)
{
    QVector<AzureDevOpsOrganizationEntry> organizations;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        return organizations;
    }

    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        AzureDevOpsOrganizationEntry entry = organizationEntryFromJson(value.toObject());
        if (!entry.organizationName.trimmed().isEmpty()) {
            organizations.push_back(entry);
        }
    }
    return organizations;
}

QString organizationsToJson(const QVector<AzureDevOpsOrganizationEntry>& organizations)
{
    QJsonArray array;
    for (const AzureDevOpsOrganizationEntry& entry : organizations) {
        if (entry.organizationName.trimmed().isEmpty()) {
            continue;
        }
        array.push_back(toJson(entry));
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QVector<AzureDevOpsNotificationTarget> notificationTargetsFromJson(const QString& json)
{
    QVector<AzureDevOpsNotificationTarget> targets;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        return targets;
    }

    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }
        const AzureDevOpsNotificationTarget target = notificationTargetFromJson(value.toObject());
        if (!target.organization.isEmpty() && !target.project.isEmpty()) {
            targets.push_back(target);
        }
    }
    return targets;
}

QString notificationTargetsToJson(const QVector<AzureDevOpsNotificationTarget>& targets)
{
    QJsonArray array;
    for (const AzureDevOpsNotificationTarget& target : targets) {
        if (target.organization.trimmed().isEmpty() || target.project.trimmed().isEmpty()) {
            continue;
        }
        array.push_back(toJson(target));
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

int findOrganizationIndex(const QVector<AzureDevOpsOrganizationEntry>& organizations,
                          const QString& organizationName)
{
    const QString needle = organizationName.trimmed();
    for (int index = 0; index < organizations.size(); ++index) {
        if (organizations.at(index).organizationName.trimmed().compare(needle, Qt::CaseInsensitive) == 0) {
            return index;
        }
    }
    return -1;
}

int findProjectIndex(const QVector<AzureDevOpsProjectEntry>& projects, const QString& projectName)
{
    const QString needle = projectName.trimmed();
    for (int index = 0; index < projects.size(); ++index) {
        if (projects.at(index).projectName.trimmed().compare(needle, Qt::CaseInsensitive) == 0) {
            return index;
        }
    }
    return -1;
}

int findNotificationTargetIndex(const QVector<AzureDevOpsNotificationTarget>& targets,
                                const QString& organizationName,
                                const QString& projectName)
{
    const QString orgNeedle = organizationName.trimmed();
    const QString projectNeedle = projectName.trimmed();
    for (int index = 0; index < targets.size(); ++index) {
        const AzureDevOpsNotificationTarget& target = targets.at(index);
        if (target.organization.trimmed().compare(orgNeedle, Qt::CaseInsensitive) == 0
            && target.project.trimmed().compare(projectNeedle, Qt::CaseInsensitive) == 0) {
            return index;
        }
    }
    return -1;
}

bool sameCredentialContext(const AzureDevOpsConnectionSettings& lhs,
                           const AzureDevOpsConnectionSettings& rhs)
{
    return lhs.baseUrl.trimmed().compare(rhs.baseUrl.trimmed(), Qt::CaseInsensitive) == 0
        && lhs.personalAccessToken.trimmed() == rhs.personalAccessToken.trimmed();
}

void copyPollState(AzureDevOpsNotificationTarget* destination,
                   const AzureDevOpsNotificationTarget& source)
{
    if (!destination) {
        return;
    }
    destination->lastNotifiedBuildId = qMax(0, source.lastNotifiedBuildId);
    destination->lastNotifiedPullRequestUpdatedAtMs =
        qMax<qint64>(0, source.lastNotifiedPullRequestUpdatedAtMs);
    destination->lastNotifiedAssignedWorkItemUpdatedAtMs =
        qMax<qint64>(0, source.lastNotifiedAssignedWorkItemUpdatedAtMs);
    destination->lastNotifiedMentionCommentAtMs =
        qMax<qint64>(0, source.lastNotifiedMentionCommentAtMs);
    destination->lastNotifiedBuildResult = source.lastNotifiedBuildResult.trimmed();
    destination->lastPollAttemptAtMs = qMax<qint64>(0, source.lastPollAttemptAtMs);
    destination->lastPollSuccessAtMs = qMax<qint64>(0, source.lastPollSuccessAtMs);
    destination->lastPollErrorMessage = source.lastPollErrorMessage.trimmed();
    destination->lastPollErrorCategory = source.lastPollErrorCategory.trimmed();
    destination->consecutivePollFailures = qMax(0, source.consecutivePollFailures);
}

void normalizeOrganizations(AzureDevOpsConnectionSettings& config)
{
    for (AzureDevOpsOrganizationEntry& organizationEntry : config.organizations) {
        organizationEntry.organizationName = organizationEntry.organizationName.trimmed();
        organizationEntry.organizationId = organizationEntry.organizationId.trimmed();
        organizationEntry.organizationUrl = organizationEntry.organizationUrl.trimmed();
        QVector<AzureDevOpsProjectEntry> normalizedProjects;
        for (AzureDevOpsProjectEntry projectEntry : organizationEntry.projects) {
            projectEntry.projectId = projectEntry.projectId.trimmed();
            projectEntry.projectName = projectEntry.projectName.trimmed();
            projectEntry.state = projectEntry.state.trimmed();
            if (!projectEntry.projectName.isEmpty()
                && findProjectIndex(normalizedProjects, projectEntry.projectName) < 0) {
                normalizedProjects.push_back(projectEntry);
            }
        }
        organizationEntry.projects = normalizedProjects;
    }

    QVector<AzureDevOpsOrganizationEntry> normalizedOrganizations;
    for (AzureDevOpsOrganizationEntry organizationEntry : config.organizations) {
        if (!organizationEntry.organizationName.isEmpty()
            && findOrganizationIndex(normalizedOrganizations, organizationEntry.organizationName) < 0) {
            normalizedOrganizations.push_back(organizationEntry);
        }
    }
    config.organizations = normalizedOrganizations;

    QVector<AzureDevOpsNotificationTarget> normalizedTargets;
    for (AzureDevOpsNotificationTarget target : config.notificationTargets) {
        target.organization = target.organization.trimmed();
        target.project = target.project.trimmed();
        target.lastNotifiedBuildId = qMax(0, target.lastNotifiedBuildId);
        target.lastNotifiedPullRequestUpdatedAtMs =
            qMax<qint64>(0, target.lastNotifiedPullRequestUpdatedAtMs);
        target.lastNotifiedAssignedWorkItemUpdatedAtMs =
            qMax<qint64>(0, target.lastNotifiedAssignedWorkItemUpdatedAtMs);
        target.lastNotifiedMentionCommentAtMs =
            qMax<qint64>(0, target.lastNotifiedMentionCommentAtMs);
        target.lastNotifiedBuildResult = target.lastNotifiedBuildResult.trimmed();
        target.lastPollAttemptAtMs = qMax<qint64>(0, target.lastPollAttemptAtMs);
        target.lastPollSuccessAtMs = qMax<qint64>(0, target.lastPollSuccessAtMs);
        target.lastPollErrorMessage = target.lastPollErrorMessage.trimmed();
        target.lastPollErrorCategory = target.lastPollErrorCategory.trimmed();
        target.consecutivePollFailures = qMax(0, target.consecutivePollFailures);
        if (target.organization.isEmpty() || target.project.isEmpty()) {
            continue;
        }
        if (findNotificationTargetIndex(normalizedTargets, target.organization, target.project) < 0) {
            normalizedTargets.push_back(target);
        }
    }
    config.notificationTargets = normalizedTargets;

    config.organization = config.organization.trimmed();
    config.project = config.project.trimmed();
    config.currentUserId = config.currentUserId.trimmed();
    config.currentUserDisplayName = config.currentUserDisplayName.trimmed();
    config.currentUserUniqueName = config.currentUserUniqueName.trimmed();
    config.currentUserEmail = config.currentUserEmail.trimmed();

    if (!config.organization.isEmpty()) {
        int organizationIndex = findOrganizationIndex(config.organizations, config.organization);
        if (organizationIndex < 0) {
            AzureDevOpsOrganizationEntry organizationEntry;
            organizationEntry.organizationName = config.organization;
            if (!config.project.isEmpty()) {
                organizationEntry.projects.push_back({QString(), config.project, QString()});
            }
            config.organizations.push_back(organizationEntry);
            organizationIndex = config.organizations.size() - 1;
        } else if (!config.project.isEmpty()) {
            QVector<AzureDevOpsProjectEntry>& projects = config.organizations[organizationIndex].projects;
            if (findProjectIndex(projects, config.project) < 0) {
                projects.push_back({QString(), config.project, QString()});
            }
        }
    } else if (!config.organizations.isEmpty()) {
        config.organization = config.organizations.first().organizationName;
    }

    if (!config.organization.isEmpty()) {
        const int organizationIndex = findOrganizationIndex(config.organizations, config.organization);
        if (organizationIndex >= 0) {
            const auto& projects = config.organizations.at(organizationIndex).projects;
            if (config.project.isEmpty() && !projects.isEmpty()) {
                config.project = projects.first().projectName;
            }
        }
    }
}

AzureDevOpsConnectionSettings loadFromSettings(QSettings& settings)
{
    AzureDevOpsConnectionSettings config;
    settings.beginGroup(QString::fromLatin1(kAzureDevOpsGroup));
    config.enabled = settings.value(QString::fromLatin1(kEnabledKey), false).toBool();
    config.baseUrl = settings.value(QString::fromLatin1(kBaseUrlKey),
                                    QStringLiteral("https://dev.azure.com")).toString().trimmed();
    config.organization = settings.value(QString::fromLatin1(kOrganizationKey)).toString().trimmed();
    config.project = settings.value(QString::fromLatin1(kProjectKey)).toString().trimmed();
    config.personalAccessToken =
        settings.value(QString::fromLatin1(kPatTokenKey)).toString().trimmed();
    config.currentUserId =
        settings.value(QString::fromLatin1(kCurrentUserIdKey)).toString().trimmed();
    config.currentUserDisplayName =
        settings.value(QString::fromLatin1(kCurrentUserDisplayNameKey)).toString().trimmed();
    config.currentUserUniqueName =
        settings.value(QString::fromLatin1(kCurrentUserUniqueNameKey)).toString().trimmed();
    config.currentUserEmail =
        settings.value(QString::fromLatin1(kCurrentUserEmailKey)).toString().trimmed();
    config.organizations = organizationsFromJson(
        settings.value(QString::fromLatin1(kOrganizationsJsonKey)).toString());
    config.notificationTargets = notificationTargetsFromJson(
        settings.value(QString::fromLatin1(kNotificationTargetsJsonKey)).toString());
    config.notificationsEnabled =
        settings.value(QString::fromLatin1(kNotificationsEnabledKey), false).toBool();
    config.notificationConversationId =
        settings.value(QString::fromLatin1(kNotificationConversationIdKey)).toString().trimmed();
    config.notificationConversationTitle =
        settings.value(QString::fromLatin1(kNotificationConversationTitleKey)).toString().trimmed();
    config.notificationPollIntervalMinutes = qMax(
        1,
        settings.value(QString::fromLatin1(kNotificationPollIntervalMinutesKey), 3).toInt());
    config.lastNotifiedBuildId =
        qMax(0, settings.value(QString::fromLatin1(kLastNotifiedBuildIdKey), 0).toInt());
    config.lastPollAttemptAtMs =
        qMax<qint64>(0, settings.value(QString::fromLatin1(kLastPollAttemptAtMsKey), 0).toLongLong());
    config.lastPollSuccessAtMs =
        qMax<qint64>(0, settings.value(QString::fromLatin1(kLastPollSuccessAtMsKey), 0).toLongLong());
    config.lastPollErrorMessage =
        settings.value(QString::fromLatin1(kLastPollErrorMessageKey)).toString().trimmed();
    config.lastPollErrorCategory =
        settings.value(QString::fromLatin1(kLastPollErrorCategoryKey)).toString().trimmed();
    config.consecutivePollFailures =
        qMax(0, settings.value(QString::fromLatin1(kConsecutivePollFailuresKey), 0).toInt());
    settings.endGroup();
    normalizeOrganizations(config);
    return config;
}

void saveToSettings(const AzureDevOpsConnectionSettings& rawConfig, QSettings& settings)
{
    AzureDevOpsConnectionSettings config = rawConfig;
    normalizeOrganizations(config);
    settings.beginGroup(QString::fromLatin1(kAzureDevOpsGroup));
    settings.setValue(QString::fromLatin1(kEnabledKey), config.enabled);
    settings.setValue(QString::fromLatin1(kBaseUrlKey), config.baseUrl.trimmed());
    settings.setValue(QString::fromLatin1(kOrganizationKey), config.organization.trimmed());
    settings.setValue(QString::fromLatin1(kProjectKey), config.project.trimmed());
    settings.setValue(QString::fromLatin1(kPatTokenKey), config.personalAccessToken.trimmed());
    settings.setValue(QString::fromLatin1(kCurrentUserIdKey), config.currentUserId.trimmed());
    settings.setValue(QString::fromLatin1(kCurrentUserDisplayNameKey),
                      config.currentUserDisplayName.trimmed());
    settings.setValue(QString::fromLatin1(kCurrentUserUniqueNameKey),
                      config.currentUserUniqueName.trimmed());
    settings.setValue(QString::fromLatin1(kCurrentUserEmailKey), config.currentUserEmail.trimmed());
    settings.setValue(QString::fromLatin1(kOrganizationsJsonKey), organizationsToJson(config.organizations));
    settings.setValue(QString::fromLatin1(kNotificationTargetsJsonKey),
                      notificationTargetsToJson(config.notificationTargets));
    settings.setValue(QString::fromLatin1(kNotificationsEnabledKey), config.notificationsEnabled);
    settings.setValue(QString::fromLatin1(kNotificationConversationIdKey),
                      config.notificationConversationId.trimmed());
    settings.setValue(QString::fromLatin1(kNotificationConversationTitleKey),
                      config.notificationConversationTitle.trimmed());
    settings.setValue(QString::fromLatin1(kNotificationPollIntervalMinutesKey),
                      qMax(1, config.notificationPollIntervalMinutes));
    settings.setValue(QString::fromLatin1(kLastNotifiedBuildIdKey),
                      qMax(0, config.lastNotifiedBuildId));
    settings.setValue(QString::fromLatin1(kLastPollAttemptAtMsKey),
                      qMax<qint64>(0, config.lastPollAttemptAtMs));
    settings.setValue(QString::fromLatin1(kLastPollSuccessAtMsKey),
                      qMax<qint64>(0, config.lastPollSuccessAtMs));
    settings.setValue(QString::fromLatin1(kLastPollErrorMessageKey),
                      config.lastPollErrorMessage.trimmed());
    settings.setValue(QString::fromLatin1(kLastPollErrorCategoryKey),
                      config.lastPollErrorCategory.trimmed());
    settings.setValue(QString::fromLatin1(kConsecutivePollFailuresKey),
                      qMax(0, config.consecutivePollFailures));
    settings.endGroup();
    settings.sync();
}

}  // namespace

bool AzureDevOpsConnectionSettings::hasCredentialConfiguration() const
{
    return !baseUrl.trimmed().isEmpty() && !personalAccessToken.trimmed().isEmpty();
}

bool AzureDevOpsConnectionSettings::hasCurrentUserIdentity() const
{
    return !currentUserId.trimmed().isEmpty()
        || !currentUserDisplayName.trimmed().isEmpty()
        || !currentUserUniqueName.trimmed().isEmpty()
        || !currentUserEmail.trimmed().isEmpty();
}

bool AzureDevOpsConnectionSettings::hasProjectSelection() const
{
    return !organization.trimmed().isEmpty() && !project.trimmed().isEmpty();
}

bool AzureDevOpsConnectionSettings::hasRequiredConfiguration() const
{
    return enabled && hasCredentialConfiguration() && hasProjectSelection();
}

bool AzureDevOpsConnectionSettings::hasNotificationConfiguration() const
{
    return enabled
        && hasCredentialConfiguration()
        && notificationsEnabled
        && (!enabledNotificationTargets().isEmpty() || hasProjectSelection())
        && notificationPollIntervalMinutes > 0;
}

QStringList AzureDevOpsConnectionSettings::knownOrganizationNames() const
{
    QStringList names;
    for (const AzureDevOpsOrganizationEntry& organizationEntry : organizations) {
        const QString name = organizationEntry.organizationName.trimmed();
        if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive)) {
            names.push_back(name);
        }
    }
    if (!organization.trimmed().isEmpty() && !names.contains(organization.trimmed(), Qt::CaseInsensitive)) {
        names.push_back(organization.trimmed());
    }
    return names;
}

QStringList AzureDevOpsConnectionSettings::knownProjectsForOrganization(const QString& organizationName) const
{
    QStringList names;
    const int organizationIndex = findOrganizationIndex(organizations, organizationName);
    if (organizationIndex >= 0) {
        for (const AzureDevOpsProjectEntry& projectEntry : organizations.at(organizationIndex).projects) {
            const QString projectName = projectEntry.projectName.trimmed();
            if (!projectName.isEmpty() && !names.contains(projectName, Qt::CaseInsensitive)) {
                names.push_back(projectName);
            }
        }
    }
    if (organization.trimmed().compare(organizationName.trimmed(), Qt::CaseInsensitive) == 0
        && !project.trimmed().isEmpty()
        && !names.contains(project.trimmed(), Qt::CaseInsensitive)) {
        names.push_back(project.trimmed());
    }
    return names;
}

QStringList AzureDevOpsConnectionSettings::currentUserIdentityTokens() const
{
    QStringList tokens;
    const auto pushToken = [&tokens](const QString& value) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !tokens.contains(trimmed, Qt::CaseInsensitive)) {
            tokens.push_back(trimmed);
        }
    };

    pushToken(currentUserId);
    pushToken(currentUserDisplayName);
    pushToken(currentUserUniqueName);
    pushToken(currentUserEmail);

    const QString email = currentUserEmail.trimmed();
    const int atIndex = email.indexOf(QLatin1Char('@'));
    if (atIndex > 0) {
        pushToken(email.left(atIndex));
    }

    const QString uniqueName = currentUserUniqueName.trimmed();
    const int uniqueAtIndex = uniqueName.indexOf(QLatin1Char('@'));
    if (uniqueAtIndex > 0) {
        pushToken(uniqueName.left(uniqueAtIndex));
    }

    return tokens;
}

void AzureDevOpsConnectionSettings::rememberOrganizations(
    const QVector<AzureDevOpsOrganizationInfo>& discoveredOrganizations)
{
    for (const AzureDevOpsOrganizationInfo& organizationInfo : discoveredOrganizations) {
        const QString organizationName = organizationInfo.organizationName.trimmed();
        if (organizationName.isEmpty()) {
            continue;
        }
        const int organizationIndex = findOrganizationIndex(organizations, organizationName);
        if (organizationIndex >= 0) {
            AzureDevOpsOrganizationEntry& organizationEntry = organizations[organizationIndex];
            organizationEntry.organizationId = chooseFirstNonEmpty({organizationInfo.organizationId, organizationEntry.organizationId});
            organizationEntry.organizationUrl = chooseFirstNonEmpty({organizationInfo.organizationUrl, organizationEntry.organizationUrl});
        } else {
            AzureDevOpsOrganizationEntry organizationEntry;
            organizationEntry.organizationId = organizationInfo.organizationId.trimmed();
            organizationEntry.organizationName = organizationName;
            organizationEntry.organizationUrl = organizationInfo.organizationUrl.trimmed();
            organizations.push_back(organizationEntry);
        }
    }
    normalizeSelection();
}

void AzureDevOpsConnectionSettings::rememberProjects(
    const QString& organizationName,
    const QVector<AzureDevOpsProjectInfo>& discoveredProjects)
{
    const QString normalizedOrganizationName = organizationName.trimmed();
    if (normalizedOrganizationName.isEmpty()) {
        return;
    }
    int organizationIndex = findOrganizationIndex(organizations, normalizedOrganizationName);
    if (organizationIndex < 0) {
        AzureDevOpsOrganizationEntry organizationEntry;
        organizationEntry.organizationName = normalizedOrganizationName;
        organizations.push_back(organizationEntry);
        organizationIndex = organizations.size() - 1;
    }

    QVector<AzureDevOpsProjectEntry> rememberedProjects;
    for (const AzureDevOpsProjectInfo& projectInfo : discoveredProjects) {
        const AzureDevOpsProjectEntry entry = projectEntryFromInfo(projectInfo);
        if (!entry.projectName.trimmed().isEmpty()
            && findProjectIndex(rememberedProjects, entry.projectName) < 0) {
            rememberedProjects.push_back(entry);
        }
    }
    organizations[organizationIndex].projects = rememberedProjects;
    normalizeSelection();
}

QVector<AzureDevOpsNotificationTarget> AzureDevOpsConnectionSettings::enabledNotificationTargets() const
{
    QVector<AzureDevOpsNotificationTarget> targets;
    for (const AzureDevOpsNotificationTarget& target : notificationTargets) {
        if (target.enabled
            && !target.organization.trimmed().isEmpty()
            && !target.project.trimmed().isEmpty()) {
            targets.push_back(target);
        }
    }

    if (targets.isEmpty() && !organization.trimmed().isEmpty() && !project.trimmed().isEmpty()) {
        AzureDevOpsNotificationTarget fallback;
        fallback.organization = organization.trimmed();
        fallback.project = project.trimmed();
        fallback.enabled = true;
        fallback.lastNotifiedBuildId = qMax(0, lastNotifiedBuildId);
        targets.push_back(fallback);
    }

    return targets;
}

void AzureDevOpsConnectionSettings::rememberNotificationTarget(const QString& organizationName,
                                                               const QString& projectName)
{
    const QString org = organizationName.trimmed();
    const QString proj = projectName.trimmed();
    if (org.isEmpty() || proj.isEmpty()) {
        return;
    }

    const int existingIndex = findNotificationTargetIndex(notificationTargets, org, proj);
    if (existingIndex >= 0) {
        notificationTargets[existingIndex].enabled = true;
        return;
    }

    AzureDevOpsNotificationTarget target;
    target.organization = org;
    target.project = proj;
    target.enabled = true;
    notificationTargets.push_back(target);
    normalizeSelection();
}

bool AzureDevOpsConnectionSettings::isDefaultNotificationTarget(const QString& organizationName,
                                                                const QString& projectName) const
{
    return organization.trimmed().compare(organizationName.trimmed(), Qt::CaseInsensitive) == 0
        && project.trimmed().compare(projectName.trimmed(), Qt::CaseInsensitive) == 0;
}

void AzureDevOpsConnectionSettings::setNotificationTargetEnabled(const QString& organizationName,
                                                                 const QString& projectName,
                                                                 bool enabledValue)
{
    const int existingIndex =
        findNotificationTargetIndex(notificationTargets, organizationName, projectName);
    if (existingIndex < 0) {
        if (!enabledValue) {
            return;
        }
        rememberNotificationTarget(organizationName, projectName);
        return;
    }

    notificationTargets[existingIndex].enabled = enabledValue;
    normalizeSelection();
}

void AzureDevOpsConnectionSettings::setDefaultNotificationTarget(const QString& organizationName,
                                                                 const QString& projectName)
{
    const QString org = organizationName.trimmed();
    const QString proj = projectName.trimmed();
    if (org.isEmpty() || proj.isEmpty()) {
        return;
    }

    rememberNotificationTarget(org, proj);
    organization = org;
    project = proj;
    normalizeSelection();
}

void AzureDevOpsConnectionSettings::normalizeSelection()
{
    normalizeOrganizations(*this);
}

namespace AzureDevOpsSettingsStore {

AzureDevOpsConnectionSettings load(QSettings* settings)
{
    if (settings) {
        return loadFromSettings(*settings);
    }
    QSettings ownedSettings = AppSettings::createSettings();
    return loadFromSettings(ownedSettings);
}

void save(const AzureDevOpsConnectionSettings& config, QSettings* settings)
{
    if (settings) {
        saveToSettings(config, *settings);
        return;
    }
    QSettings ownedSettings = AppSettings::createSettings();
    saveToSettings(config, ownedSettings);
}

AzureDevOpsConnectionSettings mergePollState(const AzureDevOpsConnectionSettings& latestSettings,
                                             const AzureDevOpsConnectionSettings& polledSettings)
{
    AzureDevOpsConnectionSettings merged = latestSettings;
    merged.lastNotifiedBuildId = qMax(0, polledSettings.lastNotifiedBuildId);
    merged.lastPollAttemptAtMs = qMax<qint64>(0, polledSettings.lastPollAttemptAtMs);
    merged.lastPollSuccessAtMs = qMax<qint64>(0, polledSettings.lastPollSuccessAtMs);
    merged.lastPollErrorMessage = polledSettings.lastPollErrorMessage.trimmed();
    merged.lastPollErrorCategory = polledSettings.lastPollErrorCategory.trimmed();
    merged.consecutivePollFailures = qMax(0, polledSettings.consecutivePollFailures);

    if (sameCredentialContext(latestSettings, polledSettings)) {
        if (!polledSettings.currentUserId.trimmed().isEmpty()) {
            merged.currentUserId = polledSettings.currentUserId.trimmed();
        }
        if (!polledSettings.currentUserDisplayName.trimmed().isEmpty()) {
            merged.currentUserDisplayName = polledSettings.currentUserDisplayName.trimmed();
        }
        if (!polledSettings.currentUserUniqueName.trimmed().isEmpty()) {
            merged.currentUserUniqueName = polledSettings.currentUserUniqueName.trimmed();
        }
        if (!polledSettings.currentUserEmail.trimmed().isEmpty()) {
            merged.currentUserEmail = polledSettings.currentUserEmail.trimmed();
        }
        if (merged.organizations.isEmpty() && !polledSettings.organizations.isEmpty()) {
            merged.organizations = polledSettings.organizations;
        }
    }

    for (AzureDevOpsNotificationTarget& target : merged.notificationTargets) {
        const int sourceIndex = findNotificationTargetIndex(polledSettings.notificationTargets,
                                                            target.organization,
                                                            target.project);
        if (sourceIndex < 0) {
            continue;
        }
        copyPollState(&target, polledSettings.notificationTargets.at(sourceIndex));
    }

    normalizeOrganizations(merged);
    return merged;
}

QString formatPollHealthSummary(const AzureDevOpsConnectionSettings& config, int nextPollMinutes)
{
    const auto formatTime = [](qint64 ms) {
        return ms > 0
            ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("暂无");
    };
    QString categoryText = QStringLiteral("无");
    if (config.lastPollErrorCategory == QStringLiteral("auth")) {
        categoryText = QStringLiteral("认证");
    } else if (config.lastPollErrorCategory == QStringLiteral("network")) {
        categoryText = QStringLiteral("网络");
    } else if (!config.lastPollErrorCategory.trimmed().isEmpty()) {
        categoryText = QStringLiteral("其他");
    }
    return QStringLiteral(
               "最近成功：%1\n最近失败：%2\n错误类型：%3\n连续失败：%4 次\n下次轮询：约 %5 分钟后")
        .arg(formatTime(config.lastPollSuccessAtMs),
             config.lastPollErrorMessage.trimmed().isEmpty()
                 ? QStringLiteral("无")
                 : config.lastPollErrorMessage.trimmed(),
             categoryText,
             QString::number(qMax(0, config.consecutivePollFailures)),
             QString::number(qMax(1, nextPollMinutes)));
}

}  // namespace AzureDevOpsSettingsStore
