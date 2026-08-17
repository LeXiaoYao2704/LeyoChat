#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "MessageServiceHttpContracts.h"

class TestMessageServiceHttpContracts : public QObject {
    Q_OBJECT

private slots:
    void parseStoreMessageRequest_usesAuthenticatedSender();
    void parseStoreMessageRequest_rejectsMissingRequiredFields();
    void parseStoreMessageRequest_rejectsEmptyRecipients();
    void parseStoreMessageRequest_compactsPayloadObject();
    void storedMessageToJson_roundTripsPayloadObject();
    void parseAckSeq_rejectsNegativeAndMissingSeq();
};

void TestMessageServiceHttpContracts::parseStoreMessageRequest_usesAuthenticatedSender()
{
    QJsonObject object;
    object[QStringLiteral("clientMessageId")] = QStringLiteral("local-1");
    object[QStringLiteral("conversationId")] = QStringLiteral("conv-1");
    object[QStringLiteral("workspaceId")] = QStringLiteral("ws-1");
    object[QStringLiteral("senderId")] = QStringLiteral("spoofed-client");
    object[QStringLiteral("type")] = QStringLiteral("chat_text");
    object[QStringLiteral("body")] = QStringLiteral("hello");
    object[QStringLiteral("contentType")] = QStringLiteral("text/plain");
    object[QStringLiteral("recipientIds")] =
        QJsonArray{QStringLiteral("client-2"), QStringLiteral("client-3")};

    QString error;
    const auto request = MessageServiceHttpContracts::parseStoreMessageRequest(
        object, QStringLiteral("client-1"), &error);

    QVERIFY2(request.has_value(), qPrintable(error));
    QCOMPARE(request->senderId, QStringLiteral("client-1"));
    QCOMPARE(request->clientMessageId, QStringLiteral("local-1"));
    QCOMPARE(request->conversationId, QStringLiteral("conv-1"));
    QCOMPARE(request->workspaceId, QStringLiteral("ws-1"));
    QCOMPARE(request->type, QStringLiteral("chat_text"));
    QCOMPARE(request->recipientIds,
             QStringList({QStringLiteral("client-2"), QStringLiteral("client-3")}));
}

void TestMessageServiceHttpContracts::parseStoreMessageRequest_rejectsMissingRequiredFields()
{
    QJsonObject object;
    object[QStringLiteral("clientMessageId")] = QStringLiteral("local-1");
    object[QStringLiteral("workspaceId")] = QStringLiteral("ws-1");
    object[QStringLiteral("type")] = QStringLiteral("chat_text");
    object[QStringLiteral("recipientIds")] = QJsonArray{QStringLiteral("client-2")};

    QString error;
    const auto request = MessageServiceHttpContracts::parseStoreMessageRequest(
        object, QStringLiteral("client-1"), &error);

    QVERIFY(!request.has_value());
    QVERIFY(error.contains(QStringLiteral("conversationId")));
}

void TestMessageServiceHttpContracts::parseStoreMessageRequest_rejectsEmptyRecipients()
{
    QJsonObject object;
    object[QStringLiteral("clientMessageId")] = QStringLiteral("local-1");
    object[QStringLiteral("conversationId")] = QStringLiteral("conv-1");
    object[QStringLiteral("workspaceId")] = QStringLiteral("ws-1");
    object[QStringLiteral("type")] = QStringLiteral("chat_text");
    object[QStringLiteral("recipientIds")] = QJsonArray{};

    QString error;
    const auto request = MessageServiceHttpContracts::parseStoreMessageRequest(
        object, QStringLiteral("client-1"), &error);

    QVERIFY(!request.has_value());
    QVERIFY(error.contains(QStringLiteral("recipientIds")));
}

void TestMessageServiceHttpContracts::parseStoreMessageRequest_compactsPayloadObject()
{
    QJsonObject object;
    object[QStringLiteral("clientMessageId")] = QStringLiteral("local-1");
    object[QStringLiteral("conversationId")] = QStringLiteral("conv-1");
    object[QStringLiteral("workspaceId")] = QStringLiteral("ws-1");
    object[QStringLiteral("type")] = QStringLiteral("chat_text");
    object[QStringLiteral("payload")] = QJsonObject{
        {QStringLiteral("format"), QStringLiteral("plain")},
        {QStringLiteral("revision"), 2}
    };
    object[QStringLiteral("recipientIds")] = QJsonArray{QStringLiteral("client-2")};

    QString error;
    const auto request = MessageServiceHttpContracts::parseStoreMessageRequest(
        object, QStringLiteral("client-1"), &error);

    QVERIFY2(request.has_value(), qPrintable(error));
    const auto payloadDoc = QJsonDocument::fromJson(request->payloadJson.toUtf8());
    QVERIFY(payloadDoc.isObject());
    QCOMPARE(payloadDoc.object()[QStringLiteral("format")].toString(),
             QStringLiteral("plain"));
    QCOMPARE(payloadDoc.object()[QStringLiteral("revision")].toInt(), 2);
}

void TestMessageServiceHttpContracts::storedMessageToJson_roundTripsPayloadObject()
{
    StoredMessage message;
    message.serverMessageId = QStringLiteral("server-1");
    message.clientMessageId = QStringLiteral("local-1");
    message.conversationId = QStringLiteral("conv-1");
    message.workspaceId = QStringLiteral("ws-1");
    message.senderId = QStringLiteral("client-1");
    message.serverSeq = 7;
    message.type = QStringLiteral("chat_text");
    message.body = QStringLiteral("hello");
    message.payloadJson = QStringLiteral("{\"format\":\"plain\"}");
    message.fileId = QStringLiteral("file-1");
    message.contentType = QStringLiteral("text/plain");
    message.replyToMessageId = QStringLiteral("server-0");
    message.createdAtMs = 1000;

    const auto object = MessageServiceHttpContracts::storedMessageToJson(message);

    QCOMPARE(object[QStringLiteral("serverMessageId")].toString(),
             QStringLiteral("server-1"));
    QCOMPARE(object[QStringLiteral("serverSeq")].toInteger(), qint64(7));
    QCOMPARE(object[QStringLiteral("payload")].toObject()[QStringLiteral("format")].toString(),
             QStringLiteral("plain"));
    QCOMPARE(object[QStringLiteral("fileId")].toString(), QStringLiteral("file-1"));
    QCOMPARE(object[QStringLiteral("replyToMessageId")].toString(),
             QStringLiteral("server-0"));
}

void TestMessageServiceHttpContracts::parseAckSeq_rejectsNegativeAndMissingSeq()
{
    QString error;
    QVERIFY(!MessageServiceHttpContracts::parseAckSeq(
                QJsonObject{}, QStringLiteral("receivedSeq"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("receivedSeq")));

    QJsonObject negative;
    negative[QStringLiteral("receivedSeq")] = -1;
    error.clear();
    QVERIFY(!MessageServiceHttpContracts::parseAckSeq(
                negative, QStringLiteral("receivedSeq"), &error).has_value());
    QVERIFY(error.contains(QStringLiteral("receivedSeq")));

    QJsonObject valid;
    valid[QStringLiteral("receivedSeq")] = 3;
    error.clear();
    const auto seq = MessageServiceHttpContracts::parseAckSeq(
        valid, QStringLiteral("receivedSeq"), &error);
    QVERIFY2(seq.has_value(), qPrintable(error));
    QCOMPARE(*seq, qint64(3));
}

QTEST_MAIN(TestMessageServiceHttpContracts)
#include "TestMessageServiceHttpContracts.moc"
