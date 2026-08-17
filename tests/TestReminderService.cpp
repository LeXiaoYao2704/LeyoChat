#include <QtTest/QSignalSpy>
#include <QtTest/QTest>

#include <optional>

#include <QDateTime>
#include <QTemporaryDir>
#include <QUuid>

#include "domain/ReminderItem.h"
#include "services/ReminderService.h"
#include "storage/DatabaseManager.h"
#include "storage/ReminderRepository.h"

namespace {
QString uniqueConnectionName()
{
    return QStringLiteral("reminder-service-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}

ReminderItem serviceReminder(const QString& reminderId, qint64 dueAtMs)
{
    ReminderItem item;
    item.reminderId = reminderId;
    item.targetType = QStringLiteral("message");
    item.targetId = QStringLiteral("msg-1");
    item.conversationId = QStringLiteral("conv-1");
    item.titleSnapshot = QStringLiteral("Conversation");
    item.previewSnapshot = QStringLiteral("Reply to this");
    item.dueAtMs = dueAtMs;
    item.createdAtMs = 1000;
    item.updatedAtMs = 1000;
    item.state = QStringLiteral("scheduled");
    item.sourceMessageId = QStringLiteral("msg-1");
    return item;
}
}

class TestReminderService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void scheduleReminderGeneratesIdAndPersists();
    void scheduleReminderTrimsProvidedReminderId();
    void scheduleReminderRejectsPastDueTime();
    void scheduleReminderRejectsEmptyTargetId();
    void checkNowEmitsDueReminderOnceAndMarksFired();
    void completionDismissAndSnoozeUpdateState();
    void terminalReminderActionsDoNotRewriteStateOrEmitChanges();
    void invalidReminderActionsDoNotEmitChanges();
};

void TestReminderService::initTestCase()
{
    qRegisterMetaType<ReminderItem>("ReminderItem");
}

void TestReminderService::scheduleReminderGeneratesIdAndPersists()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("schedule.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);
    ReminderService service(&repository);
    QSignalSpy scheduledSpy(&service, &ReminderService::reminderScheduled);
    QSignalSpy changedSpy(&service, &ReminderService::remindersChanged);

    ReminderItem item = serviceReminder(QString(), QDateTime::currentMSecsSinceEpoch() + 3600000);
    QString errorMessage;
    QVERIFY(service.scheduleReminder(item, &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(scheduledSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);

    const ReminderItem emitted = qvariant_cast<ReminderItem>(scheduledSpy.takeFirst().at(0));
    QVERIFY(!emitted.reminderId.trimmed().isEmpty());
    QCOMPARE(emitted.state, QStringLiteral("scheduled"));
    QVERIFY(emitted.createdAtMs > 0);
    QVERIFY(emitted.updatedAtMs >= emitted.createdAtMs);

    const std::optional<ReminderItem> stored = repository.findReminderById(emitted.reminderId);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->targetType, QStringLiteral("message"));
    QCOMPARE(stored->conversationId, QStringLiteral("conv-1"));
    QCOMPARE(stored->sourceMessageId, QStringLiteral("msg-1"));
}

void TestReminderService::scheduleReminderTrimsProvidedReminderId()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("trim-id.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);
    ReminderService service(&repository);
    QSignalSpy scheduledSpy(&service, &ReminderService::reminderScheduled);

    ReminderItem item =
        serviceReminder(QStringLiteral(" custom-reminder "),
                        QDateTime::currentMSecsSinceEpoch() + 3600000);

    QString errorMessage;
    QVERIFY(service.scheduleReminder(item, &errorMessage));
    QVERIFY(errorMessage.isEmpty());
    QCOMPARE(scheduledSpy.count(), 1);

    const ReminderItem emitted = qvariant_cast<ReminderItem>(scheduledSpy.takeFirst().at(0));
    QCOMPARE(emitted.reminderId, QStringLiteral("custom-reminder"));
    const std::optional<ReminderItem> stored =
        repository.findReminderById(QStringLiteral("custom-reminder"));
    QVERIFY(stored.has_value());
    QCOMPARE(stored->reminderId, QStringLiteral("custom-reminder"));
}

void TestReminderService::scheduleReminderRejectsPastDueTime()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("reject.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);
    ReminderService service(&repository);
    QSignalSpy scheduledSpy(&service, &ReminderService::reminderScheduled);

    ReminderItem item = serviceReminder(QStringLiteral("past"), QDateTime::currentMSecsSinceEpoch() - 1);
    QString errorMessage;
    QVERIFY(!service.scheduleReminder(item, &errorMessage));
    QVERIFY(!errorMessage.trimmed().isEmpty());
    QCOMPARE(scheduledSpy.count(), 0);
    QVERIFY(!repository.findReminderById(QStringLiteral("past")).has_value());
}

void TestReminderService::scheduleReminderRejectsEmptyTargetId()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("empty-target.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);
    ReminderService service(&repository);
    QSignalSpy scheduledSpy(&service, &ReminderService::reminderScheduled);

    ReminderItem item =
        serviceReminder(QStringLiteral("empty-target"),
                        QDateTime::currentMSecsSinceEpoch() + 3600000);
    item.targetId = QStringLiteral("  ");

    QString errorMessage;
    QVERIFY(!service.scheduleReminder(item, &errorMessage));
    QVERIFY(errorMessage.contains(QStringLiteral("target"), Qt::CaseInsensitive));
    QCOMPARE(scheduledSpy.count(), 0);
    QVERIFY(!repository.findReminderById(QStringLiteral("empty-target")).has_value());
}

void TestReminderService::checkNowEmitsDueReminderOnceAndMarksFired()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("due.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);
    QVERIFY(repository.upsertReminder(
        serviceReminder(QStringLiteral("due-reminder"), QDateTime::currentMSecsSinceEpoch() - 1000)));

    ReminderService service(&repository);
    QSignalSpy dueSpy(&service, &ReminderService::reminderDue);
    QSignalSpy changedSpy(&service, &ReminderService::remindersChanged);

    service.checkNow();
    QCOMPARE(dueSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    const ReminderItem emitted = qvariant_cast<ReminderItem>(dueSpy.at(0).at(0));
    QCOMPARE(emitted.reminderId, QStringLiteral("due-reminder"));
    QCOMPARE(emitted.state, QStringLiteral("due"));
    QVERIFY(emitted.firedAtMs > 0);

    const std::optional<ReminderItem> stored = repository.findReminderById(QStringLiteral("due-reminder"));
    QVERIFY(stored.has_value());
    QCOMPARE(stored->state, QStringLiteral("due"));
    QVERIFY(stored->firedAtMs > 0);

    service.checkNow();
    QCOMPARE(dueSpy.count(), 1);
}

void TestReminderService::completionDismissAndSnoozeUpdateState()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("mutations.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);
    QVERIFY(repository.upsertReminder(
        serviceReminder(QStringLiteral("mutable"), QDateTime::currentMSecsSinceEpoch() + 3600000)));
    QVERIFY(repository.upsertReminder(
        serviceReminder(QStringLiteral("dismissible"), QDateTime::currentMSecsSinceEpoch() + 3600000)));

    ReminderService service(&repository);
    QSignalSpy changedSpy(&service, &ReminderService::remindersChanged);

    const qint64 snoozedDueAtMs = QDateTime::currentMSecsSinceEpoch() + 7200000;
    QVERIFY(service.snooze(QStringLiteral("mutable"), snoozedDueAtMs));
    std::optional<ReminderItem> loaded = repository.findReminderById(QStringLiteral("mutable"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("scheduled"));
    QCOMPARE(loaded->dueAtMs, snoozedDueAtMs);

    QVERIFY(service.markDone(QStringLiteral("mutable")));
    loaded = repository.findReminderById(QStringLiteral("mutable"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("done"));
    QVERIFY(loaded->completedAtMs > 0);

    QVERIFY(service.dismiss(QStringLiteral("dismissible")));
    loaded = repository.findReminderById(QStringLiteral("dismissible"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("dismissed"));
    QCOMPARE(changedSpy.count(), 3);
}

void TestReminderService::terminalReminderActionsDoNotRewriteStateOrEmitChanges()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("terminal-actions.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);

    ReminderItem completed =
        serviceReminder(QStringLiteral("completed"), QDateTime::currentMSecsSinceEpoch() + 3600000);
    completed.state = QStringLiteral("done");
    completed.completedAtMs = 2000;
    completed.updatedAtMs = 2000;
    QVERIFY(repository.upsertReminder(completed));

    ReminderItem dismissed =
        serviceReminder(QStringLiteral("dismissed"), QDateTime::currentMSecsSinceEpoch() + 3600000);
    dismissed.state = QStringLiteral("dismissed");
    dismissed.updatedAtMs = 3000;
    QVERIFY(repository.upsertReminder(dismissed));

    ReminderService service(&repository);
    QSignalSpy changedSpy(&service, &ReminderService::remindersChanged);

    QVERIFY(!service.dismiss(QStringLiteral("completed")));
    QVERIFY(!service.markDone(QStringLiteral("dismissed")));
    QCOMPARE(changedSpy.count(), 0);

    std::optional<ReminderItem> loaded = repository.findReminderById(QStringLiteral("completed"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("done"));
    QCOMPARE(loaded->completedAtMs, 2000);
    QCOMPARE(loaded->updatedAtMs, 2000);

    loaded = repository.findReminderById(QStringLiteral("dismissed"));
    QVERIFY(loaded.has_value());
    QCOMPARE(loaded->state, QStringLiteral("dismissed"));
    QCOMPARE(loaded->completedAtMs, 0);
    QCOMPARE(loaded->updatedAtMs, 3000);
}

void TestReminderService::invalidReminderActionsDoNotEmitChanges()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString connectionName = uniqueConnectionName();
    DatabaseManager manager(tempDir.filePath(QStringLiteral("invalid-actions.db")), connectionName);
    QVERIFY(manager.open());
    ReminderRepository repository(connectionName);
    ReminderService service(&repository);
    QSignalSpy changedSpy(&service, &ReminderService::remindersChanged);

    const qint64 futureDueAtMs = QDateTime::currentMSecsSinceEpoch() + 3600000;

    QVERIFY(!service.markDone(QString()));
    QVERIFY(!service.markDone(QStringLiteral("missing")));
    QVERIFY(!service.dismiss(QStringLiteral("   ")));
    QVERIFY(!service.dismiss(QStringLiteral("missing")));
    QVERIFY(!service.snooze(QStringLiteral("   "), futureDueAtMs));
    QVERIFY(!service.snooze(QStringLiteral("missing"), futureDueAtMs));
    QCOMPARE(changedSpy.count(), 0);
}

QTEST_MAIN(TestReminderService)
#include "TestReminderService.moc"
