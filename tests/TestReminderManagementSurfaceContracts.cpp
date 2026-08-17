#include <QtTest/QtTest>

#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>

#include "app/ReminderActionRouting.h"
#include "domain/ReminderItem.h"
#include "ui/NotificationsPage.h"

namespace {

QString readSourceFile(const QString& relativePath)
{
    const QString path = QFINDTESTDATA(relativePath.toUtf8().constData());
    if (path.isEmpty()) {
        return {};
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

} // namespace

class TestReminderManagementSurfaceContracts : public QObject {
    Q_OBJECT

private slots:
    void notificationsPageActiveReminderActionsEmitExpectedSignals()
    {
        NotificationsPage page;
        QSignalSpy openSpy(&page, &NotificationsPage::messageUrlOpenRequested);
        QSignalSpy doneSpy(&page, &NotificationsPage::reminderDoneRequested);
        QSignalSpy snoozeSpy(&page, &NotificationsPage::reminderSnoozeRequested);

        ReminderItem reminder;
        reminder.reminderId = QStringLiteral("reminder/with space");
        reminder.targetType = QStringLiteral("message");
        reminder.titleSnapshot = QStringLiteral("回复张三");
        reminder.previewSnapshot = QStringLiteral("确认合同附件");
        reminder.dueAtMs = 333;

        page.setActiveReminders({reminder});

        auto* reminderList = page.findChild<QListWidget*>(QStringLiteral("activeReminderList"));
        QVERIFY(reminderList != nullptr);
        QCOMPARE(reminderList->count(), 1);

        auto* openButton = reminderList->findChild<QPushButton*>(QStringLiteral("activeReminderOpenButton"));
        auto* doneButton = reminderList->findChild<QPushButton*>(QStringLiteral("activeReminderDoneButton"));
        auto* snoozeButton = reminderList->findChild<QPushButton*>(QStringLiteral("activeReminderSnoozeButton"));
        QVERIFY(openButton != nullptr);
        QVERIFY(doneButton != nullptr);
        QVERIFY(snoozeButton != nullptr);

        openButton->click();
        doneButton->click();
        snoozeButton->click();

        QCOMPARE(openSpy.count(), 1);
        QCOMPARE(openSpy.takeFirst().at(0).toString(),
                 reminderActionUrl(QStringLiteral("open"), reminder.reminderId));
        QCOMPARE(doneSpy.count(), 1);
        QCOMPARE(doneSpy.takeFirst().at(0).toString(), reminder.reminderId);
        QCOMPARE(snoozeSpy.count(), 1);
        const QList<QVariant> snoozeArgs = snoozeSpy.takeFirst();
        QCOMPARE(snoozeArgs.at(0).toString(), reminder.reminderId);
        QCOMPARE(snoozeArgs.at(1).toInt(), 30);
    }

    void notificationsPageClearsActiveReminderSurfaceWhenNoRemindersRemain()
    {
        NotificationsPage page;

        ReminderItem reminder;
        reminder.reminderId = QStringLiteral("r-active");
        reminder.targetType = QStringLiteral("message");
        reminder.titleSnapshot = QStringLiteral("active reminder");
        reminder.previewSnapshot = QStringLiteral("reply later");
        reminder.dueAtMs = 1000;

        page.setActiveReminders({reminder});

        auto* section = page.findChild<QWidget*>(QStringLiteral("activeRemindersSection"));
        auto* reminderList = page.findChild<QListWidget*>(QStringLiteral("activeReminderList"));
        QVERIFY(section != nullptr);
        QVERIFY(reminderList != nullptr);
        QVERIFY(!section->isHidden());
        QCOMPARE(reminderList->count(), 1);

        page.setActiveReminders({});

        QVERIFY(section->isHidden());
        QCOMPARE(reminderList->count(), 0);
    }

    void notificationsPageSortsActiveRemindersByDueTimeThenId()
    {
        NotificationsPage page;

        ReminderItem later;
        later.reminderId = QStringLiteral("r-later");
        later.targetType = QStringLiteral("message");
        later.dueAtMs = 3000;

        ReminderItem beta;
        beta.reminderId = QStringLiteral("r-beta");
        beta.targetType = QStringLiteral("contact");
        beta.dueAtMs = 1000;

        ReminderItem alpha;
        alpha.reminderId = QStringLiteral("r-alpha");
        alpha.targetType = QStringLiteral("group_file");
        alpha.dueAtMs = 1000;

        page.setActiveReminders({later, beta, alpha});

        auto* reminderList = page.findChild<QListWidget*>(QStringLiteral("activeReminderList"));
        QVERIFY(reminderList != nullptr);
        QCOMPARE(reminderList->count(), 3);
        QCOMPARE(reminderList->item(0)->data(Qt::UserRole).toString(), QStringLiteral("r-alpha"));
        QCOMPARE(reminderList->item(1)->data(Qt::UserRole).toString(), QStringLiteral("r-beta"));
        QCOMPARE(reminderList->item(2)->data(Qt::UserRole).toString(), QStringLiteral("r-later"));
    }

    void activeReminderButtonsIgnoreBlankReminderIds()
    {
        NotificationsPage page;
        QSignalSpy openSpy(&page, &NotificationsPage::messageUrlOpenRequested);
        QSignalSpy doneSpy(&page, &NotificationsPage::reminderDoneRequested);
        QSignalSpy snoozeSpy(&page, &NotificationsPage::reminderSnoozeRequested);

        ReminderItem reminder;
        reminder.reminderId = QStringLiteral("   ");
        reminder.targetType = QStringLiteral("message");
        reminder.titleSnapshot = QStringLiteral("blank id reminder");
        reminder.previewSnapshot = QStringLiteral("must not emit actions");
        reminder.dueAtMs = 1000;

        page.setActiveReminders({reminder});

        auto* reminderList = page.findChild<QListWidget*>(QStringLiteral("activeReminderList"));
        QVERIFY(reminderList != nullptr);
        QCOMPARE(reminderList->count(), 1);

        auto* openButton = reminderList->findChild<QPushButton*>(QStringLiteral("activeReminderOpenButton"));
        auto* doneButton = reminderList->findChild<QPushButton*>(QStringLiteral("activeReminderDoneButton"));
        auto* snoozeButton = reminderList->findChild<QPushButton*>(QStringLiteral("activeReminderSnoozeButton"));
        QVERIFY(openButton != nullptr);
        QVERIFY(doneButton != nullptr);
        QVERIFY(snoozeButton != nullptr);

        openButton->click();
        doneButton->click();
        snoozeButton->click();

        QCOMPARE(openSpy.count(), 0);
        QCOMPARE(doneSpy.count(), 0);
        QCOMPARE(snoozeSpy.count(), 0);
    }

    void applicationRefreshesActiveReminderSurface()
    {
        const QString source = readSourceFile(QStringLiteral("../src/app/LeyoApplication.cpp"));
        QVERIFY2(!source.isEmpty(), "LeyoApplication.cpp must be available");

        QVERIFY2(source.contains(QStringLiteral("reminderRepository.loadActiveReminders")),
                 "Application must load active reminders for the management surface");
        QVERIFY2(source.contains(QStringLiteral("m_mainWindow->setActiveReminders")),
                 "Application must push active reminders to MainWindow");
        QVERIFY2(source.contains(QStringLiteral("&ReminderService::remindersChanged")),
                 "Application must refresh active reminders when reminder state changes");
        QVERIFY2(source.contains(QStringLiteral("&MainWindow::reminderDoneRequested")),
                 "Application must handle reminder completion from NotificationsPage");
        QVERIFY2(source.contains(QStringLiteral("&MainWindow::reminderSnoozeRequested")),
                 "Application must handle reminder snooze from NotificationsPage");
    }

    void reminderOpenRouteReturnsToMessagesPageAndRetriesMessageJump()
    {
        const QString appSource = readSourceFile(QStringLiteral("../src/app/LeyoApplication.cpp"));
        const QString windowHeader = readSourceFile(QStringLiteral("../src/ui/MainWindow.h"));
        const QString windowSource = readSourceFile(QStringLiteral("../src/ui/MainWindow.cpp"));
        QVERIFY2(!appSource.isEmpty(), "LeyoApplication.cpp must be available");
        QVERIFY2(!windowHeader.isEmpty(), "MainWindow.h must be available");
        QVERIFY2(!windowSource.isEmpty(), "MainWindow.cpp must be available");

        QVERIFY2(windowHeader.contains(QStringLiteral("void showMessagesPage();")),
                 "MainWindow must expose an explicit way for reminder actions to return to the messages page");
        QVERIFY2(windowSource.contains(QStringLiteral("void MainWindow::showMessagesPage()")),
                 "MainWindow must implement the messages-page navigation helper");
        QVERIFY2(windowSource.contains(QStringLiteral("navigation(m_conversationsPageKey)")),
                 "The messages-page helper must navigate to the conversations page");
        QVERIFY2(appSource.contains(QStringLiteral("m_mainWindow->showMessagesPage();")),
                 "Reminder open actions must return to the messages page before selecting a conversation");
        QVERIFY2(appSource.contains(QStringLiteral("kMessageJumpMaxAttempts")),
                 "Reminder message jumps must retry while the target conversation messages load");
        QVERIFY2(appSource.contains(QStringLiteral("scrollToMessageWhenAvailable")),
                 "Message jump retry logic must be centralized and reused by reminder opens");
    }
};

QTEST_MAIN(TestReminderManagementSurfaceContracts)
#include "TestReminderManagementSurfaceContracts.moc"
