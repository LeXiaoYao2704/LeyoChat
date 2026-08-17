#pragma once

#include <optional>
#include <vector>

#include <QByteArray>
#include <QString>

#include "domain/ChatMessage.h"
#include "domain/ConversationSummary.h"
#include "domain/MessageEnvelope.h"

class ConversationRepository;

class ChatService {
public:
    struct PendingGroupFanOutEnvelope {
        QString targetId;
        QByteArray envelopeBlob;
        qint64 createdAtMs = 0;
    };

    static bool shouldAutoActivateIncomingConversation(const QString& currentConversationId,
                                                       const QString& incomingConversationId,
                                                       bool isNudge);
    static QString createOutgoingMessage(const QString& localClientId,
                                         ConversationRepository* repository,
                                         const QString& conversationId,
                                         const QString& targetId,
                                         const QString& body);
    static QString createOutgoingMessage(const QString& localClientId,
                                         ConversationRepository* repository,
                                         const QString& conversationId,
                                         const QString& targetId,
                                         const QString& body,
                                         const QString& replyToMessageId,
                                         const QString& replyToSenderId,
                                         const QString& replyToBody);
    static bool markMessageSent(ConversationRepository* repository, const QString& messageId);
    static bool markMessageServerAcked(ConversationRepository* repository, const QString& messageId);
    static bool markMessageReceived(ConversationRepository* repository, const QString& messageId);
    static bool markMessageRead(ConversationRepository* repository, const QString& messageId);
    static bool buildEnvelope(const QString& localClientId,
                              ConversationRepository* repository,
                              const QString& messageId,
                              const QString& targetId,
                              MessageEnvelope* outEnvelope,
                              QString* errorMessage = nullptr);
    static bool storeIncomingEnvelope(const QString& localClientId,
                                      ConversationRepository* repository,
                                      const MessageEnvelope& envelope);
    static bool storeIncomingGroupEnvelope(ConversationRepository* repository,
                                           const MessageEnvelope& envelope,
                                           const QString& groupId,
                                           const QString& title);
    static bool persistOutgoingGroupFanOut(ConversationRepository* repository,
                                           const QString& groupId,
                                           const QString& title,
                                           const std::vector<PendingGroupFanOutEnvelope>& pendingEnvelopes,
                                           const MessageEnvelope& selfEnvelope);
    static bool persistPendingGroupFanOutOnly(ConversationRepository* repository,
                                              const QString& groupId,
                                              const std::vector<PendingGroupFanOutEnvelope>& pendingEnvelopes);
    static std::vector<ChatMessage> loadMessages(ConversationRepository* repository, const QString& conversationId);
    static std::vector<ConversationSummary> loadConversationSummaries(ConversationRepository* repository);
    static bool applyReaction(ConversationRepository* repository,
                              const QString& messageId,
                              const QString& reactorClientId,
                              const QString& emoji);

private:
    static bool updateMessageState(ConversationRepository* repository,
                                   const QString& messageId,
                                   MessageDeliveryState state);
    static bool upsertConversationSummary(ConversationRepository* repository,
                                          const ConversationSummary& summary,
                                          const QString& conversationType);
};
