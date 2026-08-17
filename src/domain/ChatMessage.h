#pragma once

#include <string>

#include <QtGlobal>

enum class MessageDeliveryState {
    Pending,
    Sent,
    ServerAcked,
    Received,
    Read,
    Failed
};

struct ChatMessage {
    std::wstring messageId;
    std::wstring conversationId;
    std::wstring senderId;
    std::wstring body;
    qint64 createdAtMs = 0;
    MessageDeliveryState deliveryState = MessageDeliveryState::Pending;
    std::wstring attachmentName{};
    std::wstring localFilePath{};
    std::wstring messageType = L"text";
    std::wstring payloadJson{};
    int groupReadCount = 0;   // populated by loadMessages via LEFT JOIN; 0 for private chat
    bool isRecalled = false;
    qint64 recalledAtMs = 0;
    qint64 editedAtMs = 0;
    qint64 lastMutationAtMs = 0;
    std::wstring lastEditorId{};
    std::wstring replyToMessageId{};
    std::wstring replyToSenderId{};
    std::wstring replyToBody{};
    std::wstring fileCardJson{};
    std::wstring mentionedIds{};  // JSON array of mentioned clientIds, e.g. ["id1","__all__"]
    std::wstring reactionsJson{}; // JSON: {"👍":["clientA","clientB"],...}
};
