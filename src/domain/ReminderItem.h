#pragma once

#include <QMetaType>
#include <QtGlobal>
#include <QString>

struct ReminderItem {
    QString reminderId;
    QString targetType;
    QString targetId;
    QString conversationId;
    QString groupId;
    QString contactId;
    QString resourceId;
    QString titleSnapshot;
    QString previewSnapshot;
    QString note;
    qint64 dueAtMs = 0;
    qint64 createdAtMs = 0;
    qint64 updatedAtMs = 0;
    qint64 firedAtMs = 0;
    qint64 completedAtMs = 0;
    QString state = QStringLiteral("scheduled");
    QString sourceMessageId;
    QString payloadJson;

    bool isActive() const
    {
        return state == QStringLiteral("scheduled") || state == QStringLiteral("due");
    }
};

Q_DECLARE_METATYPE(ReminderItem)
