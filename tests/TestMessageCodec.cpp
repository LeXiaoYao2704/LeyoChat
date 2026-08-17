#include <QtTest/QTest>
#include <QCoreApplication>
#include <QRegularExpression>

#include "architecture/ResourceReferenceMessage.h"
#include "domain/GroupProtocol.h"
#include "domain/MessageEnvelope.h"
#include "domain/MessageMutation.h"
#include "network/MessageCodec.h"
#include "network/PeerHandshake.h"

class TestMessageCodec : public QObject {
    Q_OBJECT

private slots:
    void roundTripsTextMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "msg-001";
        envelope.type = MessageType::ChatText;
        envelope.senderId = "client-a";
        envelope.targetId = "client-b";
        envelope.conversationId = "conv-001";
        envelope.body = "hello";
        envelope.createdAtMs = 1712000000000;

        const std::string encoded = MessageCodec::encode(envelope);
        const auto decoded = MessageCodec::decode(encoded);

        QVERIFY(decoded.has_value());
        QVERIFY(decoded->messageId == envelope.messageId);
        QCOMPARE(decoded->type, envelope.type);
        QVERIFY(decoded->senderId == envelope.senderId);
        QVERIFY(decoded->targetId == envelope.targetId);
        QVERIFY(decoded->conversationId == envelope.conversationId);
        QVERIFY(decoded->body == envelope.body);
        QCOMPARE(decoded->createdAtMs, envelope.createdAtMs);
    }

    void roundTripsTextMessageWithCustomContentType() {
        MessageEnvelope envelope;
        envelope.messageId = "msg-nudge-001";
        envelope.type = MessageType::ChatText;
        envelope.senderId = "client-a";
        envelope.targetId = "client-b";
        envelope.conversationId = "conv-001";
        envelope.body = "nudge";
        envelope.contentType = "nudge";
        envelope.createdAtMs = 1712000000100;

        const std::string encoded = MessageCodec::encode(envelope);
        const auto decoded = MessageCodec::decode(encoded);

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::ChatText);
        QVERIFY(decoded->contentType == envelope.contentType);
        QVERIFY(decoded->body == envelope.body);
    }

    void roundTripsExtendedMessageFields();

    void rejectsInvalidPayload() {
        const auto decoded = MessageCodec::decode("not-json");
        QVERIFY(!decoded.has_value());
    }

    void roundTripsFileAttachmentMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "file-001";
        envelope.type = MessageType::FileAttachment;
        envelope.senderId = "client-a";
        envelope.targetId = "client-b";
        envelope.conversationId = "client-a|client-b";
        envelope.body = "ZmFrZS1ieXRlcw==";
        envelope.attachmentName = "report.txt";
        envelope.createdAtMs = 1712000001000;

        const std::string encoded = MessageCodec::encode(envelope);
        const auto decoded = MessageCodec::decode(encoded);

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, envelope.type);
        QVERIFY(decoded->attachmentName == envelope.attachmentName);
        QVERIFY(decoded->body == envelope.body);
    }

    void roundTripsFileControlMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "control-001";
        envelope.type = MessageType::FileControl;
        envelope.senderId = "client-a";
        envelope.targetId = "client-b";
        envelope.conversationId = "conv-ctrl";
        envelope.controlType = "offer";
        envelope.fileTaskId = "task-001";
        envelope.attachmentName = "report.zip";
        envelope.fileHash = "hash-001";
        envelope.dataHost = "192.0.2.10";
        envelope.fileSize = 8192;
        envelope.chunkSize = 1024;
        envelope.chunkCount = 8;
        envelope.dataPort = 45460;
        envelope.completedChunks = {0, 1, 3};
        envelope.createdAtMs = 1712000002000;

        const std::string encoded = MessageCodec::encode(envelope);
        const auto decoded = MessageCodec::decode(encoded);

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, envelope.type);
        QVERIFY(decoded->controlType == envelope.controlType);
        QVERIFY(decoded->fileTaskId == envelope.fileTaskId);
        QVERIFY(decoded->fileHash == envelope.fileHash);
        QVERIFY(decoded->dataHost == envelope.dataHost);
        QCOMPARE(decoded->fileSize, envelope.fileSize);
        QCOMPARE(decoded->chunkSize, envelope.chunkSize);
        QCOMPARE(decoded->chunkCount, envelope.chunkCount);
        QCOMPARE(decoded->dataPort, envelope.dataPort);
        QCOMPARE(decoded->completedChunks.size(), envelope.completedChunks.size());
        QCOMPARE(decoded->completedChunks[2], envelope.completedChunks[2]);
    }

    void roundTripsCallControlMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "call-control-001";
        envelope.type = MessageType::CallControl;
        envelope.senderId = "client-a";
        envelope.targetId = "client-b";
        envelope.conversationId = "conv-call";
        envelope.controlType = "call-offer";
        envelope.payloadJson = "{\"call_id\":\"c-1\",\"media_flags\":1}";
        envelope.reason = "";
        envelope.createdAtMs = 1712000002300;

        const auto decoded = MessageCodec::decode(MessageCodec::encode(envelope));

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::CallControl);
        QVERIFY(decoded->controlType == envelope.controlType);
        QVERIFY(decoded->payloadJson == envelope.payloadJson);
    }

    void roundTripsFileChunkMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "chunk-001";
        envelope.type = MessageType::FileChunk;
        envelope.senderId = "client-a";
        envelope.targetId = "client-b";
        envelope.fileTaskId = "task-001";
        envelope.chunkIndex = 3;
        envelope.body = "QUJDREVGRw==";
        envelope.createdAtMs = 1712000002500;

        const auto decoded = MessageCodec::decode(MessageCodec::encode(envelope));

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::FileChunk);
        QVERIFY(decoded->fileTaskId == envelope.fileTaskId);
        QCOMPARE(decoded->chunkIndex, envelope.chunkIndex);
        QVERIFY(decoded->body == envelope.body);
    }

    void roundTripsPeerDirectorySnapshotMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "peers-001";
        envelope.type = MessageType::PeerDirectorySnapshot;
        envelope.senderId = "client-a";
        envelope.body =
            R"({"peers":[{"client_id":"client-b","display_name":"Alice","host":"192.0.2.10","port":45454}]})";
        envelope.createdAtMs = 1712000003000;

        const std::string encoded = MessageCodec::encode(envelope);
        const auto decoded = MessageCodec::decode(encoded);

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, envelope.type);
        QVERIFY(decoded->senderId == envelope.senderId);
        QVERIFY(decoded->body == envelope.body);
    }

    void handshakeHelloRoundTripsAvatarPayload() {
        PeerHello hello;
        hello.clientId = QStringLiteral("client-a");
        hello.displayName = QStringLiteral("Alice");
        hello.listenPort = 45454;
        hello.signature = QStringLiteral("研发一组");
        hello.presence = PeerPresenceStatus::Away;
        hello.avatarBase64 = QStringLiteral("aGVsbG8=");

        const MessageEnvelope envelope = PeerHandshake::buildHelloEnvelope(hello);
        const auto parsed = PeerHandshake::parseHelloEnvelope(envelope);

        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->clientId, hello.clientId);
        QCOMPARE(parsed->displayName, hello.displayName);
        QCOMPARE(parsed->listenPort, hello.listenPort);
        QCOMPARE(parsed->signature, hello.signature);
        QCOMPARE(parsed->presence, hello.presence);
        QCOMPARE(parsed->avatarBase64, hello.avatarBase64);
    }

    void roundTripsGroupMetaMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "group-meta-001";
        envelope.type = MessageType::GroupMeta;
        envelope.senderId = "owner-001";
        envelope.targetId = "user-002";
        envelope.conversationId = "group-001";
        envelope.body = R"({"event_type":"create","group_id":"group-001","group_version":1})";
        envelope.createdAtMs = 1712200000000;

        const std::string encoded = MessageCodec::encode(envelope);
        const auto decoded = MessageCodec::decode(encoded);

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::GroupMeta);
        QVERIFY(decoded->body == envelope.body);
    }

    void roundTripsGroupMessage() {
        MessageEnvelope envelope;
        envelope.messageId = "group-msg-001";
        envelope.type = MessageType::GroupMessage;
        envelope.senderId = "owner-001";
        envelope.targetId = "user-002";
        envelope.conversationId = "group-001";
        envelope.body = R"({"group_id":"group-001","message_kind":"text","text":"hello team"})";
        envelope.createdAtMs = 1712200001000;

        const auto decoded = MessageCodec::decode(MessageCodec::encode(envelope));
        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::GroupMessage);
    }

    void roundTripsResourceReferenceMessage() {
        ResourceReferenceMessagePayload payload;
        payload.resource.resourceId = QStringLiteral("res-file-001");
        payload.resource.resourceKind = QStringLiteral("shared_file");
        payload.resource.title = QStringLiteral("Spec Board");
        payload.resource.workspaceId = QStringLiteral("workspace-design");
        payload.resource.serviceId = QStringLiteral("service-leyo");
        payload.resource.version = QStringLiteral("v3");
        payload.resource.origin = ResourceOrigin::Service;
        payload.summary = QStringLiteral("共享规格文件");

        const MessageEnvelope envelope = buildResourceReferenceEnvelope(QStringLiteral("resource-001"),
                                                                       QStringLiteral("client-a"),
                                                                       QStringLiteral("group-001"),
                                                                       QStringLiteral("group-001"),
                                                                       payload,
                                                                       1712200002000);

        const auto decoded = MessageCodec::decode(MessageCodec::encode(envelope));
        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::ResourceReference);
        QVERIFY(decoded->resourceId == envelope.resourceId);
        QVERIFY(decoded->resourceKind == envelope.resourceKind);
        QVERIFY(decoded->resourceTitle == envelope.resourceTitle);
        QVERIFY(decoded->workspaceId == envelope.workspaceId);
        QVERIFY(decoded->serviceId == envelope.serviceId);

        const auto parsedPayload = parseResourceReferenceEnvelope(*decoded);
        QVERIFY(parsedPayload.has_value());
        QCOMPARE(parsedPayload->resource.resourceId, payload.resource.resourceId);
        QCOMPARE(parsedPayload->resource.resourceKind, payload.resource.resourceKind);
        QCOMPARE(parsedPayload->resource.title, payload.resource.title);
        QCOMPARE(parsedPayload->resource.workspaceId, payload.resource.workspaceId);
        QCOMPARE(parsedPayload->resource.serviceId, payload.resource.serviceId);
        QCOMPARE(parsedPayload->summary, payload.summary);
    }

    void decodesLegacyPayloadWithoutExtendedFields();
    void keepsUnknownSubtypeTextSafe();
    void unknownTypeEmitsDropWarning();
    void encodesChatTextWithoutEmptyCompletedChunks();
    void skipsUnknownJsonValueTypes();

    // @mention 相关测试
    void roundTripsMentionedIds();
    void roundTripsMentionedIdsWithAllMarker();
    void roundTripsEmptyMentionedIds();
    void decodesLegacyMessageWithoutMentionedIds();

    void messageMutation_recall_encodeDecode() {
        MessageEnvelope envelope;
        envelope.messageId      = "mutation-recall-001";
        envelope.type           = MessageType::MessageMutation;
        envelope.senderId       = "client-a";
        envelope.targetId       = "client-b";
        envelope.conversationId = "conv-001";
        envelope.messageSubtype = "recall";
        envelope.payloadJson    =
            buildRecallPayloadJson(QStringLiteral("msg-original-001"), 1712000010000LL);
        envelope.createdAtMs    = 1712000010000LL;

        const auto decoded = MessageCodec::decode(MessageCodec::encode(envelope));

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::MessageMutation);
        QVERIFY(decoded->messageId      == envelope.messageId);
        QVERIFY(decoded->senderId       == envelope.senderId);
        QVERIFY(decoded->targetId       == envelope.targetId);
        QVERIFY(decoded->conversationId == envelope.conversationId);
        QVERIFY(decoded->messageSubtype == std::string("recall"));
        QVERIFY(decoded->payloadJson    == envelope.payloadJson);
        QCOMPARE(decoded->createdAtMs,    envelope.createdAtMs);
    }

    void messageMutation_edit_encodeDecode() {
        MessageEnvelope envelope;
        envelope.messageId      = "mutation-edit-001";
        envelope.type           = MessageType::MessageMutation;
        envelope.senderId       = "client-a";
        envelope.targetId       = "client-b";
        envelope.conversationId = "conv-001";
        envelope.messageSubtype = "edit";
        envelope.payloadJson    =
            buildEditPayloadJson(QStringLiteral("msg-original-002"),
                                 QStringLiteral("updated body text"),
                                 QStringLiteral("plain"),
                                 1712000020000LL);
        envelope.createdAtMs    = 1712000020000LL;

        const auto decoded = MessageCodec::decode(MessageCodec::encode(envelope));

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->type, MessageType::MessageMutation);
        QVERIFY(decoded->messageId      == envelope.messageId);
        QVERIFY(decoded->messageSubtype == std::string("edit"));
        QVERIFY(decoded->payloadJson    == envelope.payloadJson);
        QCOMPARE(decoded->createdAtMs,    envelope.createdAtMs);
        // verify the payload JSON contains the expected fields
        QVERIFY(decoded->payloadJson.find("new_body")          != std::string::npos);
        QVERIFY(decoded->payloadJson.find("updated body text") != std::string::npos);
    }
};

void TestMessageCodec::roundTripsExtendedMessageFields() {
    MessageEnvelope envelope;
    envelope.messageId = "msg-ext-001";
    envelope.type = MessageType::ChatText;
    envelope.senderId = "client-a";
    envelope.targetId = "client-b";
    envelope.conversationId = "conv-ext-001";
    envelope.body = "preview";
    envelope.messageSubtype = "resource_ref";
    envelope.payloadJson = R"({"kind":"shared_file","resource_id":"res-001"})";
    envelope.resourceKind = "shared_file";
    envelope.createdAtMs = 1712000000111;

    const auto decoded = MessageCodec::decode(MessageCodec::encode(envelope));

    QVERIFY(decoded.has_value());
    QVERIFY(decoded->messageSubtype == envelope.messageSubtype);
    QVERIFY(decoded->payloadJson == envelope.payloadJson);
    QVERIFY(decoded->resourceKind == envelope.resourceKind);
}

void TestMessageCodec::decodesLegacyPayloadWithoutExtendedFields() {
    const std::string legacyPayload =
        R"({"message_id":"legacy-001","type":"chat_text","sender_id":"client-a","target_id":"client-b","conversation_id":"conv-legacy","body":"hello","content_type":"plain","attachment_name":"","file_size":0,"chunk_size":0,"chunk_count":0,"chunk_index":0,"data_port":0,"completed_chunks":[],"created_at_ms":1712000000123})";

    const auto decoded = MessageCodec::decode(legacyPayload);

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->type, MessageType::ChatText);
    QVERIFY(decoded->messageSubtype.empty());
    QVERIFY(decoded->payloadJson.empty());
    QVERIFY(decoded->resourceKind.empty());
}

void TestMessageCodec::keepsUnknownSubtypeTextSafe() {
    const std::string payload =
        R"({"message_id":"unknown-sub-001","type":"chat_text","sender_id":"client-a","target_id":"client-b","conversation_id":"conv-unknown","body":"hello","content_type":"plain","message_subtype":"future_magic","payload_json":"{\"x\":1}","attachment_name":"","file_size":0,"chunk_size":0,"chunk_count":0,"chunk_index":0,"data_port":0,"completed_chunks":[],"created_at_ms":1712000000999})";

    const auto decoded = MessageCodec::decode(payload);

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->type, MessageType::ChatText);
    QVERIFY(decoded->messageSubtype == std::string("future_magic"));
    QVERIFY(decoded->body == std::string("hello"));
}

void TestMessageCodec::unknownTypeEmitsDropWarning() {
    const std::string payload =
        R"({"message_id":"future-type-001","type":"future_type","sender_id":"client-a","target_id":"client-b","conversation_id":"conv-future","body":"hello","created_at_ms":1712000001001})";

    QTest::ignoreMessage(
        QtWarningMsg,
        QRegularExpression(QStringLiteral("\\[message-decode\\] dropped unknown type.*type= future_type.*msgId= future-type-001")));

    const auto decoded = MessageCodec::decode(payload);

    QVERIFY(!decoded.has_value());
}

void TestMessageCodec::encodesChatTextWithoutEmptyCompletedChunks() {
    MessageEnvelope envelope;
    envelope.messageId = "chat-no-chunks-001";
    envelope.type = MessageType::ChatText;
    envelope.senderId = "client-a";
    envelope.targetId = "client-b";
    envelope.conversationId = "client-a|client-b";
    envelope.body = "hello";
    envelope.createdAtMs = 1712000001000;

    const std::string encoded = MessageCodec::encode(envelope);

    QVERIFY(encoded.find("\"completed_chunks\"") == std::string::npos);
}

void TestMessageCodec::skipsUnknownJsonValueTypes() {
    const std::string payload =
        R"({"message_id":"future-fields-001","type":"chat_text","sender_id":"client-a","target_id":"client-b","conversation_id":"conv-future","body":"hello","future_number":7,"future_object":{"nested":[1,{"ok":true}]},"future_array":[false,null,"x"],"future_bool":true,"future_null":null,"created_at_ms":1712000001111})";

    const auto decoded = MessageCodec::decode(payload);

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->type, MessageType::ChatText);
    QVERIFY(decoded->messageId == std::string("future-fields-001"));
    QVERIFY(decoded->body == std::string("hello"));
    QCOMPARE(decoded->createdAtMs, 1712000001111LL);
}

void TestMessageCodec::roundTripsMentionedIds() {
    MessageEnvelope envelope;
    envelope.messageId = "mention-001";
    envelope.type = MessageType::GroupMessage;
    envelope.senderId = "client-a";
    envelope.targetId = "client-b";
    envelope.conversationId = "group-001";
    envelope.body = R"({"group_id":"group-001","message_kind":"text","content_type":"html","text":"@Alice hello"})";
    envelope.mentionedIds = {"client-alice", "client-bob"};
    envelope.createdAtMs = 1712000099000;

    const std::string encoded = MessageCodec::encode(envelope);
    const auto decoded = MessageCodec::decode(encoded);

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->mentionedIds.size(), static_cast<std::size_t>(2));
    QVERIFY(decoded->mentionedIds[0] == "client-alice");
    QVERIFY(decoded->mentionedIds[1] == "client-bob");
}

void TestMessageCodec::roundTripsMentionedIdsWithAllMarker() {
    MessageEnvelope envelope;
    envelope.messageId = "mention-all-001";
    envelope.type = MessageType::GroupMessage;
    envelope.senderId = "client-a";
    envelope.targetId = "client-c";
    envelope.conversationId = "group-002";
    envelope.body = R"({"group_id":"group-002","message_kind":"text","content_type":"html","text":"@所有人 notice"})";
    envelope.mentionedIds = {"__all__"};
    envelope.createdAtMs = 1712000099100;

    const std::string encoded = MessageCodec::encode(envelope);
    const auto decoded = MessageCodec::decode(encoded);

    QVERIFY(decoded.has_value());
    QCOMPARE(decoded->mentionedIds.size(), static_cast<std::size_t>(1));
    QVERIFY(decoded->mentionedIds[0] == "__all__");
}

void TestMessageCodec::roundTripsEmptyMentionedIds() {
    MessageEnvelope envelope;
    envelope.messageId = "mention-empty-001";
    envelope.type = MessageType::GroupMessage;
    envelope.senderId = "client-a";
    envelope.targetId = "client-b";
    envelope.conversationId = "group-003";
    envelope.body = R"({"group_id":"group-003","message_kind":"text","content_type":"html","text":"no mentions"})";
    envelope.createdAtMs = 1712000099200;

    const std::string encoded = MessageCodec::encode(envelope);
    const auto decoded = MessageCodec::decode(encoded);

    QVERIFY(decoded.has_value());
    QVERIFY(decoded->mentionedIds.empty());
}

void TestMessageCodec::decodesLegacyMessageWithoutMentionedIds() {
    // 旧版消息不包含 mentioned_ids 字段
    const std::string payload =
        R"({"message_id":"legacy-001","type":"group_message","sender_id":"client-a","target_id":"client-b","conversation_id":"group-old","body":"hello","attachment_name":"","file_size":0,"chunk_size":0,"chunk_count":0,"chunk_index":0,"data_port":0,"completed_chunks":[],"created_at_ms":1712000000100})";

    const auto decoded = MessageCodec::decode(payload);
    QVERIFY(decoded.has_value());
    QVERIFY(decoded->mentionedIds.empty());
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TestMessageCodec tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "TestMessageCodec.moc"
