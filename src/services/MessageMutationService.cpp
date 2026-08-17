#include "services/MessageMutationService.h"

#include "domain/ChatMessage.h"
#include "domain/MessageMutation.h"
#include "storage/ConversationRepository.h"

#include <QUuid>

MutationEligibility MessageMutationService::evaluateLocalEligibility(
    ConversationRepository* repository,
    const QString& messageId,
    const QString& localClientId,
    MessageMutationKind kind,
    qint64 nowMs)
{
    if (!repository) {
        return MutationEligibility::MessageNotFound;
    }

    ChatMessage msg;
    if (!repository->findMessageMutationStateById(messageId, &msg)) {
        return MutationEligibility::MessageNotFound;
    }

    if (msg.senderId != localClientId.toStdWString()) {
        return MutationEligibility::NotSender;
    }

    if (nowMs - msg.createdAtMs > kMutationWindowMs) {
        return MutationEligibility::WindowExpired;
    }

    if (msg.isRecalled) {
        return MutationEligibility::AlreadyRecalled;
    }

    if (kind == MessageMutationKind::Edit) {
        if (msg.messageType != L"text") {
            return MutationEligibility::EditNotAllowedForType;
        }
    }

    return MutationEligibility::Eligible;
}

std::optional<MessageEnvelope> MessageMutationService::buildDirectMutationEnvelope(
    const QString& localClientId,
    const QString& targetId,
    const MessageMutation& mutation)
{
    if (mutation.targetMessageId.isEmpty() || mutation.mutatedAtMs <= 0) {
        return std::nullopt;
    }

    MessageEnvelope envelope;
    envelope.type = MessageType::MessageMutation;

    const QString mutationMessageId = mutation.mutationMessageId.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : mutation.mutationMessageId;

    envelope.messageId = mutationMessageId.toStdString();
    envelope.senderId = localClientId.toStdString();
    envelope.targetId = targetId.toStdString();
    envelope.conversationId = mutation.conversationId.toStdString();
    envelope.messageSubtype = (mutation.kind == MessageMutationKind::Recall) ? "recall" : "edit";

    if (mutation.kind == MessageMutationKind::Recall) {
        envelope.payloadJson = buildRecallPayloadJson(mutation.targetMessageId, mutation.mutatedAtMs);
    } else {
        envelope.payloadJson = buildEditPayloadJson(
            mutation.targetMessageId, mutation.newBody, mutation.newContentType, mutation.mutatedAtMs);
    }

    envelope.createdAtMs = mutation.mutatedAtMs;

    return envelope;
}

bool MessageMutationService::applyIncomingMutation(
    ConversationRepository* repository,
    const MessageEnvelope& envelope)
{
    if (!repository) {
        return false;
    }

    // 1. Parse mutation payload
    const auto mutationOpt = parseMutationPayload(envelope);
    if (!mutationOpt.has_value()) {
        return false;
    }

    const MessageMutation& mutation = *mutationOpt;

    // 2. Load original message
    ChatMessage original;
    if (!repository->findMessageMutationStateById(mutation.targetMessageId, &original)) {
        return false;
    }

    // 2a. Validate conversationId matches the stored message's conversation
    if (QString::fromStdWString(original.conversationId) != mutation.conversationId) {
        return false;
    }

    // 3. Validate actor = original sender
    if (QString::fromStdWString(original.senderId) != QString::fromStdString(envelope.senderId)) {
        return false;
    }

    // 4. Validate mutation window
    if (mutation.mutatedAtMs < original.createdAtMs)
        return false;
    if (mutation.mutatedAtMs - original.createdAtMs > kMutationWindowMs + kClockSkewToleranceMs) {
        return false;
    }

    // 5. Treat already-applied same-actor mutations as handled so replayed
    // control envelopes can still be acknowledged and removed from pending fan-out.
    if (mutation.mutatedAtMs <= original.lastMutationAtMs) {
        return QString::fromStdWString(original.lastEditorId)
            == QString::fromStdString(envelope.senderId);
    }

    // 6. Reject edit on recalled message or non-text message
    if (mutation.kind == MessageMutationKind::Edit) {
        if (original.isRecalled) {
            return false;
        }
        if (original.messageType != L"text") {
            return false;
        }
    }

    // 7. Apply the mutation
    bool applied = false;
    if (mutation.kind == MessageMutationKind::Recall) {
        applied = repository->applyMessageRecall(
            mutation.targetMessageId, mutation.actorId, mutation.mutatedAtMs);
    } else {
        applied = repository->applyMessageEdit(
            mutation.targetMessageId, mutation.actorId, mutation.mutatedAtMs, mutation.newBody);
    }

    if (!applied) {
        return false;
    }

    // 8. Refresh conversation preview (best-effort)
    repository->refreshConversationPreviewFromLatestVisibleMessage(mutation.conversationId);

    return true;
}
