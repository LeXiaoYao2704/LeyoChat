#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

struct StoredMessage {
    QString serverMessageId;
    QString clientMessageId;
    QString conversationId;
    QString workspaceId;
    QString senderId;
    qint64 serverSeq = 0;
    QString type;
    QString body;
    QString payloadJson;
    QString fileId;
    QString contentType;
    QString replyToMessageId;
    qint64 createdAtMs = 0;
    qint64 updatedAtMs = 0;
};

struct StoredMessageEvent {
    qint64 eventId = 0;
    QString workspaceId;
    QString conversationId;
    QString eventType;
    QString payloadJson;
    qint64 createdAtMs = 0;
};

struct StoreMessageRequest {
    QString clientMessageId;
    QString conversationId;
    QString workspaceId;
    QString senderId;
    QString type;
    QString body;
    QString payloadJson;
    QString fileId;
    QString contentType;
    QString replyToMessageId;
    QStringList recipientIds;
    qint64 createdAtMs = 0;
};

struct StoreMessageResult {
    bool ok = false;
    bool duplicate = false;
    StoredMessage message;
    QString error;
};

struct StoredConversation {
    QString conversationId;
    QString workspaceId;
    qint64 latestServerSeq = 0;
    qint64 updatedAtMs = 0;
};

struct MessageDeliveryRecord {
    QString serverMessageId;
    QString recipientId;
    QString state;
    qint64 deliveredAtMs = 0;
    qint64 readAtMs = 0;
    qint64 retryCount = 0;
};

struct MessageWorkspaceRecord {
    QString workspaceId;
    QString displayName;
    QString createdById;
    bool enabled = true;
    qint64 createdAtMs = 0;
    qint64 updatedAtMs = 0;
};

struct MessageAuditRecord {
    qint64 auditId = 0;
    QString workspaceId;
    QString actorClientId;
    QString action;
    QString outcome;
    QString metadataJson;
    qint64 createdAtMs = 0;
};

struct MessageClientCapabilityProfile {
    QString workspaceId;
    QString clientId;
    QString appVersion;
    QStringList capabilities;
    qint64 updatedAtMs = 0;

    bool supports(const QString& capability) const;
};

class MessageServiceDatabase {
public:
    explicit MessageServiceDatabase(const QString& databasePath,
                                    const QString& connectionName =
                                        QStringLiteral("leyo-message-service"));
    ~MessageServiceDatabase();

    bool open();

    StoreMessageResult storeMessage(const StoreMessageRequest& request) const;
    std::optional<StoredMessage> findMessageByServerId(
        const QString& serverMessageId) const;
    std::optional<StoredMessage> findMessageByClientId(
        const QString& senderId,
        const QString& clientMessageId) const;
    StoredMessageEvent appendMessageCreatedEvent(
        const StoredMessage& message) const;
    StoredMessageEvent appendSessionStatusEvent(
        const QString& workspaceId,
        const QString& eventType,
        const QString& sessionId,
        const QString& clientId,
        const QString& deviceId,
        qint64 connectedAtMs,
        qint64 lastSeenAtMs,
        qint64 lastEventId) const;
    QVector<StoredMessageEvent> listMessageEventsAfter(
        const QString& workspaceId,
        qint64 afterEventId,
        int limit) const;
    QVector<StoredMessageEvent> listMessageEventsAfterForClient(
        const QString& workspaceId,
        const QString& clientId,
        qint64 afterEventId,
        int limit) const;
    QVector<StoredMessage> listMessagesAfterSeq(const QString& conversationId,
                                                qint64 afterSeq,
                                                int limit) const;
    QVector<StoredConversation> listConversationsForMember(
        const QString& workspaceId,
        const QString& clientId,
        int limit) const;
    QVector<MessageDeliveryRecord> listDeliveries(
        const QString& serverMessageId) const;
    bool upsertWorkspace(const MessageWorkspaceRecord& workspace) const;
    QVector<MessageWorkspaceRecord> listWorkspaces() const;
    MessageAuditRecord appendAuditEvent(const QString& workspaceId,
                                        const QString& actorClientId,
                                        const QString& action,
                                        const QString& outcome,
                                        QJsonObject metadata = {}) const;
    QVector<MessageAuditRecord> listAuditEvents(const QString& workspaceId,
                                                int limit) const;
    bool isConversationMember(const QString& conversationId,
                              const QString& clientId) const;
    bool markDelivered(const QString& serverMessageId,
                       const QString& recipientId,
                       qint64 receivedSeq) const;
    bool markRead(const QString& serverMessageId,
                  const QString& recipientId,
                  qint64 readSeq) const;
    bool upsertClientCapabilities(const QString& workspaceId,
                                  const QString& clientId,
                                  const QString& appVersion,
                                  const QStringList& capabilities,
                                  qint64 updatedAtMs) const;
    QVector<MessageClientCapabilityProfile> loadClientCapabilities(
        const QString& workspaceId,
        const QStringList& clientIds) const;

private:
    bool runMigrations() const;
    qint64 nextConversationSeq(const QString& conversationId) const;
    bool upsertConversationMember(const QString& conversationId,
                                  const QString& clientId,
                                  const QString& role,
                                  qint64 nowMs) const;
    bool upsertCursor(const QString& conversationId,
                      const QString& clientId,
                      qint64 receivedSeq,
                      qint64 readSeq,
                      qint64 nowMs) const;
    std::optional<MessageDeliveryRecord> findDeliveryRecord(
        const QString& serverMessageId,
        const QString& recipientId) const;
    StoredMessageEvent appendMessageEventWithPayload(
        const QString& workspaceId,
        const QString& conversationId,
        const QString& eventType,
        QJsonObject payload) const;
    StoredMessageEvent appendMessageStateEvent(
        const StoredMessage& message,
        const QString& eventType,
        const QString& recipientId,
        const QString& cursorField,
        qint64 cursorSeq) const;

    QString m_databasePath;
    QString m_connectionName;
};
