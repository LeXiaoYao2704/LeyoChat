#include "services/ReliableGroupEnvelopeSender.h"

#include <initializer_list>
#include <optional>
#include <utility>

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QScopeGuard>
#include <QSet>

#include "domain/ChatMessage.h"
#include "integrations/ServerMessageClient.h"
#include "storage/ConversationRepository.h"

namespace {

QString firstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return {};
}

QString fromEnvelopeUtf8(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size())).trimmed();
}

QString groupTitleOrFallback(const QString& groupTitle, const QString& groupId)
{
    const QString title = groupTitle.trimmed();
    return title.isEmpty() ? groupId.trimmed() : title;
}

QByteArray bytesFromString(const std::string& value)
{
    return QByteArray(value.data(), static_cast<int>(value.size()));
}

QJsonObject objectFromJsonString(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const QJsonDocument document = QJsonDocument::fromJson(bytesFromString(value));
    return document.isObject() ? document.object() : QJsonObject{};
}

QString envelopeBodyText(const MessageEnvelope& envelope)
{
    const QJsonObject body = objectFromJsonString(envelope.body);
    const QString text = body.value(QStringLiteral("text")).toString().trimmed();
    if (!text.isEmpty()) {
        return text;
    }

    if (envelope.type == MessageType::ResourceReference) {
        const QString title = firstNonEmpty({
            body.value(QStringLiteral("title")).toString(),
            objectFromJsonString(envelope.payloadJson).value(QStringLiteral("title")).toString(),
            body.value(QStringLiteral("resource_id")).toString(),
            objectFromJsonString(envelope.payloadJson).value(QStringLiteral("resource_id")).toString()
        });
        return title.isEmpty() ? QStringLiteral("[resource]") : title;
    }

    if (envelope.type == MessageType::MessageMutation
        || envelope.type == MessageType::MessageReaction
        || envelope.type == MessageType::PinMessage) {
        return {};
    }

    return fromEnvelopeUtf8(envelope.body);
}

QString envelopeContentType(const MessageEnvelope& envelope)
{
    if (envelope.type == MessageType::MessageMutation) {
        return QStringLiteral("mutation");
    }
    if (envelope.type == MessageType::MessageReaction) {
        return QStringLiteral("reaction");
    }
    if (envelope.type == MessageType::PinMessage) {
        return QStringLiteral("pin");
    }

    const QString direct = fromEnvelopeUtf8(envelope.contentType);
    if (!direct.isEmpty()) {
        return direct;
    }

    const QJsonObject body = objectFromJsonString(envelope.body);
    const QString fromBody = body.value(QStringLiteral("content_type")).toString().trimmed();
    return fromBody.isEmpty() ? QStringLiteral("plain") : fromBody;
}

QString serviceTypeFor(const MessageEnvelope& envelope)
{
    const QString subtype = fromEnvelopeUtf8(envelope.messageSubtype);
    if (envelope.type == MessageType::MessageMutation) {
        return QStringLiteral("message_mutation");
    }
    if (envelope.type == MessageType::MessageReaction) {
        return QStringLiteral("message_reaction");
    }
    if (envelope.type == MessageType::PinMessage) {
        return QStringLiteral("pin_message");
    }
    if (envelope.type == MessageType::ResourceReference
        || subtype == QStringLiteral("resource_ref")) {
        return QStringLiteral("resource_ref");
    }
    if (subtype == QStringLiteral("sticker")) {
        return QStringLiteral("sticker");
    }
    if (!subtype.isEmpty() && subtype != QStringLiteral("text")) {
        return subtype;
    }
    return QStringLiteral("chat_text");
}

QJsonObject envelopePayloadObject(const MessageEnvelope& envelope)
{
    QJsonObject payload = objectFromJsonString(envelope.payloadJson);
    if (!payload.isEmpty()) {
        return payload;
    }

    const QJsonObject body = objectFromJsonString(envelope.body);
    return body;
}

QVector<QString> requestedRecipientsInEnvelopeSet(
    const QVector<QString>& requestedRecipientIds,
    const QVector<QString>& envelopeRecipientIds)
{
    QSet<QString> available;
    for (const QString& recipientId : envelopeRecipientIds) {
        const QString normalized = recipientId.trimmed();
        if (!normalized.isEmpty()) {
            available.insert(normalized);
        }
    }

    QVector<QString> result;
    QSet<QString> seen;
    for (const QString& recipientId : requestedRecipientIds) {
        const QString normalized = recipientId.trimmed();
        if (normalized.isEmpty()
            || !available.contains(normalized)
            || seen.contains(normalized)) {
            continue;
        }
        seen.insert(normalized);
        result.push_back(normalized);
    }
    return result;
}

std::vector<MessageEnvelope> envelopesForRecipients(
    const std::vector<MessageEnvelope>& envelopes,
    const QVector<QString>& recipientIds)
{
    QSet<QString> requested;
    for (const QString& recipientId : recipientIds) {
        const QString normalized = recipientId.trimmed();
        if (!normalized.isEmpty()) {
            requested.insert(normalized);
        }
    }

    std::vector<MessageEnvelope> result;
    for (const MessageEnvelope& envelope : envelopes) {
        if (requested.contains(fromEnvelopeUtf8(envelope.targetId))) {
            result.push_back(envelope);
        }
    }
    return result;
}

bool hasSupportedEnvelopeType(const MessageEnvelope& envelope)
{
    return envelope.type == MessageType::GroupMessage
        || envelope.type == MessageType::ResourceReference
        || envelope.type == MessageType::MessageMutation
        || envelope.type == MessageType::MessageReaction
        || envelope.type == MessageType::PinMessage;
}

}  // namespace

ReliableGroupEnvelopeSender::ReliableGroupEnvelopeSender(
    QString localClientId,
    ConversationRepository* repository,
    const IServerMessageClient* serverClient,
    P2PSendCallback p2pSendCallback)
    : m_localClientId(std::move(localClientId))
    , m_repository(repository)
    , m_serverClient(serverClient)
    , m_p2pSendCallback(std::move(p2pSendCallback))
{
}

ReliableGroupEnvelopeSendResult ReliableGroupEnvelopeSender::send(
    const ReliableGroupEnvelopeSendRequest& request) const
{
    ReliableGroupEnvelopeSendResult result;
    ValidatedEnvelopes envelopes;
    QString validationError;
    if (!validateEnvelopes(request, &envelopes, &validationError)) {
        result.errorMessage =
            firstNonEmpty({validationError, QStringLiteral("group envelope request is invalid")});
        return result;
    }
    result.messageId = envelopes.messageId;
    [[maybe_unused]] const auto routeResultLog = qScopeGuard([&]() {
        qInfo().noquote()
            << "[route-result] type=group-envelope"
            << "msgId=" << result.messageId.left(8)
            << "groupId=" << request.groupId.trimmed()
            << "recipients=" << envelopes.recipientIds.size()
            << "success=" << result.success
            << "channel=" << transportChannelName(result.channelUsed)
            << "error=" << result.errorMessage;
    });

    const bool hasRecipientPartition =
        !request.serverRecipientIds.isEmpty()
        || !request.p2pRecipientIds.isEmpty();
    if (hasRecipientPartition) {
        const QVector<QString> serverRecipients =
            requestedRecipientsInEnvelopeSet(request.serverRecipientIds,
                                             envelopes.recipientIds);
        const QVector<QString> p2pRecipients =
            requestedRecipientsInEnvelopeSet(request.p2pRecipientIds,
                                             envelopes.recipientIds);
        if (serverRecipients.isEmpty() && p2pRecipients.isEmpty()) {
            result.errorMessage =
                QStringLiteral("group envelope recipient partition has no matching recipients");
            return result;
        }
        const TransportChannel partitionChannel =
            !serverRecipients.isEmpty() && !p2pRecipients.isEmpty()
                ? TransportChannel::Mixed
                : (!serverRecipients.isEmpty() ? TransportChannel::MessageService
                                               : TransportChannel::P2P);
        qInfo().noquote()
            << "[route-decision] type=group-envelope partitioned=true"
            << "msgId=" << result.messageId.left(8)
            << "groupId=" << request.groupId.trimmed()
            << "mode=" << transportModeName(request.settings.mode)
            << "serviceConfigured=" << request.settings.canUseMessageService()
            << "serviceReachable=" << request.serviceReachable
            << "p2pAvailable=" << request.p2pAvailable
            << "recipients=" << envelopes.recipientIds.size()
            << "serverRecipients=" << serverRecipients.size()
            << "p2pRecipients=" << p2pRecipients.size()
            << "selected=" << transportChannelName(partitionChannel);

        if (!serverRecipients.isEmpty()
            && (!request.settings.canUseMessageService()
                || !request.serviceReachable
                || request.settings.mode == RemoteChatTransportMode::P2POnly)) {
            result.errorMessage =
                QStringLiteral("message service is not available for partitioned group envelopes");
            return result;
        }
        if (!p2pRecipients.isEmpty()
            && (request.settings.mode == RemoteChatTransportMode::ServerOnly
                || !request.settings.allowP2PFallback
                || !request.p2pAvailable)) {
            result.errorMessage =
                QStringLiteral("p2p is not available for partitioned group envelopes");
            return result;
        }

        bool usedService = false;
        bool usedP2P = false;
        QString p2pError;
        if (!p2pRecipients.isEmpty()) {
            ReliableGroupEnvelopeSendRequest p2pRequest = request;
            p2pRequest.envelopes =
                envelopesForRecipients(request.envelopes, p2pRecipients);
            ValidatedEnvelopes p2pEnvelopes = envelopes;
            p2pEnvelopes.recipientIds = p2pRecipients;
            const bool updateP2PDeliveryState = serverRecipients.isEmpty();
            if (!sendViaP2P(p2pRequest,
                            p2pEnvelopes,
                            updateP2PDeliveryState,
                            &p2pError)) {
                result.channelUsed = TransportChannel::P2P;
                result.errorMessage =
                    firstNonEmpty({p2pError, QStringLiteral("p2p send failed")});
                return result;
            }
            usedP2P = true;
        }

        QString serviceError;
        if (!serverRecipients.isEmpty()) {
            ValidatedEnvelopes serverEnvelopes = envelopes;
            serverEnvelopes.recipientIds = serverRecipients;
            const ServiceSendStatus serviceStatus =
                sendViaMessageService(request, serverEnvelopes, &serviceError);
            if (serviceStatus != ServiceSendStatus::Succeeded) {
                if (serviceStatus
                    == ServiceSendStatus::AcceptedButLocalFinalizeFailed) {
                    result.channelUsed =
                        usedP2P ? TransportChannel::Mixed : TransportChannel::MessageService;
                    result.errorMessage = firstNonEmpty(
                        {serviceError,
                         QStringLiteral("message service accepted but local finalize failed")});
                    return result;
                }
                if (request.settings.allowP2PFallback
                    && request.settings.mode != RemoteChatTransportMode::ServerOnly
                    && request.p2pAvailable) {
                    ReliableGroupEnvelopeSendRequest fallbackRequest = request;
                    fallbackRequest.envelopes =
                        envelopesForRecipients(request.envelopes, serverRecipients);
                    QString fallbackP2PError;
                    if (sendViaP2P(fallbackRequest,
                                   serverEnvelopes,
                                   true,
                                   &fallbackP2PError)) {
                        result.success = true;
                        result.channelUsed =
                            usedP2P ? TransportChannel::Mixed : TransportChannel::P2P;
                        return result;
                    }

                    result.channelUsed =
                        usedP2P ? TransportChannel::Mixed : TransportChannel::MessageService;
                    result.errorMessage =
                        firstNonEmpty({fallbackP2PError,
                                       serviceError,
                                       QStringLiteral("group envelope send failed")});
                    return result;
                }

                result.channelUsed =
                    usedP2P ? TransportChannel::Mixed : TransportChannel::MessageService;
                result.errorMessage =
                    firstNonEmpty({serviceError,
                                   QStringLiteral("message service send failed")});
                return result;
            }
            usedService = true;
        }

        result.success = true;
        result.channelUsed =
            usedService && usedP2P
                ? TransportChannel::Mixed
                : (usedService ? TransportChannel::MessageService
                               : TransportChannel::P2P);
        return result;
    }

    TransportPolicyInput policyInput;
    policyInput.mode = request.settings.mode;
    policyInput.serviceConfigured = request.settings.canUseMessageService();
    policyInput.serviceReachable = request.serviceReachable;
    policyInput.receiverServerCapable = true;
    policyInput.p2pAvailable = request.p2pAvailable;
    policyInput.allowP2PFallback = request.settings.allowP2PFallback;
    const TransportDecision decision =
        TransportPolicy::chooseGroupTextChannel(policyInput);
    qInfo().noquote()
        << "[route-decision] type=group-envelope partitioned=false"
        << "msgId=" << result.messageId.left(8)
        << "groupId=" << request.groupId.trimmed()
        << "mode=" << transportModeName(request.settings.mode)
        << "serviceConfigured=" << policyInput.serviceConfigured
        << "serviceReachable=" << policyInput.serviceReachable
        << "p2pAvailable=" << policyInput.p2pAvailable
        << "recipients=" << envelopes.recipientIds.size()
        << "selected=" << transportChannelName(decision.primary)
        << "mayFallbackToP2P=" << decision.mayFallbackToP2P;

    if (decision.primary == TransportChannel::MessageService) {
        QString serviceError;
        result.channelUsed = TransportChannel::MessageService;
        const ServiceSendStatus serviceStatus =
            sendViaMessageService(request, envelopes, &serviceError);
        if (serviceStatus == ServiceSendStatus::Succeeded) {
            result.success = true;
            return result;
        }
        if (serviceStatus
            == ServiceSendStatus::AcceptedButLocalFinalizeFailed) {
            result.errorMessage = firstNonEmpty(
                {serviceError, QStringLiteral("message service accepted but local finalize failed")});
            return result;
        }

        if (decision.mayFallbackToP2P) {
            QString p2pError;
            if (sendViaP2P(request, envelopes, true, &p2pError)) {
                result.success = true;
                result.channelUsed = TransportChannel::P2P;
                return result;
            }
            result.errorMessage =
                firstNonEmpty({p2pError, serviceError, QStringLiteral("group envelope send failed")});
            return result;
        }

        result.errorMessage =
            firstNonEmpty({serviceError, QStringLiteral("message service send failed")});
        return result;
    }

    if (decision.primary == TransportChannel::P2P) {
        QString p2pError;
        result.channelUsed = TransportChannel::P2P;
        if (sendViaP2P(request, envelopes, true, &p2pError)) {
            result.success = true;
            return result;
        }
        result.errorMessage =
            firstNonEmpty({p2pError, QStringLiteral("p2p group envelope send failed")});
        return result;
    }

    result.errorMessage = QStringLiteral("no available group envelope transport");
    return result;
}

bool ReliableGroupEnvelopeSender::validateEnvelopes(
    const ReliableGroupEnvelopeSendRequest& request,
    ValidatedEnvelopes* out,
    QString* errorMessage) const
{
    if (!out) {
        return false;
    }

    const QString localClientId = m_localClientId.trimmed();
    const QString groupId = request.groupId.trimmed();
    if (!m_repository || localClientId.isEmpty() || groupId.isEmpty()
        || request.envelopes.empty()) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("local client, repository, group, and envelopes are required");
        }
        return false;
    }

    QString messageId;
    QVector<QString> recipientIds;
    QSet<QString> seenRecipients;
    MessageEnvelope firstEnvelope;
    bool hasFirstEnvelope = false;

    for (const MessageEnvelope& envelope : request.envelopes) {
        if (!hasSupportedEnvelopeType(envelope)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("group envelope type is not supported");
            }
            return false;
        }

        const QString envelopeMessageId = fromEnvelopeUtf8(envelope.messageId);
        const QString senderId = fromEnvelopeUtf8(envelope.senderId);
        const QString targetId = fromEnvelopeUtf8(envelope.targetId);
        const QString envelopeGroupId = fromEnvelopeUtf8(envelope.conversationId);
        const bool payloadOnly =
            envelope.type == MessageType::MessageMutation
            || envelope.type == MessageType::MessageReaction
            || envelope.type == MessageType::PinMessage;
        const bool hasRequiredContent =
            payloadOnly ? !envelope.payloadJson.empty() : !envelope.body.empty();

        if (envelopeMessageId.isEmpty() || senderId != localClientId
            || targetId.isEmpty() || envelopeGroupId != groupId
            || !hasRequiredContent) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("group envelope has inconsistent message, sender, target, group, or content");
            }
            return false;
        }

        if (messageId.isEmpty()) {
            messageId = envelopeMessageId;
        } else if (messageId != envelopeMessageId) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("group envelopes must share one message id");
            }
            return false;
        }

        if (!seenRecipients.contains(targetId)) {
            seenRecipients.insert(targetId);
            recipientIds.push_back(targetId);
        }

        if (!hasFirstEnvelope) {
            firstEnvelope = envelope;
            hasFirstEnvelope = true;
        }
    }

    if (messageId.isEmpty() || recipientIds.isEmpty() || !hasFirstEnvelope) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("group envelopes have no recipients");
        }
        return false;
    }

    out->messageId = messageId;
    out->recipientIds = recipientIds;
    out->firstEnvelope = std::move(firstEnvelope);
    return true;
}

ReliableGroupEnvelopeSender::ServiceSendStatus
ReliableGroupEnvelopeSender::sendViaMessageService(
    const ReliableGroupEnvelopeSendRequest& request,
    const ValidatedEnvelopes& envelopes,
    QString* errorMessage) const
{
    if (!m_serverClient) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("message service client is not configured");
        }
        return ServiceSendStatus::FailedBeforeAck;
    }

    const MessageEnvelope& envelope = envelopes.firstEnvelope;
    ServerMessageDraft draft;
    draft.clientMessageId = envelopes.messageId;
    draft.conversationId = request.groupId.trimmed();
    draft.workspaceId = request.settings.workspaceId.trimmed();
    draft.type = serviceTypeFor(envelope);
    draft.body = envelopeBodyText(envelope);
    draft.payload = envelopePayloadObject(envelope);
    draft.contentType = envelopeContentType(envelope);
    draft.replyToMessageId = fromEnvelopeUtf8(envelope.replyToMessageId);
    draft.recipientIds = envelopes.recipientIds;

    QString sendError;
    const std::optional<ServerMessageAck> ack =
        m_serverClient->sendMessage(draft, &sendError);
    if (!ack) {
        if (errorMessage) {
            *errorMessage =
                firstNonEmpty({sendError, QStringLiteral("message service send failed")});
        }
        return ServiceSendStatus::FailedBeforeAck;
    }

    if (ack->conversationId.trimmed() != draft.conversationId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("message service ack conversation mismatch");
        }
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }

    if (!markExistingMessageState(envelopes.messageId,
                                  MessageDeliveryState::ServerAcked,
                                  ack->serverMessageId,
                                  errorMessage)) {
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }
    clearPendingFanOutForRecipients(envelopes.messageId, envelopes.recipientIds);
    return ServiceSendStatus::Succeeded;
}

bool ReliableGroupEnvelopeSender::sendViaP2P(
    const ReliableGroupEnvelopeSendRequest& request,
    const ValidatedEnvelopes& envelopes,
    bool updateDeliveryState,
    QString* errorMessage) const
{
    if (!m_p2pSendCallback) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("p2p send callback is not configured");
        }
        return false;
    }

    ReliableGroupEnvelopeP2PRequest p2pRequest;
    p2pRequest.messageId = envelopes.messageId;
    p2pRequest.groupId = request.groupId.trimmed();
    p2pRequest.groupTitle = groupTitleOrFallback(request.groupTitle, request.groupId);
    p2pRequest.envelopes = request.envelopes;
    p2pRequest.acceptQueuedOnlyDelivery = !updateDeliveryState;

    QString callbackError;
    if (!m_p2pSendCallback(p2pRequest, &callbackError)) {
        if (errorMessage) {
            *errorMessage =
                firstNonEmpty({callbackError, QStringLiteral("p2p send failed")});
        }
        return false;
    }

    if (updateDeliveryState
        && !markExistingMessageState(envelopes.messageId,
                                     MessageDeliveryState::Sent,
                                     QString(),
                                     errorMessage)) {
        return false;
    }
    return true;
}

bool ReliableGroupEnvelopeSender::markExistingMessageState(
    const QString& messageId,
    MessageDeliveryState state,
    const QString& serverMessageId,
    QString* errorMessage) const
{
    ChatMessage existing;
    if (!m_repository->findMessageById(messageId, &existing)) {
        return true;
    }

    const QString normalizedServerMessageId = serverMessageId.trimmed();
    if (!normalizedServerMessageId.isEmpty()
        && !m_repository->saveRemoteMessageIdMapping(normalizedServerMessageId,
                                                     messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to save message service id mapping");
        }
        return false;
    }
    if (!m_repository->updateDeliveryState(messageId, state)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to update group envelope delivery state");
        }
        return false;
    }
    return true;
}

void ReliableGroupEnvelopeSender::clearPendingFanOutForRecipients(
    const QString& messageId,
    const QVector<QString>& recipientIds) const
{
    const QString normalizedMessageId = messageId.trimmed();
    if (!m_repository || normalizedMessageId.isEmpty()) {
        return;
    }

    for (const QString& recipientId : recipientIds) {
        const QString normalizedRecipientId = recipientId.trimmed();
        if (normalizedRecipientId.isEmpty()) {
            continue;
        }
        if (!m_repository->deletePendingGroupEnvelopeForTargetMessage(
                normalizedRecipientId, normalizedMessageId)) {
            qWarning().noquote()
                << "[group-envelope] failed to clear service-delivered pending envelope"
                << "msgId=" << normalizedMessageId.left(8)
                << "target=" << normalizedRecipientId.left(8);
        }
    }
}
