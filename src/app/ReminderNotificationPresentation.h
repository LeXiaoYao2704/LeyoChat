#pragma once

#include "app/ReminderActionRouting.h"
#include "domain/ReminderItem.h"
#include "domain/SystemNotificationItem.h"

#include <QString>

inline SystemNotificationItem notificationFromReminder(const ReminderItem& reminder)
{
    const QString title = reminder.titleSnapshot.trimmed();
    const QString summary = reminder.previewSnapshot.trimmed();
    const QString note = reminder.note.trimmed();

    SystemNotificationItem item;
    item.notificationId = QStringLiteral("reminder:%1").arg(reminder.reminderId);
    item.sourceKey = QStringLiteral("reminder");
    item.sourceLabel = QStringLiteral("本机提醒");
    item.title = title.isEmpty() ? QStringLiteral("本机提醒") : title;
    item.summary = summary.isEmpty() ? QStringLiteral("提醒时间到了") : summary;
    item.detail = note.isEmpty() ? item.summary : note;
    item.actionLabel = QStringLiteral("查看提醒");
    item.actionUrl = reminderActionUrl(QStringLiteral("open"), reminder.reminderId);
    item.occurredAtMs = reminder.dueAtMs;
    item.unread = true;
    return item;
}
