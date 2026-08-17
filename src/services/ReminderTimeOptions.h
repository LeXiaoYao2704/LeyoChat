#pragma once

#include <QDateTime>

namespace ReminderTimeOptions {

QDateTime thirtyMinutesLater(const QDateTime& now);
QDateTime oneHourLater(const QDateTime& now);
QDateTime tomorrowAtNine(const QDateTime& now);
bool isValidDueTime(const QDateTime& due, const QDateTime& now);

}
