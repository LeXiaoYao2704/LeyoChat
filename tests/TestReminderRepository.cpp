#include <QtTest/QTest>

#include <optional>
#include <vector>

#include <QTemporaryDir>
#include <QUuid>

#include "domain/ReminderItem.h"
#include "storage/DatabaseManager.h"
#include "storage/ReminderRepository.h"

namespace {
QString uniqueConnectionName()
{
    return QStringLiteral("reminder-repository-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

ReminderItem baseReminder(const QString& reminderId, qint64 dueAtMs)
{
    ReminderItem item;
    item.reminderId = reminderId;
    item.targetType = QStringLiteral("message");
    item.targetId = QStringLiteral("msg-1");
    item.conversationId = QStringLiteral("conv-1");
    item.groupId = QStringLiteral("group-1");
    item.contactId = QStringLiteral("contact-1");
    item.resourceId = QStringLiteral("resource-1");
    item.titleSnapshot = QStringLiteral("Conversation title");
    item.previewSnapshot = QStringLiteral("Message preview");
    item.note = QStringLiteral("Reply later");
    item.dueAtMs = dueAtMs;
    item.createdAtMs = 1000;
    item.updatedAtMs = 1000;
    item.state = QStringLiteral("scheduled");
    item.sourceMessageId = QStringLiteral("msg-1");
    item.payloadJson = QStringLiteral(R"({"kind":"message"})");
    return item;
}
}

class TestReminderRepository : public QObject {
    Q_OBJECT

private slots:
    void upsertAndFindRoundTripsReminder();
    void loadActiveRemindersReturnsScheduledAndDueOnly();
    void loadDueRemindersFiltersByStateDueTimeAndLimit();
    void stateMutationsUpdateStoredReminder();
    void markFiredDoesNotReactivateCompletedReminder();
    void rescheduleDoesNotReactivateCompletedReminder();
    void markDoneDoesNotRewriteDismissedReminder();
    void dismissDoesNotRewriteCompletedReminder();
    void deleteReminderRemovesStoredReminder();
};

void TestReminderRepository::upsertAndFindRoundTripsReminder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    const ReminderItem item = baseReminder(QStringLiteral("reminder-1"), 3000);
    QVERIFY(repository.upsertReminder(item));

    const std::optional<ReminderItem> loaded = repository.findReminderById(QStringLiteral("reminder-1"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->reminderId, item.reminderId);
    QCOMPARE(loaded->targetType, item.targetType);
    QCOMPARE(loaded->targetId, item.targetId);
    QCOMPARE(loaded->conversationId, item.conversationId);
    QCOMPARE(loaded->groupId, item.groupId);
    QCOMPARE(loaded->contactId, item.contactId);
    QCOMPARE(loaded->resourceId, item.resourceId);
    QCOMPARE(loaded->titleSnapshot, item.titleSnapshot);
    QCOMPARE(loaded->previewSnapshot, item.previewSnapshot);
    QCOMPARE(loaded->note, item.note);
    QCOMPARE(loaded->dueAtMs, item.dueAtMs);
    QCOMPARE(loaded->createdAtMs, item.createdAtMs);
    QCOMPARE(loaded->updatedAtMs, item.updatedAtMs);
    QCOMPARE(loaded->state, item.state);
    QCOMPARE(loaded->sourceMessageId, item.sourceMessageId);
    QCOMPARE(loaded->payloadJson, item.payloadJson);
}

void TestReminderRepository::loadActiveRemindersReturnsScheduledAndDueOnly()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("active-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    ReminderItem scheduled = baseReminder(QStringLiteral("scheduled"), 3000);
    ReminderItem due = baseReminder(QStringLiteral("due"), 2000);
    due.state = QStringLiteral("due");
    ReminderItem done = baseReminder(QStringLiteral("done"), 1000);
    done.state = QStringLiteral("done");

    QVERIFY(repository.upsertReminder(scheduled));
    QVERIFY(repository.upsertReminder(due));
    QVERIFY(repository.upsertReminder(done));

    const std::vector<ReminderItem> active = repository.loadActiveReminders();
    QCOMPARE(active.size(), 2u);
    QCOMPARE(active[0].reminderId, QStringLiteral("due"));
    QCOMPARE(active[1].reminderId, QStringLiteral("scheduled"));
}

void TestReminderRepository::loadDueRemindersFiltersByStateDueTimeAndLimit()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("due-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    QVERIFY(repository.upsertReminder(baseReminder(QStringLiteral("due-1"), 1000)));
    QVERIFY(repository.upsertReminder(baseReminder(QStringLiteral("due-2"), 2000)));
    QVERIFY(repository.upsertReminder(baseReminder(QStringLiteral("future"), 5000)));
    ReminderItem dismissed = baseReminder(QStringLiteral("dismissed"), 500);
    dismissed.state = QStringLiteral("dismissed");
    QVERIFY(repository.upsertReminder(dismissed));

    const std::vector<ReminderItem> due = repository.loadDueReminders(2500, 1);
    QCOMPARE(due.size(), 1u);
    QCOMPARE(due[0].reminderId, QStringLiteral("due-1"));
}

void TestReminderRepository::stateMutationsUpdateStoredReminder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("state-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    QVERIFY(repository.upsertReminder(baseReminder(QStringLiteral("stateful"), 1000)));

    QVERIFY(repository.markFired(QStringLiteral("stateful"), 2000));
    std::optional<ReminderItem> loaded = repository.findReminderById(QStringLiteral("stateful"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("due"));
    QCOMPARE(loaded->firedAtMs, 2000);

    QVERIFY(repository.rescheduleReminder(QStringLiteral("stateful"), 4000, 3000));
    loaded = repository.findReminderById(QStringLiteral("stateful"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("scheduled"));
    QCOMPARE(loaded->dueAtMs, 4000);
    QCOMPARE(loaded->updatedAtMs, 3000);
    QCOMPARE(loaded->firedAtMs, 0);

    QVERIFY(repository.markDone(QStringLiteral("stateful"), 5000));
    loaded = repository.findReminderById(QStringLiteral("stateful"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("done"));
    QCOMPARE(loaded->completedAtMs, 5000);

    QVERIFY(repository.upsertReminder(baseReminder(QStringLiteral("dismissible"), 7000)));
    QVERIFY(repository.dismissReminder(QStringLiteral("dismissible"), 6000));
    loaded = repository.findReminderById(QStringLiteral("dismissible"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("dismissed"));
    QCOMPARE(loaded->updatedAtMs, 6000);
}

void TestReminderRepository::markFiredDoesNotReactivateCompletedReminder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("fired-state-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    ReminderItem completed = baseReminder(QStringLiteral("completed"), 1000);
    completed.state = QStringLiteral("done");
    completed.completedAtMs = 2000;
    completed.updatedAtMs = 2000;
    QVERIFY(repository.upsertReminder(completed));

    QVERIFY(!repository.markFired(QStringLiteral("completed"), 3000));

    const std::optional<ReminderItem> loaded =
        repository.findReminderById(QStringLiteral("completed"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("done"));
    QCOMPARE(loaded->completedAtMs, 2000);
    QCOMPARE(loaded->firedAtMs, 0);
    QCOMPARE(loaded->updatedAtMs, 2000);
}

void TestReminderRepository::rescheduleDoesNotReactivateCompletedReminder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("reschedule-state-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    ReminderItem completed = baseReminder(QStringLiteral("completed"), 1000);
    completed.state = QStringLiteral("done");
    completed.completedAtMs = 2000;
    completed.updatedAtMs = 2000;
    QVERIFY(repository.upsertReminder(completed));

    QVERIFY(!repository.rescheduleReminder(QStringLiteral("completed"), 4000, 3000));

    const std::optional<ReminderItem> loaded =
        repository.findReminderById(QStringLiteral("completed"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("done"));
    QCOMPARE(loaded->dueAtMs, 1000);
    QCOMPARE(loaded->completedAtMs, 2000);
    QCOMPARE(loaded->updatedAtMs, 2000);
}

void TestReminderRepository::markDoneDoesNotRewriteDismissedReminder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("done-state-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    ReminderItem dismissed = baseReminder(QStringLiteral("dismissed"), 1000);
    dismissed.state = QStringLiteral("dismissed");
    dismissed.updatedAtMs = 2000;
    QVERIFY(repository.upsertReminder(dismissed));

    QVERIFY(!repository.markDone(QStringLiteral("dismissed"), 3000));

    const std::optional<ReminderItem> loaded =
        repository.findReminderById(QStringLiteral("dismissed"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("dismissed"));
    QCOMPARE(loaded->completedAtMs, 0);
    QCOMPARE(loaded->updatedAtMs, 2000);
}

void TestReminderRepository::dismissDoesNotRewriteCompletedReminder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("dismiss-state-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    ReminderItem completed = baseReminder(QStringLiteral("completed"), 1000);
    completed.state = QStringLiteral("done");
    completed.completedAtMs = 2000;
    completed.updatedAtMs = 2000;
    QVERIFY(repository.upsertReminder(completed));

    QVERIFY(!repository.dismissReminder(QStringLiteral("completed"), 3000));

    const std::optional<ReminderItem> loaded =
        repository.findReminderById(QStringLiteral("completed"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("done"));
    QCOMPARE(loaded->completedAtMs, 2000);
    QCOMPARE(loaded->updatedAtMs, 2000);
}

void TestReminderRepository::deleteReminderRemovesStoredReminder()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("delete-reminders.db")), connectionName);
    QVERIFY(manager.open());

    ReminderRepository repository(connectionName);
    QVERIFY(repository.upsertReminder(baseReminder(QStringLiteral("delete-me"), 1000)));
    QVERIFY(repository.deleteReminder(QStringLiteral("delete-me")));
    QVERIFY(!repository.findReminderById(QStringLiteral("delete-me")).has_value());
    QVERIFY(!repository.deleteReminder(QStringLiteral("delete-me")));
}

QTEST_MAIN(TestReminderRepository)
#include "TestReminderRepository.moc"
