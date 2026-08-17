#pragma once

#include <vector>

#include <QList>
#include <QSet>
#include <QString>
#include <QVector>

#include "domain/ConversationSummary.h"
#include "domain/PeerEndpoint.h"

namespace RemotePresenceUiAdapter {

QSet<QString> directConversationIdsForOnlineClients(
    const QString& localClientId,
    const std::vector<ConversationSummary>& summaries,
    const QSet<QString>& onlineClientIds);

QSet<QString> directConversationIdsForOnlineClients(
    const QString& localClientId,
    const QList<ConversationSummary>& summaries,
    const QSet<QString>& onlineClientIds);

QSet<QString> directConversationIdsForOnlinePeers(
    const QString& localClientId,
    const std::vector<ConversationSummary>& summaries,
    const QVector<PeerEndpoint>& peers,
    qint64 nowMs);

QSet<QString> directConversationIdsForOnlinePeers(
    const QString& localClientId,
    const QList<ConversationSummary>& summaries,
    const QVector<PeerEndpoint>& peers,
    qint64 nowMs);

QVector<PeerEndpoint> applyOnlineClientsToPeers(
    QVector<PeerEndpoint> peers,
    const QSet<QString>& onlineClientIds,
    qint64 nowMs);

}  // namespace RemotePresenceUiAdapter
