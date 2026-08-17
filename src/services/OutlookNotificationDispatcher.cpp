#include "services/OutlookNotificationDispatcher.h"

#include <QDateTime>
#include <QUuid>

#include "architecture/ResourceReferenceMessage.h"
#include "services/DirectConversationAddressing.h"
#include "services/GroupService.h"
#include "services/ResourceRefRouter.h"

ResourceRefPayload OutlookNotificationDispatcher::payloadForEvent(
    const OutlookNotificationEvent& event)
{
    return OutlookNotificationContracts::makeNotificationPayload(event);
}

std::optional<DirectResourceReferenceDraft> OutlookNotificationDispatcher::buildDirectDraft(
    const QString& localClientId,
    const QString& targetClientId,
    const QString& conversationTitle,
    const OutlookNotificationEvent& event)
{
    const QString normalizedLocalClientId = localClientId.trimmed();
    const QString normalizedTargetClientId = targetClientId.trimmed();
    if (normalizedLocalClientId.isEmpty() || normalizedTargetClientId.isEmpty()) {
        return std::nullopt;
    }

    const QString canonicalConversationId =
        DirectConversationAddressing::conversationIdForPeers(normalizedLocalClientId,
                                                             normalizedTargetClientId);
    if (canonicalConversationId.isEmpty()) {
        return std::nullopt;
    }

    const ResourceRefPayload payload = payloadForEvent(event);
    const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 createdAtMs = QDateTime::currentMSecsSinceEpoch();
    const MessageEnvelope envelope =
        buildResourceReferenceEnvelope(messageId,
                                       normalizedLocalClientId,
                                       normalizedTargetClientId,
                                       canonicalConversationId,
                                       payload,
                                       createdAtMs);
    const QString preview = ResourceRefRouter::previewLabel(envelope).trimmed();

    DirectResourceReferenceDraft draft;
    draft.targetClientId = normalizedTargetClientId;
    draft.conversationId = canonicalConversationId;
    draft.conversationTitle = conversationTitle.trimmed().isEmpty()
        ? normalizedTargetClientId
        : conversationTitle.trimmed();
    draft.messageId = messageId;
    draft.preview = preview;
    draft.envelope = envelope;
    draft.message = ChatMessage{
        messageId.toStdWString(),
        canonicalConversationId.toStdWString(),
        normalizedLocalClientId.toStdWString(),
        preview.toStdWString(),
        createdAtMs,
        MessageDeliveryState::Pending,
        {},
        {},
        L"resource_ref",
        QString::fromUtf8(envelope.payloadJson.data(),
                          static_cast<int>(envelope.payloadJson.size())).toStdWString(),
    };
    return draft;
}

std::vector<MessageEnvelope> OutlookNotificationDispatcher::buildGroupFanOut(
    const QString& localClientId,
    const QString& groupId,
    const OutlookNotificationEvent& event,
    const GroupService* groupService)
{
    if (!groupService) {
        return {};
    }

    return groupService->buildGroupResourceReferenceFanOut(localClientId,
                                                           groupId,
                                                           payloadForEvent(event));
}
