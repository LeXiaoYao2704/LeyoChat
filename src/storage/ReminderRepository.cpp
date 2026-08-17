#include "storage/ReminderRepository.h"

#include <utility>

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>

namespace {
ReminderItem reminderFromQuery(const QSqlQuery& query)
{
    ReminderItem item;
    item.reminderId = query.value(0).toString();
    item.targetType = query.value(1).toString();
    item.targetId = query.value(2).toString();
    item.conversationId = query.value(3).toString();
    item.groupId = query.value(4).toString();
    item.contactId = query.value(5).toString();
    item.resourceId = query.value(6).toString();
    item.titleSnapshot = query.value(7).toString();
    item.previewSnapshot = query.value(8).toString();
    item.note = query.value(9).toString();
    item.dueAtMs = query.value(10).toLongLong();
    item.createdAtMs = query.value(11).toLongLong();
    item.updatedAtMs = query.value(12).toLongLong();
    item.firedAtMs = query.value(13).toLongLong();
    item.completedAtMs = query.value(14).toLongLong();
    item.state = query.value(15).toString();
    item.sourceMessageId = query.value(16).toString();
    item.payloadJson = query.value(17).toString();
    return item;
}

QString reminderSelectColumns()
{
    return QStringLiteral(
        "reminder_id, target_type, target_id, conversation_id, group_id, contact_id, "
        "resource_id, title_snapshot, preview_snapshot, note, due_at_ms, created_at_ms, "
        "updated_at_ms, fired_at_ms, completed_at_ms, state, source_message_id, payload_json");
}

bool hasAffectedRows(const QSqlQuery& query)
{
    return query.numRowsAffected() > 0;
}
}

ReminderRepository::ReminderRepository(QString connectionName)
    : m_connectionName(std::move(connectionName))
{
}

bool ReminderRepository::upsertReminder(const ReminderItem& item) const
{
    if (item.reminderId.trimmed().isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT OR REPLACE INTO reminders (
            reminder_id, target_type, target_id, conversation_id, group_id, contact_id,
            resource_id, title_snapshot, preview_snapshot, note, due_at_ms, created_at_ms,
            updated_at_ms, fired_at_ms, completed_at_ms, state, source_message_id, payload_json
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )"));
    query.addBindValue(item.reminderId);
    query.addBindValue(item.targetType);
    query.addBindValue(item.targetId);
    query.addBindValue(item.conversationId);
    query.addBindValue(item.groupId);
    query.addBindValue(item.contactId);
    query.addBindValue(item.resourceId);
    query.addBindValue(item.titleSnapshot);
    query.addBindValue(item.previewSnapshot);
    query.addBindValue(item.note);
    query.addBindValue(item.dueAtMs);
    query.addBindValue(item.createdAtMs);
    query.addBindValue(item.updatedAtMs);
    query.addBindValue(item.firedAtMs);
    query.addBindValue(item.completedAtMs);
    query.addBindValue(item.state);
    query.addBindValue(item.sourceMessageId);
    query.addBindValue(item.payloadJson);
    return query.exec();
}

std::optional<ReminderItem> ReminderRepository::findReminderById(const QString& reminderId) const
{
    const QString trimmedId = reminderId.trimmed();
    if (trimmedId.isEmpty()) {
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("SELECT %1 FROM reminders WHERE reminder_id = ? LIMIT 1")
                      .arg(reminderSelectColumns()));
    query.addBindValue(trimmedId);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    return reminderFromQuery(query);
}

std::vector<ReminderItem> ReminderRepository::loadActiveReminders(int limit) const
{
    std::vector<ReminderItem> reminders;
    if (limit <= 0) {
        return reminders;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT %1
        FROM reminders
        WHERE state IN ('scheduled', 'due')
        ORDER BY due_at_ms ASC, created_at_ms ASC, reminder_id ASC
        LIMIT ?
    )").arg(reminderSelectColumns()));
    query.addBindValue(limit);
    if (!query.exec()) {
        return reminders;
    }

    while (query.next()) {
        reminders.push_back(reminderFromQuery(query));
    }
    return reminders;
}

std::vector<ReminderItem> ReminderRepository::loadDueReminders(qint64 nowMs, int limit) const
{
    std::vector<ReminderItem> reminders;
    if (limit <= 0) {
        return reminders;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT %1
        FROM reminders
        WHERE state = 'scheduled' AND due_at_ms <= ?
        ORDER BY due_at_ms ASC, created_at_ms ASC, reminder_id ASC
        LIMIT ?
    )").arg(reminderSelectColumns()));
    query.addBindValue(nowMs);
    query.addBindValue(limit);
    if (!query.exec()) {
        return reminders;
    }

    while (query.next()) {
        reminders.push_back(reminderFromQuery(query));
    }
    return reminders;
}

bool ReminderRepository::markFired(const QString& reminderId, qint64 firedAtMs) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE reminders
        SET state = 'due',
            fired_at_ms = ?,
            updated_at_ms = ?
        WHERE reminder_id = ?
          AND state = 'scheduled'
    )"));
    query.addBindValue(firedAtMs);
    query.addBindValue(firedAtMs);
    query.addBindValue(reminderId.trimmed());
    return query.exec() && hasAffectedRows(query);
}

bool ReminderRepository::markDone(const QString& reminderId, qint64 completedAtMs) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE reminders
        SET state = 'done',
            completed_at_ms = ?,
            updated_at_ms = ?
        WHERE reminder_id = ?
          AND state IN ('scheduled', 'due')
    )"));
    query.addBindValue(completedAtMs);
    query.addBindValue(completedAtMs);
    query.addBindValue(reminderId.trimmed());
    return query.exec() && hasAffectedRows(query);
}

bool ReminderRepository::dismissReminder(const QString& reminderId, qint64 updatedAtMs) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE reminders
        SET state = 'dismissed',
            updated_at_ms = ?
        WHERE reminder_id = ?
          AND state IN ('scheduled', 'due')
    )"));
    query.addBindValue(updatedAtMs);
    query.addBindValue(reminderId.trimmed());
    return query.exec() && hasAffectedRows(query);
}

bool ReminderRepository::rescheduleReminder(const QString& reminderId, qint64 dueAtMs, qint64 updatedAtMs) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE reminders
        SET state = 'scheduled',
            due_at_ms = ?,
            updated_at_ms = ?,
            fired_at_ms = 0,
            completed_at_ms = 0
        WHERE reminder_id = ?
          AND state IN ('scheduled', 'due')
    )"));
    query.addBindValue(dueAtMs);
    query.addBindValue(updatedAtMs);
    query.addBindValue(reminderId.trimmed());
    return query.exec() && hasAffectedRows(query);
}

bool ReminderRepository::deleteReminder(const QString& reminderId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("DELETE FROM reminders WHERE reminder_id = ?"));
    query.addBindValue(reminderId.trimmed());
    return query.exec() && hasAffectedRows(query);
}
