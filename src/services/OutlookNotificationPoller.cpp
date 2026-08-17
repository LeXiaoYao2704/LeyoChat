#include "services/OutlookNotificationPoller.h"

#include <QDebug>
#include <QRegularExpression>
#include <QSet>

namespace {

constexpr int kRecentIdCacheLimit = 100;
constexpr int kUpcomingEventHorizonMinutes = 60;

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
                                 QStringLiteral("token"),
                                 QStringLiteral("oauth"),
                                 QStringLiteral("refresh"),
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

QString eventFingerprint(const OutlookCalendarEventResource& resource)
{
    const QString state = resource.cancelled ? QStringLiteral("cancelled") : QStringLiteral("active");
    return QStringLiteral("%1|%2|%3")
        .arg(resource.resourceId.trimmed(),
             resource.changeKey.trimmed().isEmpty() ? QStringLiteral("baseline")
                                                    : resource.changeKey.trimmed(),
             state);
}

QString baseEventIdFromFingerprint(const QString& fingerprint)
{
    return fingerprint.section('|', 0, 0).trimmed();
}

OutlookNotificationEvent mailEventFromResource(const OutlookMailResource& resource)
{
    OutlookNotificationEvent event;
    event.kind = OutlookNotificationKind::MailReceived;
    event.serviceId = resource.serviceId;
    event.workspaceId = resource.workspaceId;
    event.resourceId = resource.resourceId;
    event.title = resource.subject;

    const QString senderLine = resource.sender.trimmed().isEmpty()
        ? QStringLiteral("收到一封新的未读邮件")
        : QStringLiteral("来自 %1 的未读邮件").arg(resource.sender.trimmed());
    if (resource.bodyPreview.trimmed().isEmpty()) {
        event.summary = senderLine;
    } else {
        // 截取前 120 字符作为预览
        const QString preview = resource.bodyPreview.trimmed().left(120);
        event.summary = QStringLiteral("%1\n%2").arg(senderLine, preview);
    }

    event.status = resource.receivedAtLabel;
    event.webUrl = resource.webUrl;
    event.actor = resource.sender;
    event.htmlBody = resource.htmlBody;

    return event;
}

OutlookNotificationKind eventKindForResource(const OutlookCalendarEventResource& resource,
                                             const QSet<QString>& knownBaseEventIds)
{
    if (resource.cancelled) {
        return OutlookNotificationKind::CalendarCancelled;
    }
    if (knownBaseEventIds.contains(resource.resourceId.trimmed())) {
        return OutlookNotificationKind::CalendarUpdated;
    }
    return OutlookNotificationKind::CalendarReminder;
}

OutlookNotificationEvent calendarEventFromResource(const OutlookCalendarEventResource& resource,
                                                   const QSet<QString>& knownBaseEventIds)
{
    OutlookNotificationEvent event;
    event.kind = eventKindForResource(resource, knownBaseEventIds);
    event.serviceId = resource.serviceId;
    event.workspaceId = resource.workspaceId;
    event.resourceId = resource.resourceId;
    event.title = resource.subject;
    switch (event.kind) {
    case OutlookNotificationKind::CalendarReminder:
        event.summary = resource.location.trimmed().isEmpty()
            ? resource.whenLabel
            : QStringLiteral("%1 / %2").arg(resource.whenLabel, resource.location);
        break;
    case OutlookNotificationKind::CalendarUpdated:
        event.summary = resource.lastModifiedLabel.trimmed().isEmpty()
            ? QStringLiteral("会议有新的变更")
            : QStringLiteral("会议已更新，最近变更时间：%1").arg(resource.lastModifiedLabel);
        break;
    case OutlookNotificationKind::CalendarCancelled:
        event.summary = QStringLiteral("会议已取消");
        break;
    default:
        break;
    }
    event.status = resource.cancelled ? QStringLiteral("已取消") : resource.whenLabel;
    event.webUrl = resource.webUrl;
    event.actor = resource.organizer;
    return event;
}

}  // namespace

OutlookNotificationPoller::OutlookNotificationPoller(
    OutlookConnectionSettings settings,
    std::shared_ptr<IOutlookEwsTransport> ewsTransport)
    : m_settings(std::move(settings))
    , m_adapter(m_settings, std::move(ewsTransport))
{
}

OutlookNotificationPollResult OutlookNotificationPoller::poll(const QDateTime& now,
                                                              QString* errorMessage)
{
    OutlookNotificationPollResult result;
    result.updatedSettings = m_settings;
    result.updatedSettings.lastPollAttemptAtMs = now.toMSecsSinceEpoch();
    if (!m_settings.hasNotificationConfiguration()) {
        if (errorMessage) {
            errorMessage->clear();
        }
        return result;
    }

    QString mailErrorMessage;
    const QVector<OutlookMailResource> mails = m_adapter.fetchUnreadMail(10, &mailErrorMessage);

    QString eventErrorMessage;
    const QVector<OutlookCalendarEventResource> events =
        m_adapter.fetchUpcomingEvents(now, kUpcomingEventHorizonMinutes, &eventErrorMessage);

    // Merge errors: collect non-empty parts and join so neither masks the other.
    QStringList errorParts;
    if (!mailErrorMessage.trimmed().isEmpty()) {
        errorParts << mailErrorMessage.trimmed();
    }
    if (!eventErrorMessage.trimmed().isEmpty()) {
        errorParts << eventErrorMessage.trimmed();
    }
    const QString combinedError = errorParts.join(QStringLiteral("; "));
    if (errorMessage) {
        *errorMessage = combinedError;
    }

    const QSet<QString> knownMailIds(
        m_settings.recentMailIds.cbegin(), m_settings.recentMailIds.cend());
    const QSet<QString> knownEventFingerprints(
        m_settings.recentEventIds.cbegin(), m_settings.recentEventIds.cend());

    QSet<QString> knownBaseEventIds;
    for (const QString& fingerprint : m_settings.recentEventIds) {
        const QString baseId = baseEventIdFromFingerprint(fingerprint);
        if (!baseId.isEmpty()) {
            knownBaseEventIds.insert(baseId);
        }
    }

    QStringList latestMailIds;
    for (const auto& mail : mails) {
        const QString resourceId = mail.resourceId.trimmed();
        if (resourceId.isEmpty()) {
            continue;
        }
        latestMailIds.push_back(resourceId);
        if (knownMailIds.contains(resourceId)) {
            qInfo().noquote() << QStringLiteral("[notifications][outlook] dedupe skip mail %1")
                                     .arg(resourceId);
            continue;
        }
        result.events.push_back(mailEventFromResource(mail));
    }

    QStringList latestEventFingerprints;
    for (const auto& eventResource : events) {
        const QString resourceId = eventResource.resourceId.trimmed();
        if (resourceId.isEmpty()) {
            continue;
        }

        const QString fingerprint = eventFingerprint(eventResource);
        latestEventFingerprints.push_back(fingerprint);
        if (knownEventFingerprints.contains(fingerprint)) {
            qInfo().noquote() << QStringLiteral("[notifications][outlook] dedupe skip event %1")
                                     .arg(fingerprint);
            continue;
        }
        result.events.push_back(calendarEventFromResource(eventResource, knownBaseEventIds));
    }

    result.updatedSettings = m_adapter.settings();
    result.updatedSettings.lastPollAttemptAtMs = now.toMSecsSinceEpoch();
    result.updatedSettings.recentMailIds =
        rememberRecentIds(result.updatedSettings.recentMailIds, latestMailIds, kRecentIdCacheLimit);
    result.updatedSettings.recentEventIds = rememberRecentIds(
        result.updatedSettings.recentEventIds, latestEventFingerprints, kRecentIdCacheLimit);
    if (errorMessage && !errorMessage->trimmed().isEmpty()) {
        result.updatedSettings.lastPollErrorMessage = errorMessage->trimmed();
        result.updatedSettings.lastPollErrorCategory = classifyPollErrorCategory(*errorMessage);
        result.updatedSettings.consecutivePollFailures += 1;
    } else {
        result.updatedSettings.lastPollSuccessAtMs = now.toMSecsSinceEpoch();
        result.updatedSettings.lastPollErrorMessage.clear();
        result.updatedSettings.lastPollErrorCategory.clear();
        result.updatedSettings.consecutivePollFailures = 0;
    }
    return result;
}

QStringList OutlookNotificationPoller::rememberRecentIds(const QStringList& existing,
                                                         const QStringList& latest,
                                                         int maxCount)
{
    QStringList merged;
    QSet<QString> seen;

    const auto appendUnique = [&](const QString& value) {
        const QString trimmed = value.trimmed();
        if (trimmed.isEmpty() || seen.contains(trimmed)) {
            return;
        }
        seen.insert(trimmed);
        merged.push_back(trimmed);
    };

    for (const QString& value : latest) {
        appendUnique(value);
    }
    for (const QString& value : existing) {
        appendUnique(value);
    }
    while (merged.size() > maxCount) {
        merged.removeLast();
    }
    return merged;
}
