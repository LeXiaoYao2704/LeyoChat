#pragma once

#include <QString>

#include "architecture/ResourceReference.h"
#include "domain/ResourceRefPayload.h"

enum class OutlookNotificationKind {
    MailReceived,
    CalendarReminder,
    CalendarUpdated,
    CalendarCancelled
};

struct OutlookNotificationEvent {
    OutlookNotificationKind kind = OutlookNotificationKind::MailReceived;
    QString serviceId;
    QString workspaceId;
    QString resourceId;
    QString title;
    QString summary;
    QString status;
    QString webUrl;
    QString actor;
    QString htmlBody;
};

namespace OutlookNotificationContracts {

ResourceReference makeNotificationReference(const OutlookNotificationEvent& event);
ResourceRefPayload makeNotificationPayload(const OutlookNotificationEvent& event);

}  // namespace OutlookNotificationContracts
