#pragma once

#include <QString>

struct SystemNotificationItem {
    QString notificationId;
    QString sourceKey;
    QString sourceLabel;
    QString title;
    QString summary;
    QString detail;
    QString actionLabel;
    QString actionUrl;
    QString htmlBody;
    qint64 occurredAtMs = 0;
    bool unread = true;
};
