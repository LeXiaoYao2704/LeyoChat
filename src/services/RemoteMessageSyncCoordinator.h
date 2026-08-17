#pragma once

#include <vector>

#include <QString>
#include <QStringList>

#include "domain/ConversationSummary.h"
#include "services/MessageSyncService.h"

class ConversationRepository;
class IServerMessageClient;

struct RemoteMessageSyncRunResult {
    bool success = true;
    int attemptedCount = 0;
    int succeededCount = 0;
    int failedCount = 0;
    int storedCount = 0;
    int skippedDuplicateCount = 0;
    int pendingDeliveryAcksAttempted = 0;
    int pendingDeliveryAcksAcknowledged = 0;
    int pendingReadAcksAttempted = 0;
    int pendingReadAcksAcknowledged = 0;
    QStringList failedConversationIds;
    QStringList newIncomingConversationIds;
    QVector<IncomingMessageNotificationEvent> newIncomingNotifications;
    QString errorMessage;
};

qint64 remoteMessageSyncRetryDelayMs(int consecutiveFailures,
                                     qint64 baseDelayMs = 30000,
                                     qint64 maxDelayMs = 300000);

class RemoteMessageSyncCoordinator {
public:
    RemoteMessageSyncCoordinator(QString localClientId,
                                 ConversationRepository* repository,
                                 const IServerMessageClient* serverClient,
                                 int pageLimit = 100,
                                 bool stopOnFirstFailure = false);
    RemoteMessageSyncCoordinator(QString localClientId,
                                 ConversationRepository* repository,
                                 const IServerMessageClient* serverClient,
                                 int pageLimit,
                                 bool stopOnFirstFailure,
                                 IncomingStickerCacheCallback stickerCacheCallback);

    static QStringList directConversationIdsForLocalClient(
        const QString& localClientId,
        const std::vector<ConversationSummary>& summaries);
    static QStringList serviceConversationIdsForLocalClient(
        const QString& localClientId,
        const std::vector<ConversationSummary>& summaries,
        const QStringList& knownGroupConversationIds,
        const QStringList& serverConversationIds = {});

    RemoteMessageSyncRunResult syncDirectConversations(
        const QStringList& conversationIds) const;

private:
    QString m_localClientId;
    ConversationRepository* m_repository = nullptr;
    const IServerMessageClient* m_serverClient = nullptr;
    int m_pageLimit = 100;
    bool m_stopOnFirstFailure = false;
    IncomingStickerCacheCallback m_stickerCacheCallback;
};
