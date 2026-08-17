#include <QtTest/QtTest>

#include "services/ReminderTimeOptions.h"

class TestReminderTimeOptions : public QObject {
    Q_OBJECT

private slots:
    void quickOptionsUseExpectedOffsets()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(15, 20), QTimeZone::systemTimeZone());

        QCOMPARE(ReminderTimeOptions::thirtyMinutesLater(now), now.addSecs(30 * 60));
        QCOMPARE(ReminderTimeOptions::oneHourLater(now), now.addSecs(60 * 60));
    }

    void tomorrowAtNineUsesLocalDate()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(23, 55), QTimeZone::systemTimeZone());

        const QDateTime due = ReminderTimeOptions::tomorrowAtNine(now);

        QCOMPARE(due.date(), QDate(2026, 6, 29));
        QCOMPARE(due.time(), QTime(9, 0));
        QCOMPARE(due.timeZone(), now.timeZone());
    }

    void validDueTimeMustBeFuture()
    {
        const QDateTime now(QDate(2026, 6, 28), QTime(12, 0), QTimeZone::systemTimeZone());

        QVERIFY(ReminderTimeOptions::isValidDueTime(now.addSecs(1), now));
        QVERIFY(!ReminderTimeOptions::isValidDueTime(now, now));
        QVERIFY(!ReminderTimeOptions::isValidDueTime(now.addSecs(-1), now));
        QVERIFY(!ReminderTimeOptions::isValidDueTime(QDateTime(), now));
    }
};

QTEST_MAIN(TestReminderTimeOptions)
#include "TestReminderTimeOptions.moc"
