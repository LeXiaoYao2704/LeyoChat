#include <QtTest>

#include "services/OutlookNotificationDispatcher.h"

class TestOutlookNotificationDispatcher : public QObject {
    Q_OBJECT

private slots:
    void buildsDirectDraftFromOutlookEvent()
    {
        OutlookNotificationEvent event;
        event.kind = OutlookNotificationKind::MailReceived;
        event.serviceId = QStringLiteral("local-outlook");
        event.workspaceId = QStringLiteral("local-mail");
        event.resourceId = QStringLiteral("mail:42");
        event.title = QStringLiteral("来自采购部的新邮件");
        event.summary = QStringLiteral("主题：四月付款计划");
        event.status = QStringLiteral("unread");
        event.webUrl = QStringLiteral("https://outlook.office.com/mail/");
        event.actor = QStringLiteral("王小明");

        const auto draft = OutlookNotificationDispatcher::buildDirectDraft(
            QStringLiteral("local-user"),
            QStringLiteral("target-user"),
            QStringLiteral("系统通知"),
            event);

        QVERIFY(draft.has_value());
        QCOMPARE(draft->targetClientId, QStringLiteral("target-user"));
        QCOMPARE(draft->conversationTitle, QStringLiteral("系统通知"));
        QVERIFY(QString::fromStdWString(draft->message.messageType) == QStringLiteral("resource_ref"));
        QVERIFY(draft->preview.contains(QStringLiteral("Outlook")));
    }
};

QTEST_APPLESS_MAIN(TestOutlookNotificationDispatcher)
#include "TestOutlookNotificationDispatcher.moc"
