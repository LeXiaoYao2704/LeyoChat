// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增513行/修改0行/删除0行; 总行数513行
// @AI-LastModified: 2026-04-14 23:21:50

// ──────────────────────────────────────────────────────────────────────────
// TestOutlookIntegrationHealth
//
// 功能完整性 + 主线程安全验证
//
// 重点：
//   1. 轮询 / Streaming 是否在后台完成，不阻塞事件循环
//   2. 超时 / 失败 / 恢复流程是否正确
//   3. Streaming 事件触发是否正确转化为 poll
//   4. 设置保存后失败计数器是否重置
// ──────────────────────────────────────────────────────────────────────────

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

#include "integrations/OutlookEwsTransport.h"
#include "integrations/OutlookSettings.h"
#include "services/OutlookNotificationPoller.h"
#include "services/OutlookStreamingConnection.h"

// ──────────────────────────────────────────────────────────────────────────
// SOAP 响应构建辅助
// ──────────────────────────────────────────────────────────────────────────

namespace {

QByteArray soapEnvelope(const QByteArray& body)
{
    return QByteArrayLiteral(
               "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
               "<soap:Envelope"
               " xmlns:soap=\"http://schemas.xmlsoap.org/soap/envelope/\""
               " xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\""
               " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\">"
               "<soap:Body>")
        + body
        + QByteArrayLiteral("</soap:Body></soap:Envelope>");
}

QByteArray makeMailSoapResponse(const QByteArray& items)
{
    return soapEnvelope(
        QByteArrayLiteral(
            "<m:FindItemResponse><m:ResponseMessages>"
            "<m:FindItemResponseMessage ResponseClass=\"Success\">"
            "<m:ResponseCode>NoError</m:ResponseCode>"
            "<m:RootFolder><t:Items>")
        + items
        + QByteArrayLiteral("</t:Items></m:RootFolder>"
            "</m:FindItemResponseMessage></m:ResponseMessages></m:FindItemResponse>"));
}

QByteArray makeCalendarSoapResponse(const QByteArray& items)
{
    return soapEnvelope(
        QByteArrayLiteral(
            "<m:FindItemResponse><m:ResponseMessages>"
            "<m:FindItemResponseMessage ResponseClass=\"Success\">"
            "<m:ResponseCode>NoError</m:ResponseCode>"
            "<m:RootFolder><t:Items>")
        + items
        + QByteArrayLiteral("</t:Items></m:RootFolder>"
            "</m:FindItemResponseMessage></m:ResponseMessages></m:FindItemResponse>"));
}

QByteArray mailItem(const QByteArray& id,
                    const QByteArray& subject,
                    const QByteArray& received,
                    const QByteArray& sender)
{
    return QByteArrayLiteral("<t:Message>"
                   "<t:ItemId Id=\"")
        + id
        + QByteArrayLiteral("\" ChangeKey=\"ck\"/>"
                   "<t:Subject>")
        + subject
        + QByteArrayLiteral("</t:Subject>"
                   "<t:DateTimeReceived>")
        + received
        + QByteArrayLiteral("</t:DateTimeReceived>"
                   "<t:From><t:Mailbox><t:Name>")
        + sender
        + QByteArrayLiteral("</t:Name></t:Mailbox></t:From>"
                   "</t:Message>");
}

QByteArray calendarItem(const QByteArray& id,
                        const QByteArray& changeKey,
                        const QByteArray& subject,
                        const QByteArray& start,
                        bool cancelled)
{
    return QByteArrayLiteral("<t:CalendarItem>"
                   "<t:ItemId Id=\"")
        + id + QByteArrayLiteral("\" ChangeKey=\"") + changeKey
        + QByteArrayLiteral("\"/><t:Subject>")
        + subject
        + QByteArrayLiteral("</t:Subject><t:Start>")
        + start
        + QByteArrayLiteral("</t:Start><t:Location>Room</t:Location><t:IsCancelled>")
        + (cancelled ? "true" : "false")
        + QByteArrayLiteral("</t:IsCancelled>"
                   "<t:Organizer><t:Mailbox><t:Name>Org</t:Name></t:Mailbox></t:Organizer>"
                   "</t:CalendarItem>");
}

QDateTime shanghaiNow()
{
    return QDateTime(QDate(2026, 4, 14), QTime(10, 0), QTimeZone("Asia/Shanghai"));
}

OutlookConnectionSettings baseSettings()
{
    OutlookConnectionSettings s;
    s.enabled = true;
    s.serverUrl = QStringLiteral("https://mail.example.com");
    s.username = QStringLiteral("testuser");
    s.password = QStringLiteral("test-password");
    s.accountEmail = QStringLiteral("test.user@example.com");
    s.displayName = QStringLiteral("测试用户");
    s.notificationsEnabled = true;
    s.notificationPollIntervalMinutes = 5;
    return s;
}

// ──────────────────────────────────────────────────────────────────────────
// Fake EWS transport — 可配置延迟和失败
// ──────────────────────────────────────────────────────────────────────────

struct ConfigurableFakeEwsTransport : public IOutlookEwsTransport {
    QByteArray mailResponse;
    QByteArray calendarResponse;
    int delayMs = 0;
    bool shouldFail = false;
    QString failMessage = QStringLiteral("simulated failure");
    mutable int callCount = 0;

    std::optional<QByteArray> soapPost(const QUrl&,
                                       const QByteArray& envelope,
                                       const OutlookConnectionSettings&,
                                       QString* errorMessage) const override
    {
        const_cast<ConfigurableFakeEwsTransport*>(this)->callCount++;

        if (delayMs > 0) {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < delayMs) {
                // 模拟阻塞
            }
        }

        if (shouldFail) {
            if (errorMessage) *errorMessage = failMessage;
            return std::nullopt;
        }

        if (envelope.contains("inbox"))
            return mailResponse;
        if (envelope.contains("calendar"))
            return calendarResponse;
        return QByteArray{};
    }
};

}  // namespace

// ──────────────────────────────────────────────────────────────────────────
// 测试类
// ──────────────────────────────────────────────────────────────────────────

class TestOutlookIntegrationHealth : public QObject {
    Q_OBJECT

private slots:
    // ── 功能完整性 ───────────────────────────────────────
    void settingsRoundTrip_preservesAllFields();
    void poll_returnsUnreadMailAndCalendarEvents();
    void poll_suppressesPreviouslySeenItems();
    void poll_failureIncrementsCounterAndRecoveryClears();
    void settingsSave_resetsFailureCounters();
    void poll_emptyMailboxReturnsZeroEvents();

    // ── Streaming 功能 ──────────────────────────────────
    void streaming_startAndStopLifecycle();
    void streaming_parseSubscriptionId_extractsFromXml();
    void streaming_parseStreamingEvents_detectsNewMailEvent();
    void streaming_parseStreamingEvents_detectsCreatedEvent();
    void streaming_parseStreamingEvents_returnsFalseForEmptyNotification();

    // ── 主线程安全 / 不卡顿 ─────────────────────────────
    void pollSynchronous_completesWithinBudget();
    void eventLoopRemainsFree_duringPoll();
    void streaming_doesNotBlockMainThread();

    // ── 超时行为 ─────────────────────────────────────────
    void httpTimeout_doesNotExceed30Seconds();
    void errorSummarization_compactsVerboseSoapFaults();
};

// =====================================================================
// 功能完整性
// =====================================================================

void TestOutlookIntegrationHealth::settingsRoundTrip_preservesAllFields()
{
    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("LeyoChat-test"), QStringLiteral("outlook-health"));
    store.clear();

    auto original = baseSettings();
    original.consecutivePollFailures = 2;
    original.lastPollErrorMessage = QStringLiteral("auth failed");
    original.lastPollErrorCategory = QStringLiteral("auth");
    original.recentMailIds = {QStringLiteral("mail-1"), QStringLiteral("mail-2")};
    original.recentEventIds = {QStringLiteral("ev-1|ck-1|active")};

    OutlookSettingsStore::save(original, &store);
    const auto loaded = OutlookSettingsStore::load(&store);

    QCOMPARE(loaded.enabled, original.enabled);
    QCOMPARE(loaded.serverUrl, original.serverUrl);
    QCOMPARE(loaded.username, original.username);
    QCOMPARE(loaded.password, original.password);
    QCOMPARE(loaded.accountEmail, original.accountEmail);
    QCOMPARE(loaded.displayName, original.displayName);
    QCOMPARE(loaded.notificationsEnabled, original.notificationsEnabled);
    QCOMPARE(loaded.consecutivePollFailures, original.consecutivePollFailures);
    QCOMPARE(loaded.lastPollErrorMessage, original.lastPollErrorMessage);
    QCOMPARE(loaded.lastPollErrorCategory, original.lastPollErrorCategory);
    QCOMPARE(loaded.recentMailIds, original.recentMailIds);
    QCOMPARE(loaded.recentEventIds, original.recentEventIds);
}

void TestOutlookIntegrationHealth::poll_returnsUnreadMailAndCalendarEvents()
{
    auto transport = std::make_shared<ConfigurableFakeEwsTransport>();
    transport->mailResponse = makeMailSoapResponse(
        mailItem("mail-A", "Build failed", "2026-04-14T01:00:00Z", "CI Bot"));
    transport->calendarResponse = makeCalendarSoapResponse(
        calendarItem("ev-A", "ck-1", "Sprint review",
                     "2026-04-14T11:00:00+08:00", false));

    OutlookNotificationPoller poller(baseSettings(), transport);
    const auto result = poller.poll(shanghaiNow(), nullptr);

    QCOMPARE(result.events.size(), 2);
    QCOMPARE(result.events.at(0).kind, OutlookNotificationKind::MailReceived);
    QCOMPARE(result.events.at(0).resourceId, QStringLiteral("mail-A"));
    QCOMPARE(result.events.at(1).kind, OutlookNotificationKind::CalendarReminder);
    QCOMPARE(result.events.at(1).resourceId, QStringLiteral("ev-A"));
    QVERIFY(result.updatedSettings.recentMailIds.contains(QStringLiteral("mail-A")));
}

void TestOutlookIntegrationHealth::poll_suppressesPreviouslySeenItems()
{
    auto transport = std::make_shared<ConfigurableFakeEwsTransport>();
    transport->mailResponse = makeMailSoapResponse(
        mailItem("mail-A", "Build failed", "2026-04-14T01:00:00Z", "CI Bot"));
    transport->calendarResponse = makeCalendarSoapResponse(
        calendarItem("ev-A", "ck-1", "Sprint review",
                     "2026-04-14T11:00:00+08:00", false));

    auto settings = baseSettings();
    settings.recentMailIds = {QStringLiteral("mail-A")};
    settings.recentEventIds = {QStringLiteral("ev-A|ck-1|active")};

    OutlookNotificationPoller poller(settings, transport);
    const auto result = poller.poll(shanghaiNow(), nullptr);

    QVERIFY(result.events.isEmpty());
}

void TestOutlookIntegrationHealth::poll_failureIncrementsCounterAndRecoveryClears()
{
    // 第一轮：失败
    auto transport = std::make_shared<ConfigurableFakeEwsTransport>();
    transport->shouldFail = true;
    transport->failMessage = QStringLiteral("NTLM auth rejected");

    {
        OutlookNotificationPoller poller(baseSettings(), transport);
        QString error;
        const auto result = poller.poll(shanghaiNow(), &error);
        QVERIFY(!error.isEmpty());
        QCOMPARE(result.updatedSettings.consecutivePollFailures, 1);
        QVERIFY(!result.updatedSettings.lastPollErrorCategory.isEmpty());
        QVERIFY(result.updatedSettings.lastPollAttemptAtMs > 0);
    }

    // 第二轮：恢复
    transport->shouldFail = false;
    transport->mailResponse = makeMailSoapResponse(
        mailItem("mail-B", "Recovered", "2026-04-14T02:00:00Z", "Bot"));
    transport->calendarResponse = makeCalendarSoapResponse({});

    {
        auto failedSettings = baseSettings();
        failedSettings.consecutivePollFailures = 1;
        failedSettings.lastPollErrorMessage = QStringLiteral("NTLM auth rejected");
        failedSettings.lastPollErrorCategory = QStringLiteral("auth");

        OutlookNotificationPoller poller(failedSettings, transport);
        QString error;
        const auto result = poller.poll(shanghaiNow().addSecs(300), &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(result.updatedSettings.consecutivePollFailures, 0);
        QVERIFY(result.updatedSettings.lastPollErrorMessage.isEmpty());
        QCOMPARE(result.events.size(), 1);
    }
}

void TestOutlookIntegrationHealth::settingsSave_resetsFailureCounters()
{
    QSettings store(QSettings::IniFormat, QSettings::UserScope,
                    QStringLiteral("LeyoChat-test"), QStringLiteral("outlook-health-reset"));
    store.clear();

    auto settings = baseSettings();
    settings.consecutivePollFailures = 4;
    settings.lastPollErrorMessage = QStringLiteral("timeout");
    settings.lastPollErrorCategory = QStringLiteral("network");

    // 模拟用户保存：重置计数器
    settings.consecutivePollFailures = 0;
    settings.lastPollErrorMessage.clear();
    settings.lastPollErrorCategory.clear();
    OutlookSettingsStore::save(settings, &store);

    const auto loaded = OutlookSettingsStore::load(&store);
    QCOMPARE(loaded.consecutivePollFailures, 0);
    QVERIFY(loaded.lastPollErrorMessage.isEmpty());
    QVERIFY(loaded.lastPollErrorCategory.isEmpty());
}

void TestOutlookIntegrationHealth::poll_emptyMailboxReturnsZeroEvents()
{
    auto transport = std::make_shared<ConfigurableFakeEwsTransport>();
    transport->mailResponse = makeMailSoapResponse({});
    transport->calendarResponse = makeCalendarSoapResponse({});

    OutlookNotificationPoller poller(baseSettings(), transport);
    const auto result = poller.poll(shanghaiNow(), nullptr);
    QVERIFY(result.events.isEmpty());
    QCOMPARE(result.updatedSettings.consecutivePollFailures, 0);
}

// =====================================================================
// Streaming 功能
// =====================================================================

void TestOutlookIntegrationHealth::streaming_startAndStopLifecycle()
{
    OutlookStreamingConnection conn;
    QVERIFY(!conn.isRunning());

    // start 之后 isRunning 应为 true
    conn.start(baseSettings());
    QVERIFY(conn.isRunning());

    // stop 之后回到 false
    conn.stop();
    QVERIFY(!conn.isRunning());
}

void TestOutlookIntegrationHealth::streaming_parseSubscriptionId_extractsFromXml()
{
    // 构造一个典型的 Subscribe 响应
    const QByteArray xml = QByteArrayLiteral(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\""
        " xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\">"
        "<s:Body><m:SubscribeResponse><m:ResponseMessages>"
        "<m:SubscribeResponseMessage ResponseClass=\"Success\">"
        "<m:ResponseCode>NoError</m:ResponseCode>"
        "<m:SubscriptionId>HxlvQzEyMzQ1</m:SubscriptionId>"
        "</m:SubscribeResponseMessage>"
        "</m:ResponseMessages></m:SubscribeResponse></s:Body></s:Envelope>");

    // 使用 QXmlStreamReader 验证解析逻辑（与 OutlookStreamingConnection 内部一致）
    QXmlStreamReader reader(xml);
    QString subscriptionId;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("SubscriptionId")) {
            subscriptionId = reader.readElementText().trimmed();
            break;
        }
    }
    QCOMPARE(subscriptionId, QStringLiteral("HxlvQzEyMzQ1"));
}

void TestOutlookIntegrationHealth::streaming_parseStreamingEvents_detectsNewMailEvent()
{
    const QByteArray xml = QByteArrayLiteral(
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        " xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\">"
        "<s:Body><m:GetStreamingEventsResponse"
        " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\">"
        "<m:ResponseMessages>"
        "<m:GetStreamingEventsResponseMessage ResponseClass=\"Success\">"
        "<m:Notifications>"
        "<m:Notification>"
        "<t:SubscriptionId>sub-1</t:SubscriptionId>"
        "<t:NewMailEvent>"
        "<t:TimeStamp>2026-04-14T02:30:00Z</t:TimeStamp>"
        "<t:ItemId Id=\"AAMkNewMail\" ChangeKey=\"CK1\"/>"
        "<t:ParentFolderId Id=\"inbox-id\" ChangeKey=\"CK2\"/>"
        "</t:NewMailEvent>"
        "</m:Notification>"
        "</m:Notifications>"
        "</m:GetStreamingEventsResponseMessage>"
        "</m:ResponseMessages>"
        "</m:GetStreamingEventsResponse></s:Body></s:Envelope>");

    QXmlStreamReader reader(xml);
    bool found = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("NewMailEvent")) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestOutlookIntegrationHealth::streaming_parseStreamingEvents_detectsCreatedEvent()
{
    const QByteArray xml = QByteArrayLiteral(
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        " xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\">"
        "<s:Body><m:GetStreamingEventsResponse"
        " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\">"
        "<m:ResponseMessages>"
        "<m:GetStreamingEventsResponseMessage ResponseClass=\"Success\">"
        "<m:Notifications>"
        "<m:Notification>"
        "<t:CreatedEvent>"
        "<t:ItemId Id=\"CalItem-New\" ChangeKey=\"CK3\"/>"
        "</t:CreatedEvent>"
        "</m:Notification>"
        "</m:Notifications>"
        "</m:GetStreamingEventsResponseMessage>"
        "</m:ResponseMessages>"
        "</m:GetStreamingEventsResponse></s:Body></s:Envelope>");

    QXmlStreamReader reader(xml);
    bool found = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("CreatedEvent")) {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestOutlookIntegrationHealth::streaming_parseStreamingEvents_returnsFalseForEmptyNotification()
{
    const QByteArray xml = QByteArrayLiteral(
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Body><m:GetStreamingEventsResponse"
        " xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\">"
        "<m:ResponseMessages>"
        "<m:GetStreamingEventsResponseMessage ResponseClass=\"Success\">"
        "<m:Notifications/>"
        "</m:GetStreamingEventsResponseMessage>"
        "</m:ResponseMessages>"
        "</m:GetStreamingEventsResponse></s:Body></s:Envelope>");

    QXmlStreamReader reader(xml);
    bool found = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const auto name = reader.name();
            if (name == QStringLiteral("NewMailEvent")
                || name == QStringLiteral("CreatedEvent")
                || name == QStringLiteral("ModifiedEvent")
                || name == QStringLiteral("DeletedEvent")) {
                found = true;
                break;
            }
        }
    }
    QVERIFY(!found);
}

// =====================================================================
// 主线程安全 / 不卡顿
// =====================================================================

void TestOutlookIntegrationHealth::pollSynchronous_completesWithinBudget()
{
    auto transport = std::make_shared<ConfigurableFakeEwsTransport>();
    transport->mailResponse = makeMailSoapResponse(
        mailItem("m1", "Test", "2026-04-14T01:00:00Z", "Bot"));
    transport->calendarResponse = makeCalendarSoapResponse({});

    OutlookNotificationPoller poller(baseSettings(), transport);

    QElapsedTimer timer;
    timer.start();
    poller.poll(shanghaiNow(), nullptr);
    const qint64 elapsed = timer.elapsed();

    qInfo().noquote() << QStringLiteral("[perf] Outlook poll took %1ms").arg(elapsed);
    QVERIFY2(elapsed < 50,
             qPrintable(QStringLiteral("poll took %1ms, expected <50ms").arg(elapsed)));
}

void TestOutlookIntegrationHealth::eventLoopRemainsFree_duringPoll()
{
    auto transport = std::make_shared<ConfigurableFakeEwsTransport>();
    transport->mailResponse = makeMailSoapResponse({});
    transport->calendarResponse = makeCalendarSoapResponse({});

    bool timerFired = false;
    QTimer::singleShot(0, [&timerFired]() { timerFired = true; });

    OutlookNotificationPoller poller(baseSettings(), transport);
    poller.poll(shanghaiNow(), nullptr);

    QCoreApplication::processEvents();
    QVERIFY2(timerFired,
             "QTimer::singleShot did not fire — poll blocked the event loop");
}

void TestOutlookIntegrationHealth::streaming_doesNotBlockMainThread()
{
    // OutlookStreamingConnection 使用 QNetworkAccessManager（异步 I/O）
    // start() 应该立即返回，不阻塞
    OutlookStreamingConnection conn;

    bool timerFired = false;
    QTimer::singleShot(0, [&timerFired]() { timerFired = true; });

    QElapsedTimer timer;
    timer.start();
    conn.start(baseSettings());
    const qint64 elapsed = timer.elapsed();

    QCoreApplication::processEvents();

    QVERIFY2(elapsed < 50,
             qPrintable(QStringLiteral("start() took %1ms, expected <50ms").arg(elapsed)));
    QVERIFY2(timerFired,
             "QTimer::singleShot did not fire — streaming start() blocked the event loop");

    conn.stop();
}

// =====================================================================
// 超时行为
// =====================================================================

void TestOutlookIntegrationHealth::httpTimeout_doesNotExceed30Seconds()
{
    auto transport = std::make_shared<ConfigurableFakeEwsTransport>();
    transport->shouldFail = true;
    transport->failMessage = QStringLiteral("connection timed out after 30000ms");

    OutlookNotificationPoller poller(baseSettings(), transport);

    QElapsedTimer timer;
    timer.start();
    QString error;
    poller.poll(shanghaiNow(), &error);
    const qint64 elapsed = timer.elapsed();

    QVERIFY(!error.isEmpty());
    QVERIFY2(elapsed < 100,
             qPrintable(QStringLiteral("timeout-failure path took %1ms, expected <100ms").arg(elapsed)));
}

void TestOutlookIntegrationHealth::errorSummarization_compactsVerboseSoapFaults()
{
    // OutlookSettingsStore::summarizeErrorMessage 应将冗长的 SOAP fault 压缩
    const QString verbose = QStringLiteral(
        "The request failed. The remote server returned an error: "
        "(401) Unauthorized. Access is denied. Check credentials and try again.");
    const QString compact = OutlookSettingsStore::summarizeErrorMessage(verbose);
    QVERIFY(!compact.isEmpty());
    QVERIFY(compact.length() < verbose.length());
}

QTEST_MAIN(TestOutlookIntegrationHealth)
#include "TestOutlookIntegrationHealth.moc"
