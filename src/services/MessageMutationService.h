#pragma once

#include <optional>
#include <QString>
#include "domain/MessageEnvelope.h"
#include "domain/MessageMutation.h"

class ConversationRepository;

enum class MutationEligibility {
    Eligible,
    MessageNotFound,
    NotSender,
    WindowExpired,
    AlreadyRecalled,
    EditNotAllowedForType,
};

class MessageMutationService {
public:
    // Local eligibility: can the local user mutate this message right now?
    static MutationEligibility evaluateLocalEligibility(
        ConversationRepository* repository,
        const QString& messageId,
        const QString& localClientId,
        MessageMutationKind kind,
        qint64 nowMs);

    // Build a direct-chat mutation envelope ready to send to a peer.
    // Returns nullopt if the mutation struct is invalid.
    static std::optional<MessageEnvelope> buildDirectMutationEnvelope(
        const QString& localClientId,
        const QString& targetId,
        const MessageMutation& mutation);

    // Validate and apply an incoming mutation (direct or group).
    // Returns true if the mutation was valid and applied.
    static bool applyIncomingMutation(
        ConversationRepository* repository,
        const MessageEnvelope& envelope);

private:
    static constexpr qint64 kMutationWindowMs = 120000;    // 2 minutes
    static constexpr qint64 kClockSkewToleranceMs = 10000; // 10 seconds
};
