#include "app/ReminderItemFactory.h"

#include "services/ReminderTimeOptions.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace {

ReminderItem baseReminder(const QDateTime& now)
{
    ReminderItem item;
    const qint64 nowMs = now.toMSecsSinceEpoch();
    item.createdAtMs = nowMs;
    item.updatedAtMs = nowMs;
    item.state = QStringLiteral("scheduled");
    return item;
}

QString compactPayload(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

} // namespace

std::optional<ReminderItem> makeMessageReminderItem(const QString& messageId,
                                                    const QString& conversationId,
                                                    const QString& titleSnapshot,
                                                    const QString& previewSnapshot,
                                                    const QString& note,
                                                    qint64 dueAtMs,
                                                    const QDateTime& now)
{
    const QString trimmedMessageId = messageId.trimmed();
    if (trimmedMessageId.isEmpty()) {
        return std::nullopt;
    }

    ReminderItem item = baseReminder(now);
    item.targetType = QStringLiteral("message");
    item.targetId = trimmedMessageId;
    item.conversationId = conversationId.trimmed();
    item.titleSnapshot = titleSnapshot.trimmed().isEmpty()
                             ? QStringLiteral("\u6D88\u606F\u63D0\u9192")
                             : titleSnapshot.trimmed();
    item.previewSnapshot = previewSnapshot.trimmed().isEmpty()
                               ? QStringLiteral("\u7A0D\u540E\u56DE\u590D\u8FD9\u6761\u6D88\u606F")
                               : previewSnapshot.trimmed();
    item.note = note.trimmed();
    item.dueAtMs = dueAtMs;
    item.sourceMessageId = trimmedMessageId;
    return item;
}

std::optional<ReminderItem> makeContactReminderItem(const QString& contactId,
                                                    const QString& displayName,
                                                    const QString& previewSnapshot,
                                                    const QDateTime& now)
{
    const QString trimmedContactId = contactId.trimmed();
    if (trimmedContactId.isEmpty()) {
        return std::nullopt;
    }

    ReminderItem item = baseReminder(now);
    item.targetType = QStringLiteral("contact");
    item.targetId = trimmedContactId;
    item.contactId = trimmedContactId;
    item.titleSnapshot = displayName.trimmed().isEmpty()
                             ? trimmedContactId
                             : displayName.trimmed();
    item.previewSnapshot = previewSnapshot.trimmed().isEmpty()
                               ? QStringLiteral("\u8DDF\u8FDB\u8054\u7CFB\u4EBA")
                               : previewSnapshot.trimmed();
    item.dueAtMs = ReminderTimeOptions::tomorrowAtNine(now).toMSecsSinceEpoch();
    return item;
}

std::optional<ReminderItem> makeGroupAnnouncementReminderItem(const QString& groupId,
                                                              const QString& groupName,
                                                              const QString& announcement,
                                                              const QString& note,
                                                              qint64 dueAtMs,
                                                              const QDateTime& now)
{
    const QString trimmedGroupId = groupId.trimmed();
    if (trimmedGroupId.isEmpty()) {
        return std::nullopt;
    }

    ReminderItem item = baseReminder(now);
    item.targetType = QStringLiteral("group_announcement");
    item.targetId = trimmedGroupId;
    item.groupId = trimmedGroupId;
    item.titleSnapshot = groupName.trimmed().isEmpty()
                             ? trimmedGroupId
                             : groupName.trimmed();
    item.previewSnapshot = announcement.trimmed().isEmpty()
                               ? QStringLiteral("\u7FA4\u516C\u544A\u63D0\u9192")
                               : announcement.trimmed().left(160);
    item.note = note.trimmed();
    item.dueAtMs = dueAtMs;
    item.payloadJson = compactPayload(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("announcement")},
    });
    return item;
}

std::optional<ReminderItem> makeGroupFileReminderItem(const QString& groupId,
                                                      const QString& resourceId,
                                                      const QString& fileName,
                                                      const QString& previewSnapshot,
                                                      const QString& note,
                                                      qint64 dueAtMs,
                                                      const QDateTime& now)
{
    const QString trimmedGroupId = groupId.trimmed();
    const QString trimmedResourceId = resourceId.trimmed();
    if (trimmedGroupId.isEmpty() || trimmedResourceId.isEmpty()) {
        return std::nullopt;
    }

    ReminderItem item = baseReminder(now);
    item.targetType = QStringLiteral("group_file");
    item.targetId = trimmedResourceId;
    item.groupId = trimmedGroupId;
    item.resourceId = trimmedResourceId;
    item.titleSnapshot = fileName.trimmed().isEmpty()
                             ? trimmedResourceId
                             : fileName.trimmed();
    item.previewSnapshot = previewSnapshot.trimmed().isEmpty()
                               ? QStringLiteral("\u7FA4\u6587\u4EF6\u63D0\u9192")
                               : previewSnapshot.trimmed().left(160);
    item.note = note.trimmed();
    item.dueAtMs = dueAtMs;
    item.payloadJson = compactPayload(QJsonObject{
        {QStringLiteral("kind"), QStringLiteral("group_file")},
        {QStringLiteral("groupId"), trimmedGroupId},
        {QStringLiteral("resourceId"), trimmedResourceId},
        {QStringLiteral("fileName"), item.titleSnapshot},
    });
    return item;
}
