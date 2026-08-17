#include "services/ReminderService.h"

#include <QDateTime>
#include <QUuid>

#include "services/ReminderTimeOptions.h"
#include "storage/ReminderRepository.h"

namespace {
constexpr int kReminderPollIntervalMs = 60 * 1000;

void setError(QString* errorMessage, const QString& value)
{
    if (errorMessage) {
        *errorMessage = value;
    }
}
}

ReminderService::ReminderService(ReminderRepository* repository, QObject* parent)
    : QObject(parent)
    , m_repository(repository)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &ReminderService::checkNow);
}

void ReminderService::start()
{
    checkNow();
    armNextTimer();
}

void ReminderService::stop()
{
    m_timer.stop();
}

void ReminderService::checkNow()
{
    if (!m_repository) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const std::vector<ReminderItem> dueReminders = m_repository->loadDueReminders(nowMs);
    for (ReminderItem reminder : dueReminders) {
        if (!m_repository->markFired(reminder.reminderId, nowMs)) {
            continue;
        }

        reminder.state = QStringLiteral("due");
        reminder.firedAtMs = nowMs;
        reminder.updatedAtMs = nowMs;
        emit reminderDue(reminder);
        emit remindersChanged();
    }

    armNextTimer();
}

bool ReminderService::scheduleReminder(ReminderItem item, QString* errorMessage)
{
    if (!m_repository) {
        setError(errorMessage, QStringLiteral("Reminder storage is unavailable."));
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const QDateTime now = QDateTime::fromMSecsSinceEpoch(nowMs);
    const QDateTime due = QDateTime::fromMSecsSinceEpoch(item.dueAtMs);
    if (!ReminderTimeOptions::isValidDueTime(due, now)) {
        setError(errorMessage, QStringLiteral("Reminder time must be in the future."));
        return false;
    }
    if (item.targetType.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("Reminder target type is required."));
        return false;
    }
    if (item.targetId.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("Reminder target id is required."));
        return false;
    }

    if (item.reminderId.trimmed().isEmpty()) {
        item.reminderId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    } else {
        item.reminderId = item.reminderId.trimmed();
    }
    item.targetType = item.targetType.trimmed();
    item.targetId = item.targetId.trimmed();
    item.state = QStringLiteral("scheduled");
    item.firedAtMs = 0;
    item.completedAtMs = 0;
    if (item.createdAtMs <= 0) {
        item.createdAtMs = nowMs;
    }
    item.updatedAtMs = nowMs;

    if (!m_repository->upsertReminder(item)) {
        setError(errorMessage, QStringLiteral("Failed to save reminder."));
        return false;
    }

    setError(errorMessage, QString());
    emit reminderScheduled(item);
    emit remindersChanged();
    armNextTimer();
    return true;
}

bool ReminderService::markDone(const QString& reminderId)
{
    if (!m_repository) {
        return false;
    }

    const bool ok = m_repository->markDone(reminderId, QDateTime::currentMSecsSinceEpoch());
    if (ok) {
        emit remindersChanged();
    }
    return ok;
}

bool ReminderService::dismiss(const QString& reminderId)
{
    if (!m_repository) {
        return false;
    }

    const bool ok = m_repository->dismissReminder(reminderId, QDateTime::currentMSecsSinceEpoch());
    if (ok) {
        emit remindersChanged();
    }
    return ok;
}

bool ReminderService::snooze(const QString& reminderId, qint64 dueAtMs)
{
    if (!m_repository) {
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!ReminderTimeOptions::isValidDueTime(QDateTime::fromMSecsSinceEpoch(dueAtMs),
                                             QDateTime::fromMSecsSinceEpoch(nowMs))) {
        return false;
    }

    const bool ok = m_repository->rescheduleReminder(reminderId, dueAtMs, nowMs);
    if (ok) {
        emit remindersChanged();
        armNextTimer();
    }
    return ok;
}

void ReminderService::armNextTimer()
{
    if (!m_repository) {
        return;
    }
    if (m_timer.isActive()) {
        m_timer.stop();
    }
    m_timer.start(kReminderPollIntervalMs);
}
