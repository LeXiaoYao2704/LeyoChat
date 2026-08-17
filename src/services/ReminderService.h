#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "domain/ReminderItem.h"

class ReminderRepository;

class ReminderService : public QObject {
    Q_OBJECT

public:
    explicit ReminderService(ReminderRepository* repository, QObject* parent = nullptr);

    void start();
    void stop();
    void checkNow();
    bool scheduleReminder(ReminderItem item, QString* errorMessage = nullptr);
    bool markDone(const QString& reminderId);
    bool dismiss(const QString& reminderId);
    bool snooze(const QString& reminderId, qint64 dueAtMs);

signals:
    void reminderScheduled(const ReminderItem& item);
    void reminderDue(const ReminderItem& item);
    void remindersChanged();

private:
    void armNextTimer();

    ReminderRepository* m_repository = nullptr;
    QTimer m_timer;
};
