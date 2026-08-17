#include "services/AzureDevOpsBuildNotificationPoller.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QDebug>

namespace {

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

QString stringAtPath(const QJsonObject& object, std::initializer_list<QStringView> path)
{
    QJsonValue current(object);
    for (const QStringView part : path) {
        if (!current.isObject()) {
            return {};
        }
        current = current.toObject().value(part.toString());
    }

    if (current.isString()) {
        return current.toString().trimmed();
    }
    if (current.isDouble()) {
        return QString::number(static_cast<qint64>(current.toDouble()));
    }
    if (current.isObject()) {
        const QJsonObject nested = current.toObject();
        const QString displayName = nested.value(QStringLiteral("displayName")).toString().trimmed();
        if (!displayName.isEmpty()) {
            return displayName;
        }
        const QString name = nested.value(QStringLiteral("name")).toString().trimmed();
        if (!name.isEmpty()) {
            return name;
        }
        const QString id = nested.value(QStringLiteral("id")).toString().trimmed();
        if (!id.isEmpty()) {
            return id;
        }
    }
    return {};
}

QString normalizedBaseUrl(const AzureDevOpsConnectionSettings& settings)
{
    QString value = settings.baseUrl.trimmed();
    if (value.isEmpty()) {
        value = QStringLiteral("https://dev.azure.com");
    }
    while (value.endsWith(QLatin1Char('/'))) {
        value.chop(1);
    }
    return value;
}

QUrl latestBuildsUrl(const AzureDevOpsConnectionSettings& settings)
{
    // 限制只查询最近 7 天完成的构建，避免服务端扫描全量记录导致超时
    const QString minFinishTime = QDateTime::currentDateTimeUtc()
                                      .addDays(-7)
                                      .toString(Qt::ISODate);
    return QUrl(QStringLiteral("%1/%2/%3/_apis/build/builds?queryOrder=finishTimeDescending&$top=1&minFinishTime=%4&api-version=7.0")
                    .arg(normalizedBaseUrl(settings),
                         settings.organization.trimmed(),
                         settings.project.trimmed(),
                         minFinishTime));
}

QUrl pullRequestsByReviewerUrl(const AzureDevOpsConnectionSettings& settings)
{
    return QUrl(QStringLiteral("%1/%2/%3/_apis/git/pullrequests?searchCriteria.status=active&searchCriteria.reviewerId=%4&$top=10&api-version=7.0")
                    .arg(normalizedBaseUrl(settings),
                         settings.organization.trimmed(),
                         settings.project.trimmed(),
                         settings.currentUserId.trimmed()));
}

QUrl pullRequestsByCreatorUrl(const AzureDevOpsConnectionSettings& settings)
{
    return QUrl(QStringLiteral("%1/%2/%3/_apis/git/pullrequests?searchCriteria.status=active&searchCriteria.creatorId=%4&$top=10&api-version=7.0")
                    .arg(normalizedBaseUrl(settings),
                         settings.organization.trimmed(),
                         settings.project.trimmed(),
                         settings.currentUserId.trimmed()));
}

QUrl wiqlAssignedToMeUrl(const AzureDevOpsConnectionSettings& settings)
{
    return QUrl(QStringLiteral("%1/%2/%3/_apis/wit/wiql?api-version=7.0")
                    .arg(normalizedBaseUrl(settings),
                         settings.organization.trimmed(),
                         settings.project.trimmed()));
}

QUrl workItemDetailsUrl(const AzureDevOpsConnectionSettings& settings, const QString& workItemId)
{
    return QUrl(QStringLiteral("%1/%2/%3/_apis/wit/workitems/%4?api-version=7.0")
                    .arg(normalizedBaseUrl(settings),
                         settings.organization.trimmed(),
                         settings.project.trimmed(),
                         workItemId.trimmed()));
}

QUrl workItemUpdatesUrl(const AzureDevOpsConnectionSettings& settings, const QString& workItemId)
{
    return QUrl(QStringLiteral("%1/%2/%3/_apis/wit/workitems/%4/updates?$top=10&api-version=7.0")
                    .arg(normalizedBaseUrl(settings),
                         settings.organization.trimmed(),
                         settings.project.trimmed(),
                         workItemId.trimmed()));
}

qint64 parseIsoDateMs(const QString& value)
{
    const QDateTime dateTime = QDateTime::fromString(value.trimmed(), Qt::ISODate);
    return dateTime.isValid() ? dateTime.toMSecsSinceEpoch() : 0;
}

QString normalizedResult(const QString& value)
{
    return value.trimmed().toLower();
}

bool isFailureResult(const QString& value)
{
    const QString token = normalizedResult(value);
    return token == QStringLiteral("failed")
        || token == QStringLiteral("partiallysucceeded")
        || token == QStringLiteral("canceled")
        || token == QStringLiteral("cancelled");
}

bool isSuccessResult(const QString& value)
{
    const QString token = normalizedResult(value);
    return token == QStringLiteral("succeeded")
        || token == QStringLiteral("success");
}

bool hasPendingReviewRequest(const QJsonObject& pullRequestObject)
{
    const QJsonArray reviewers = pullRequestObject.value(QStringLiteral("reviewers")).toArray();
    for (const QJsonValue& reviewerValue : reviewers) {
        if (!reviewerValue.isObject()) {
            continue;
        }
        const QJsonObject reviewer = reviewerValue.toObject();
        const bool isRequired = reviewer.value(QStringLiteral("isRequired")).toBool(false);
        const int vote = reviewer.value(QStringLiteral("vote")).toInt(0);
        const bool pendingVote = vote == 0 || vote == 5;
        if (isRequired && pendingVote) {
            return true;
        }
    }
    return false;
}

QString pendingReviewerSummary(const QJsonObject& pullRequestObject)
{
    QStringList reviewerNames;
    const QJsonArray reviewers = pullRequestObject.value(QStringLiteral("reviewers")).toArray();
    for (const QJsonValue& reviewerValue : reviewers) {
        if (!reviewerValue.isObject()) {
            continue;
        }
        const QJsonObject reviewer = reviewerValue.toObject();
        const bool isRequired = reviewer.value(QStringLiteral("isRequired")).toBool(false);
        const int vote = reviewer.value(QStringLiteral("vote")).toInt(0);
        const bool pendingVote = vote == 0 || vote == 5;
        if (!isRequired || !pendingVote) {
            continue;
        }
        const QString reviewerName = chooseFirstNonEmpty({
            stringAtPath(reviewer, {QStringLiteral("displayName")}),
            stringAtPath(reviewer, {QStringLiteral("uniqueName")}),
            stringAtPath(reviewer, {QStringLiteral("name")}),
        });
        if (!reviewerName.isEmpty()) {
            reviewerNames.push_back(reviewerName);
        }
    }
    reviewerNames.removeDuplicates();
    if (reviewerNames.isEmpty()) {
        return QStringLiteral("需要处理 PR 审核请求");
    }
    return QStringLiteral("等待 %1 审阅").arg(reviewerNames.join(QStringLiteral("、")));
}

QString classifyPollErrorCategory(const QString& errorMessage)
{
    const QString normalized = errorMessage.trimmed().toLower();
    if (normalized.isEmpty()) {
        return {};
    }
    for (const QString& token : {QStringLiteral("401"),
                                 QStringLiteral("403"),
                                 QStringLiteral("unauthorized"),
                                 QStringLiteral("forbidden"),
                                 QStringLiteral("pat"),
                                 QStringLiteral("token"),
                                 QStringLiteral("auth"),
                                 QStringLiteral("login"),
                                 QStringLiteral("expired")}) {
        if (normalized.contains(token)) {
            return QStringLiteral("auth");
        }
    }
    for (const QString& token : {QStringLiteral("timeout"),
                                 QStringLiteral("timed out"),
                                 QStringLiteral("network"),
                                 QStringLiteral("host"),
                                 QStringLiteral("ssl"),
                                 QStringLiteral("refused"),
                                 QStringLiteral("unreachable"),
                                 QStringLiteral("temporary"),
                                 QStringLiteral("connection")}) {
        if (normalized.contains(token)) {
            return QStringLiteral("network");
        }
    }
    return QStringLiteral("unknown");
}

void updateSettingsPollHealthFromTargets(AzureDevOpsConnectionSettings* settings,
                                         const QVector<AzureDevOpsNotificationTarget>& persistedTargets)
{
    if (!settings) {
        return;
    }

    settings->lastPollAttemptAtMs = 0;
    settings->lastPollSuccessAtMs = 0;
    settings->lastPollErrorMessage.clear();
    settings->lastPollErrorCategory.clear();
    settings->consecutivePollFailures = 0;

    const auto matchesDefaultTarget = [settings](const AzureDevOpsNotificationTarget& target) {
        return target.organization.trimmed().compare(settings->organization.trimmed(), Qt::CaseInsensitive) == 0
            && target.project.trimmed().compare(settings->project.trimmed(), Qt::CaseInsensitive) == 0;
    };

    const AzureDevOpsNotificationTarget* preferredFailure = nullptr;
    const AzureDevOpsNotificationTarget* fallbackFailure = nullptr;
    for (const AzureDevOpsNotificationTarget& target : persistedTargets) {
        settings->lastPollAttemptAtMs = qMax(settings->lastPollAttemptAtMs, target.lastPollAttemptAtMs);
        settings->lastPollSuccessAtMs = qMax(settings->lastPollSuccessAtMs, target.lastPollSuccessAtMs);
        if (target.lastPollErrorMessage.trimmed().isEmpty()) {
            continue;
        }
        if (matchesDefaultTarget(target)) {
            preferredFailure = &target;
            break;
        }
        if (!fallbackFailure
            || target.consecutivePollFailures > fallbackFailure->consecutivePollFailures
            || (target.consecutivePollFailures == fallbackFailure->consecutivePollFailures
                && target.lastPollAttemptAtMs > fallbackFailure->lastPollAttemptAtMs)) {
            fallbackFailure = &target;
        }
    }

    const AzureDevOpsNotificationTarget* chosenFailure =
        preferredFailure ? preferredFailure : fallbackFailure;
    if (!chosenFailure) {
        return;
    }

    settings->lastPollErrorMessage = chosenFailure->lastPollErrorMessage.trimmed();
    settings->lastPollErrorCategory = chosenFailure->lastPollErrorCategory.trimmed();
    settings->consecutivePollFailures = qMax(0, chosenFailure->consecutivePollFailures);
}

struct WorkItemUpdateSummary {
    AzureDevOpsNotificationKind kind = AzureDevOpsNotificationKind::WorkItemAssignedToMe;
    QString summary;
    QString actor;
};

WorkItemUpdateSummary summarizeWorkItemUpdateV2(const QJsonObject& updateObject)
{
    WorkItemUpdateSummary summary;
    summary.kind = AzureDevOpsNotificationKind::WorkItemAssignedToMe;
    summary.actor = chooseFirstNonEmpty({
        stringAtPath(updateObject, {QStringLiteral("revisedBy"), QStringLiteral("displayName")}),
        stringAtPath(updateObject, {QStringLiteral("revisedBy"), QStringLiteral("name")}),
    });

    const QJsonObject fields = updateObject.value(QStringLiteral("fields")).toObject();
    const QJsonObject stateChange = fields.value(QStringLiteral("System.State")).toObject();
    const QString oldState = chooseFirstNonEmpty({
        stateChange.value(QStringLiteral("oldValue")).toString(),
        stringAtPath(stateChange, {QStringLiteral("oldValue")}),
    });
    const QString newState = chooseFirstNonEmpty({
        stateChange.value(QStringLiteral("newValue")).toString(),
        stringAtPath(stateChange, {QStringLiteral("newValue")}),
    });
    if (!oldState.isEmpty() || !newState.isEmpty()) {
        summary.kind = AzureDevOpsNotificationKind::WorkItemUpdated;
        summary.summary = (!oldState.isEmpty() && !newState.isEmpty())
            ? QStringLiteral("工作项状态变化 %1 -> %2").arg(oldState, newState)
            : QStringLiteral("工作项状态有更新");
        return summary;
    }

    const QJsonObject assignmentChange = fields.value(QStringLiteral("System.AssignedTo")).toObject();
    const QString assignedTo = chooseFirstNonEmpty({
        stringAtPath(assignmentChange, {QStringLiteral("newValue"), QStringLiteral("displayName")}),
        stringAtPath(assignmentChange, {QStringLiteral("newValue"), QStringLiteral("name")}),
        assignmentChange.value(QStringLiteral("newValue")).toString(),
    });
    if (!assignedTo.isEmpty()) {
        summary.kind = AzureDevOpsNotificationKind::WorkItemAssignedToMe;
        summary.summary = QStringLiteral("工作项已分配给你");
        return summary;
    }

    const QJsonObject historyChange = fields.value(QStringLiteral("System.History")).toObject();
    // 只检查 newValue：有新评论时 newValue 包含用户输入的 HTML 文本；
    // oldValue 代表历史记录被修改，不是新增评论。
    const QString historyNewValue =
        historyChange.value(QStringLiteral("newValue")).toString().trimmed();
    // 过滤掉空白或仅含 HTML 标签的内容（系统自动操作留下的空审计痕迹）
    const QString strippedHistory = QString(historyNewValue)
        .remove(QRegularExpression(QStringLiteral("<[^>]*>")))
        .trimmed();
    if (!strippedHistory.isEmpty()) {
        const bool containsMention = historyNewValue.contains(QLatin1Char('@'));
        summary.kind = containsMention
            ? AzureDevOpsNotificationKind::WorkItemMentioned
            : AzureDevOpsNotificationKind::WorkItemCommented;
        summary.summary = containsMention
            ? QStringLiteral("评论中提及了你")
            : QStringLiteral("工作项有新的评论");
        return summary;
    }

    // Fallback：有人修改了工作项但不是评论、状态变更或分配变更
    summary.kind = AzureDevOpsNotificationKind::WorkItemUpdated;
    summary.summary = !summary.actor.trimmed().isEmpty()
        ? QStringLiteral("工作项有更新")
        : QStringLiteral("工作项需要你关注");
    return summary;
}

}  // namespace

AzureDevOpsBuildNotificationPoller::AzureDevOpsBuildNotificationPoller(
    AzureDevOpsConnectionSettings settings,
    std::shared_ptr<IAzureDevOpsApiTransport> transport)
    : m_settings(std::move(settings))
    , m_transport(std::move(transport))
{
}

std::optional<AzureDevOpsNotificationEvent> AzureDevOpsBuildNotificationPoller::pollLatestBuild(
    int lastSeenBuildId,
    int* latestObservedBuildId,
    QString* errorMessage) const
{
    if (latestObservedBuildId) {
        *latestObservedBuildId = qMax(0, lastSeenBuildId);
    }

    if (!m_settings.hasNotificationConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 自动通知配置不完整");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return std::nullopt;
    }

    const auto document = m_transport->getJson(latestBuildsUrl(m_settings), m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        return std::nullopt;
    }

    const QJsonArray items = document->object().value(QStringLiteral("value")).toArray();
    if (items.isEmpty() || !items.first().isObject()) {
        return std::nullopt;
    }

    const QJsonObject buildObject = items.first().toObject();
    const int buildId = stringAtPath(buildObject, {QStringLiteral("id")}).toInt();
    if (latestObservedBuildId) {
        *latestObservedBuildId = qMax(buildId, qMax(0, lastSeenBuildId));
    }
    if (buildId <= 0 || buildId <= lastSeenBuildId) {
        if (buildId > 0) {
            qInfo().noquote() << QStringLiteral("[notifications][azure-devops] dedupe skip build %1")
                                     .arg(QString::number(buildId));
        }
        return std::nullopt;
    }

    AzureDevOpsNotificationEvent event;
    event.kind = AzureDevOpsNotificationKind::BuildCompleted;
    event.serviceId = QStringLiteral("local-azure-devops");
    event.workspaceId = QStringLiteral("local-devops");
    event.resourceId = QStringLiteral("build:%1").arg(buildId);
    event.title = chooseFirstNonEmpty({
        stringAtPath(buildObject, {QStringLiteral("definition"), QStringLiteral("name")}),
        stringAtPath(buildObject, {QStringLiteral("buildNumber")}),
        QStringLiteral("%1 构建通知").arg(m_settings.project.trimmed()),
    });
    event.summary = chooseFirstNonEmpty({
        stringAtPath(buildObject, {QStringLiteral("buildNumber")}),
        stringAtPath(buildObject, {QStringLiteral("sourceBranch")}),
        QStringLiteral("项目有新的构建结果"),
    });
    event.status = chooseFirstNonEmpty({
        stringAtPath(buildObject, {QStringLiteral("result")}),
        stringAtPath(buildObject, {QStringLiteral("status")}),
        QStringLiteral("completed"),
    });
    event.webUrl = chooseFirstNonEmpty({
        stringAtPath(buildObject, {QStringLiteral("_links"), QStringLiteral("web"), QStringLiteral("href")}),
        QStringLiteral("%1/%2/%3/_build/results?buildId=%4")
            .arg(normalizedBaseUrl(m_settings),
                 m_settings.organization.trimmed(),
                 m_settings.project.trimmed(),
                 QString::number(buildId)),
    });
    event.actor = chooseFirstNonEmpty({
        stringAtPath(buildObject, {QStringLiteral("requestedFor"), QStringLiteral("displayName")}),
        stringAtPath(buildObject, {QStringLiteral("requestedBy"), QStringLiteral("displayName")}),
        QStringLiteral("Azure DevOps"),
    });
    return event;
}

std::optional<AzureDevOpsNotificationEvent> AzureDevOpsBuildNotificationPoller::pollLatestPullRequest(
    qint64 lastSeenUpdatedAtMs,
    qint64* latestObservedUpdatedAtMs,
    QString* errorMessage) const
{
    if (latestObservedUpdatedAtMs) {
        *latestObservedUpdatedAtMs = qMax<qint64>(0, lastSeenUpdatedAtMs);
    }

    if (!m_settings.hasNotificationConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 自动通知配置不完整");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return std::nullopt;
    }

    // 当 currentUserId 为空时跳过 PR 轮询，避免推送与用户无关的 PR
    const QString userId = m_settings.currentUserId.trimmed();
    if (userId.isEmpty()) {
        qWarning() << "[azure-devops] currentUserId 为空，跳过 PR 轮询";
        return std::nullopt;
    }

    // 分别查询"用户是审阅者"和"用户是创建者"的 PR，合并后去重
    QJsonArray mergedItems;
    QSet<QString> seenPrIds;
    for (const QUrl& url : {pullRequestsByReviewerUrl(m_settings), pullRequestsByCreatorUrl(m_settings)}) {
        const auto doc = m_transport->getJson(url, m_settings, errorMessage);
        if (!doc.has_value() || !doc->isObject()) {
            continue;
        }
        const QJsonArray items = doc->object().value(QStringLiteral("value")).toArray();
        for (const QJsonValue& v : items) {
            if (!v.isObject()) continue;
            const QString prId = stringAtPath(v.toObject(), {QStringLiteral("pullRequestId")});
            if (!seenPrIds.contains(prId)) {
                seenPrIds.insert(prId);
                mergedItems.append(v);
            }
        }
    }
    qint64 bestSeenMs = qMax<qint64>(0, lastSeenUpdatedAtMs);
    QJsonObject chosenObject;
    for (const QJsonValue& value : mergedItems) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const qint64 updatedAtMs = parseIsoDateMs(
            chooseFirstNonEmpty({
                stringAtPath(object, {QStringLiteral("completionQueueTime")}),  // PR 排队合并时间
                stringAtPath(object, {QStringLiteral("closedDate")}),           // PR 关闭/拒绝时间
                stringAtPath(object, {QStringLiteral("creationDate")}),         // PR 创建时间（兜底）
            }));
        bestSeenMs = qMax(bestSeenMs, updatedAtMs);
        if (updatedAtMs > lastSeenUpdatedAtMs && chosenObject.isEmpty()) {
            chosenObject = object;
        }
    }
    if (latestObservedUpdatedAtMs) {
        *latestObservedUpdatedAtMs = bestSeenMs;
    }
    if (chosenObject.isEmpty()) {
        return std::nullopt;
    }

    const bool reviewRequested = hasPendingReviewRequest(chosenObject);
    AzureDevOpsNotificationEvent event;
    event.kind = reviewRequested
        ? AzureDevOpsNotificationKind::PullRequestReviewRequested
        : AzureDevOpsNotificationKind::PullRequestUpdated;
    event.serviceId = QStringLiteral("local-azure-devops");
    event.workspaceId = QStringLiteral("local-devops");
    event.resourceId = QStringLiteral("pull-request:%1:%2")
                           .arg(stringAtPath(chosenObject, {QStringLiteral("repository"), QStringLiteral("name")}),
                                stringAtPath(chosenObject, {QStringLiteral("pullRequestId")}));
    event.title = chooseFirstNonEmpty({
        stringAtPath(chosenObject, {QStringLiteral("title")}),
        reviewRequested
            ? QStringLiteral("%1 PR 审核请求").arg(m_settings.project.trimmed())
            : QStringLiteral("%1 PR 更新").arg(m_settings.project.trimmed()),
    });
    event.summary = reviewRequested
        ? pendingReviewerSummary(chosenObject)
        : chooseFirstNonEmpty({
              stringAtPath(chosenObject, {QStringLiteral("repository"), QStringLiteral("name")}),
              QStringLiteral("PR 有新的更新"),
          });
    event.status = chooseFirstNonEmpty({
        stringAtPath(chosenObject, {QStringLiteral("status")}),
        QStringLiteral("active"),
    });
    event.webUrl = chooseFirstNonEmpty({
        stringAtPath(chosenObject, {QStringLiteral("_links"), QStringLiteral("web"), QStringLiteral("href")}),
        QStringLiteral("%1/%2/%3/_git/%4/pullrequest/%5")
            .arg(normalizedBaseUrl(m_settings),
                 m_settings.organization.trimmed(),
                 m_settings.project.trimmed(),
                 stringAtPath(chosenObject, {QStringLiteral("repository"), QStringLiteral("name")}),
                 stringAtPath(chosenObject, {QStringLiteral("pullRequestId")})),
    });
    event.actor = chooseFirstNonEmpty({
        stringAtPath(chosenObject, {QStringLiteral("createdBy"), QStringLiteral("displayName")}),
        QStringLiteral("Azure DevOps"),
    });
    return event;
}

std::optional<AzureDevOpsNotificationEvent> AzureDevOpsBuildNotificationPoller::pollLatestAssignedWorkItem(
    qint64 lastSeenUpdatedAtMs,
    qint64* latestObservedUpdatedAtMs,
    QString* errorMessage) const
{
    if (latestObservedUpdatedAtMs) {
        *latestObservedUpdatedAtMs = qMax<qint64>(0, lastSeenUpdatedAtMs);
    }

    if (!m_settings.hasNotificationConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 自动通知配置不完整");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return std::nullopt;
    }

    const QJsonDocument wiqlBody(QJsonObject{
        {QStringLiteral("query"),
         QStringLiteral(
             "Select Top 1 [System.Id], [System.Title], [System.State], [System.ChangedDate] "
             "From WorkItems "
             "Where [System.TeamProject] = @project "
             "And [System.AssignedTo] = @Me "
             "Order By [System.ChangedDate] Desc")},
    });
    const auto queryDocument =
        m_transport->postJson(wiqlAssignedToMeUrl(m_settings), m_settings, wiqlBody, errorMessage);
    if (!queryDocument.has_value() || !queryDocument->isObject()) {
        return std::nullopt;
    }

    const QJsonArray items = queryDocument->object().value(QStringLiteral("workItems")).toArray();
    if (items.isEmpty() || !items.first().isObject()) {
        return std::nullopt;
    }

    const QJsonObject firstItem = items.first().toObject();
    const QString workItemId = stringAtPath(firstItem, {QStringLiteral("id")});
    if (workItemId.isEmpty()) {
        return std::nullopt;
    }

    const auto detailsDocument =
        m_transport->getJson(workItemDetailsUrl(m_settings, workItemId), m_settings, errorMessage);
    if (!detailsDocument.has_value() || !detailsDocument->isObject()) {
        return std::nullopt;
    }

    const QJsonObject object = detailsDocument->object();
    const qint64 changedAtMs = parseIsoDateMs(
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.ChangedDate")}));
    if (latestObservedUpdatedAtMs) {
        *latestObservedUpdatedAtMs = qMax(changedAtMs, qMax<qint64>(0, lastSeenUpdatedAtMs));
    }
    if (changedAtMs <= lastSeenUpdatedAtMs) {
        if (!workItemId.isEmpty()) {
            qInfo().noquote() << QStringLiteral("[notifications][azure-devops] dedupe skip work item %1")
                                     .arg(workItemId);
        }
        return std::nullopt;
    }

    WorkItemUpdateSummary updateSummary;
    const auto updatesDocument =
        m_transport->getJson(workItemUpdatesUrl(m_settings, workItemId), m_settings, nullptr);
    if (updatesDocument.has_value() && updatesDocument->isObject()) {
        const QJsonArray updates = updatesDocument->object().value(QStringLiteral("value")).toArray();
        if (!updates.isEmpty() && updates.last().isObject()) {
            updateSummary = summarizeWorkItemUpdateV2(updates.last().toObject());
        }
    }

    AzureDevOpsNotificationEvent event;
    event.kind = updateSummary.kind;
    event.serviceId = QStringLiteral("local-azure-devops");
    event.workspaceId = QStringLiteral("local-devops");
    event.resourceId = QStringLiteral("work-item:%1").arg(workItemId);
    event.title = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.Title")}),
        QStringLiteral("%1 工作项更新").arg(m_settings.project.trimmed()),
    });
    event.summary = chooseFirstNonEmpty({
        updateSummary.summary,
        QStringLiteral("工作项需要你关注"),
    });
    event.status = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.State")}),
        QStringLiteral("active"),
    });
    event.webUrl = chooseFirstNonEmpty({
        stringAtPath(object, {QStringLiteral("_links"), QStringLiteral("html"), QStringLiteral("href")}),
        QStringLiteral("%1/%2/%3/_workitems/edit/%4")
            .arg(normalizedBaseUrl(m_settings),
                 m_settings.organization.trimmed(),
                 m_settings.project.trimmed(),
                 workItemId),
    });
    event.actor = chooseFirstNonEmpty({
        updateSummary.actor,
        stringAtPath(object, {QStringLiteral("fields"), QStringLiteral("System.AssignedTo")}),
        QStringLiteral("Azure DevOps"),
    });
    return event;
}

std::optional<AzureDevOpsNotificationEvent> AzureDevOpsBuildNotificationPoller::pollLatestMentionComment(
    qint64 lastSeenUpdatedAtMs,
    qint64* latestObservedUpdatedAtMs,
    QString* errorMessage) const
{
    if (latestObservedUpdatedAtMs) {
        *latestObservedUpdatedAtMs = qMax<qint64>(0, lastSeenUpdatedAtMs);
    }

    if (!m_settings.hasNotificationConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 自动通知配置不完整");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 传输层未初始化");
        }
        return std::nullopt;
    }

    const QStringList identityTokens = m_settings.currentUserIdentityTokens();
    if (identityTokens.isEmpty()) {
        return std::nullopt;
    }

    // 查询最近被他人修改的工作项（排除自己的变更）
    const QJsonDocument wiqlBody(QJsonObject{
        {QStringLiteral("query"),
         QStringLiteral(
             "Select Top 5 [System.Id], [System.Title], [System.State], [System.ChangedDate] "
             "From WorkItems "
             "Where [System.TeamProject] = @project "
             "And [System.ChangedBy] <> @Me "
             "Order By [System.ChangedDate] Desc")},
    });
    const auto queryDocument =
        m_transport->postJson(wiqlAssignedToMeUrl(m_settings), m_settings, wiqlBody, errorMessage);
    if (!queryDocument.has_value() || !queryDocument->isObject()) {
        return std::nullopt;
    }

    const QJsonArray items = queryDocument->object().value(QStringLiteral("workItems")).toArray();
    const int limit = qMin(5, static_cast<int>(items.size()));

    for (int i = 0; i < limit; ++i) {
        if (!items.at(i).isObject()) {
            continue;
        }
        const QString workItemId = stringAtPath(items.at(i).toObject(), {QStringLiteral("id")});
        if (workItemId.isEmpty()) {
            continue;
        }

        // 获取工作项最近的更新记录
        const auto updatesDocument =
            m_transport->getJson(workItemUpdatesUrl(m_settings, workItemId), m_settings, nullptr);
        if (!updatesDocument.has_value() || !updatesDocument->isObject()) {
            continue;
        }

        const QJsonArray updates = updatesDocument->object().value(QStringLiteral("value")).toArray();
        if (updates.isEmpty() || !updates.last().isObject()) {
            continue;
        }

        const QJsonObject lastUpdate = updates.last().toObject();
        const QJsonObject fields = lastUpdate.value(QStringLiteral("fields")).toObject();
        const QJsonObject historyChange = fields.value(QStringLiteral("System.History")).toObject();
        const QString historyNewValue =
            historyChange.value(QStringLiteral("newValue")).toString().trimmed();
        if (historyNewValue.isEmpty()) {
            continue;
        }

        // 检查评论是否包含当前用户的 @提及
        bool mentionsUser = false;
        for (const QString& token : identityTokens) {
            if (historyNewValue.contains(token, Qt::CaseInsensitive)) {
                mentionsUser = true;
                break;
            }
        }
        if (!mentionsUser) {
            continue;
        }

        // 获取工作项详情
        const auto detailsDocument =
            m_transport->getJson(workItemDetailsUrl(m_settings, workItemId), m_settings, nullptr);
        if (!detailsDocument.has_value() || !detailsDocument->isObject()) {
            continue;
        }

        const QJsonObject detailsObject = detailsDocument->object();
        const qint64 changedAtMs = parseIsoDateMs(
            stringAtPath(detailsObject, {QStringLiteral("fields"), QStringLiteral("System.ChangedDate")}));
        if (latestObservedUpdatedAtMs) {
            *latestObservedUpdatedAtMs = qMax(changedAtMs, qMax<qint64>(0, lastSeenUpdatedAtMs));
        }
        if (changedAtMs <= lastSeenUpdatedAtMs) {
            qInfo().noquote()
                << QStringLiteral("[notifications][azure-devops] dedupe skip mention work item %1")
                       .arg(workItemId);
            continue;
        }

        const QString actor = chooseFirstNonEmpty({
            stringAtPath(lastUpdate, {QStringLiteral("revisedBy"), QStringLiteral("displayName")}),
            stringAtPath(lastUpdate, {QStringLiteral("revisedBy"), QStringLiteral("name")}),
        });
        AzureDevOpsNotificationEvent event;
        event.kind = AzureDevOpsNotificationKind::WorkItemMentioned;
        event.serviceId = QStringLiteral("local-azure-devops");
        event.workspaceId = QStringLiteral("local-devops");
        event.resourceId = QStringLiteral("mention-comment:%1").arg(workItemId);
        event.title = chooseFirstNonEmpty({
            stringAtPath(detailsObject, {QStringLiteral("fields"), QStringLiteral("System.Title")}),
            QStringLiteral("%1 工作项评论").arg(m_settings.project.trimmed()),
        });
        event.summary = QStringLiteral("评论中 @提及 了你");
        event.status = chooseFirstNonEmpty({
            stringAtPath(detailsObject, {QStringLiteral("fields"), QStringLiteral("System.State")}),
            QStringLiteral("active"),
        });
        event.webUrl = chooseFirstNonEmpty({
            stringAtPath(detailsObject, {QStringLiteral("_links"), QStringLiteral("html"), QStringLiteral("href")}),
            QStringLiteral("%1/%2/%3/_workitems/edit/%4")
                .arg(normalizedBaseUrl(m_settings),
                     m_settings.organization.trimmed(),
                     m_settings.project.trimmed(),
                     workItemId),
        });
        event.actor = chooseFirstNonEmpty({actor, QStringLiteral("Azure DevOps")});
        return event;
    }

    return std::nullopt;
}

QVector<AzureDevOpsNotificationEvent> AzureDevOpsBuildNotificationPoller::pollTrackedBuilds(
    AzureDevOpsConnectionSettings* settings,
    QString* errorMessage) const
{
    QVector<AzureDevOpsNotificationEvent> events;
    if (!settings) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 轮询目标为空");
        }
        return events;
    }

    QStringList errors;
    QVector<AzureDevOpsNotificationTarget> targets = settings->enabledNotificationTargets();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (targets.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 自动通知配置不完整");
        }
        return events;
    }

    for (AzureDevOpsNotificationTarget& target : targets) {
        AzureDevOpsConnectionSettings scopedSettings = *settings;
        scopedSettings.organization = target.organization;
        scopedSettings.project = target.project;
        const AzureDevOpsBuildNotificationPoller scopedPoller(scopedSettings, m_transport);
        int latestObservedBuildId = target.lastNotifiedBuildId;
        QString targetError;
        const auto event = scopedPoller.pollLatestBuild(target.lastNotifiedBuildId,
                                                        &latestObservedBuildId,
                                                        &targetError);
        target.lastPollAttemptAtMs = nowMs;
        if (event.has_value()) {
            target.lastNotifiedBuildResult = event->status.trimmed();
            events.push_back(*event);
        }
        target.lastNotifiedBuildId = qMax(target.lastNotifiedBuildId, latestObservedBuildId);
        if (!targetError.trimmed().isEmpty()) {
            target.lastPollErrorMessage = targetError.trimmed();
            target.lastPollErrorCategory = classifyPollErrorCategory(targetError);
            target.consecutivePollFailures += 1;
            errors.push_back(QStringLiteral("%1/%2: %3")
                                 .arg(target.organization, target.project, targetError.trimmed()));
        } else {
            target.lastPollSuccessAtMs = nowMs;
            target.lastPollErrorMessage.clear();
            target.lastPollErrorCategory.clear();
            target.consecutivePollFailures = 0;
        }
    }

    QVector<AzureDevOpsNotificationTarget> persistedTargets = settings->notificationTargets;
    for (const AzureDevOpsNotificationTarget& updatedTarget : targets) {
        bool matched = false;
        for (AzureDevOpsNotificationTarget& existingTarget : persistedTargets) {
            if (existingTarget.organization.trimmed().compare(updatedTarget.organization.trimmed(), Qt::CaseInsensitive) == 0
                && existingTarget.project.trimmed().compare(updatedTarget.project.trimmed(), Qt::CaseInsensitive) == 0) {
                existingTarget.enabled = updatedTarget.enabled;
                existingTarget.lastNotifiedBuildId = updatedTarget.lastNotifiedBuildId;
                existingTarget.lastNotifiedBuildResult = updatedTarget.lastNotifiedBuildResult;
                existingTarget.lastPollAttemptAtMs = updatedTarget.lastPollAttemptAtMs;
                existingTarget.lastPollSuccessAtMs = updatedTarget.lastPollSuccessAtMs;
                existingTarget.lastPollErrorMessage = updatedTarget.lastPollErrorMessage;
                existingTarget.consecutivePollFailures = updatedTarget.consecutivePollFailures;
                matched = true;
                break;
            }
        }
        if (!matched) {
            persistedTargets.push_back(updatedTarget);
        }
    }

    settings->notificationTargets = persistedTargets;
    settings->normalizeSelection();
    updateSettingsPollHealthFromTargets(settings, persistedTargets);
    if (errorMessage) {
        *errorMessage = errors.join(QStringLiteral("\n"));
    }
    return events;
}

QVector<AzureDevOpsNotificationEvent> AzureDevOpsBuildNotificationPoller::pollTrackedNotifications(
    AzureDevOpsConnectionSettings* settings,
    QString* errorMessage) const
{
    QVector<AzureDevOpsNotificationEvent> events;
    if (!settings) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 轮询目标为空");
        }
        return events;
    }

    // 自动发现 currentUserId（on-prem 首次轮询时触发）
    if (settings->currentUserId.trimmed().isEmpty() && settings->hasCredentialConfiguration()) {
        LocalAzureDevOpsAdapter adapter(*settings, m_transport);
        AzureDevOpsConnectionSettings resolved = *settings;
        if (adapter.discoverCurrentUser(&resolved, nullptr)) {
            settings->currentUserId = resolved.currentUserId;
            settings->currentUserDisplayName = resolved.currentUserDisplayName;
            settings->currentUserUniqueName = resolved.currentUserUniqueName;
            settings->currentUserEmail = resolved.currentUserEmail;
            qInfo().noquote() << QStringLiteral(
                "[notifications][azure-devops] 轮询前自动发现用户: %1 (%2)")
                .arg(settings->currentUserDisplayName.trimmed(),
                     settings->currentUserId.trimmed());
        }
    }

    QStringList errors;
    QVector<AzureDevOpsNotificationTarget> targets = settings->enabledNotificationTargets();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (targets.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Azure DevOps 自动通知配置不完整");
        }
        return events;
    }

    for (AzureDevOpsNotificationTarget& target : targets) {
        AzureDevOpsConnectionSettings scopedSettings = *settings;
        scopedSettings.organization = target.organization;
        scopedSettings.project = target.project;
        const AzureDevOpsBuildNotificationPoller scopedPoller(scopedSettings, m_transport);

        int latestBuildId = target.lastNotifiedBuildId;
        qint64 latestPullRequestUpdatedAtMs = target.lastNotifiedPullRequestUpdatedAtMs;
        qint64 latestAssignedWorkItemUpdatedAtMs = target.lastNotifiedAssignedWorkItemUpdatedAtMs;
        qint64 latestMentionCommentAtMs = target.lastNotifiedMentionCommentAtMs;

        QString buildError;
        const auto buildEvent = scopedPoller.pollLatestBuild(target.lastNotifiedBuildId,
                                                             &latestBuildId,
                                                             &buildError);
        if (buildEvent.has_value()) {
            AzureDevOpsNotificationEvent event = *buildEvent;
            if (isFailureResult(target.lastNotifiedBuildResult)
                && isSuccessResult(buildEvent->status)) {
                event.kind = AzureDevOpsNotificationKind::BuildRecovered;
                event.summary = QStringLiteral("构建已从失败恢复为成功");
            }
            target.lastNotifiedBuildResult = buildEvent->status.trimmed();
            events.push_back(event);
        }
        if (!buildError.trimmed().isEmpty()) {
            errors.push_back(QStringLiteral("%1/%2 构建: %3")
                                 .arg(target.organization, target.project, buildError.trimmed()));
        }

        QString prError;
        const auto prEvent = scopedPoller.pollLatestPullRequest(
            target.lastNotifiedPullRequestUpdatedAtMs,
            &latestPullRequestUpdatedAtMs,
            &prError);
        if (prEvent.has_value()) {
            events.push_back(*prEvent);
        }
        if (!prError.trimmed().isEmpty()) {
            errors.push_back(QStringLiteral("%1/%2 PR: %3")
                                 .arg(target.organization, target.project, prError.trimmed()));
        }

        QString workItemError;
        const auto workItemEvent = scopedPoller.pollLatestAssignedWorkItem(
            target.lastNotifiedAssignedWorkItemUpdatedAtMs,
            &latestAssignedWorkItemUpdatedAtMs,
            &workItemError);
        if (workItemEvent.has_value()) {
            events.push_back(*workItemEvent);
        }
        if (!workItemError.trimmed().isEmpty()) {
            errors.push_back(QStringLiteral("%1/%2 工作项: %3")
                                 .arg(target.organization, target.project, workItemError.trimmed()));
        }

        QString mentionError;
        const auto mentionEvent = scopedPoller.pollLatestMentionComment(
            target.lastNotifiedMentionCommentAtMs,
            &latestMentionCommentAtMs,
            &mentionError);
        if (mentionEvent.has_value()) {
            events.push_back(*mentionEvent);
        }
        if (!mentionError.trimmed().isEmpty()) {
            errors.push_back(QStringLiteral("%1/%2 @提及: %3")
                                 .arg(target.organization, target.project, mentionError.trimmed()));
        }

        target.lastPollAttemptAtMs = nowMs;
        target.lastNotifiedBuildId = qMax(target.lastNotifiedBuildId, latestBuildId);
        target.lastNotifiedPullRequestUpdatedAtMs =
            qMax(target.lastNotifiedPullRequestUpdatedAtMs, latestPullRequestUpdatedAtMs);
        target.lastNotifiedAssignedWorkItemUpdatedAtMs =
            qMax(target.lastNotifiedAssignedWorkItemUpdatedAtMs, latestAssignedWorkItemUpdatedAtMs);
        target.lastNotifiedMentionCommentAtMs =
            qMax(target.lastNotifiedMentionCommentAtMs, latestMentionCommentAtMs);
        QStringList combinedErrors;
        for (const QString& candidate : {buildError.trimmed(), prError.trimmed(), workItemError.trimmed(), mentionError.trimmed()}) {
            if (!candidate.isEmpty()) {
                combinedErrors.push_back(candidate);
            }
        }
        const QString combinedError = combinedErrors.join(QStringLiteral(" | "));
        if (!combinedError.isEmpty()) {
            target.lastPollErrorMessage = combinedError;
            target.lastPollErrorCategory = classifyPollErrorCategory(combinedError);
            target.consecutivePollFailures += 1;
        } else {
            target.lastPollSuccessAtMs = nowMs;
            target.lastPollErrorMessage.clear();
            target.lastPollErrorCategory.clear();
            target.consecutivePollFailures = 0;
        }
    }

    QVector<AzureDevOpsNotificationTarget> persistedTargets = settings->notificationTargets;
    for (const AzureDevOpsNotificationTarget& updatedTarget : targets) {
        bool matched = false;
        for (AzureDevOpsNotificationTarget& existingTarget : persistedTargets) {
            if (existingTarget.organization.trimmed().compare(updatedTarget.organization.trimmed(), Qt::CaseInsensitive) == 0
                && existingTarget.project.trimmed().compare(updatedTarget.project.trimmed(), Qt::CaseInsensitive) == 0) {
                existingTarget.enabled = updatedTarget.enabled;
                existingTarget.lastNotifiedBuildId = updatedTarget.lastNotifiedBuildId;
                existingTarget.lastNotifiedPullRequestUpdatedAtMs =
                    updatedTarget.lastNotifiedPullRequestUpdatedAtMs;
                existingTarget.lastNotifiedAssignedWorkItemUpdatedAtMs =
                    updatedTarget.lastNotifiedAssignedWorkItemUpdatedAtMs;
                existingTarget.lastNotifiedMentionCommentAtMs =
                    updatedTarget.lastNotifiedMentionCommentAtMs;
                existingTarget.lastNotifiedBuildResult = updatedTarget.lastNotifiedBuildResult;
                existingTarget.lastPollAttemptAtMs = updatedTarget.lastPollAttemptAtMs;
                existingTarget.lastPollSuccessAtMs = updatedTarget.lastPollSuccessAtMs;
                existingTarget.lastPollErrorMessage = updatedTarget.lastPollErrorMessage;
                existingTarget.lastPollErrorCategory = updatedTarget.lastPollErrorCategory;
                existingTarget.consecutivePollFailures = updatedTarget.consecutivePollFailures;
                matched = true;
                break;
            }
        }
        if (!matched) {
            persistedTargets.push_back(updatedTarget);
        }
    }

    settings->notificationTargets = persistedTargets;
    settings->normalizeSelection();
    updateSettingsPollHealthFromTargets(settings, persistedTargets);
    if (errorMessage) {
        *errorMessage = errors.join(QStringLiteral("\n"));
    }
    return events;
}
