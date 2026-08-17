#include "app/RemotePresenceUiAdapter.h"

#include <algorithm>

#include "domain/PeerPresenceEvaluator.h"
#include "services/DirectConversationAddressing.h"

namespace RemotePresenceUiAdapter {

namespace {
template <typename Summaries>
QSet<QString> directConversationIdsForOnlineClients(
    const QString& localClientId,
    const Summaries& summaries,
    const QSet<QString>& onlineClientIds)
{
    QSet<QString> conversationIds;
    const QString normalizedLocalClientId = localClientId.trimmed();
    if (normalizedLocalClientId.isEmpty() || onlineClientIds.isEmpty()) {
        return conversationIds;
    }

    for (const ConversationSummary& summary : summaries) {
        const QString conversationId =
            QString::fromStdWString(summary.conversationId).trimmed();
        const QString otherClientId =
            DirectConversationAddressing::otherParticipant(
                normalizedLocalClientId,
                conversationId);
        if (!otherClientId.isEmpty() && onlineClientIds.contains(otherClientId)) {
            conversationIds.insert(conversationId);
        }
    }
    return conversationIds;
}

template <typename Summaries>
QSet<QString> directConversationIdsForOnlinePeers(
    const QString& localClientId,
    const Summaries& summaries,
    const QVector<PeerEndpoint>& peers,
    qint64 nowMs)
{
    QSet<QString> onlineClientIds;
    for (const PeerEndpoint& peer : peers) {
        if (!PeerPresenceEvaluator::isOnlineOrAway(peer, nowMs)) {
            continue;
        }
        const QString clientId = QString::fromStdString(peer.clientId).trimmed();
        if (!clientId.isEmpty()) {
            onlineClientIds.insert(clientId);
        }
    }
    return directConversationIdsForOnlineClients(
        localClientId,
        summaries,
        onlineClientIds);
}
}  // namespace

QSet<QString> directConversationIdsForOnlineClients(
    const QString& localClientId,
    const std::vector<ConversationSummary>& summaries,
    const QSet<QString>& onlineClientIds)
{
    return directConversationIdsForOnlineClients<std::vector<ConversationSummary>>(
        localClientId,
        summaries,
        onlineClientIds);
}

QSet<QString> directConversationIdsForOnlineClients(
    const QString& localClientId,
    const QList<ConversationSummary>& summaries,
    const QSet<QString>& onlineClientIds)
{
    return directConversationIdsForOnlineClients<QList<ConversationSummary>>(
        localClientId,
        summaries,
        onlineClientIds);
}

QSet<QString> directConversationIdsForOnlinePeers(
    const QString& localClientId,
    const std::vector<ConversationSummary>& summaries,
    const QVector<PeerEndpoint>& peers,
    qint64 nowMs)
{
    return directConversationIdsForOnlinePeers<std::vector<ConversationSummary>>(
        localClientId,
        summaries,
        peers,
        nowMs);
}

QSet<QString> directConversationIdsForOnlinePeers(
    const QString& localClientId,
    const QList<ConversationSummary>& summaries,
    const QVector<PeerEndpoint>& peers,
    qint64 nowMs)
{
    return directConversationIdsForOnlinePeers<QList<ConversationSummary>>(
        localClientId,
        summaries,
        peers,
        nowMs);
}

QVector<PeerEndpoint> applyOnlineClientsToPeers(
    QVector<PeerEndpoint> peers,
    const QSet<QString>& onlineClientIds,
    qint64 nowMs)
{
    if (onlineClientIds.isEmpty()) {
        return peers;
    }

    for (PeerEndpoint& peer : peers) {
        const QString clientId = QString::fromStdString(peer.clientId).trimmed();
        if (!clientId.isEmpty() && onlineClientIds.contains(clientId)) {
            peer.isConnected = true;
            peer.presence = PeerPresenceStatus::Online;
            peer.lastPresenceAtMs = std::max<qint64>(1, nowMs);
        }
    }
    return peers;
}

}  // namespace RemotePresenceUiAdapter
