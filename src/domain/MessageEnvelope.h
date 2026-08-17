#pragma once

#include <string>
#include <vector>

#include <QtGlobal>

enum class MessageType {
    ChatText,
    FileAttachment,
    ReceiptReceived,
    ReceiptRead,
    HandshakeHello,
    PeerDirectorySnapshot,
    GroupMeta,
    GroupMessage,
    ResourceReference,
    FileControl,
    FileChunk,  // 通过控制连接传输文件数据块，无需额外 TCP 端口。
    MessageMutation,   // 消息变更（撤回/编辑）
    PinMessage,        // 群消息置顶/取消置顶
    TypingIndicator,   // 正在输入指示
    TlsUpgrade,        // TLS 升级协商
    CallControl,       // 语音通话/桌面共享/远程控制信令
    CallRecord,        // 通话记录（本地插入聊天记录）
    MessageReaction    // 消息表情回应
};

struct MessageEnvelope {
    std::string messageId;
    MessageType type = MessageType::ChatText;
    std::string senderId;
    std::string targetId;
    std::string conversationId;
    std::string body;
    std::string contentType;   // "plain" | "html" (empty == "plain")
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
    qint64 fileSize = 0;
    qint64 chunkSize = 0;
    int chunkCount = 0;
    int chunkIndex = 0;  // FileChunk 消息中：当前数据块的序号。
    quint16 dataPort = 0;
    std::vector<int> completedChunks;
    qint64 createdAtMs = 0;
    std::string replyToMessageId;
    std::string replyToSenderId;
    std::string replyToBody;
    std::vector<std::string> mentionedIds;  // @提及的用户 clientId 列表；"__all__" 表示 @所有人
};
