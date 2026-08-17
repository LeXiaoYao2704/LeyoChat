#include "services/DirectConversationAddressing.h"

#include <QStringList>

QString DirectConversationAddressing::conversationIdForPeers(const QString& firstParticipant,
                                                            const QString& secondParticipant) {
    const QString first = firstParticipant.trimmed();
    const QString second = secondParticipant.trimmed();
    if (first.isEmpty() || second.isEmpty()) {
        return {};
    }

    return first <= second ? QStringLiteral("%1|%2").arg(first, second)
                           : QStringLiteral("%1|%2").arg(second, first);
}

QString DirectConversationAddressing::otherParticipant(const QString& localClientId,
                                                       const QString& conversationId) {
    const QStringList parts = conversationId.split('|', Qt::KeepEmptyParts);
    if (parts.size() != 2) {
        return {};
    }

    if (parts.at(0) == localClientId) {
        return parts.at(1);
    }
    if (parts.at(1) == localClientId) {
        return parts.at(0);
    }

    return {};
}
