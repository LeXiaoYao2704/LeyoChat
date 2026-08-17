#include <QtTest>

#include "app/IntegrationNotificationPresentation.h"

class TestIntegrationNotificationPresentation : public QObject {
    Q_OBJECT

private slots:
    void trayUnreadPresentation_defaultsToAppNameWhenEmpty();
    void trayUnreadPresentation_usesUnreadTitleAndCount();
    void integrationFailureSummary_clarifiesAuthRecovery();
    void integrationFailureSummary_clarifiesNetworkRecovery();
};

void TestIntegrationNotificationPresentation::trayUnreadPresentation_defaultsToAppNameWhenEmpty()
{
    QCOMPARE(IntegrationNotificationPresentation::trayUnreadToolTip(QStringLiteral("LeyoChat"), 0),
             QStringLiteral("LeyoChat"));
    QCOMPARE(IntegrationNotificationPresentation::trayUnreadStatusActionText(QString(), 0),
             QStringLiteral("当前没有未处理提醒"));
}

void TestIntegrationNotificationPresentation::trayUnreadPresentation_usesUnreadTitleAndCount()
{
    QCOMPARE(IntegrationNotificationPresentation::trayUnreadToolTip(QStringLiteral("LeyoChat"), 3),
             QStringLiteral("LeyoChat · 3 条未处理提醒"));
    QCOMPARE(
        IntegrationNotificationPresentation::trayUnreadStatusActionText(QStringLiteral("PR 审核请求"), 3),
        QStringLiteral("PR 审核请求 · 3 条未处理提醒"));
}

void TestIntegrationNotificationPresentation::integrationFailureSummary_clarifiesAuthRecovery()
{
    const QString summary = IntegrationNotificationPresentation::integrationFailureSummary(
        QStringLiteral("Outlook"),
        QStringLiteral("auth"),
        2,
        5);
    QVERIFY(summary.contains(QStringLiteral("授权状态")));
    QVERIFY(summary.contains(QStringLiteral("刷新令牌已过期")));
}

void TestIntegrationNotificationPresentation::integrationFailureSummary_clarifiesNetworkRecovery()
{
    const QString summary = IntegrationNotificationPresentation::integrationFailureSummary(
        QStringLiteral("Azure DevOps"),
        QStringLiteral("network"),
        3,
        10);
    QVERIFY(summary.contains(QStringLiteral("网络不可达")));
    QVERIFY(summary.contains(QStringLiteral("10 分钟后自动重试")));
}

QTEST_MAIN(TestIntegrationNotificationPresentation)
#include "TestIntegrationNotificationPresentation.moc"
