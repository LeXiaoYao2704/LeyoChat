#include "app/GroupFanOutPayloadBuilder.h"

#include "network/MessageCodec.h"

std::vector<ChatService::PendingGroupFanOutEnvelope> buildPendingGroupFanOut(
    const std::vector<MessageEnvelope>& envelopes,
    std::vector<GroupFanOutPayload>* payloads)
{
    std::vector<ChatService::PendingGroupFanOutEnvelope> pending;
    pending.reserve(envelopes.size());
    if (payloads) {
        payloads->clear();
        payloads->reserve(envelopes.size());
    }

    for (const auto& envelope : envelopes) {
        const QString targetId = QString::fromStdString(envelope.targetId);
        const QString messageId = QString::fromStdString(envelope.messageId);
        const QByteArray blob = QByteArray::fromStdString(MessageCodec::encode(envelope));
        pending.push_back(ChatService::PendingGroupFanOutEnvelope{
            targetId, blob, envelope.createdAtMs});
        if (payloads) {
            payloads->push_back(GroupFanOutPayload{targetId, blob, messageId});
        }
    }
    return pending;
}
