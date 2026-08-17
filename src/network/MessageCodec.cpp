#include "network/MessageCodec.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

#include <QChar>
#include <QDebug>
#include <QString>

namespace {
std::atomic<quint64> g_unknownMessageTypeDrops{0};

QString clippedUtf8(std::string_view value, std::size_t maxBytes) {
    const std::size_t length = std::min(value.size(), maxBytes);
    return QString::fromUtf8(value.data(), static_cast<int>(length));
}

std::string_view messageTypeToString(MessageType type) {
    switch (type) {
    case MessageType::ChatText:
        return "chat_text";
    case MessageType::FileAttachment:
        return "file_attachment";
    case MessageType::ReceiptReceived:
        return "receipt_received";
    case MessageType::ReceiptRead:
        return "receipt_read";
    case MessageType::HandshakeHello:
        return "handshake_hello";
    case MessageType::PeerDirectorySnapshot:
        return "peer_directory_snapshot";
    case MessageType::GroupMeta:
        return "group_meta";
    case MessageType::GroupMessage:
        return "group_message";
    case MessageType::ResourceReference:
        return "resource_reference";
    case MessageType::FileControl:
        return "file_control";
    case MessageType::FileChunk:
        return "file_chunk";
    case MessageType::MessageMutation:
        return "message_mutation";
    case MessageType::PinMessage:
        return "pin_message";
    case MessageType::TypingIndicator:
        return "typing_indicator";
    case MessageType::TlsUpgrade:
        return "tls_upgrade";
    case MessageType::CallControl:
        return "call_control";
    case MessageType::CallRecord:
        return "call_record";
    case MessageType::MessageReaction:
        return "message_reaction";
    }

    return "chat_text";
}

std::optional<MessageType> messageTypeFromString(std::string_view type) {
    if (type == "chat_text") {
        return MessageType::ChatText;
    }
    if (type == "file_attachment") {
        return MessageType::FileAttachment;
    }
    if (type == "receipt_received") {
        return MessageType::ReceiptReceived;
    }
    if (type == "receipt_read") {
        return MessageType::ReceiptRead;
    }
    if (type == "handshake_hello") {
        return MessageType::HandshakeHello;
    }
    if (type == "peer_directory_snapshot") {
        return MessageType::PeerDirectorySnapshot;
    }
    if (type == "group_meta") {
        return MessageType::GroupMeta;
    }
    if (type == "group_message") {
        return MessageType::GroupMessage;
    }
    if (type == "resource_reference") {
        return MessageType::ResourceReference;
    }
    if (type == "file_control") {
        return MessageType::FileControl;
    }
    if (type == "file_chunk") {
        return MessageType::FileChunk;
    }
    if (type == "message_mutation") {
        return MessageType::MessageMutation;
    }
    if (type == "pin_message") {
        return MessageType::PinMessage;
    }
    if (type == "typing_indicator") {
        return MessageType::TypingIndicator;
    }
    if (type == "tls_upgrade") {
        return MessageType::TlsUpgrade;
    }
    if (type == "call_control") {
        return MessageType::CallControl;
    }
    if (type == "call_record") {
        return MessageType::CallRecord;
    }
    if (type == "message_reaction") {
        return MessageType::MessageReaction;
    }

    return std::nullopt;
}

void appendEscapedJsonString(std::string& output, std::string_view value) {
    output.push_back('"');
    for (const char byte : value) {
        switch (byte) {
        case '"':
            output.append("\\\"");
            break;
        case '\\':
            output.append("\\\\");
            break;
        case '\b':
            output.append("\\b");
            break;
        case '\f':
            output.append("\\f");
            break;
        case '\n':
            output.append("\\n");
            break;
        case '\r':
            output.append("\\r");
            break;
        case '\t':
            output.append("\\t");
            break;
        default:
            if (static_cast<unsigned char>(byte) < 0x20U) {
                char escaped[7] = {};
                std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned char>(byte));
                output.append(escaped);
            } else {
                output.push_back(byte);
            }
            break;
        }
    }
    output.push_back('"');
}

void appendJsonKeyValue(std::string& output, std::string_view key, std::string_view value, bool& firstField) {
    if (!firstField) {
        output.push_back(',');
    }
    firstField = false;
    appendEscapedJsonString(output, key);
    output.push_back(':');
    appendEscapedJsonString(output, value);
}

void appendJsonKeyValue(std::string& output, std::string_view key, qint64 value, bool& firstField) {
    if (!firstField) {
        output.push_back(',');
    }
    firstField = false;
    appendEscapedJsonString(output, key);
    output.push_back(':');
    const std::string encodedValue = std::to_string(value);
    output.append(encodedValue);
}

void appendJsonKeyValue(std::string& output, std::string_view key, int value, bool& firstField) {
    appendJsonKeyValue(output, key, static_cast<qint64>(value), firstField);
}

void appendJsonKeyValue(std::string& output, std::string_view key, quint16 value, bool& firstField) {
    appendJsonKeyValue(output, key, static_cast<qint64>(value), firstField);
}

void appendJsonIntArray(std::string& output, std::string_view key, const std::vector<int>& values, bool& firstField) {
    if (!firstField) {
        output.push_back(',');
    }
    firstField = false;
    appendEscapedJsonString(output, key);
    output.push_back(':');
    output.push_back('[');
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output.push_back(',');
        }
        output.append(std::to_string(values[index]));
    }
    output.push_back(']');
}

void appendJsonStringArray(std::string& output, std::string_view key, const std::vector<std::string>& values, bool& firstField) {
    if (!firstField) {
        output.push_back(',');
    }
    firstField = false;
    appendEscapedJsonString(output, key);
    output.push_back(':');
    output.push_back('[');
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            output.push_back(',');
        }
        appendEscapedJsonString(output, values[index]);
    }
    output.push_back(']');
}

void skipWhitespace(std::string_view payload, size_t& position) {
    while (position < payload.size()) {
        const char ch = payload[position];
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            break;
        }
        ++position;
    }
}

bool consumeChar(std::string_view payload, size_t& position, char expected) {
    skipWhitespace(payload, position);
    if (position >= payload.size() || payload[position] != expected) {
        return false;
    }
    ++position;
    return true;
}

bool parseHexDigit(char ch, quint16& value) {
    if (ch >= '0' && ch <= '9') {
        value = static_cast<quint16>(ch - '0');
        return true;
    }
    if (ch >= 'a' && ch <= 'f') {
        value = static_cast<quint16>(10 + ch - 'a');
        return true;
    }
    if (ch >= 'A' && ch <= 'F') {
        value = static_cast<quint16>(10 + ch - 'A');
        return true;
    }
    return false;
}

void appendCodePointUtf8(std::string& output, char32_t codePoint) {
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

bool appendUnicodeEscape(std::string_view payload, size_t& position, std::string& utf8Bytes) {
    if (position + 4 > payload.size()) {
        return false;
    }

    quint16 codeUnit = 0;
    for (int offset = 0; offset < 4; ++offset) {
        quint16 digit = 0;
        if (!parseHexDigit(payload[position + offset], digit)) {
            return false;
        }
        codeUnit = static_cast<quint16>((codeUnit << 4) | digit);
    }
    position += 4;

    if (QChar::isHighSurrogate(codeUnit)) {
        if (position + 6 > payload.size() || payload[position] != '\\' || payload[position + 1] != 'u') {
            return false;
        }
        position += 2;

        quint16 lowSurrogate = 0;
        for (int offset = 0; offset < 4; ++offset) {
            quint16 digit = 0;
            if (!parseHexDigit(payload[position + offset], digit)) {
                return false;
            }
            lowSurrogate = static_cast<quint16>((lowSurrogate << 4) | digit);
        }
        position += 4;
        if (!QChar::isLowSurrogate(lowSurrogate)) {
            return false;
        }

        const char32_t codePoint = QChar::surrogateToUcs4(codeUnit, lowSurrogate);
        appendCodePointUtf8(utf8Bytes, codePoint);
        return true;
    }

    appendCodePointUtf8(utf8Bytes, codeUnit);
    return true;
}

bool parseJsonString(std::string_view payload, size_t& position, std::string& value) {
    skipWhitespace(payload, position);
    if (position >= payload.size() || payload[position] != '"') {
        return false;
    }
    ++position;

    std::string utf8Bytes;
    while (position < payload.size()) {
        const char ch = payload[position++];
        if (ch == '"') {
            value = utf8Bytes;
            return true;
        }

        if (ch != '\\') {
            utf8Bytes.push_back(ch);
            continue;
        }

        if (position >= payload.size()) {
            return false;
        }

        const char escaped = payload[position++];
        switch (escaped) {
        case '"':
        case '\\':
        case '/':
            utf8Bytes.push_back(escaped);
            break;
        case 'b':
            utf8Bytes.push_back('\b');
            break;
        case 'f':
            utf8Bytes.push_back('\f');
            break;
        case 'n':
            utf8Bytes.push_back('\n');
            break;
        case 'r':
            utf8Bytes.push_back('\r');
            break;
        case 't':
            utf8Bytes.push_back('\t');
            break;
        case 'u':
            if (!appendUnicodeEscape(payload, position, utf8Bytes)) {
                return false;
            }
            break;
        default:
            return false;
        }
    }

    return false;
}

bool skipJsonValue(std::string_view payload, size_t& position);

bool skipJsonLiteral(std::string_view payload, size_t& position, std::string_view literal) {
    skipWhitespace(payload, position);
    if (position + literal.size() > payload.size()
        || payload.substr(position, literal.size()) != literal) {
        return false;
    }
    position += literal.size();
    return true;
}

bool skipJsonNumber(std::string_view payload, size_t& position) {
    skipWhitespace(payload, position);
    const size_t start = position;
    if (position < payload.size() && payload[position] == '-') {
        ++position;
    }
    if (position >= payload.size() || payload[position] < '0' || payload[position] > '9') {
        return false;
    }
    if (payload[position] == '0') {
        ++position;
    } else {
        while (position < payload.size() && payload[position] >= '0' && payload[position] <= '9') {
            ++position;
        }
    }
    if (position < payload.size() && payload[position] == '.') {
        ++position;
        const size_t fractionStart = position;
        while (position < payload.size() && payload[position] >= '0' && payload[position] <= '9') {
            ++position;
        }
        if (position == fractionStart) {
            return false;
        }
    }
    if (position < payload.size() && (payload[position] == 'e' || payload[position] == 'E')) {
        ++position;
        if (position < payload.size() && (payload[position] == '+' || payload[position] == '-')) {
            ++position;
        }
        const size_t exponentStart = position;
        while (position < payload.size() && payload[position] >= '0' && payload[position] <= '9') {
            ++position;
        }
        if (position == exponentStart) {
            return false;
        }
    }
    return position > start;
}

bool skipJsonArray(std::string_view payload, size_t& position) {
    if (!consumeChar(payload, position, '[')) {
        return false;
    }
    skipWhitespace(payload, position);
    if (position < payload.size() && payload[position] == ']') {
        ++position;
        return true;
    }
    while (true) {
        if (!skipJsonValue(payload, position)) {
            return false;
        }
        skipWhitespace(payload, position);
        if (position >= payload.size()) {
            return false;
        }
        if (payload[position] == ']') {
            ++position;
            return true;
        }
        if (payload[position] != ',') {
            return false;
        }
        ++position;
    }
}

bool skipJsonObject(std::string_view payload, size_t& position) {
    if (!consumeChar(payload, position, '{')) {
        return false;
    }
    skipWhitespace(payload, position);
    if (position < payload.size() && payload[position] == '}') {
        ++position;
        return true;
    }
    while (true) {
        std::string key;
        if (!parseJsonString(payload, position, key) || !consumeChar(payload, position, ':')) {
            return false;
        }
        if (!skipJsonValue(payload, position)) {
            return false;
        }
        skipWhitespace(payload, position);
        if (position >= payload.size()) {
            return false;
        }
        if (payload[position] == '}') {
            ++position;
            return true;
        }
        if (payload[position] != ',') {
            return false;
        }
        ++position;
    }
}

bool skipJsonValue(std::string_view payload, size_t& position) {
    skipWhitespace(payload, position);
    if (position >= payload.size()) {
        return false;
    }
    if (payload[position] == '"') {
        std::string ignored;
        return parseJsonString(payload, position, ignored);
    }
    if (payload[position] == '{') {
        return skipJsonObject(payload, position);
    }
    if (payload[position] == '[') {
        return skipJsonArray(payload, position);
    }
    if (payload[position] == 't') {
        return skipJsonLiteral(payload, position, "true");
    }
    if (payload[position] == 'f') {
        return skipJsonLiteral(payload, position, "false");
    }
    if (payload[position] == 'n') {
        return skipJsonLiteral(payload, position, "null");
    }
    return skipJsonNumber(payload, position);
}

bool parseJsonInteger(std::string_view payload, size_t& position, qint64& value) {
    skipWhitespace(payload, position);
    if (position >= payload.size()) {
        return false;
    }

    const size_t start = position;
    if (payload[position] == '-') {
        ++position;
    }
    if (position >= payload.size() || payload[position] < '0' || payload[position] > '9') {
        return false;
    }
    while (position < payload.size() && payload[position] >= '0' && payload[position] <= '9') {
        ++position;
    }

    qint64 parsed = 0;
    const char* begin = payload.data() + start;
    const char* end = payload.data() + position;
    const auto result = std::from_chars(begin, end, parsed);
    if (result.ec != std::errc{} || result.ptr != end) {
        return false;
    }
    value = parsed;
    return true;
}

bool parseJsonIntArray(std::string_view payload, size_t& position, std::vector<int>& values) {
    if (!consumeChar(payload, position, '[')) {
        return false;
    }

    values.clear();
    skipWhitespace(payload, position);
    if (position < payload.size() && payload[position] == ']') {
        ++position;
        return true;
    }

    while (true) {
        qint64 parsed = 0;
        if (!parseJsonInteger(payload, position, parsed)) {
            return false;
        }
        values.push_back(static_cast<int>(parsed));

        skipWhitespace(payload, position);
        if (position >= payload.size()) {
            return false;
        }
        if (payload[position] == ']') {
            ++position;
            return true;
        }
        if (payload[position] != ',') {
            return false;
        }
        ++position;
    }
}

bool parseJsonStringArray(std::string_view payload, size_t& position, std::vector<std::string>& values) {
    if (!consumeChar(payload, position, '[')) {
        return false;
    }

    values.clear();
    skipWhitespace(payload, position);
    if (position < payload.size() && payload[position] == ']') {
        ++position;
        return true;
    }

    while (true) {
        std::string parsed;
        if (!parseJsonString(payload, position, parsed)) {
            return false;
        }
        values.push_back(std::move(parsed));

        skipWhitespace(payload, position);
        if (position >= payload.size()) {
            return false;
        }
        if (payload[position] == ']') {
            ++position;
            return true;
        }
        if (payload[position] != ',') {
            return false;
        }
        ++position;
    }
}
}

std::string MessageCodec::encode(const MessageEnvelope& envelope) {
    std::string payload;
    payload.push_back('{');
    bool firstField = true;
    appendJsonKeyValue(payload, "message_id", envelope.messageId, firstField);
    appendJsonKeyValue(payload, "type", messageTypeToString(envelope.type), firstField);
    appendJsonKeyValue(payload, "sender_id", envelope.senderId, firstField);
    appendJsonKeyValue(payload, "target_id", envelope.targetId, firstField);
    appendJsonKeyValue(payload, "conversation_id", envelope.conversationId, firstField);
    appendJsonKeyValue(payload, "body", envelope.body, firstField);
    appendJsonKeyValue(payload, "content_type", envelope.contentType, firstField);
    appendJsonKeyValue(payload, "message_subtype", envelope.messageSubtype, firstField);
    appendJsonKeyValue(payload, "payload_json", envelope.payloadJson, firstField);
    appendJsonKeyValue(payload, "attachment_name", envelope.attachmentName, firstField);
    appendJsonKeyValue(payload, "resource_id", envelope.resourceId, firstField);
    appendJsonKeyValue(payload, "resource_kind", envelope.resourceKind, firstField);
    appendJsonKeyValue(payload, "resource_title", envelope.resourceTitle, firstField);
    appendJsonKeyValue(payload, "workspace_id", envelope.workspaceId, firstField);
    appendJsonKeyValue(payload, "service_id", envelope.serviceId, firstField);
    appendJsonKeyValue(payload, "control_type", envelope.controlType, firstField);
    appendJsonKeyValue(payload, "file_task_id", envelope.fileTaskId, firstField);
    appendJsonKeyValue(payload, "file_hash", envelope.fileHash, firstField);
    appendJsonKeyValue(payload, "data_host", envelope.dataHost, firstField);
    appendJsonKeyValue(payload, "reason", envelope.reason, firstField);
    appendJsonKeyValue(payload, "file_size", envelope.fileSize, firstField);
    appendJsonKeyValue(payload, "chunk_size", envelope.chunkSize, firstField);
    appendJsonKeyValue(payload, "chunk_count", envelope.chunkCount, firstField);
    appendJsonKeyValue(payload, "chunk_index", envelope.chunkIndex, firstField);
    appendJsonKeyValue(payload, "data_port", envelope.dataPort, firstField);
    if (!envelope.completedChunks.empty()) {
        appendJsonIntArray(payload, "completed_chunks", envelope.completedChunks, firstField);
    }
    appendJsonKeyValue(payload, "created_at_ms", envelope.createdAtMs, firstField);
    appendJsonKeyValue(payload, "reply_to_message_id", envelope.replyToMessageId, firstField);
    appendJsonKeyValue(payload, "reply_to_sender_id", envelope.replyToSenderId, firstField);
    appendJsonKeyValue(payload, "reply_to_body", envelope.replyToBody, firstField);
    if (!envelope.mentionedIds.empty()) {
        appendJsonStringArray(payload, "mentioned_ids", envelope.mentionedIds, firstField);
    }
    payload.push_back('}');
    return payload;
}

std::optional<MessageEnvelope> MessageCodec::decode(std::string_view payload) {
    size_t position = 0;
    if (!consumeChar(payload, position, '{')) {
        return std::nullopt;
    }

    std::string messageId;
    std::string typeName;
    std::string senderId;
    std::string targetId;
    std::string conversationId;
    std::string body;
    std::string contentType;
    std::string messageSubtype;
    std::string payloadJson;
    std::string attachmentName;
    std::string resourceId;
    std::string resourceKind;
    std::string resourceTitle;
    std::string workspaceId;
    std::string serviceId;
    std::string controlType;
    std::string fileTaskId;
    std::string fileHash;
    std::string dataHost;
    std::string reason;
    std::string replyToMessageId;
    std::string replyToSenderId;
    std::string replyToBody;
    qint64 fileSize = 0;
    qint64 chunkSize = 0;
    qint64 chunkCount = 0;
    qint64 chunkIndex = 0;
    qint64 dataPort = 0;
    std::vector<int> completedChunks;
    std::vector<std::string> mentionedIds;
    qint64 createdAtMs = 0;
    bool firstField = true;

    while (true) {
        skipWhitespace(payload, position);
        if (position >= payload.size()) {
            return std::nullopt;
        }
        if (payload[position] == '}') {
            ++position;
            break;
        }
        if (!firstField && !consumeChar(payload, position, ',')) {
            return std::nullopt;
        }
        firstField = false;

        std::string key;
        if (!parseJsonString(payload, position, key) || !consumeChar(payload, position, ':')) {
            return std::nullopt;
        }

        if (key == "created_at_ms" || key == "file_size" || key == "chunk_size" || key == "chunk_count"
            || key == "chunk_index" || key == "data_port") {
            qint64 parsed = 0;
            if (!parseJsonInteger(payload, position, parsed)) {
                return std::nullopt;
            }
            if (key == "created_at_ms") {
                createdAtMs = parsed;
            } else if (key == "file_size") {
                fileSize = parsed;
            } else if (key == "chunk_size") {
                chunkSize = parsed;
            } else if (key == "chunk_count") {
                chunkCount = parsed;
            } else if (key == "chunk_index") {
                chunkIndex = parsed;
            } else if (key == "data_port") {
                dataPort = parsed;
            }
            continue;
        }

        if (key == "completed_chunks") {
            if (!parseJsonIntArray(payload, position, completedChunks)) {
                return std::nullopt;
            }
            continue;
        }

        if (key == "mentioned_ids") {
            if (!parseJsonStringArray(payload, position, mentionedIds)) {
                return std::nullopt;
            }
            continue;
        }

        const bool expectsStringValue =
            key == "message_id" || key == "type" || key == "sender_id"
            || key == "target_id" || key == "conversation_id" || key == "body"
            || key == "content_type" || key == "message_subtype" || key == "payload_json"
            || key == "attachment_name" || key == "resource_id" || key == "resource_kind"
            || key == "resource_title" || key == "workspace_id" || key == "service_id"
            || key == "control_type" || key == "file_task_id" || key == "file_hash"
            || key == "data_host" || key == "reason" || key == "reply_to_message_id"
            || key == "reply_to_sender_id" || key == "reply_to_body";
        if (!expectsStringValue) {
            if (!skipJsonValue(payload, position)) {
                return std::nullopt;
            }
            continue;
        }

        std::string stringValue;
        if (!parseJsonString(payload, position, stringValue)) {
            return std::nullopt;
        }

        if (key == "message_id") {
            messageId = stringValue;
        } else if (key == "type") {
            typeName = stringValue;
        } else if (key == "sender_id") {
            senderId = stringValue;
        } else if (key == "target_id") {
            targetId = stringValue;
        } else if (key == "conversation_id") {
            conversationId = stringValue;
        } else if (key == "body") {
            body = stringValue;
        } else if (key == "content_type") {
            contentType = stringValue;
        } else if (key == "message_subtype") {
            messageSubtype = stringValue;
        } else if (key == "payload_json") {
            payloadJson = stringValue;
        } else if (key == "attachment_name") {
            attachmentName = stringValue;
        } else if (key == "resource_id") {
            resourceId = stringValue;
        } else if (key == "resource_kind") {
            resourceKind = stringValue;
        } else if (key == "resource_title") {
            resourceTitle = stringValue;
        } else if (key == "workspace_id") {
            workspaceId = stringValue;
        } else if (key == "service_id") {
            serviceId = stringValue;
        } else if (key == "control_type") {
            controlType = stringValue;
        } else if (key == "file_task_id") {
            fileTaskId = stringValue;
        } else if (key == "file_hash") {
            fileHash = stringValue;
        } else if (key == "data_host") {
            dataHost = stringValue;
        } else if (key == "reason") {
            reason = stringValue;
        } else if (key == "reply_to_message_id") {
            replyToMessageId = stringValue;
        } else if (key == "reply_to_sender_id") {
            replyToSenderId = stringValue;
        } else if (key == "reply_to_body") {
            replyToBody = stringValue;
        }
    }

    skipWhitespace(payload, position);
    if (position != payload.size()) {
        return std::nullopt;
    }

    const auto type = messageTypeFromString(typeName);
    if (!type.has_value()) {
        const quint64 dropCount = g_unknownMessageTypeDrops.fetch_add(1, std::memory_order_relaxed) + 1;
        qWarning().noquote() << "[message-decode] dropped unknown type"
                             << "count=" << dropCount
                             << "type=" << clippedUtf8(typeName, 80)
                             << "msgId=" << clippedUtf8(messageId, 64);
        return std::nullopt;
    }

    MessageEnvelope envelope;
    envelope.messageId = messageId;
    envelope.type = *type;
    envelope.senderId = senderId;
    envelope.targetId = targetId;
    envelope.conversationId = conversationId;
    envelope.body = body;
    envelope.contentType = contentType;
    envelope.messageSubtype = messageSubtype;
    envelope.payloadJson = payloadJson;
    envelope.attachmentName = attachmentName;
    envelope.resourceId = resourceId;
    envelope.resourceKind = resourceKind;
    envelope.resourceTitle = resourceTitle;
    envelope.workspaceId = workspaceId;
    envelope.serviceId = serviceId;
    envelope.controlType = controlType;
    envelope.fileTaskId = fileTaskId;
    envelope.fileHash = fileHash;
    envelope.dataHost = dataHost;
    envelope.reason = reason;
    envelope.fileSize = fileSize;
    envelope.chunkSize = chunkSize;
    envelope.chunkCount = static_cast<int>(chunkCount);
    envelope.chunkIndex = static_cast<int>(chunkIndex);
    envelope.dataPort = static_cast<quint16>(dataPort);
    envelope.completedChunks = completedChunks;
    envelope.createdAtMs = createdAtMs;
    envelope.replyToMessageId = replyToMessageId;
    envelope.replyToSenderId = replyToSenderId;
    envelope.replyToBody = replyToBody;
    envelope.mentionedIds = std::move(mentionedIds);
    return envelope;
}
