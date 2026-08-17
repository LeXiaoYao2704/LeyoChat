#include <QtTest>

#include "integrations/OutlookNotificationContracts.h"

class TestOutlookNotificationContracts : public QObject {
    Q_OBJECT

private slots:
    void mailNotificationPayload_containsOpenAction();
    void cancelledMeetingReference_usesServiceOrigin();
};

void TestOutlookNotificationContracts::mailNotificationPayload_containsOpenAction()
{
    const OutlookNotificationEvent event{
        OutlookNotificationKind::MailReceived,
        QStringLiteral("svc-outlook"),
        QStringLiteral("workspace-mail"),
        QStringLiteral("mail-42"),
        QStringLiteral("阶段二周报"),
        QStringLiteral("来自张三的未读邮件"),
        QStringLiteral("未读"),
        QStringLiteral("https://outlook.office.com/mail/42"),
        QStringLiteral("张三"),
    };

    const ResourceRefPayload payload =
        OutlookNotificationContracts::makeNotificationPayload(event);
    QCOMPARE(payload.kind, QStringLiteral("outlook_mail"));
    QCOMPARE(payload.resourceId, QStringLiteral("mail-42"));
    QCOMPARE(payload.actions.size(), 1);
    QCOMPARE(payload.actions.front().target,
             QStringLiteral("https://outlook.office.com/mail/42"));
}

void TestOutlookNotificationContracts::cancelledMeetingReference_usesServiceOrigin()
{
    const OutlookNotificationEvent event{
        OutlookNotificationKind::CalendarCancelled,
        QStringLiteral("svc-outlook"),
        QStringLiteral("workspace-mail"),
        QStringLiteral("event-7"),
        QStringLiteral("项目评审会"),
        QStringLiteral("会议已取消"),
        QStringLiteral("已取消"),
        QStringLiteral("https://outlook.office.com/calendar/event/7"),
        QStringLiteral("产品部"),
    };

    const ResourceReference reference =
        OutlookNotificationContracts::makeNotificationReference(event);
    QCOMPARE(reference.resourceKind, QStringLiteral("outlook_event"));
    QCOMPARE(reference.origin, ResourceOrigin::Service);
    QVERIFY(reference.summary.contains(QStringLiteral("会议已取消")));
}

QTEST_MAIN(TestOutlookNotificationContracts)
#include "TestOutlookNotificationContracts.moc"
