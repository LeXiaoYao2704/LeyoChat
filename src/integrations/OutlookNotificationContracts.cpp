#include "integrations/OutlookNotificationContracts.h"

namespace {

QString eventKindResourceKind(OutlookNotificationKind kind)
{
    switch (kind) {
    case OutlookNotificationKind::MailReceived:
        return QStringLiteral("outlook_mail");
    case OutlookNotificationKind::CalendarReminder:
    case OutlookNotificationKind::CalendarUpdated:
    case OutlookNotificationKind::CalendarCancelled:
        return QStringLiteral("outlook_event");
    default:
        return QStringLiteral("outlook_notification");
    }
}

QString eventLabel(OutlookNotificationKind kind)
{
    switch (kind) {
    case OutlookNotificationKind::MailReceived:
        return QStringLiteral("Outlook 邮件提醒");
    case OutlookNotificationKind::CalendarReminder:
        return QStringLiteral("Outlook 会议提醒");
    case OutlookNotificationKind::CalendarUpdated:
        return QStringLiteral("Outlook 会议变更");
    case OutlookNotificationKind::CalendarCancelled:
        return QStringLiteral("Outlook 会议取消");
    default:
        return QStringLiteral("Outlook 通知");
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

namespace OutlookNotificationContracts {

ResourceReference makeNotificationReference(const OutlookNotificationEvent& event)
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

ResourceRefPayload makeNotificationPayload(const OutlookNotificationEvent& event)
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

}  // namespace OutlookNotificationContracts
