#include <QtTest>
#include <QDateTime>

#include "integrations/OutlookEwsTransport.h"
#include "integrations/OutlookSettings.h"

class TestOutlookEwsTransport : public QObject {
    Q_OBJECT

private slots:
    void buildSoapEnvelope_wrapsBodyInCorrectNamespaces();
    void findUnreadMailEnvelope_containsMaxItemsAndInboxFolder();
    void findCalendarEnvelope_containsCalendarViewWithDates();
    void parseMailFindItemResponse_extractsMailFields();
    void parseMailFindItemResponse_skipsItemsWithEmptyId();
    void parseCalendarFindItemResponse_extractsCalendarFields();
    void isSoapResponseOk_returnsTrueForNoError();
    void isSoapResponseOk_returnsFalseAndExtractsMessageForFault();
};

void TestOutlookEwsTransport::buildSoapEnvelope_wrapsBodyInCorrectNamespaces()
{
    const QByteArray envelope = OutlookEws::buildSoapEnvelope("<m:Test/>");
    QVERIFY(envelope.contains("soap:Envelope"));
    QVERIFY(envelope.contains("http://schemas.xmlsoap.org/soap/envelope/"));
    QVERIFY(envelope.contains("http://schemas.microsoft.com/exchange/services/2006/types"));
    QVERIFY(envelope.contains("http://schemas.microsoft.com/exchange/services/2006/messages"));
    QVERIFY(envelope.contains("<soap:Body>"));
    QVERIFY(envelope.contains("<m:Test/>"));
    QVERIFY(envelope.contains("</soap:Body>"));
}

void TestOutlookEwsTransport::findUnreadMailEnvelope_containsMaxItemsAndInboxFolder()
{
    const QByteArray envelope = OutlookEws::findUnreadMailEnvelope(7);
    QVERIFY(envelope.contains("FindItem"));
    QVERIFY(envelope.contains("message:IsRead"));
    QVERIFY(envelope.contains("false"));
    QVERIFY(envelope.contains("7")); // maxItems
    QVERIFY(envelope.contains("inbox"));
}

void TestOutlookEwsTransport::findCalendarEnvelope_containsCalendarViewWithDates()
{
    QDateTime start = QDateTime::fromString(QStringLiteral("2026-04-12T09:00:00"), Qt::ISODate);
    QDateTime end   = QDateTime::fromString(QStringLiteral("2026-04-12T11:00:00"), Qt::ISODate);
    start.setTimeSpec(Qt::UTC);
    end.setTimeSpec(Qt::UTC);

    const QByteArray envelope = OutlookEws::findCalendarEnvelope(start, end);
    QVERIFY(envelope.contains("CalendarView"));
    QVERIFY(envelope.contains("2026-04-12T09:00:00"));
    QVERIFY(envelope.contains("2026-04-12T11:00:00"));
    QVERIFY(envelope.contains("calendar"));
}

void TestOutlookEwsTransport::parseMailFindItemResponse_extractsMailFields()
{
    const QByteArray xml = QByteArrayLiteral(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        "  xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\""
        "  xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\">"
        "<s:Body><m:FindItemResponse><m:ResponseMessages>"
        "<m:FindItemResponseMessage ResponseClass=\"Success\">"
        "<m:ResponseCode>NoError</m:ResponseCode>"
        "<m:RootFolder><t:Items>"
        "<t:Message>"
        "  <t:ItemId Id=\"AAMkABC\" ChangeKey=\"CQAAA\"/>"
        "  <t:Subject>项目评审</t:Subject>"
        "  <t:DateTimeReceived>2026-04-12T08:00:00Z</t:DateTimeReceived>"
        "  <t:IsRead>false</t:IsRead>"
        "  <t:From><t:Mailbox><t:Name>李四</t:Name></t:Mailbox></t:From>"
        "</t:Message>"
        "</t:Items></m:RootFolder>"
        "</m:FindItemResponseMessage>"
        "</m:ResponseMessages></m:FindItemResponse></s:Body></s:Envelope>");

    const auto mails = OutlookEws::parseMailFindItemResponse(xml, QStringLiteral("user@example.com"));
    QCOMPARE(mails.size(), 1);
    QCOMPARE(mails.at(0).resourceId, QStringLiteral("AAMkABC"));
    QCOMPARE(mails.at(0).subject, QStringLiteral("项目评审"));
    QCOMPARE(mails.at(0).sender, QStringLiteral("李四"));
    QVERIFY(!mails.at(0).receivedAtLabel.isEmpty());
    QCOMPARE(mails.at(0).mailbox, QStringLiteral("user@example.com"));
}

void TestOutlookEwsTransport::parseMailFindItemResponse_skipsItemsWithEmptyId()
{
    const QByteArray xml = QByteArrayLiteral(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        "  xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\""
        "  xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\">"
        "<s:Body><m:FindItemResponse><m:ResponseMessages>"
        "<m:FindItemResponseMessage ResponseClass=\"Success\">"
        "<m:ResponseCode>NoError</m:ResponseCode>"
        "<m:RootFolder><t:Items>"
        "<t:Message>"
        "  <t:ItemId Id=\"\" ChangeKey=\"\"/>"
        "  <t:Subject>No ID item</t:Subject>"
        "</t:Message>"
        "</t:Items></m:RootFolder>"
        "</m:FindItemResponseMessage>"
        "</m:ResponseMessages></m:FindItemResponse></s:Body></s:Envelope>");

    const auto mails = OutlookEws::parseMailFindItemResponse(xml, QStringLiteral("user@example.com"));
    QCOMPARE(mails.size(), 0);
}

void TestOutlookEwsTransport::parseCalendarFindItemResponse_extractsCalendarFields()
{
    const QByteArray xml = QByteArrayLiteral(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
        "  xmlns:m=\"http://schemas.microsoft.com/exchange/services/2006/messages\""
        "  xmlns:t=\"http://schemas.microsoft.com/exchange/services/2006/types\">"
        "<s:Body><m:FindItemResponse><m:ResponseMessages>"
        "<m:FindItemResponseMessage ResponseClass=\"Success\">"
        "<m:ResponseCode>NoError</m:ResponseCode>"
        "<m:RootFolder><t:Items>"
        "<t:CalendarItem>"
        "  <t:ItemId Id=\"CalItem1\" ChangeKey=\"CKXYZ\"/>"
        "  <t:Subject>周例会</t:Subject>"
        "  <t:Start>2026-04-12T10:00:00Z</t:Start>"
        "  <t:Location>会议室A</t:Location>"
        "  <t:IsCancelled>false</t:IsCancelled>"
        "  <t:Organizer><t:Mailbox><t:Name>张三</t:Name></t:Mailbox></t:Organizer>"
        "</t:CalendarItem>"
        "</t:Items></m:RootFolder>"
        "</m:FindItemResponseMessage>"
        "</m:ResponseMessages></m:FindItemResponse></s:Body></s:Envelope>");

    const auto events = OutlookEws::parseCalendarFindItemResponse(xml);
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.at(0).resourceId, QStringLiteral("CalItem1"));
    QCOMPARE(events.at(0).changeKey, QStringLiteral("CKXYZ"));
    QCOMPARE(events.at(0).subject, QStringLiteral("周例会"));
    QCOMPARE(events.at(0).location, QStringLiteral("会议室A"));
    QCOMPARE(events.at(0).organizer, QStringLiteral("张三"));
    QVERIFY(!events.at(0).whenLabel.isEmpty());
    QVERIFY(!events.at(0).cancelled);
}

void TestOutlookEwsTransport::isSoapResponseOk_returnsTrueForNoError()
{
    const QByteArray xml = QByteArrayLiteral(
        "<m:FindItemResponseMessage ResponseClass=\"Success\">"
        "<m:ResponseCode>NoError</m:ResponseCode>"
        "</m:FindItemResponseMessage>");
    QString error;
    QVERIFY(OutlookEws::isSoapResponseOk(xml, &error));
    QVERIFY(error.isEmpty());
}

void TestOutlookEwsTransport::isSoapResponseOk_returnsFalseAndExtractsMessageForFault()
{
    const QByteArray xml = QByteArrayLiteral(
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\">"
        "<s:Body><s:Fault>"
        "<faultcode>s:Client</faultcode>"
        "<faultstring>无效凭据</faultstring>"
        "</s:Fault></s:Body></s:Envelope>");
    QString error;
    QVERIFY(!OutlookEws::isSoapResponseOk(xml, &error));
    QVERIFY(error.contains(QStringLiteral("无效凭据")));
}

QTEST_MAIN(TestOutlookEwsTransport)
#include "TestOutlookEwsTransport.moc"
