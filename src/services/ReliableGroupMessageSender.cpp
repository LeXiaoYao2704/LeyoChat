#include "services/ReliableGroupMessageSender.h"

#include <initializer_list>
#include <optional>
#include <utility>

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QScopeGuard>
#include <QSet>

#include "domain/ChatMessage.h"
#include "integrations/ServerMessageClient.h"
#include "services/ChatService.h"
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

}  // namespace

ReliableGroupMessageSender::ReliableGroupMessageSender(
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

ReliableGroupMessageSender::ReliableGroupMessageSender(
    QString localClientId,
    ConversationRepository* repository,
    const IServerMessageClient* serverClient,
    ReliableGroupMessageP2PStatusCallback p2pStatusCallback)
    : m_localClientId(std::move(localClientId))
    , m_repository(repository)
    , m_serverClient(serverClient)
    , m_p2pStatusCallback(std::move(p2pStatusCallback))
{
}

ReliableGroupMessageSendResult ReliableGroupMessageSender::sendText(
    const ReliableGroupMessageSendRequest& request) const
{
    ReliableGroupMessageSendResult result;
    ValidatedEnvelopes envelopes;
    QString validationError;
    if (!validateEnvelopes(request, &envelopes, &validationError)) {
        result.errorMessage =
            firstNonEmpty({validationError, QStringLiteral("group message request is invalid")});
        return result;
    }
    result.messageId = envelopes.messageId;
    [[maybe_unused]] const auto routeResultLog = qScopeGuard([&]() {
        qInfo().noquote()
            << "[route-result] type=group-text"
            << "msgId=" << result.messageId.left(8)
            << "groupId=" << request.groupId.trimmed()
            << "recipients=" << envelopes.recipientIds.size()
            << "success=" << result.success
            << "channel=" << transportChannelName(result.channelUsed)
            << "error=" << result.errorMessage;
    });

    QString persistError;
    if (!persistLocalPendingMessage(request, envelopes, &persistError)) {
        result.errorMessage =
            firstNonEmpty({persistError, QStringLiteral("failed to create local group message")});
        return result;
    }

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
                QStringLiteral("group message recipient partition has no matching recipients");
            return result;
        }
        const TransportChannel partitionChannel =
            !serverRecipients.isEmpty() && !p2pRecipients.isEmpty()
                ? TransportChannel::Mixed
                : (!serverRecipients.isEmpty() ? TransportChannel::MessageService
                                               : TransportChannel::P2P);
        qInfo().noquote()
            << "[route-decision] type=group-text partitioned=true"
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
                QStringLiteral("message service is not available for partitioned group recipients");
            return result;
        }
        if (!p2pRecipients.isEmpty()
            && (request.settings.mode == RemoteChatTransportMode::ServerOnly
                || !request.settings.allowP2PFallback
                || !request.p2pAvailable)) {
            result.errorMessage =
                QStringLiteral("p2p is not available for partitioned group recipients");
            return result;
        }

        bool usedService = false;
        bool usedP2P = false;
        QString p2pError;
        if (!p2pRecipients.isEmpty()) {
            ReliableGroupMessageSendRequest p2pRequest = request;
            p2pRequest.envelopes =
                envelopesForRecipients(request.envelopes, p2pRecipients);
            ValidatedEnvelopes p2pEnvelopes = envelopes;
            p2pEnvelopes.recipientIds = p2pRecipients;
            const bool updateP2PDeliveryState = serverRecipients.isEmpty();
            GroupFanOutDeliveryResult p2pDelivery;
            if (!sendViaP2P(p2pRequest,
                            p2pEnvelopes,
                            updateP2PDeliveryState,
                            &p2pDelivery,
                            &p2pError)) {
                result.channelUsed = TransportChannel::P2P;
                result.accepted = p2pDelivery.accepted();
                result.writtenRecipientCount = p2pDelivery.writtenCount;
                result.queuedRecipientCount = p2pDelivery.queuedCount;
                result.failedRecipientCount = p2pDelivery.failedCount;
                result.errorMessage =
                    firstNonEmpty({p2pError, QStringLiteral("p2p send failed")});
                return result;
            }
            result.accepted = p2pDelivery.accepted();
            result.writtenRecipientCount += p2pDelivery.writtenCount;
            result.queuedRecipientCount += p2pDelivery.queuedCount;
            result.failedRecipientCount += p2pDelivery.failedCount;
            if (p2pDelivery.failedCount > 0) {
                result.channelUsed = TransportChannel::P2P;
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
                    ReliableGroupMessageSendRequest fallbackRequest = request;
                    fallbackRequest.envelopes =
                        envelopesForRecipients(request.envelopes, serverRecipients);
                    QString fallbackP2PError;
                    GroupFanOutDeliveryResult fallbackDelivery;
                    if (sendViaP2P(fallbackRequest,
                                   serverEnvelopes,
                                   true,
                                   &fallbackDelivery,
                                   &fallbackP2PError)) {
                        result.accepted = fallbackDelivery.accepted();
                        result.writtenRecipientCount += fallbackDelivery.writtenCount;
                        result.queuedRecipientCount += fallbackDelivery.queuedCount;
                        result.failedRecipientCount += fallbackDelivery.failedCount;
                        result.success = fallbackDelivery.allWritten()
                            && result.queuedRecipientCount == 0
                            && result.failedRecipientCount == 0;
                        result.channelUsed =
                            usedP2P ? TransportChannel::Mixed : TransportChannel::P2P;
                        return result;
                    }

                    result.channelUsed =
                        usedP2P ? TransportChannel::Mixed : TransportChannel::MessageService;
                    result.errorMessage =
                        firstNonEmpty({fallbackP2PError,
                                       serviceError,
                                       QStringLiteral("group message send failed")});
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
            result.accepted = true;
            result.writtenRecipientCount += serverRecipients.size();
        }

        result.success = result.accepted && result.queuedRecipientCount == 0
            && result.failedRecipientCount == 0;
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
        << "[route-decision] type=group-text partitioned=false"
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
            GroupFanOutDeliveryResult p2pDelivery;
            if (sendViaP2P(request, envelopes, true, &p2pDelivery, &p2pError)) {
                result.accepted = p2pDelivery.accepted();
                result.writtenRecipientCount = p2pDelivery.writtenCount;
                result.queuedRecipientCount = p2pDelivery.queuedCount;
                result.failedRecipientCount = p2pDelivery.failedCount;
                result.success = p2pDelivery.allWritten();
                result.channelUsed = TransportChannel::P2P;
                return result;
            }
            result.errorMessage =
                firstNonEmpty({p2pError, serviceError, QStringLiteral("group message send failed")});
            return result;
        }

        result.errorMessage =
            firstNonEmpty({serviceError, QStringLiteral("message service send failed")});
        return result;
    }

    if (decision.primary == TransportChannel::P2P) {
        QString p2pError;
        result.channelUsed = TransportChannel::P2P;
        GroupFanOutDeliveryResult p2pDelivery;
        if (sendViaP2P(request, envelopes, true, &p2pDelivery, &p2pError)) {
            result.accepted = p2pDelivery.accepted();
            result.writtenRecipientCount = p2pDelivery.writtenCount;
            result.queuedRecipientCount = p2pDelivery.queuedCount;
            result.failedRecipientCount = p2pDelivery.failedCount;
            result.success = p2pDelivery.allWritten();
            return result;
        }
        result.errorMessage =
            firstNonEmpty({p2pError, QStringLiteral("p2p group message send failed")});
        return result;
    }

    result.errorMessage = QStringLiteral("no available group message transport");
    return result;
}

bool ReliableGroupMessageSender::validateEnvelopes(
    const ReliableGroupMessageSendRequest& request,
    ValidatedEnvelopes* out,
    QString* errorMessage) const
{
    if (!out) {
        return false;
    }

    const QString localClientId = m_localClientId.trimmed();
    const QString groupId = request.groupId.trimmed();
    const QString body = request.body.trimmed();
    if (!m_repository || localClientId.isEmpty() || groupId.isEmpty()
        || body.isEmpty() || request.envelopes.empty()) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("local client, repository, group, body, and envelopes are required");
        }
        return false;
    }

    QString messageId;
    QVector<QString> recipientIds;
    QSet<QString> seenRecipients;
    MessageEnvelope firstEnvelope;
    bool hasFirstEnvelope = false;

    for (const MessageEnvelope& envelope : request.envelopes) {
        if (envelope.type != MessageType::GroupMessage) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("group text envelopes must use GroupMessage type");
            }
            return false;
        }

        const QString envelopeMessageId = fromEnvelopeUtf8(envelope.messageId);
        const QString senderId = fromEnvelopeUtf8(envelope.senderId);
        const QString targetId = fromEnvelopeUtf8(envelope.targetId);
        const QString envelopeGroupId = fromEnvelopeUtf8(envelope.conversationId);
        if (envelopeMessageId.isEmpty() || senderId != localClientId
            || targetId.isEmpty() || envelopeGroupId != groupId
            || envelope.body.empty()) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("group text envelope has inconsistent message, sender, target, group, or body");
            }
            return false;
        }

        if (messageId.isEmpty()) {
            messageId = envelopeMessageId;
        } else if (messageId != envelopeMessageId) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("group text envelopes must share one message id");
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
            *errorMessage = QStringLiteral("group text envelopes have no recipients");
        }
        return false;
    }

    out->messageId = messageId;
    out->recipientIds = recipientIds;
    out->selfEnvelope = std::move(firstEnvelope);
    return true;
}

bool ReliableGroupMessageSender::persistLocalPendingMessage(
    const ReliableGroupMessageSendRequest& request,
    const ValidatedEnvelopes& envelopes,
    QString* errorMessage) const
{
    ChatMessage existing;
    if (m_repository->findMessageById(envelopes.messageId, &existing)) {
        const QString existingConversationId =
            QString::fromStdWString(existing.conversationId).trimmed();
        const QString existingSenderId =
            QString::fromStdWString(existing.senderId).trimmed();
        if (existingConversationId == request.groupId.trimmed()
            && existingSenderId == m_localClientId.trimmed()) {
            return true;
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("group message id already belongs to another message");
        }
        return false;
    }

    if (!ChatService::storeIncomingGroupEnvelope(
            m_repository,
            envelopes.selfEnvelope,
            request.groupId.trimmed(),
            groupTitleOrFallback(request.groupTitle, request.groupId))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to persist local group message");
        }
        return false;
    }

    if (!m_repository->updateDeliveryState(envelopes.messageId,
                                           MessageDeliveryState::Pending)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to mark group message pending");
        }
        return false;
    }
    return true;
}

ReliableGroupMessageSender::ServiceSendStatus
ReliableGroupMessageSender::sendViaMessageService(
    const ReliableGroupMessageSendRequest& request,
    const ValidatedEnvelopes& envelopes,
    QString* errorMessage) const
{
    if (!m_serverClient) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("message service client is not configured");
        }
        return ServiceSendStatus::FailedBeforeAck;
    }

    ServerMessageDraft draft;
    draft.clientMessageId = envelopes.messageId;
    draft.conversationId = request.groupId.trimmed();
    draft.workspaceId = request.settings.workspaceId.trimmed();
    draft.type = QStringLiteral("chat_text");
    draft.body = request.body.trimmed();
    draft.contentType = QStringLiteral("html");
    draft.recipientIds = envelopes.recipientIds;
    draft.payload.insert(QStringLiteral("group_id"), draft.conversationId);
    draft.payload.insert(QStringLiteral("message_kind"), QStringLiteral("text"));
    draft.replyToMessageId =
        fromEnvelopeUtf8(envelopes.selfEnvelope.replyToMessageId);
    const QString replyToSenderId =
        fromEnvelopeUtf8(envelopes.selfEnvelope.replyToSenderId);
    if (!replyToSenderId.isEmpty()) {
        draft.payload.insert(QStringLiteral("reply_to_sender_id"), replyToSenderId);
    }
    if (!envelopes.selfEnvelope.replyToBody.empty()) {
        draft.payload.insert(
            QStringLiteral("reply_to_body"),
            QString::fromUtf8(envelopes.selfEnvelope.replyToBody.data(),
                              static_cast<int>(envelopes.selfEnvelope.replyToBody.size())));
    }
    if (!envelopes.selfEnvelope.mentionedIds.empty()) {
        QJsonArray mentionedIds;
        for (const std::string& mentionedId : envelopes.selfEnvelope.mentionedIds) {
            const QString normalized = fromEnvelopeUtf8(mentionedId);
            if (!normalized.isEmpty()) {
                mentionedIds.push_back(normalized);
            }
        }
        if (!mentionedIds.isEmpty()) {
            draft.payload.insert(QStringLiteral("mentioned_ids"), mentionedIds);
        }
    }

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

    if (!m_repository->saveRemoteMessageIdMapping(ack->serverMessageId,
                                                  envelopes.messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to save message service id mapping");
        }
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }
    if (!ChatService::markMessageServerAcked(m_repository, envelopes.messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to mark group message server acknowledged");
        }
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }
    return ServiceSendStatus::Succeeded;
}

bool ReliableGroupMessageSender::sendViaP2P(
    const ReliableGroupMessageSendRequest& request,
    const ValidatedEnvelopes& envelopes,
    bool updateDeliveryState,
    GroupFanOutDeliveryResult* deliveryResult,
    QString* errorMessage) const
{
    if (!m_p2pSendCallback && !m_p2pStatusCallback) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("p2p send callback is not configured");
        }
        return false;
    }

    ReliableGroupMessageP2PRequest p2pRequest;
    p2pRequest.messageId = envelopes.messageId;
    p2pRequest.groupId = request.groupId.trimmed();
    p2pRequest.groupTitle = groupTitleOrFallback(request.groupTitle, request.groupId);
    p2pRequest.body = request.body.trimmed();
    p2pRequest.envelopes = request.envelopes;
    p2pRequest.acceptQueuedOnlyDelivery = !updateDeliveryState;

    QString callbackError;
    GroupFanOutDeliveryResult localDelivery;
    if (m_p2pStatusCallback) {
        localDelivery = m_p2pStatusCallback(p2pRequest, &callbackError);
    } else {
        const bool sent = m_p2pSendCallback
            && m_p2pSendCallback(p2pRequest, &callbackError);
        localDelivery.attemptedCount = static_cast<int>(envelopes.recipientIds.size());
        if (sent) {
            localDelivery.writtenCount = localDelivery.attemptedCount;
            localDelivery.deliveredCount = localDelivery.writtenCount;
        } else {
            localDelivery.failedCount = localDelivery.attemptedCount;
        }
    }
    if (deliveryResult) {
        *deliveryResult = localDelivery;
    }
    if (!localDelivery.accepted()) {
        if (errorMessage) {
            *errorMessage =
                firstNonEmpty({callbackError, QStringLiteral("p2p send failed")});
        }
        return false;
    }

    if (updateDeliveryState && localDelivery.allWritten()
        && !ChatService::markMessageSent(m_repository, envelopes.messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to mark p2p group message sent");
        }
        return false;
    }
    return true;
}
