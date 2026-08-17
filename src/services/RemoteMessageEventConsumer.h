#pragma once

#include <QString>
#include <QStringList>

#include "services/MessageSyncService.h"

class ConversationRepository;
class IServerMessageClient;

struct RemoteMessageEventConsumerResult {
    bool success = false;
    int eventsSeen = 0;
    int conversationsTriggered = 0;
    int conversationsSynced = 0;
    int conversationsFailed = 0;
    int sessionsSynced = 0;
    int pendingDeliveryAcksAttempted = 0;
    int pendingDeliveryAcksAcknowledged = 0;
    int pendingReadAcksAttempted = 0;
    int pendingReadAcksAcknowledged = 0;
    qint64 previousEventId = 0;
    qint64 nextEventId = 0;
    QStringList triggeredConversationIds;
    QStringList newIncomingConversationIds;
    QVector<IncomingMessageNotificationEvent> newIncomingNotifications;
    QString errorMessage;
};

class RemoteMessageEventConsumer {
public:
    RemoteMessageEventConsumer(QString localClientId,
                               QString workspaceId,
                               QString deviceId,
                               ConversationRepository* repository,
                               const IServerMessageClient* serverClient,
                               int eventLimit = 100,
                               int messagePageLimit = 100,
                               QString appVersion = {},
                               QStringList capabilities = {});
    RemoteMessageEventConsumer(QString localClientId,
                               QString workspaceId,
                               QString deviceId,
                               ConversationRepository* repository,
                               const IServerMessageClient* serverClient,
                               int eventLimit,
                               int messagePageLimit,
                               QString appVersion,
                               QStringList capabilities,
                               IncomingStickerCacheCallback stickerCacheCallback);

    RemoteMessageEventConsumerResult consumeOnce() const;

private:
    QString m_localClientId;
    QString m_workspaceId;
    QString m_deviceId;
    ConversationRepository* m_repository = nullptr;
    const IServerMessageClient* m_serverClient = nullptr;
    int m_eventLimit = 100;
    int m_messagePageLimit = 100;
    QString m_appVersion;
    QStringList m_capabilities;
    IncomingStickerCacheCallback m_stickerCacheCallback;
};
