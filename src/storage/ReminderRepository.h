#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "domain/ReminderItem.h"

class ReminderRepository {
public:
    explicit ReminderRepository(QString connectionName);

    bool upsertReminder(const ReminderItem& item) const;
    std::optional<ReminderItem> findReminderById(const QString& reminderId) const;
    std::vector<ReminderItem> loadActiveReminders(int limit = 500) const;
    std::vector<ReminderItem> loadDueReminders(qint64 nowMs, int limit = 50) const;
    bool markFired(const QString& reminderId, qint64 firedAtMs) const;
    bool markDone(const QString& reminderId, qint64 completedAtMs) const;
    bool dismissReminder(const QString& reminderId, qint64 updatedAtMs) const;
    bool rescheduleReminder(const QString& reminderId, qint64 dueAtMs, qint64 updatedAtMs) const;
    bool deleteReminder(const QString& reminderId) const;

private:
    QString m_connectionName;
};
