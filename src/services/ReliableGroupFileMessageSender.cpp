#include "services/ReliableGroupFileMessageSender.h"

#include <initializer_list>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QScopeGuard>
#include <QSet>

#include "domain/ChatMessage.h"
#include "domain/ConversationSummary.h"
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

QString compactJson(const QJsonObject& object)
{
    if (object.isEmpty()) {
        return {};
    }

    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(bytes);
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

ReliableGroupFileMessageSender::ReliableGroupFileMessageSender(
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

ReliableGroupFileMessageSendResult ReliableGroupFileMessageSender::sendFileCard(
    const ReliableGroupFileMessageSendRequest& request) const
{
    ReliableGroupFileMessageSendResult result;
    ValidatedEnvelopes envelopes;
    QString validationError;
    if (!validateEnvelopes(request, &envelopes, &validationError)) {
        result.errorMessage =
            firstNonEmpty({validationError, QStringLiteral("group file message request is invalid")});
        return result;
    }
    result.messageId = envelopes.messageId;
    [[maybe_unused]] const auto routeResultLog = qScopeGuard([&]() {
        qInfo().noquote()
            << "[route-result] type=group-file-card"
            << "msgId=" << result.messageId.left(8)
            << "fileId=" << request.fileId.trimmed().left(8)
            << "groupId=" << request.groupId.trimmed()
            << "recipients=" << envelopes.recipientIds.size()
            << "success=" << result.success
            << "channel=" << transportChannelName(result.channelUsed)
            << "error=" << result.errorMessage;
    });

    QString persistError;
    if (!persistLocalPendingMessage(request, envelopes, &persistError)) {
        result.errorMessage =
            firstNonEmpty({persistError, QStringLiteral("failed to create local group file message")});
        return result;
    }

    if (envelopes.recipientIds.isEmpty() && request.envelopes.empty()) {
        if (!m_repository->updateDeliveryState(envelopes.messageId,
                                               MessageDeliveryState::Sent)) {
            result.errorMessage =
                QStringLiteral("failed to mark local group file message sent");
            return result;
        }
        result.success = true;
        result.channelUsed = TransportChannel::None;
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
                QStringLiteral("group file recipient partition has no matching recipients");
            return result;
        }
        const TransportChannel partitionChannel =
            !serverRecipients.isEmpty() && !p2pRecipients.isEmpty()
                ? TransportChannel::Mixed
                : (!serverRecipients.isEmpty() ? TransportChannel::MessageService
                                               : TransportChannel::P2P);
        qInfo().noquote()
            << "[route-decision] type=group-file-card partitioned=true"
            << "msgId=" << result.messageId.left(8)
            << "fileId=" << request.fileId.trimmed().left(8)
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
                QStringLiteral("message service is not available for partitioned group file recipients");
            return result;
        }
        if (!p2pRecipients.isEmpty()
            && (request.settings.mode == RemoteChatTransportMode::ServerOnly
                || !request.settings.allowP2PFallback
                || !request.p2pAvailable)) {
            result.errorMessage =
                QStringLiteral("p2p is not available for partitioned group file recipients");
            return result;
        }

        bool usedService = false;
        bool usedP2P = false;
        QString p2pError;
        if (!p2pRecipients.isEmpty()) {
            ReliableGroupFileMessageSendRequest p2pRequest = request;
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
                    ReliableGroupFileMessageSendRequest fallbackRequest = request;
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
                                       QStringLiteral("group file message send failed")});
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
        TransportPolicy::chooseGroupFileMessageChannel(policyInput);
    qInfo().noquote()
        << "[route-decision] type=group-file-card partitioned=false"
        << "msgId=" << result.messageId.left(8)
        << "fileId=" << request.fileId.trimmed().left(8)
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
                firstNonEmpty({p2pError, serviceError, QStringLiteral("group file message send failed")});
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
            firstNonEmpty({p2pError, QStringLiteral("p2p group file message send failed")});
        return result;
    }

    result.errorMessage = QStringLiteral("no available group file message transport");
    return result;
}

bool ReliableGroupFileMessageSender::validateEnvelopes(
    const ReliableGroupFileMessageSendRequest& request,
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
        || body.isEmpty() || request.localFileCard.isEmpty()
        || request.broadcastFileCard.isEmpty()) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("local client, repository, group, file card, and body are required");
        }
        return false;
    }

    const QString requestedMessageId = request.messageId.trimmed();
    if (request.envelopes.empty()) {
        if (requestedMessageId.isEmpty()) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("group file message id is required without recipients");
            }
            return false;
        }

        out->messageId = requestedMessageId;
        out->selfEnvelope.createdAtMs = request.createdAtMs;
        return true;
    }

    QString messageId;
    QVector<QString> recipientIds;
    QSet<QString> seenRecipients;
    MessageEnvelope firstEnvelope;
    bool hasFirstEnvelope = false;

    for (const MessageEnvelope& envelope : request.envelopes) {
        if (envelope.type != MessageType::GroupMessage
            || envelope.messageSubtype != "group_file_card") {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("group file envelopes must use GroupMessage group_file_card type");
            }
            return false;
        }

        const QString envelopeMessageId = fromEnvelopeUtf8(envelope.messageId);
        const QString senderId = fromEnvelopeUtf8(envelope.senderId);
        const QString targetId = fromEnvelopeUtf8(envelope.targetId);
        const QString envelopeGroupId = fromEnvelopeUtf8(envelope.conversationId);
        if (envelopeMessageId.isEmpty() || senderId != localClientId
            || targetId.isEmpty() || envelopeGroupId != groupId
            || envelope.body.empty() || envelope.payloadJson.empty()) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("group file envelope has inconsistent message, sender, target, group, body, or payload");
            }
            return false;
        }

        if (messageId.isEmpty()) {
            messageId = envelopeMessageId;
        } else if (messageId != envelopeMessageId) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("group file envelopes must share one message id");
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

    if (!requestedMessageId.isEmpty() && requestedMessageId != messageId) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("group file request message id does not match envelopes");
        }
        return false;
    }

    if (messageId.isEmpty() || recipientIds.isEmpty() || !hasFirstEnvelope) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("group file envelopes have no recipients");
        }
        return false;
    }

    out->messageId = messageId;
    out->recipientIds = recipientIds;
    out->selfEnvelope = std::move(firstEnvelope);
    return true;
}

bool ReliableGroupFileMessageSender::persistLocalPendingMessage(
    const ReliableGroupFileMessageSendRequest& request,
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
            *errorMessage =
                QStringLiteral("group file message id already belongs to another message");
        }
        return false;
    }

    const qint64 createdAtMs =
        envelopes.selfEnvelope.createdAtMs > 0
            ? envelopes.selfEnvelope.createdAtMs
            : (request.createdAtMs > 0
                   ? request.createdAtMs
                   : QDateTime::currentMSecsSinceEpoch());

    ChatMessage message;
    message.messageId = envelopes.messageId.toStdWString();
    message.conversationId = request.groupId.trimmed().toStdWString();
    message.senderId = m_localClientId.trimmed().toStdWString();
    message.body = request.body.trimmed().toStdWString();
    message.createdAtMs = createdAtMs;
    message.deliveryState = MessageDeliveryState::Pending;
    message.localFilePath = request.localFilePath.trimmed().toStdWString();
    message.messageType = QStringLiteral("group_file_card").toStdWString();
    message.fileCardJson = compactJson(request.localFileCard).toStdWString();

    if (!m_repository->appendMessage(message, createdAtMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to persist local group file message");
        }
        return false;
    }

    if (!m_repository->upsertConversationWithType(
            ConversationSummary{
                request.groupId.trimmed().toStdWString(),
                groupTitleOrFallback(request.groupTitle, request.groupId).toStdWString(),
                request.body.trimmed().toStdWString(),
                createdAtMs
            },
            QStringLiteral("group"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to update group conversation summary");
        }
        return false;
    }
    return true;
}

ReliableGroupFileMessageSender::ServiceSendStatus
ReliableGroupFileMessageSender::sendViaMessageService(
    const ReliableGroupFileMessageSendRequest& request,
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
    draft.type = QStringLiteral("group_file_card");
    draft.body = request.body.trimmed();
    draft.payload = request.broadcastFileCard;
    if (draft.payload.value(QStringLiteral("channel")).toString().trimmed()
        != QStringLiteral("p2p")) {
        draft.payload.remove(QStringLiteral("sender_file_path"));
    }
    draft.fileId = request.fileId.trimmed();
    if (draft.fileId.isEmpty()) {
        draft.fileId = draft.payload.value(QStringLiteral("file_id")).toString().trimmed();
    }
    draft.contentType =
        QStringLiteral("application/vnd.leyochat.group-file-card+json");
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

    if (!m_repository->saveRemoteMessageIdMapping(ack->serverMessageId,
                                                  envelopes.messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to save message service id mapping");
        }
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }
    if (!m_repository->updateDeliveryState(envelopes.messageId,
                                           MessageDeliveryState::ServerAcked)) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("failed to mark group file message server acknowledged");
        }
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }
    return ServiceSendStatus::Succeeded;
}

bool ReliableGroupFileMessageSender::sendViaP2P(
    const ReliableGroupFileMessageSendRequest& request,
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

    ReliableGroupFileMessageP2PRequest p2pRequest;
    p2pRequest.messageId = envelopes.messageId;
    p2pRequest.groupId = request.groupId.trimmed();
    p2pRequest.groupTitle = groupTitleOrFallback(request.groupTitle, request.groupId);
    p2pRequest.body = request.body.trimmed();
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
        && !m_repository->updateDeliveryState(envelopes.messageId,
                                              MessageDeliveryState::Sent)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to mark p2p group file message sent");
        }
        return false;
    }
    return true;
}
