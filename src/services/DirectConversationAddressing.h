#pragma once

#include <QString>

class DirectConversationAddressing {
public:
    static QString conversationIdForPeers(const QString& firstParticipant, const QString& secondParticipant);
    static QString otherParticipant(const QString& localClientId, const QString& conversationId);
};
