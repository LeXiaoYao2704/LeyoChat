#include <QDateTime>
#include <QTimeZone>
#include <QtTest>

#include "integrations/OutlookEwsTransport.h"
#include "services/OutlookNotificationPoller.h"

// ──────────────────────────────────────────────────────────────────────────
// Minimal SOAP response builders for testing
// ──────────────────────────────────────────────────────────────────────────

static QByteArray soapEnvelope(const QByteArray& body)
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

static QByteArray makeMailSoapResponse(const QByteArray& mailItemsXml)
{
    return soapEnvelope(
        QByteArrayLiteral(
            "<m:FindItemResponse><m:ResponseMessages>"
            "<m:FindItemResponseMessage ResponseClass=\"Success\">"
            "<m:ResponseCode>NoError</m:ResponseCode>"
            "<m:RootFolder><t:Items>")
        + mailItemsXml
        + QByteArrayLiteral("</t:Items></m:RootFolder>"
            "</m:FindItemResponseMessage></m:ResponseMessages></m:FindItemResponse>"));
}

static QByteArray makeCalendarSoapResponse(const QByteArray& calItemsXml)
{
    return soapEnvelope(
        QByteArrayLiteral(
            "<m:FindItemResponse><m:ResponseMessages>"
            "<m:FindItemResponseMessage ResponseClass=\"Success\">"
            "<m:ResponseCode>NoError</m:ResponseCode>"
            "<m:RootFolder><t:Items>")
        + calItemsXml
        + QByteArrayLiteral("</t:Items></m:RootFolder>"
            "</m:FindItemResponseMessage></m:ResponseMessages></m:FindItemResponse>"));
}

static QByteArray mailItem(const QByteArray& id,
                            const QByteArray& subject,
                            const QByteArray& receivedDateTime,
                            const QByteArray& senderName)
{
    return QByteArrayLiteral("<t:Message>"
                   "<t:ItemId Id=\"")
        + id
        + QByteArrayLiteral("\" ChangeKey=\"ck-dummy\"/>"
                   "<t:Subject>")
        + subject
        + QByteArrayLiteral("</t:Subject>"
                   "<t:DateTimeReceived>")
        + receivedDateTime
        + QByteArrayLiteral("</t:DateTimeReceived>"
                   "<t:From><t:Mailbox><t:Name>")
        + senderName
        + QByteArrayLiteral("</t:Name></t:Mailbox></t:From>"
                   "</t:Message>");
}

static QByteArray calendarItem(const QByteArray& id,
                                const QByteArray& changeKey,
                                const QByteArray& subject,
                                const QByteArray& start,
                                bool isCancelled)
{
    const QByteArray cancelledStr = isCancelled ? "true" : "false";
    return QByteArrayLiteral("<t:CalendarItem>"
                   "<t:ItemId Id=\"")
        + id
        + QByteArrayLiteral("\" ChangeKey=\"")
        + changeKey
        + QByteArrayLiteral("\"/>"
                   "<t:Subject>")
        + subject
        + QByteArrayLiteral("</t:Subject>"
                   "<t:Start>")
        + start
        + QByteArrayLiteral("</t:Start>"
                   "<t:Location>Room A301</t:Location>"
                   "<t:IsCancelled>")
        + cancelledStr
        + QByteArrayLiteral("</t:IsCancelled>"
                   "<t:Organizer><t:Mailbox><t:Name>Wang Xiaoming</t:Name></t:Mailbox></t:Organizer>"
                   "</t:CalendarItem>");
}

// ──────────────────────────────────────────────────────────────────────────
// Fake EWS transport — dispatches mail vs calendar by inspecting the envelope
// ──────────────────────────────────────────────────────────────────────────

struct FakeEwsTransport : public IOutlookEwsTransport {
    QByteArray mailResponse;
    QByteArray calendarResponse;

    std::optional<QByteArray> soapPost(const QUrl&,
                                       const QByteArray& envelope,
                                       const OutlookConnectionSettings&,
                                       QString*) const override
    {
        if (envelope.contains("inbox"))
            return mailResponse;
        if (envelope.contains("calendar"))
            return calendarResponse;
        return QByteArray{};
    }
};

// ──────────────────────────────────────────────────────────────────────────
// Test class
// ──────────────────────────────────────────────────────────────────────────

class TestOutlookNotificationPoller : public QObject {
    Q_OBJECT

private slots:
    void emitsUnreadMailAndUpcomingEventNotifications();
    void emitsUpdatedMeetingNotificationWhenChangeKeyChanges();
    void emitsCancelledMeetingNotification();
    void suppressesAlreadySeenMailAndMeetingNotifications();
    void pollError_updatesDiagnosticsState();
    void recoversAfterPollErrorAndClearsFailureState();
};

namespace {

QDateTime shanghaiNow()
{
    return QDateTime(QDate(2026, 4, 10), QTime(16, 30), QTimeZone("Asia/Shanghai"));
}

OutlookConnectionSettings baseSettings()
{
    OutlookConnectionSettings settings;
    settings.enabled = true;
    settings.serverUrl = QStringLiteral("https://mail.example.com");
    settings.username = QStringLiteral("testuser");
    settings.password = QStringLiteral("test-password");
    settings.accountEmail = QStringLiteral("user@example.com");
    settings.notificationsEnabled = true;
    settings.notificationPollIntervalMinutes = 5;
    return settings;
}

}  // namespace

void TestOutlookNotificationPoller::emitsUnreadMailAndUpcomingEventNotifications()
{
    auto ewsTransport = std::make_shared<FakeEwsTransport>();
    ewsTransport->mailResponse = makeMailSoapResponse(
        mailItem("mail-1", "Build failure alert", "2026-04-10T09:00:00Z", "Build Bot"));
    ewsTransport->calendarResponse = makeCalendarSoapResponse(
        calendarItem("event-1", "ck-1", "Project daily sync",
                     "2026-04-10T17:00:00+08:00", false));

    OutlookNotificationPoller poller(baseSettings(), ewsTransport);
    const auto result = poller.poll(shanghaiNow(), nullptr);
    QCOMPARE(result.events.size(), 2);
    QCOMPARE(result.events.at(0).kind, OutlookNotificationKind::MailReceived);
    QCOMPARE(result.events.at(0).resourceId, QStringLiteral("mail-1"));
    QCOMPARE(result.events.at(1).kind, OutlookNotificationKind::CalendarReminder);
    QCOMPARE(result.events.at(1).resourceId, QStringLiteral("event-1"));
    QVERIFY(result.updatedSettings.recentMailIds.contains(QStringLiteral("mail-1")));
    QVERIFY(result.updatedSettings.recentEventIds.contains(QStringLiteral("event-1|ck-1|active")));
}

void TestOutlookNotificationPoller::emitsUpdatedMeetingNotificationWhenChangeKeyChanges()
{
    auto ewsTransport = std::make_shared<FakeEwsTransport>();
    ewsTransport->mailResponse = makeMailSoapResponse({});
    ewsTransport->calendarResponse = makeCalendarSoapResponse(
        calendarItem("event-2", "ck-2", "Design review",
                     "2026-04-10T17:15:00+08:00", false));

    OutlookConnectionSettings settings = baseSettings();
    settings.recentEventIds = {QStringLiteral("event-2|ck-1|active")};

    OutlookNotificationPoller poller(settings, ewsTransport);
    const auto result = poller.poll(shanghaiNow(), nullptr);
    QCOMPARE(result.events.size(), 1);
    QCOMPARE(result.events.front().kind, OutlookNotificationKind::CalendarUpdated);
    QCOMPARE(result.events.front().resourceId, QStringLiteral("event-2"));
    QVERIFY(result.updatedSettings.recentEventIds.contains(QStringLiteral("event-2|ck-2|active")));
}

void TestOutlookNotificationPoller::emitsCancelledMeetingNotification()
{
    auto ewsTransport = std::make_shared<FakeEwsTransport>();
    ewsTransport->mailResponse = makeMailSoapResponse({});
    ewsTransport->calendarResponse = makeCalendarSoapResponse(
        calendarItem("event-3", "ck-7", "Release sync",
                     "2026-04-10T17:30:00+08:00", true));

    OutlookConnectionSettings settings = baseSettings();
    settings.recentEventIds = {QStringLiteral("event-3|ck-6|active")};

    OutlookNotificationPoller poller(settings, ewsTransport);
    const auto result = poller.poll(shanghaiNow(), nullptr);
    QCOMPARE(result.events.size(), 1);
    QCOMPARE(result.events.front().kind, OutlookNotificationKind::CalendarCancelled);
    QCOMPARE(result.events.front().resourceId, QStringLiteral("event-3"));
    QVERIFY(result.updatedSettings.recentEventIds.contains(QStringLiteral("event-3|ck-7|cancelled")));
}

void TestOutlookNotificationPoller::suppressesAlreadySeenMailAndMeetingNotifications()
{
    auto ewsTransport = std::make_shared<FakeEwsTransport>();
    ewsTransport->mailResponse = makeMailSoapResponse(
        mailItem("mail-1", "Build failure alert", "2026-04-10T09:00:00Z", "Build Bot"));
    ewsTransport->calendarResponse = makeCalendarSoapResponse(
        calendarItem("event-1", "ck-1", "Project daily sync",
                     "2026-04-10T17:00:00+08:00", false));

    OutlookConnectionSettings settings = baseSettings();
    settings.recentMailIds = {QStringLiteral("mail-1")};
    settings.recentEventIds = {QStringLiteral("event-1|ck-1|active")};

    OutlookNotificationPoller poller(settings, ewsTransport);
    const auto result = poller.poll(shanghaiNow(), nullptr);
    QVERIFY(result.events.isEmpty());
    QVERIFY(result.updatedSettings.recentMailIds.contains(QStringLiteral("mail-1")));
    QVERIFY(result.updatedSettings.recentEventIds.contains(QStringLiteral("event-1|ck-1|active")));
}

void TestOutlookNotificationPoller::pollError_updatesDiagnosticsState()
{
    class FailingEwsTransport : public IOutlookEwsTransport {
    public:
        std::optional<QByteArray> soapPost(const QUrl&,
                                           const QByteArray&,
                                           const OutlookConnectionSettings&,
                                           QString* errorMessage) const override
        {
            if (errorMessage)
                *errorMessage = QStringLiteral("token refresh failed");
            return std::nullopt;
        }
    };

    OutlookNotificationPoller poller(baseSettings(),
                                     std::make_shared<FailingEwsTransport>());
    QString errorMessage;
    const auto result = poller.poll(shanghaiNow(), &errorMessage);
    QVERIFY(result.events.isEmpty());
    QCOMPARE(errorMessage, QStringLiteral("token refresh failed; token refresh failed"));
    QCOMPARE(result.updatedSettings.consecutivePollFailures, 1);
    QCOMPARE(result.updatedSettings.lastPollErrorMessage,
             QStringLiteral("token refresh failed; token refresh failed"));
    QCOMPARE(result.updatedSettings.lastPollErrorCategory, QStringLiteral("auth"));
    QVERIFY(result.updatedSettings.lastPollAttemptAtMs > 0);
}

void TestOutlookNotificationPoller::recoversAfterPollErrorAndClearsFailureState()
{
    class RecoveringEwsTransport : public IOutlookEwsTransport {
    public:
        mutable int requestCount = 0;

        std::optional<QByteArray> soapPost(const QUrl&,
                                           const QByteArray& envelope,
                                           const OutlookConnectionSettings&,
                                           QString* errorMessage) const override
        {
            ++requestCount;
            if (requestCount <= 2) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("temporary network failure");
                return std::nullopt;
            }
            if (envelope.contains("inbox"))
                return makeMailSoapResponse(
                    mailItem("mail-1", "Build failure alert", "2026-04-10T09:00:00Z", "Build Bot"));
            if (envelope.contains("calendar"))
                return makeCalendarSoapResponse({});
            return QByteArray{};
        }
    };

    auto transport = std::make_shared<RecoveringEwsTransport>();
    OutlookNotificationPoller poller(baseSettings(), transport);

    QString firstError;
    const auto failedResult = poller.poll(shanghaiNow(), &firstError);
    QCOMPARE(firstError, QStringLiteral("temporary network failure; temporary network failure"));
    QCOMPARE(failedResult.updatedSettings.consecutivePollFailures, 1);
    QCOMPARE(failedResult.updatedSettings.lastPollErrorCategory, QStringLiteral("network"));

    OutlookNotificationPoller recoveredPoller(failedResult.updatedSettings, transport);
    QString secondError;
    const auto recoveredResult = recoveredPoller.poll(shanghaiNow().addSecs(120), &secondError);
    QVERIFY(secondError.isEmpty());
    QCOMPARE(recoveredResult.updatedSettings.consecutivePollFailures, 0);
    QVERIFY(recoveredResult.updatedSettings.lastPollErrorMessage.isEmpty());
    QVERIFY(recoveredResult.updatedSettings.lastPollErrorCategory.isEmpty());
    QCOMPARE(recoveredResult.events.size(), 1);
    QCOMPARE(recoveredResult.events.front().kind, OutlookNotificationKind::MailReceived);
}

QTEST_MAIN(TestOutlookNotificationPoller)
#include "TestOutlookNotificationPoller.moc"
