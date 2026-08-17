#include <QSettings>
#include <QtTest>

#include "integrations/AzureDevOpsSettings.h"

class TestAzureDevOpsSettings : public QObject {
    Q_OBJECT

private slots:
    void roundTripsConfigurationThroughQSettings();
    void roundTripsMultipleOrganizationsAndProjects();
    void remembersProjectsPerOrganizationWithoutOverwritingOthers();
    void roundTripsNotificationTargetsPerProject();
    void roundTripsNotificationTargetEnabledState();
    void setDefaultNotificationTarget_updatesSelection();
    void hasRequiredConfiguration_requiresAllFieldsAndEnabled();
    void hasCredentialConfiguration_requiresOnlyBaseUrlAndToken();
    void hasNotificationConfiguration_requiresNoConversationBinding();
    void mergePollState_preservesLatestConfiguredFieldsAndTargetSet();
    void formatPollHealthSummary_includesFailureStateAndNextPoll();
};

void TestAzureDevOpsSettings::roundTripsConfigurationThroughQSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("LeyoChatTests"),
                       QStringLiteral("AzureDevOpsSettingsRoundTrip"));
    settings.clear();

    AzureDevOpsConnectionSettings config;
    config.enabled = true;
    config.baseUrl = QStringLiteral("https://dev.azure.com");
    config.organization = QStringLiteral("leyochat");
    config.project = QStringLiteral("LeyoChat");
    config.personalAccessToken = QStringLiteral("pat-token-123");
    config.lastPollAttemptAtMs = 111;
    config.lastPollSuccessAtMs = 222;
    config.lastPollErrorMessage = QStringLiteral("network error");
    config.lastPollErrorCategory = QStringLiteral("network");
    config.consecutivePollFailures = 3;

    AzureDevOpsSettingsStore::save(config, &settings);
    const AzureDevOpsConnectionSettings restored = AzureDevOpsSettingsStore::load(&settings);

    QVERIFY(restored.enabled);
    QCOMPARE(restored.baseUrl, QStringLiteral("https://dev.azure.com"));
    QCOMPARE(restored.organization, QStringLiteral("leyochat"));
    QCOMPARE(restored.project, QStringLiteral("LeyoChat"));
    QCOMPARE(restored.personalAccessToken, QStringLiteral("pat-token-123"));
    QCOMPARE(restored.lastPollAttemptAtMs, qint64(111));
    QCOMPARE(restored.lastPollSuccessAtMs, qint64(222));
    QCOMPARE(restored.lastPollErrorMessage, QStringLiteral("network error"));
    QCOMPARE(restored.lastPollErrorCategory, QStringLiteral("network"));
    QCOMPARE(restored.consecutivePollFailures, 3);
}

void TestAzureDevOpsSettings::roundTripsMultipleOrganizationsAndProjects()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("LeyoChatTests"),
                       QStringLiteral("AzureDevOpsSettingsMultipleTargets"));
    settings.clear();

    AzureDevOpsConnectionSettings config;
    config.enabled = true;
    config.baseUrl = QStringLiteral("https://dev.azure.com");
    config.personalAccessToken = QStringLiteral("pat-token-xyz");
    config.organization = QStringLiteral("org-b");
    config.project = QStringLiteral("Project-2");

    AzureDevOpsOrganizationEntry orgA;
    orgA.organizationId = QStringLiteral("org-a-id");
    orgA.organizationName = QStringLiteral("org-a");
    orgA.organizationUrl = QStringLiteral("https://dev.azure.com/org-a");
    orgA.projects.push_back({QStringLiteral("p-a-1"), QStringLiteral("Project-1"), QStringLiteral("wellFormed")});

    AzureDevOpsOrganizationEntry orgB;
    orgB.organizationId = QStringLiteral("org-b-id");
    orgB.organizationName = QStringLiteral("org-b");
    orgB.organizationUrl = QStringLiteral("https://dev.azure.com/org-b");
    orgB.projects.push_back({QStringLiteral("p-b-1"), QStringLiteral("Project-2"), QStringLiteral("wellFormed")});
    orgB.projects.push_back({QStringLiteral("p-b-2"), QStringLiteral("Project-3"), QStringLiteral("createPending")});

    config.organizations.push_back(orgA);
    config.organizations.push_back(orgB);

    AzureDevOpsSettingsStore::save(config, &settings);
    const AzureDevOpsConnectionSettings restored = AzureDevOpsSettingsStore::load(&settings);

    QCOMPARE(restored.organization, QStringLiteral("org-b"));
    QCOMPARE(restored.project, QStringLiteral("Project-2"));
    QCOMPARE(restored.organizations.size(), 2);
    QCOMPARE(restored.organizations.at(0).organizationName, QStringLiteral("org-a"));
    QCOMPARE(restored.organizations.at(1).organizationName, QStringLiteral("org-b"));
    QCOMPARE(restored.organizations.at(1).projects.size(), 2);
    QCOMPARE(restored.organizations.at(1).projects.at(0).projectName, QStringLiteral("Project-2"));
    QCOMPARE(restored.organizations.at(1).projects.at(1).projectName, QStringLiteral("Project-3"));
}

void TestAzureDevOpsSettings::remembersProjectsPerOrganizationWithoutOverwritingOthers()
{
    AzureDevOpsConnectionSettings config;
    config.organization = QStringLiteral("org-a");
    config.project = QStringLiteral("Project-1");

    config.rememberOrganizations({
        {QStringLiteral("org-a-id"), QStringLiteral("org-a"), QStringLiteral("https://dev.azure.com/org-a")},
        {QStringLiteral("org-b-id"), QStringLiteral("org-b"), QStringLiteral("https://dev.azure.com/org-b")},
    });

    config.rememberProjects(QStringLiteral("org-a"),
                            {{QStringLiteral("p-a-1"), QStringLiteral("Project-1"), QStringLiteral("wellFormed")}});
    config.rememberProjects(QStringLiteral("org-b"),
                            {{QStringLiteral("p-b-1"), QStringLiteral("Project-2"), QStringLiteral("wellFormed")},
                             {QStringLiteral("p-b-2"), QStringLiteral("Project-3"), QStringLiteral("createPending")}});

    QCOMPARE(config.organizations.size(), 2);
    QCOMPARE(config.knownProjectsForOrganization(QStringLiteral("org-a")),
             QStringList({QStringLiteral("Project-1")}));
    QCOMPARE(config.knownProjectsForOrganization(QStringLiteral("org-b")),
             QStringList({QStringLiteral("Project-2"), QStringLiteral("Project-3")}));
    QCOMPARE(config.organization, QStringLiteral("org-a"));
    QCOMPARE(config.project, QStringLiteral("Project-1"));
}

void TestAzureDevOpsSettings::roundTripsNotificationTargetsPerProject()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("LeyoChatTests"),
                       QStringLiteral("AzureDevOpsSettingsNotificationTargets"));
    settings.clear();

    AzureDevOpsConnectionSettings config;
    config.enabled = true;
    config.baseUrl = QStringLiteral("https://dev.azure.com");
    config.personalAccessToken = QStringLiteral("pat-token-xyz");
    config.organization = QStringLiteral("org-a");
    config.project = QStringLiteral("Project-1");
    config.notificationsEnabled = true;
    config.notificationPollIntervalMinutes = 3;
    config.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 101});
    config.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Project-2"), true, 202});
    config.notificationTargets[0].lastPollAttemptAtMs = 501;
    config.notificationTargets[0].lastPollSuccessAtMs = 502;
    config.notificationTargets[0].lastPollErrorMessage = QStringLiteral("timeout");
    config.notificationTargets[0].lastPollErrorCategory = QStringLiteral("network");
    config.notificationTargets[0].consecutivePollFailures = 4;

    AzureDevOpsSettingsStore::save(config, &settings);
    const AzureDevOpsConnectionSettings restored = AzureDevOpsSettingsStore::load(&settings);

    QCOMPARE(restored.notificationTargets.size(), 2);
    QCOMPARE(restored.notificationTargets.at(0).organization, QStringLiteral("org-a"));
    QCOMPARE(restored.notificationTargets.at(0).project, QStringLiteral("Project-1"));
    QCOMPARE(restored.notificationTargets.at(0).lastNotifiedBuildId, 101);
    QCOMPARE(restored.notificationTargets.at(0).lastPollAttemptAtMs, qint64(501));
    QCOMPARE(restored.notificationTargets.at(0).lastPollSuccessAtMs, qint64(502));
    QCOMPARE(restored.notificationTargets.at(0).lastPollErrorMessage, QStringLiteral("timeout"));
    QCOMPARE(restored.notificationTargets.at(0).lastPollErrorCategory, QStringLiteral("network"));
    QCOMPARE(restored.notificationTargets.at(0).consecutivePollFailures, 4);
    QCOMPARE(restored.notificationTargets.at(1).organization, QStringLiteral("org-b"));
    QCOMPARE(restored.notificationTargets.at(1).project, QStringLiteral("Project-2"));
    QCOMPARE(restored.notificationTargets.at(1).lastNotifiedBuildId, 202);
}

void TestAzureDevOpsSettings::roundTripsNotificationTargetEnabledState()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("LeyoChatTests"),
                       QStringLiteral("AzureDevOpsSettingsNotificationTargetEnabled"));
    settings.clear();

    AzureDevOpsConnectionSettings config;
    config.enabled = true;
    config.baseUrl = QStringLiteral("https://dev.azure.com");
    config.personalAccessToken = QStringLiteral("pat-token-xyz");
    config.organization = QStringLiteral("org-a");
    config.project = QStringLiteral("Project-1");
    config.notificationsEnabled = true;
    config.notificationTargets.push_back(
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 101});
    config.notificationTargets.push_back(
        {QStringLiteral("org-b"), QStringLiteral("Project-2"), false, 202});

    AzureDevOpsSettingsStore::save(config, &settings);
    const AzureDevOpsConnectionSettings restored = AzureDevOpsSettingsStore::load(&settings);

    QCOMPARE(restored.notificationTargets.size(), 2);
    QVERIFY(restored.notificationTargets.at(0).enabled);
    QVERIFY(!restored.notificationTargets.at(1).enabled);
    QCOMPARE(restored.enabledNotificationTargets().size(), 1);
    QCOMPARE(restored.enabledNotificationTargets().at(0).organization, QStringLiteral("org-a"));
}

void TestAzureDevOpsSettings::setDefaultNotificationTarget_updatesSelection()
{
    AzureDevOpsConnectionSettings config;
    config.organization = QStringLiteral("org-a");
    config.project = QStringLiteral("Project-1");
    config.rememberNotificationTarget(QStringLiteral("org-a"), QStringLiteral("Project-1"));
    config.rememberNotificationTarget(QStringLiteral("org-b"), QStringLiteral("Project-2"));

    config.setDefaultNotificationTarget(QStringLiteral("org-b"), QStringLiteral("Project-2"));

    QCOMPARE(config.organization, QStringLiteral("org-b"));
    QCOMPARE(config.project, QStringLiteral("Project-2"));
    QVERIFY(config.isDefaultNotificationTarget(QStringLiteral("org-b"), QStringLiteral("Project-2")));
}

void TestAzureDevOpsSettings::hasRequiredConfiguration_requiresAllFieldsAndEnabled()
{
    AzureDevOpsConnectionSettings config;
    QVERIFY(!config.hasRequiredConfiguration());

    config.baseUrl = QStringLiteral("https://dev.azure.com");
    config.organization = QStringLiteral("leyochat");
    config.project = QStringLiteral("LeyoChat");
    config.personalAccessToken = QStringLiteral("pat-token-123");
    QVERIFY(!config.hasRequiredConfiguration());

    config.enabled = true;
    QVERIFY(config.hasRequiredConfiguration());
}

void TestAzureDevOpsSettings::hasCredentialConfiguration_requiresOnlyBaseUrlAndToken()
{
    AzureDevOpsConnectionSettings config;
    QVERIFY(!config.hasCredentialConfiguration());

    config.baseUrl = QStringLiteral("https://dev.azure.com");
    QVERIFY(!config.hasCredentialConfiguration());

    config.personalAccessToken = QStringLiteral("pat-token-123");
    QVERIFY(config.hasCredentialConfiguration());
    QVERIFY(!config.hasProjectSelection());

    config.organization = QStringLiteral("leyochat");
    config.project = QStringLiteral("LeyoChat");
    QVERIFY(config.hasProjectSelection());
}

void TestAzureDevOpsSettings::hasNotificationConfiguration_requiresNoConversationBinding()
{
    AzureDevOpsConnectionSettings config;
    config.enabled = true;
    config.baseUrl = QStringLiteral("https://dev.azure.com");
    config.organization = QStringLiteral("leyochat");
    config.project = QStringLiteral("LeyoChat");
    config.personalAccessToken = QStringLiteral("pat-token-123");
    config.notificationsEnabled = true;
    config.notificationPollIntervalMinutes = 3;

    QVERIFY(config.hasNotificationConfiguration());
}

void TestAzureDevOpsSettings::mergePollState_preservesLatestConfiguredFieldsAndTargetSet()
{
    AzureDevOpsConnectionSettings latest;
    latest.enabled = true;
    latest.baseUrl = QStringLiteral("https://dev.azure.com");
    latest.organization = QStringLiteral("org-new");
    latest.project = QStringLiteral("Project-New");
    latest.personalAccessToken = QStringLiteral("pat-new");
    latest.notificationsEnabled = true;
    latest.notificationConversationId = QStringLiteral("conv-latest");
    latest.notificationConversationTitle = QStringLiteral("最新通知会话");
    latest.notificationPollIntervalMinutes = 9;
    latest.notificationTargets = {
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 10},
        {QStringLiteral("org-new"), QStringLiteral("Project-New"), true, 0},
    };

    AzureDevOpsConnectionSettings polled = latest;
    polled.organization = QStringLiteral("org-old");
    polled.project = QStringLiteral("Project-1");
    polled.personalAccessToken = QStringLiteral("pat-old");
    polled.notificationConversationId = QStringLiteral("conv-old");
    polled.notificationConversationTitle = QStringLiteral("旧通知会话");
    polled.notificationPollIntervalMinutes = 3;
    polled.lastPollAttemptAtMs = 1111;
    polled.lastPollSuccessAtMs = 2222;
    polled.lastPollErrorMessage = QStringLiteral("timeout");
    polled.lastPollErrorCategory = QStringLiteral("network");
    polled.consecutivePollFailures = 2;
    polled.notificationTargets = {
        {QStringLiteral("org-a"), QStringLiteral("Project-1"), true, 88},
        {QStringLiteral("org-b"), QStringLiteral("Project-2"), true, 99},
    };
    polled.notificationTargets[0].lastPollAttemptAtMs = 3333;
    polled.notificationTargets[0].lastPollSuccessAtMs = 4444;
    polled.notificationTargets[0].lastPollErrorMessage = QStringLiteral("target-timeout");
    polled.notificationTargets[0].lastPollErrorCategory = QStringLiteral("network");
    polled.notificationTargets[0].consecutivePollFailures = 3;

    const AzureDevOpsConnectionSettings merged =
        AzureDevOpsSettingsStore::mergePollState(latest, polled);

    QCOMPARE(merged.organization, QStringLiteral("org-new"));
    QCOMPARE(merged.project, QStringLiteral("Project-New"));
    QCOMPARE(merged.personalAccessToken, QStringLiteral("pat-new"));
    QCOMPARE(merged.notificationConversationId, QStringLiteral("conv-latest"));
    QCOMPARE(merged.notificationConversationTitle, QStringLiteral("最新通知会话"));
    QCOMPARE(merged.notificationPollIntervalMinutes, 9);
    QCOMPARE(merged.lastPollAttemptAtMs, qint64(1111));
    QCOMPARE(merged.lastPollSuccessAtMs, qint64(2222));
    QCOMPARE(merged.lastPollErrorMessage, QStringLiteral("timeout"));
    QCOMPARE(merged.lastPollErrorCategory, QStringLiteral("network"));
    QCOMPARE(merged.consecutivePollFailures, 2);
    QCOMPARE(merged.notificationTargets.size(), 2);
    QCOMPARE(merged.notificationTargets.at(0).organization, QStringLiteral("org-a"));
    QCOMPARE(merged.notificationTargets.at(0).project, QStringLiteral("Project-1"));
    QCOMPARE(merged.notificationTargets.at(0).lastNotifiedBuildId, 88);
    QCOMPARE(merged.notificationTargets.at(0).lastPollAttemptAtMs, qint64(3333));
    QCOMPARE(merged.notificationTargets.at(0).lastPollSuccessAtMs, qint64(4444));
    QCOMPARE(merged.notificationTargets.at(0).lastPollErrorMessage, QStringLiteral("target-timeout"));
    QCOMPARE(merged.notificationTargets.at(0).lastPollErrorCategory, QStringLiteral("network"));
    QCOMPARE(merged.notificationTargets.at(0).consecutivePollFailures, 3);
    QCOMPARE(merged.notificationTargets.at(1).organization, QStringLiteral("org-new"));
    QCOMPARE(merged.notificationTargets.at(1).project, QStringLiteral("Project-New"));
    QCOMPARE(merged.notificationTargets.at(1).lastNotifiedBuildId, 0);
}

void TestAzureDevOpsSettings::formatPollHealthSummary_includesFailureStateAndNextPoll()
{
    AzureDevOpsConnectionSettings config;
    config.lastPollSuccessAtMs = 1712800000000;
    config.lastPollErrorMessage = QStringLiteral("401 unauthorized");
    config.lastPollErrorCategory = QStringLiteral("auth");
    config.consecutivePollFailures = 2;

    const QString summary = AzureDevOpsSettingsStore::formatPollHealthSummary(config, 15);

    QVERIFY(summary.contains(QStringLiteral("最近成功：")));
    QVERIFY(summary.contains(QStringLiteral("401 unauthorized")));
    QVERIFY(summary.contains(QStringLiteral("错误类型：认证")));
    QVERIFY(summary.contains(QStringLiteral("连续失败：2 次")));
    QVERIFY(summary.contains(QStringLiteral("约 15 分钟后")));
}

QTEST_MAIN(TestAzureDevOpsSettings)
#include "TestAzureDevOpsSettings.moc"
