#include "services/ReminderTimeOptions.h"

#include <QTime>
#include <QTimeZone>

namespace ReminderTimeOptions {

QDateTime thirtyMinutesLater(const QDateTime& now)
{
    return now.addSecs(30 * 60);
}

QDateTime oneHourLater(const QDateTime& now)
{
    return now.addSecs(60 * 60);
}

QDateTime tomorrowAtNine(const QDateTime& now)
{
    const QDate tomorrow = now.date().addDays(1);
    return QDateTime(tomorrow, QTime(9, 0), now.timeZone());
}

bool isValidDueTime(const QDateTime& due, const QDateTime& now)
{
    return due.isValid() && due.toMSecsSinceEpoch() > now.toMSecsSinceEpoch();
}

}
