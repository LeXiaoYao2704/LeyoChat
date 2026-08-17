#include <QSettings>
#include <QtTest>

#include "integrations/OutlookSettings.h"

class TestOutlookSettings : public QObject {
    Q_OBJECT

private slots:
    void roundTripsConfigurationThroughQSettings();
    void hasCredentialConfiguration_requiresServerUrlAndUsername();
    void hasNotificationConfiguration_requiresEnabledCredentialsAndNotifications();
    void mergePollState_preservesLatestCredentialsButUpdatesPollingState();
    void mergePollState_stillMergesPollStateEvenWhenCredentialContextChanged();
    void formatPollHealthSummary_includesNetworkFailuresAndNextPoll();
    void summarizeErrorMessage_compactsVerboseSoapFaults();
};

void TestOutlookSettings::roundTripsConfigurationThroughQSettings()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       QStringLiteral("LeyoChatTests"),
                       QStringLiteral("OutlookSettingsRoundTrip"));
    settings.clear();

    OutlookConnectionSettings config;
    config.enabled = true;
    config.serverUrl = QStringLiteral("https://mail.example.com");
    config.username = QStringLiteral("testuser");
    config.password = QStringLiteral("secret");
    config.accountEmail = QStringLiteral("test.user@example.com");
    config.displayName = QStringLiteral("测试用户");
    config.notificationsEnabled = true;
    config.notificationPollIntervalMinutes = 5;
    config.recentMailIds = {QStringLiteral("mail-1"), QStringLiteral("mail-2")};
    config.recentEventIds = {QStringLiteral("event-1")};
    config.lastPollAttemptAtMs = 1234;
    config.lastPollSuccessAtMs = 5678;
    config.lastPollErrorMessage = QStringLiteral("connection refused");
    config.lastPollErrorCategory = QStringLiteral("network");
    config.consecutivePollFailures = 2;
    config.notificationConversationId = QStringLiteral("conv-notify-abc");
    config.notificationConversationTitle = QStringLiteral("通知频道");

    OutlookSettingsStore::save(config, &settings);
    const OutlookConnectionSettings restored = OutlookSettingsStore::load(&settings);

    QVERIFY(restored.enabled);
    QCOMPARE(restored.serverUrl, QStringLiteral("https://mail.example.com"));
    QCOMPARE(restored.username, QStringLiteral("testuser"));
    QCOMPARE(restored.password, QStringLiteral("secret"));
    QCOMPARE(restored.accountEmail, QStringLiteral("test.user@example.com"));
    QCOMPARE(restored.displayName, QStringLiteral("测试用户"));
    QVERIFY(restored.notificationsEnabled);
    QCOMPARE(restored.notificationPollIntervalMinutes, 5);
    QCOMPARE(restored.recentMailIds,
             QStringList({QStringLiteral("mail-1"), QStringLiteral("mail-2")}));
    QCOMPARE(restored.recentEventIds, QStringList({QStringLiteral("event-1")}));
    QCOMPARE(restored.lastPollAttemptAtMs, qint64(1234));
    QCOMPARE(restored.lastPollSuccessAtMs, qint64(5678));
    QCOMPARE(restored.lastPollErrorMessage, QStringLiteral("connection refused"));
    QCOMPARE(restored.lastPollErrorCategory, QStringLiteral("network"));
    QCOMPARE(restored.consecutivePollFailures, 2);
    QCOMPARE(restored.notificationConversationId, QStringLiteral("conv-notify-abc"));
    QCOMPARE(restored.notificationConversationTitle, QStringLiteral("通知频道"));
}

void TestOutlookSettings::hasCredentialConfiguration_requiresServerUrlAndUsername()
{
    OutlookConnectionSettings config;
    QVERIFY(!config.hasCredentialConfiguration());

    config.serverUrl = QStringLiteral("https://mail.example.com");
    QVERIFY(!config.hasCredentialConfiguration()); // still needs username

    config.username = QStringLiteral("testuser");
    QVERIFY(config.hasCredentialConfiguration());
}

void TestOutlookSettings::hasNotificationConfiguration_requiresEnabledCredentialsAndNotifications()
{
    OutlookConnectionSettings config;
    config.serverUrl = QStringLiteral("https://mail.example.com");
    config.username = QStringLiteral("user");
    config.enabled = true;
    config.notificationsEnabled = true;
    config.notificationPollIntervalMinutes = 5;
    QVERIFY(config.hasNotificationConfiguration());

    config.enabled = false;
    QVERIFY(!config.hasNotificationConfiguration());

    config.enabled = true;
    config.notificationsEnabled = false;
    QVERIFY(!config.hasNotificationConfiguration());

    config.notificationsEnabled = true;
    config.serverUrl.clear();
    QVERIFY(!config.hasNotificationConfiguration());
}

void TestOutlookSettings::mergePollState_preservesLatestCredentialsButUpdatesPollingState()
{
    OutlookConnectionSettings latest;
    latest.serverUrl = QStringLiteral("https://mail.com");
    latest.username = QStringLiteral("user");
    latest.password = QStringLiteral("newpass");
    latest.enabled = true;

    OutlookConnectionSettings polled;
    polled.serverUrl = QStringLiteral("https://mail.com");
    polled.username = QStringLiteral("user");
    polled.password = QStringLiteral("oldpass"); // should be ignored
    polled.recentMailIds = {QStringLiteral("m1")};
    polled.lastPollSuccessAtMs = 9999;
    polled.consecutivePollFailures = 0;

    const auto merged = OutlookSettingsStore::mergePollState(latest, polled);

    QCOMPARE(merged.password, QStringLiteral("newpass")); // latest wins
    QCOMPARE(merged.recentMailIds, QStringList{QStringLiteral("m1")});
    QCOMPARE(merged.lastPollSuccessAtMs, qint64(9999));
    QCOMPARE(merged.consecutivePollFailures, 0);
}

void TestOutlookSettings::mergePollState_stillMergesPollStateEvenWhenCredentialContextChanged()
{
    OutlookConnectionSettings latest;
    latest.serverUrl = QStringLiteral("https://newmail.com");
    latest.username = QStringLiteral("user2");

    OutlookConnectionSettings polled;
    polled.serverUrl = QStringLiteral("https://oldmail.com"); // different server
    polled.username = QStringLiteral("user1");
    polled.recentMailIds = {QStringLiteral("m1")};
    polled.lastPollSuccessAtMs = 9999;

    // Different credential context: poll state should still merge (it's non-sensitive)
    const auto merged = OutlookSettingsStore::mergePollState(latest, polled);
    QCOMPARE(merged.serverUrl, QStringLiteral("https://newmail.com")); // latest wins
    QCOMPARE(merged.recentMailIds, QStringList{QStringLiteral("m1")});
}

void TestOutlookSettings::formatPollHealthSummary_includesNetworkFailuresAndNextPoll()
{
    OutlookConnectionSettings config;
    config.lastPollSuccessAtMs = 0;
    config.lastPollErrorMessage = QStringLiteral("连接超时");
    config.lastPollErrorCategory = QStringLiteral("network");
    config.consecutivePollFailures = 3;

    const QString summary = OutlookSettingsStore::formatPollHealthSummary(config, 5);
    QVERIFY(summary.contains(QStringLiteral("网络")));
    QVERIFY(summary.contains(QStringLiteral("3")));
    QVERIFY(summary.contains(QStringLiteral("5")));
}

void TestOutlookSettings::summarizeErrorMessage_compactsVerboseSoapFaults()
{
    const QString verboseError =
        QStringLiteral("请求失败：https://mail.example.com/EWS/Exchange.asmx SOAP Fault "
                       "http://schemas.microsoft.com/exchange/services/2006/messages "
                       "The request failed because the remote server returned an error");

    const QString summary =
        OutlookSettingsStore::summarizeErrorMessage(verboseError, QStringLiteral("network"));

    QVERIFY(!summary.contains(QStringLiteral("schemas.microsoft.com")));
    QVERIFY(summary.length() < verboseError.length());
    QVERIFY(summary.contains(QStringLiteral("失败"))
            || summary.contains(QStringLiteral("超时"))
            || summary.contains(QStringLiteral("异常")));
}

QTEST_MAIN(TestOutlookSettings)
#include "TestOutlookSettings.moc"
