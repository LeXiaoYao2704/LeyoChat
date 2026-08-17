#include <QtTest>

#include "app/AttachmentPayloadHelpers.h"

#include <QJsonDocument>
#include <QJsonObject>

class TestAttachmentPayloadHelpers : public QObject {
    Q_OBJECT

private slots:
    void tryExtractInlineGroupAttachment_rejectsNonGroupMessage();
    void tryExtractInlineGroupAttachment_readsNameFromEnvelopeFirst();
    void tryExtractInlineGroupAttachment_usesJsonNameAndTextFallbacks();
    void tryExtractInlineGroupAttachment_rejectsMissingPayload();
};

void TestAttachmentPayloadHelpers::tryExtractInlineGroupAttachment_rejectsNonGroupMessage()
{
    MessageEnvelope envelope;
    envelope.type = MessageType::ChatText;
    envelope.body = R"({"message_kind":"attachment","attachment_name":"photo.png","base64":"aGVsbG8="})";

    QVERIFY(!tryExtractInlineGroupAttachment(envelope, nullptr, nullptr, nullptr));
}

void TestAttachmentPayloadHelpers::tryExtractInlineGroupAttachment_readsNameFromEnvelopeFirst()
{
    MessageEnvelope envelope;
    envelope.type = MessageType::GroupMessage;
    envelope.attachmentName = "from-envelope.png";
    envelope.body = R"({"message_kind":"attachment","attachment_name":"from-json.png","base64":"aGVsbG8=","text":"preview"})";

    QString attachmentName;
    QByteArray payload;
    QString preview;

    QVERIFY(tryExtractInlineGroupAttachment(envelope, &attachmentName, &payload, &preview));
    QCOMPARE(attachmentName, QStringLiteral("from-envelope.png"));
    QCOMPARE(payload, QByteArray("aGVsbG8="));
    QCOMPARE(preview, QStringLiteral("preview"));
}

void TestAttachmentPayloadHelpers::tryExtractInlineGroupAttachment_usesJsonNameAndTextFallbacks()
{
    MessageEnvelope envelope;
    envelope.type = MessageType::GroupMessage;
    envelope.body = R"({"message_kind":"attachment","attachment_name":"from-json.png","base64":"aGVsbG8="})";

    QString attachmentName;
    QByteArray payload;
    QString preview;

    QVERIFY(tryExtractInlineGroupAttachment(envelope, &attachmentName, &payload, &preview));
    QCOMPARE(attachmentName, QStringLiteral("from-json.png"));
    QCOMPARE(payload, QByteArray("aGVsbG8="));
    QVERIFY(preview.contains(QStringLiteral("from-json.png")));
}

void TestAttachmentPayloadHelpers::tryExtractInlineGroupAttachment_rejectsMissingPayload()
{
    MessageEnvelope envelope;
    envelope.type = MessageType::GroupMessage;
    envelope.body = R"({"message_kind":"attachment","attachment_name":"from-json.png"})";

    QVERIFY(!tryExtractInlineGroupAttachment(envelope, nullptr, nullptr, nullptr));
}

QTEST_MAIN(TestAttachmentPayloadHelpers)
#include "TestAttachmentPayloadHelpers.moc"
