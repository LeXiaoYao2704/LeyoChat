#include "services/RemoteMessageSyncCoordinator.h"

#include "services/DirectConversationAddressing.h"
#include "services/MessageSyncService.h"

#include <QDebug>
#include <QSet>
#include <QtGlobal>

#include <utility>

namespace {

void appendUniqueConversationId(QStringList* ids, QSet<QString>* seen, const QString& value)
{
    if (!ids || !seen) {
        return;
    }
    const QString conversationId = value.trimmed();
    if (conversationId.isEmpty() || seen->contains(conversationId)) {
        return;
    }
    seen->insert(conversationId);
    ids->push_back(conversationId);
}

}  // namespace

qint64 remoteMessageSyncRetryDelayMs(int consecutiveFailures,
                                     qint64 baseDelayMs,
                                     qint64 maxDelayMs)
{
    if (consecutiveFailures <= 0 || baseDelayMs <= 0 || maxDelayMs <= 0) {
        return 0;
    }

    qint64 delay = qMin(baseDelayMs, maxDelayMs);
    for (int i = 1; i < consecutiveFailures && delay < maxDelayMs; ++i) {
        if (delay > maxDelayMs / 2) {
            delay = maxDelayMs;
        } else {
            delay = qMin(maxDelayMs, delay * 2);
        }
    }
    return delay;
}

RemoteMessageSyncCoordinator::RemoteMessageSyncCoordinator(
    QString localClientId,
    ConversationRepository* repository,
    const IServerMessageClient* serverClient,
    int pageLimit,
    bool stopOnFirstFailure)
    : RemoteMessageSyncCoordinator(std::move(localClientId),
                                   repository,
                                   serverClient,
                                   pageLimit,
                                   stopOnFirstFailure,
                                   {})
{
}

RemoteMessageSyncCoordinator::RemoteMessageSyncCoordinator(
    QString localClientId,
    ConversationRepository* repository,
    const IServerMessageClient* serverClient,
    int pageLimit,
    bool stopOnFirstFailure,
    IncomingStickerCacheCallback stickerCacheCallback)
    : m_localClientId(std::move(localClientId))
    , m_repository(repository)
    , m_serverClient(serverClient)
    , m_pageLimit(qMax(1, pageLimit))
    , m_stopOnFirstFailure(stopOnFirstFailure)
    , m_stickerCacheCallback(std::move(stickerCacheCallback))
{
}

QStringList RemoteMessageSyncCoordinator::directConversationIdsForLocalClient(
    const QString& localClientId,
    const std::vector<ConversationSummary>& summaries)
{
    const QString localId = localClientId.trimmed();
    if (localId.isEmpty()) {
        return {};
    }

    QStringList conversationIds;
    QSet<QString> seen;
    for (const ConversationSummary& summary : summaries) {
        const QString conversationId =
            QString::fromStdWString(summary.conversationId).trimmed();
        if (conversationId.isEmpty() || seen.contains(conversationId)) {
            continue;
        }

        if (DirectConversationAddressing::otherParticipant(
                localId, conversationId).trimmed().isEmpty()) {
            continue;
        }

        seen.insert(conversationId);
        conversationIds.push_back(conversationId);
    }
    return conversationIds;
}

QStringList RemoteMessageSyncCoordinator::serviceConversationIdsForLocalClient(
    const QString& localClientId,
    const std::vector<ConversationSummary>& summaries,
    const QStringList& knownGroupConversationIds,
    const QStringList& serverConversationIds)
{
    QStringList conversationIds;
    QSet<QString> seen;
    for (const QString& conversationId :
         directConversationIdsForLocalClient(localClientId, summaries)) {
        appendUniqueConversationId(&conversationIds, &seen, conversationId);
    }
    for (const QString& conversationId : knownGroupConversationIds) {
        appendUniqueConversationId(&conversationIds, &seen, conversationId);
    }
    for (const QString& conversationId : serverConversationIds) {
        appendUniqueConversationId(&conversationIds, &seen, conversationId);
    }
    return conversationIds;
}

RemoteMessageSyncRunResult
RemoteMessageSyncCoordinator::syncDirectConversations(
    const QStringList& conversationIds) const
{
    RemoteMessageSyncRunResult aggregate;
    if (m_localClientId.trimmed().isEmpty() || !m_repository || !m_serverClient) {
        aggregate.success = false;
        aggregate.errorMessage = QStringLiteral(
            "remote message sync coordinator is not ready");
        return aggregate;
    }

    QSet<QString> seen;
    QSet<QString> newIncomingSeen;
    for (const QString& rawConversationId : conversationIds) {
        const QString conversationId = rawConversationId.trimmed();
        if (conversationId.isEmpty() || seen.contains(conversationId)) {
            continue;
        }
        seen.insert(conversationId);

        ++aggregate.attemptedCount;
        MessageSyncService syncService(
            m_localClientId, m_repository, m_serverClient, m_pageLimit,
            m_stickerCacheCallback);
        const MessageSyncResult result =
            syncService.syncConversation(conversationId);

        if (result.success) {
            ++aggregate.succeededCount;
            aggregate.storedCount += result.storedCount;
            aggregate.skippedDuplicateCount += result.skippedDuplicateCount;
            for (const QString& incomingConversationId :
                 result.newIncomingConversationIds) {
                appendUniqueConversationId(&aggregate.newIncomingConversationIds,
                                           &newIncomingSeen,
                                           incomingConversationId);
            }
            aggregate.newIncomingNotifications.append(
                result.newIncomingNotifications);
            continue;
        }

        ++aggregate.failedCount;
        aggregate.failedConversationIds.push_back(conversationId);
        if (aggregate.errorMessage.trimmed().isEmpty()) {
            aggregate.errorMessage = result.errorMessage.trimmed().isEmpty()
                ? QStringLiteral("remote message sync failed")
                : result.errorMessage.trimmed();
        }
        if (m_stopOnFirstFailure) {
            break;
        }
    }

    aggregate.success = aggregate.failedCount == 0;
    if (aggregate.success) {
        MessageSyncService ackSyncService(
            m_localClientId, m_repository, m_serverClient, m_pageLimit,
            m_stickerCacheCallback);
        const PendingDeliveryAckFlushResult deliveryAckResult =
            ackSyncService.flushPendingDeliveryAcks(100);
        aggregate.pendingDeliveryAcksAttempted =
            deliveryAckResult.attemptedCount;
        aggregate.pendingDeliveryAcksAcknowledged =
            deliveryAckResult.acknowledgedCount;
        if (!deliveryAckResult.success
            && deliveryAckResult.attemptedCount > 0) {
            qWarning().noquote()
                << "[remote-delivery-ack] pending flush failed during sync"
                << "attempted=" << deliveryAckResult.attemptedCount
                << "acked=" << deliveryAckResult.acknowledgedCount
                << "error=" << deliveryAckResult.errorMessage;
        }

        const PendingReadAckFlushResult readAckResult =
            ackSyncService.flushPendingReadAcks(100);
        aggregate.pendingReadAcksAttempted = readAckResult.attemptedCount;
        aggregate.pendingReadAcksAcknowledged = readAckResult.acknowledgedCount;
        if (!readAckResult.success && readAckResult.attemptedCount > 0) {
            qWarning().noquote()
                << "[remote-read-ack] pending flush failed during sync"
                << "attempted=" << readAckResult.attemptedCount
                << "acked=" << readAckResult.acknowledgedCount
                << "error=" << readAckResult.errorMessage;
        }
    }
    return aggregate;
}
