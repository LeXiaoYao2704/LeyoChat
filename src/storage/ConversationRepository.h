#pragma once

#include <optional>
#include <string>
#include <vector>

#include <QSet>
#include <QString>
#include <QVector>

#include "domain/ChatMessage.h"
#include "domain/ConversationSummary.h"
#include "domain/PeerEndpoint.h"

enum class ConversationFlag {
    Pinned,
    Starred,
    Muted,
    Done,
    ManuallyUnread,
    HasMentionMe
};

class ConversationRepository {
public:
    explicit ConversationRepository(QString connectionName);
    QString connectionName() const { return m_connectionName; }

    bool appendMessage(const ChatMessage& message, qint64 receivedAtMs = 0) const;
    bool upsertConversation(const ConversationSummary& summary) const;
    bool upsertConversationWithType(const ConversationSummary& summary, const QString& conversationType) const;
    bool updateDeliveryState(const QString& messageId, MessageDeliveryState state) const;
    bool updateDeliveryStatePreservingRead(const QString& messageId, MessageDeliveryState state) const;
    qint64 loadRemoteChatCursor(const QString& conversationId) const;
    bool saveRemoteChatCursor(const QString& conversationId, qint64 lastReceivedSeq) const;
    qint64 loadRemoteChatDeviceCursor(const QString& conversationId,
                                      const QString& deviceId) const;
    bool saveRemoteChatDeviceCursor(const QString& conversationId,
                                    const QString& deviceId,
                                    qint64 lastReceivedSeq) const;
    qint64 loadRemoteMessageEventCursor(const QString& workspaceId,
                                        const QString& deviceId) const;
    bool saveRemoteMessageEventCursor(const QString& workspaceId,
                                      const QString& deviceId,
                                      qint64 lastEventId) const;
    bool saveRemoteMessageIdMapping(const QString& serverMessageId,
                                    const QString& localMessageId) const;
    QString loadLocalMessageIdForRemoteServerId(const QString& serverMessageId) const;
    QString loadRemoteServerIdForLocalMessageId(const QString& localMessageId) const;
    struct PendingRemoteReadAck {
        QString serverMessageId;
        QString conversationId;
        qint64 readSeq = 0;
        qint64 createdAtMs = 0;
        qint64 updatedAtMs = 0;
    };
    bool enqueuePendingRemoteReadAck(const QString& serverMessageId,
                                     const QString& conversationId,
                                     qint64 readSeq) const;
    std::vector<PendingRemoteReadAck> loadPendingRemoteReadAcks(int limit = 100) const;
    bool deletePendingRemoteReadAck(const QString& serverMessageId) const;
    struct PendingRemoteDeliveryAck {
        QString serverMessageId;
        QString conversationId;
        qint64 receivedSeq = 0;
        qint64 createdAtMs = 0;
        qint64 updatedAtMs = 0;
    };
    bool enqueuePendingRemoteDeliveryAck(const QString& serverMessageId,
                                         const QString& conversationId,
                                         qint64 receivedSeq) const;
    std::vector<PendingRemoteDeliveryAck> loadPendingRemoteDeliveryAcks(int limit = 100) const;
    bool deletePendingRemoteDeliveryAck(const QString& serverMessageId) const;
    struct RemoteSessionPresence {
        QString workspaceId;
        QString clientId;
        QString deviceId;
        QString sessionId;
        bool online = false;
        qint64 connectedAtMs = 0;
        qint64 lastSeenAtMs = 0;
        qint64 lastEventId = 0;
    };
    bool saveRemoteSessionPresence(const RemoteSessionPresence& presence) const;
    bool replaceRemoteSessionPresenceForWorkspace(
        const QString& workspaceId,
        const QVector<RemoteSessionPresence>& onlinePresences) const;
    std::optional<RemoteSessionPresence> loadRemoteSessionPresence(
        const QString& workspaceId,
        const QString& clientId,
        const QString& deviceId) const;
    QSet<QString> loadOnlineRemoteSessionClientIds(const QString& workspaceId) const;
    bool updateAttachmentMetadata(const QString& messageId,
                                  const QString& attachmentName,
                                  const QString& localFilePath) const;
    bool updateMessageBody(const QString& messageId, const QString& body) const;
    bool findMessageById(const QString& messageId, ChatMessage* outMessage) const;
    bool findMessageStorageRecordById(const QString& messageId,
                                      QString* outConversationId,
                                      QString* outBody,
                                      qint64* outCreatedAtMs,
                                      QString* outAttachmentName,
                                      QString* outLocalFilePath) const;
    QString loadLatestMessageIdForConversation(const QString& conversationId) const;
    std::vector<ChatMessage> loadPendingOutgoingMessages(const std::wstring& conversationId,
                                                         const std::wstring& senderId) const;
    struct MessagePage {
        std::vector<ChatMessage> messages;
        bool hasMoreBefore = false;
    };
    MessagePage loadRecentMessagesPage(const std::wstring& conversationId, int limit = 80) const;
    MessagePage loadMessagesBeforePage(const std::wstring& conversationId,
                                       const QString& beforeMessageId,
                                       int limit = 80) const;
    std::vector<ChatMessage> loadMessages(const std::wstring& conversationId) const;
    std::vector<ChatMessage> searchMessagesByContent(const QString& keyword, int limit = 50) const;
    std::vector<ChatMessage> loadResourceRefMessages(const std::wstring& conversationId) const;
    std::vector<ConversationSummary> loadConversationSummaries() const;
    bool remapConversationId(const QString& oldConversationId, const QString& newConversationId) const;
    bool deleteConversation(const QString& conversationId) const;

    bool setConversationFlag(const QString& conversationId,
                             ConversationFlag flag, bool value) const;

    // 已知对端持久化（重启后自动重连）
    bool saveKnownPeer(const PeerEndpoint& peer) const;
    std::vector<PeerEndpoint> loadKnownPeers() const;
    bool deleteKnownPeer(const QString& clientId) const;
    bool isKnownActiveGroupConversation(const QString& conversationId) const;
    bool insertReadReceipt(const QString& messageId, const QString& readerId, qint64 readAtMs) const;
    QVector<QPair<QString, qint64>> loadReadReceiptsForMessage(const QString& messageId) const;
    bool enqueuePendingDeliveryReceipt(const QString& messageId,
                                       const QString& senderId,
                                       const QString& targetId,
                                       const QString& conversationId,
                                       qint64 receivedAtMs) const;
    bool applyPendingDeliveryReceiptForMessage(const QString& messageId) const;
    QSet<QString> loadConversationsWithUnreadMessages(const QString& localClientId) const;
    bool consumeConversationUnread(const QString& conversationId,
                                   const QString& localClientId,
                                   bool includeSentState) const;

    bool applyMessageRecall(const QString& messageId, const QString& actorId, qint64 recalledAtMs) const;
    bool applyMessageEdit(const QString& messageId, const QString& actorId, qint64 editedAtMs, const QString& newBody) const;
    bool applyReaction(const QString& messageId, const QString& reactorClientId, const QString& emoji) const;
    bool findMessageMutationStateById(const QString& messageId, ChatMessage* outMessage) const;
    bool refreshConversationPreviewFromLatestVisibleMessage(const QString& conversationId) const;

    // 群文件卡片 JSON 更新
    bool updateMessageFileCardJson(const QString& messageId, const QString& fileCardJson) const;

    // 更新消息类型和 payloadJson（贴纸等特殊消息用）
    bool updateMessageFields(const QString& messageId,
                             const QString& messageType,
                             const QString& payloadJson) const;

    // 消息置顶（每个群最多3条）
    bool pinMessageForConversation(const QString& conversationId,
                                   const QString& messageId,
                                   const QString& pinnerId,
                                   const QString& pinnerName,
                                   const QString& authorName,
                                   const QString& pinnedBody,
                                   qint64 pinnedAtMs) const;
    bool unpinMessageForConversation(const QString& conversationId, const QString& messageId) const;
    int pinnedMessageCount(const QString& conversationId) const;
    struct PinnedMessageInfo {
        QString messageId;
        QString pinnerId;
        QString pinnerName;
        QString authorName;
        QString pinnedBody;
        qint64 pinnedAtMs = 0;
    };
    std::vector<PinnedMessageInfo> loadPinnedMessages(const QString& conversationId) const;

    // 群消息离线补发队列
    bool enqueuePendingGroupEnvelope(const QString& targetId,
                                     const QString& groupId,
                                     const QByteArray& envelopeBlob,
                                     qint64 createdAtMs) const;
    struct PendingGroupEnvelope {
        qint64 id = 0;
        QString targetId;
        QString groupId;
        QByteArray envelopeBlob;
        qint64 createdAtMs = 0;
    };
    std::vector<PendingGroupEnvelope> loadPendingGroupEnvelopes(const QString& targetId, int limit = 200) const;
    std::vector<PendingGroupEnvelope> loadPendingGroupEnvelopesAfterId(
        const QString& targetId,
        qint64 afterId,
        int limit = 200) const;
    std::vector<PendingGroupEnvelope> loadAllPendingGroupEnvelopes() const;
    bool deletePendingGroupEnvelopes(const QVector<qint64>& ids) const;
    bool deletePendingGroupEnvelopeForTargetMessage(const QString& targetId, const QString& messageId) const;
    bool deletePendingGroupEnvelopesForTarget(const QString& targetId) const;

private:
    QString m_connectionName;
};
