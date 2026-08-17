#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QtTest>

#include "integrations/OutlookSettings.h"
#include "ui/OutlookNotificationDialog.h"

class TestOutlookNotificationDialog : public QObject {
    Q_OBJECT

private slots:
    void buildsDefaultMailUrlFromSettings()
    {
        OutlookConnectionSettings settings;
        settings.enabled = true;
        settings.accountEmail = QStringLiteral("you@example.com");
        settings.displayName = QStringLiteral("张三");

        OutlookNotificationDialog dialog(settings);
        auto* kindCombo =
            dialog.findChild<QComboBox*>(QStringLiteral("outlookNotificationKindCombo"));
        auto* resourceIdEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("outlookNotificationResourceIdEdit"));
        auto* urlEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("outlookNotificationUrlEdit"));

        QVERIFY(kindCombo != nullptr);
        QVERIFY(resourceIdEdit != nullptr);
        QVERIFY(urlEdit != nullptr);

        kindCombo->setCurrentIndex(0);
        resourceIdEdit->setText(QStringLiteral("mail-42"));

        QVERIFY(urlEdit->text().contains(QStringLiteral("outlook.office.com")));
        QVERIFY(urlEdit->text().contains(QStringLiteral("/mail/")));
    }

    void enablesSendAfterRequiredFields()
    {
        OutlookConnectionSettings settings;
        settings.enabled = true;
        settings.accountEmail = QStringLiteral("you@example.com");
        settings.displayName = QStringLiteral("张三");

        OutlookNotificationDialog dialog(settings);
        auto* kindCombo =
            dialog.findChild<QComboBox*>(QStringLiteral("outlookNotificationKindCombo"));
        auto* resourceIdEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("outlookNotificationResourceIdEdit"));
        auto* titleEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("outlookNotificationTitleEdit"));
        auto* summaryEdit =
            dialog.findChild<QTextEdit*>(QStringLiteral("outlookNotificationSummaryEdit"));
        auto* actorEdit =
            dialog.findChild<QLineEdit*>(QStringLiteral("outlookNotificationActorEdit"));
        auto* sendButton =
            dialog.findChild<QPushButton*>(QStringLiteral("outlookNotificationSendButton"));

        QVERIFY(kindCombo != nullptr);
        QVERIFY(resourceIdEdit != nullptr);
        QVERIFY(titleEdit != nullptr);
        QVERIFY(summaryEdit != nullptr);
        QVERIFY(actorEdit != nullptr);
        QVERIFY(sendButton != nullptr);

        kindCombo->setCurrentIndex(1);
        resourceIdEdit->setText(QStringLiteral("event-7"));
        titleEdit->setText(QStringLiteral("项目周会提醒"));
        summaryEdit->setPlainText(QStringLiteral("15:00 在三层会议室，记得带版本计划。"));
        actorEdit->setText(QStringLiteral("Outlook Calendar"));

        QVERIFY(sendButton->isEnabled());
        const OutlookNotificationEvent event = dialog.event();
        QCOMPARE(event.kind, OutlookNotificationKind::CalendarReminder);
        QCOMPARE(event.resourceId, QStringLiteral("event-7"));
        QCOMPARE(event.title, QStringLiteral("项目周会提醒"));
        QCOMPARE(event.summary, QStringLiteral("15:00 在三层会议室，记得带版本计划。"));
        QCOMPARE(event.actor, QStringLiteral("Outlook Calendar"));
        QVERIFY(event.webUrl.contains(QStringLiteral("outlook.office.com")));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestOutlookNotificationDialog tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestOutlookNotificationDialog.moc"
