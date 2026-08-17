#include "integrations/OutlookAdapterContracts.h"

namespace {

QString joinedSummary(std::initializer_list<QString> parts)
{
    QStringList filtered;
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            filtered.push_back(trimmed);
        }
    }
    return filtered.join(QStringLiteral(" / "));
}

ResourceReference baseReference(const QString& serviceId,
                                const QString& workspaceId,
                                const QString& resourceId,
                                const QString& kind,
                                const QString& title,
                                const QString& summary)
{
    ResourceReference reference;
    reference.serviceId = serviceId.trimmed();
    reference.workspaceId = workspaceId.trimmed();
    reference.resourceId = resourceId.trimmed();
    reference.resourceKind = kind;
    reference.title = title.trimmed();
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

QString eventTitle(const OutlookCalendarEventResource& resource)
{
    if (!resource.subject.trimmed().isEmpty()) {
        return resource.subject.trimmed();
    }
    return resource.cancelled ? QStringLiteral("已取消会议") : QStringLiteral("Outlook 日程");
}

QString eventStatus(const OutlookCalendarEventResource& resource)
{
    if (resource.cancelled) {
        return QStringLiteral("已取消");
    }
    return resource.whenLabel.trimmed();
}

QString eventSubtitle(const OutlookCalendarEventResource& resource)
{
    if (resource.cancelled) {
        return joinedSummary({resource.organizer, QStringLiteral("会议已取消"), resource.location});
    }
    return joinedSummary({resource.organizer, resource.location});
}

QString eventSummary(const OutlookCalendarEventResource& resource)
{
    if (resource.cancelled) {
        return joinedSummary({resource.organizer, resource.whenLabel, QStringLiteral("会议已取消")});
    }
    if (!resource.lastModifiedLabel.trimmed().isEmpty()) {
        return joinedSummary({resource.organizer, resource.whenLabel, resource.lastModifiedLabel});
    }
    return joinedSummary({resource.organizer, resource.whenLabel, resource.location});
}

}  // namespace

namespace OutlookAdapterContracts {

ResourceReference makeMailReference(const OutlookMailResource& resource)
{
    const QString title = resource.subject.trimmed().isEmpty()
        ? QStringLiteral("Outlook 邮件")
        : resource.subject.trimmed();
    return baseReference(resource.serviceId,
                         resource.workspaceId,
                         resource.resourceId,
                         QStringLiteral("outlook_mail"),
                         title,
                         joinedSummary({resource.sender, resource.mailbox, resource.receivedAtLabel}));
}

ResourceReference makeCalendarEventReference(const OutlookCalendarEventResource& resource)
{
    return baseReference(resource.serviceId,
                         resource.workspaceId,
                         resource.resourceId,
                         QStringLiteral("outlook_event"),
                         eventTitle(resource),
                         eventSummary(resource));
}

ResourceRefPayload makeMailPayload(const OutlookMailResource& resource)
{
    const ResourceReference reference = makeMailReference(resource);
    ResourceRefPayload payload = basePayload(reference.serviceId,
                                             reference.workspaceId,
                                             reference.resourceId,
                                             reference.resourceKind,
                                             reference.title,
                                             joinedSummary({resource.sender, resource.mailbox}),
                                             resource.receivedAtLabel);
    appendPrimaryLink(payload, QStringLiteral("打开邮件"), resource.webUrl);
    return payload;
}

ResourceRefPayload makeCalendarEventPayload(const OutlookCalendarEventResource& resource)
{
    const ResourceReference reference = makeCalendarEventReference(resource);
    ResourceRefPayload payload = basePayload(reference.serviceId,
                                             reference.workspaceId,
                                             reference.resourceId,
                                             reference.resourceKind,
                                             reference.title,
                                             eventSubtitle(resource),
                                             eventStatus(resource));
    appendPrimaryLink(payload, QStringLiteral("打开日程"), resource.webUrl);
    return payload;
}

}  // namespace OutlookAdapterContracts
