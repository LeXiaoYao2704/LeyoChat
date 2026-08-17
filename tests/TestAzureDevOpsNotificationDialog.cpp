#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QtTest>

#include "integrations/AzureDevOpsSettings.h"
#include "ui/AzureDevOpsNotificationDialog.h"

class TestAzureDevOpsNotificationDialog : public QObject {
    Q_OBJECT

private slots:
    void buildsDefaultBuildUrlFromSettings()
    {
        AzureDevOpsConnectionSettings settings;
        settings.baseUrl = QStringLiteral("https://dev.azure.com");
        settings.organization = QStringLiteral("leyochat");
        settings.project = QStringLiteral("LeyoChat");

        AzureDevOpsNotificationDialog dialog(settings);
        auto* kindCombo = dialog.findChild<QComboBox*>(
            QStringLiteral("azureDevOpsNotificationKindCombo"));
        auto* resourceIdEdit = dialog.findChild<QLineEdit*>(
            QStringLiteral("azureDevOpsNotificationResourceIdEdit"));
        auto* urlEdit = dialog.findChild<QLineEdit*>(
            QStringLiteral("azureDevOpsNotificationUrlEdit"));

        QVERIFY(kindCombo != nullptr);
        QVERIFY(resourceIdEdit != nullptr);
        QVERIFY(urlEdit != nullptr);

        kindCombo->setCurrentIndex(2);
        resourceIdEdit->setText(QStringLiteral("88"));

        QVERIFY(urlEdit->text().contains(QStringLiteral("buildId=88")));
        QVERIFY(urlEdit->text().contains(QStringLiteral("leyochat")));
    }

    void enablesSendAfterRequiredFields()
    {
        AzureDevOpsConnectionSettings settings;
        settings.baseUrl = QStringLiteral("https://dev.azure.com");
        settings.organization = QStringLiteral("leyochat");
        settings.project = QStringLiteral("LeyoChat");

        AzureDevOpsNotificationDialog dialog(settings);
        auto* kindCombo = dialog.findChild<QComboBox*>(
            QStringLiteral("azureDevOpsNotificationKindCombo"));
        auto* resourceIdEdit = dialog.findChild<QLineEdit*>(
            QStringLiteral("azureDevOpsNotificationResourceIdEdit"));
        auto* titleEdit = dialog.findChild<QLineEdit*>(
            QStringLiteral("azureDevOpsNotificationTitleEdit"));
        auto* summaryEdit = dialog.findChild<QTextEdit*>(
            QStringLiteral("azureDevOpsNotificationSummaryEdit"));
        auto* statusEdit = dialog.findChild<QLineEdit*>(
            QStringLiteral("azureDevOpsNotificationStatusEdit"));
        auto* actorEdit = dialog.findChild<QLineEdit*>(
            QStringLiteral("azureDevOpsNotificationActorEdit"));
        auto* sendButton = dialog.findChild<QPushButton*>(
            QStringLiteral("azureDevOpsNotificationSendButton"));

        QVERIFY(kindCombo != nullptr);
        QVERIFY(resourceIdEdit != nullptr);
        QVERIFY(titleEdit != nullptr);
        QVERIFY(summaryEdit != nullptr);
        QVERIFY(statusEdit != nullptr);
        QVERIFY(actorEdit != nullptr);
        QVERIFY(sendButton != nullptr);

        kindCombo->setCurrentIndex(1);
        resourceIdEdit->setText(QStringLiteral("109"));
        titleEdit->setText(QStringLiteral("PR #109 等待评审"));
        summaryEdit->setPlainText(QStringLiteral("这是一条用于验证 DevOps 系统通知的 PR 卡片。"));
        statusEdit->setText(QStringLiteral("active"));
        actorEdit->setText(QStringLiteral("Reviewer Bot"));

        QVERIFY(sendButton->isEnabled());
        const AzureDevOpsNotificationEvent event = dialog.event();
        QCOMPARE(event.kind, AzureDevOpsNotificationKind::PullRequestUpdated);
        QCOMPARE(event.resourceId, QStringLiteral("109"));
        QCOMPARE(event.title, QStringLiteral("PR #109 等待评审"));
        QCOMPARE(event.status, QStringLiteral("active"));
        QCOMPARE(event.actor, QStringLiteral("Reviewer Bot"));
        QVERIFY(event.webUrl.contains(QStringLiteral("pullrequest")));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestAzureDevOpsNotificationDialog tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestAzureDevOpsNotificationDialog.moc"
