#include <QtTest>

#include "integrations/OutlookAdapterContracts.h"

class TestOutlookAdapterContracts : public QObject {
    Q_OBJECT

private slots:
    void mailPayload_containsOpenAction();
    void cancelledEventReference_usesServiceOrigin();
};

void TestOutlookAdapterContracts::mailPayload_containsOpenAction()
{
    const OutlookMailResource resource{
        QStringLiteral("svc-outlook"),
        QStringLiteral("workspace-mail"),
        QStringLiteral("mail-1"),
        QStringLiteral("user@example.com"),
        QStringLiteral("阶段二内测安排"),
        QStringLiteral("侯晓刚"),
        QStringLiteral("今天 10:30"),
        QStringLiteral("https://outlook.example/mail/1"),
    };

    const ResourceRefPayload payload = OutlookAdapterContracts::makeMailPayload(resource);
    QCOMPARE(payload.kind, QStringLiteral("outlook_mail"));
    QCOMPARE(payload.actions.size(), 1);
    QCOMPARE(payload.actions.front().label, QStringLiteral("打开邮件"));
}

void TestOutlookAdapterContracts::cancelledEventReference_usesServiceOrigin()
{
    OutlookCalendarEventResource resource{
        QStringLiteral("svc-outlook"),
        QStringLiteral("workspace-mail"),
        QStringLiteral("event-1"),
        QStringLiteral("阶段二评审"),
        QStringLiteral("张大乐"),
        QStringLiteral("明天 14:00"),
        QStringLiteral("三楼会议室"),
        QStringLiteral("https://outlook.example/calendar/event-1"),
        QStringLiteral("ck-1"),
        QStringLiteral("今天 12:15"),
        true,
    };

    const ResourceReference reference = OutlookAdapterContracts::makeCalendarEventReference(resource);
    QCOMPARE(reference.resourceKind, QStringLiteral("outlook_event"));
    QCOMPARE(reference.origin, ResourceOrigin::Service);
    QVERIFY(reference.summary.contains(QStringLiteral("会议已取消")));
}

QTEST_MAIN(TestOutlookAdapterContracts)
#include "TestOutlookAdapterContracts.moc"
