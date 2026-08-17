#include <QtTest>

#include "app/ReminderActionRouting.h"
#include "app/ReminderNotificationPresentation.h"

class TestReminderNotificationPresentation : public QObject {
    Q_OBJECT

private slots:
    void buildsNotificationFromDueReminder()
    {
        ReminderItem reminder;
        reminder.reminderId = QStringLiteral("reminder-001");
        reminder.titleSnapshot = QStringLiteral("张三");
        reminder.previewSnapshot = QStringLiteral("稍后回复这条消息");
        reminder.note = QStringLiteral("确认方案");
        reminder.dueAtMs = 123456;

        const SystemNotificationItem item = notificationFromReminder(reminder);

        QCOMPARE(item.notificationId, QStringLiteral("reminder:reminder-001"));
        QCOMPARE(item.sourceKey, QStringLiteral("reminder"));
        QCOMPARE(item.sourceLabel, QStringLiteral("本机提醒"));
        QCOMPARE(item.title, QStringLiteral("张三"));
        QCOMPARE(item.summary, QStringLiteral("稍后回复这条消息"));
        QCOMPARE(item.detail, QStringLiteral("确认方案"));
        QCOMPARE(item.actionLabel, QStringLiteral("查看提醒"));
        QCOMPARE(item.actionUrl, QStringLiteral("leyochat://reminder/open/reminder-001"));
        QCOMPARE(item.occurredAtMs, 123456);
        QVERIFY(item.unread);
    }

    void usesFallbackTextWhenReminderSnapshotsAreEmpty()
    {
        ReminderItem reminder;
        reminder.reminderId = QStringLiteral("reminder-002");
        reminder.dueAtMs = 222;

        const SystemNotificationItem item = notificationFromReminder(reminder);

        QCOMPARE(item.title, QStringLiteral("本机提醒"));
        QCOMPARE(item.summary, QStringLiteral("提醒时间到了"));
        QCOMPARE(item.detail, QStringLiteral("提醒时间到了"));
    }

    void generatedActionUrlRoundTripsReminderIds()
    {
        ReminderItem reminder;
        reminder.reminderId = QStringLiteral("reminder/with space");
        reminder.titleSnapshot = QStringLiteral("复杂 ID 提醒");
        reminder.dueAtMs = 333;

        const SystemNotificationItem item = notificationFromReminder(reminder);
        const auto route = parseReminderActionUrl(item.actionUrl);

        QVERIFY(route.has_value());
        QCOMPARE(route->verb, QStringLiteral("open"));
        QCOMPARE(route->reminderId, reminder.reminderId);
    }
};

QTEST_MAIN(TestReminderNotificationPresentation)

#include "TestReminderNotificationPresentation.moc"
