#pragma once

#include <functional>

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include "integrations/ServerMessageClient.h"

class ConversationRepository;

using IncomingStickerCacheCallback =
    std::function<bool(const QString&, const QString&, const QByteArray&)>;

struct IncomingMessageNotificationEvent {
    QString conversationId;
    QString senderId;
    QString messageId;
    QString messageType;
    QString preview;
    QStringList mentionedIds;
    QJsonObject payload;
};

struct MessageSyncResult {
    bool success = false;
    int storedCount = 0;
    int skippedDuplicateCount = 0;
    qint64 previousCursor = 0;
    qint64 nextCursor = 0;
    QStringList newIncomingConversationIds;
    QVector<IncomingMessageNotificationEvent> newIncomingNotifications;
    QString errorMessage;
};

struct PendingReadAckFlushResult {
    bool success = false;
    int attemptedCount = 0;
    int acknowledgedCount = 0;
    int discardedTerminalCount = 0;
    QString errorMessage;
};

struct PendingDeliveryAckFlushResult {
    bool success = false;
    int attemptedCount = 0;
    int acknowledgedCount = 0;
    int discardedTerminalCount = 0;
    QString errorMessage;
};

class MessageSyncService {
public:
    MessageSyncService(QString localClientId,
                       ConversationRepository* repository,
                       const IServerMessageClient* serverClient,
                       int pageLimit = 100);
    MessageSyncService(QString localClientId,
                       ConversationRepository* repository,
                       const IServerMessageClient* serverClient,
                       int pageLimit,
                       IncomingStickerCacheCallback stickerCacheCallback);

    MessageSyncResult syncConversation(const QString& conversationId) const;
    PendingDeliveryAckFlushResult flushPendingDeliveryAcks(int limit = 100) const;
    PendingReadAckFlushResult flushPendingReadAcks(int limit = 100) const;

private:
    enum class PersistOutcome {
        Stored,
        SkippedDuplicate,
        Failed
    };

    PersistOutcome persistRecord(const ServerMessageRecord& record,
                                 QString* errorMessage) const;
    bool enqueueIncomingDeliveryAck(const ServerMessageRecord& record,
                                    QString* errorMessage) const;

    QString m_localClientId;
    ConversationRepository* m_repository = nullptr;
    const IServerMessageClient* m_serverClient = nullptr;
    int m_pageLimit = 100;
    IncomingStickerCacheCallback m_stickerCacheCallback;
};
