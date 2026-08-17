#include "services/ReliableDirectMessageSender.h"

#include <initializer_list>
#include <exception>
#include <optional>
#include <utility>

#include <QDebug>
#include <QScopeGuard>

#include "integrations/ServerMessageClient.h"
#include "services/ChatService.h"
#include "services/DirectConversationAddressing.h"
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

}  // namespace

qint64 reliableDirectMessageRetryDelayMs(int consecutiveFailures,
                                         qint64 baseDelayMs,
                                         qint64 maxDelayMs)
{
    if (consecutiveFailures <= 0 || baseDelayMs <= 0 || maxDelayMs <= 0) {
        return 0;
    }

    qint64 delay = qMin(baseDelayMs, maxDelayMs);
    for (int i = 1; i < consecutiveFailures && delay < maxDelayMs; ++i) {
        if (delay > maxDelayMs / 2) {
            delay = maxDelayMs;
        } else {
            delay = qMin(maxDelayMs, delay * 2);
        }
    }
    return delay;
}

ReliableDirectMessageSender::ReliableDirectMessageSender(
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

ReliableDirectMessageSendResult ReliableDirectMessageSender::sendText(
    const ReliableDirectMessageSendRequest& request) const
{
    ReliableDirectMessageSendResult result;
    const QString trimmedConversationId = request.conversationId.trimmed();
    const QString trimmedTargetId = request.targetId.trimmed();
    const QString trimmedBody = request.body.trimmed();
    if (!m_repository || m_localClientId.trimmed().isEmpty()
        || trimmedConversationId.isEmpty() || trimmedTargetId.isEmpty()
        || trimmedBody.isEmpty()) {
        result.errorMessage =
            QStringLiteral("local client, repository, conversation, target, and body are required");
        return result;
    }

    qInfo().noquote()
        << "[direct-text] create-local-begin"
        << "conversationId=" << trimmedConversationId
        << "target=" << trimmedTargetId.left(8)
        << "bodyLen=" << trimmedBody.size()
        << "replyMsgLen=" << request.replyToMessageId.size()
        << "replySenderLen=" << request.replyToSenderId.size()
        << "replyBodyLen=" << request.replyToBody.size();
    result.messageId = ChatService::createOutgoingMessage(
        m_localClientId,
        m_repository,
        trimmedConversationId,
        trimmedTargetId,
        trimmedBody,
        request.replyToMessageId,
        request.replyToSenderId,
        request.replyToBody);
    if (result.messageId.isEmpty()) {
        result.errorMessage = QStringLiteral("failed to create local message");
        return result;
    }

    return sendPersistedText(result.messageId, request);
}

ReliableDirectMessageSendResult ReliableDirectMessageSender::retryText(
    const QString& messageId,
    const ReliableDirectMessageSendRequest& request) const
{
    ReliableDirectMessageSendResult result;
    result.messageId = messageId.trimmed();
    if (!m_repository || m_localClientId.trimmed().isEmpty()
        || result.messageId.isEmpty()) {
        result.errorMessage =
            QStringLiteral("local client, repository, and message id are required");
        return result;
    }

    ChatMessage stored;
    if (!m_repository->findMessageById(result.messageId, &stored)) {
        result.errorMessage = QStringLiteral("persisted message was not found");
        return result;
    }

    const QString storedConversationId =
        QString::fromStdWString(stored.conversationId).trimmed();
    const QString storedSenderId = QString::fromStdWString(stored.senderId).trimmed();
    const QString storedMessageType =
        QString::fromStdWString(stored.messageType).trimmed();
    const QString expectedTargetId =
        DirectConversationAddressing::otherParticipant(
            m_localClientId,
            storedConversationId);
    if (storedSenderId != m_localClientId.trimmed()
        || storedConversationId != request.conversationId.trimmed()
        || storedMessageType != QStringLiteral("text")
        || (!expectedTargetId.isEmpty()
            && expectedTargetId != request.targetId.trimmed())) {
        result.errorMessage =
            QStringLiteral("persisted message does not match the local direct text request");
        return result;
    }

    ReliableDirectMessageSendRequest persistedRequest = request;
    persistedRequest.conversationId = storedConversationId;
    persistedRequest.body = QString::fromStdWString(stored.body);
    persistedRequest.replyToMessageId =
        QString::fromStdWString(stored.replyToMessageId);
    persistedRequest.replyToSenderId =
        QString::fromStdWString(stored.replyToSenderId);
    persistedRequest.replyToBody = QString::fromStdWString(stored.replyToBody);
    return sendPersistedText(result.messageId, persistedRequest);
}

ReliableDirectMessageSendResult ReliableDirectMessageSender::sendPersistedText(
    const QString& messageId,
    const ReliableDirectMessageSendRequest& request) const
{
    ReliableDirectMessageSendResult result;
    result.messageId = messageId.trimmed();
    const QString trimmedConversationId = request.conversationId.trimmed();
    const QString trimmedTargetId = request.targetId.trimmed();
    const QString trimmedBody = request.body.trimmed();
    [[maybe_unused]] const auto routeResultLog = qScopeGuard([&]() {
        qInfo().noquote()
            << "[route-result] type=direct-text"
            << "msgId=" << result.messageId.left(8)
            << "conversationId=" << trimmedConversationId
            << "target=" << trimmedTargetId.left(8)
            << "success=" << result.success
            << "channel=" << transportChannelName(result.channelUsed)
            << "error=" << result.errorMessage;
    });
    if (!m_repository || m_localClientId.trimmed().isEmpty()
        || result.messageId.isEmpty() || trimmedConversationId.isEmpty()
        || trimmedTargetId.isEmpty() || trimmedBody.isEmpty()) {
        result.errorMessage = QStringLiteral(
            "local client, repository, message, conversation, target, and body are required");
        return result;
    }

    TransportPolicyInput policyInput;
    policyInput.mode = request.settings.mode;
    policyInput.serviceConfigured = request.settings.canUseMessageService();
    policyInput.serviceReachable = request.serviceReachable;
    policyInput.receiverServerCapable = request.receiverServerCapable;
    policyInput.p2pAvailable = request.p2pAvailable;
    policyInput.allowP2PFallback = request.settings.allowP2PFallback;
    const TransportDecision decision =
        TransportPolicy::chooseDirectTextChannel(policyInput);
    qInfo().noquote()
        << "[route-decision] type=direct-text"
        << "msgId=" << result.messageId.left(8)
        << "conversationId=" << trimmedConversationId
        << "target=" << trimmedTargetId.left(8)
        << "mode=" << transportModeName(request.settings.mode)
        << "serviceConfigured=" << policyInput.serviceConfigured
        << "serviceReachable=" << policyInput.serviceReachable
        << "receiverServerCapable=" << policyInput.receiverServerCapable
        << "p2pAvailable=" << policyInput.p2pAvailable
        << "selected=" << transportChannelName(decision.primary)
        << "mayFallbackToP2P=" << decision.mayFallbackToP2P;

    if (decision.primary == TransportChannel::MessageService) {
        QString serviceError;
        result.channelUsed = TransportChannel::MessageService;
        const ServiceSendStatus serviceStatus =
            sendViaMessageService(request, result.messageId, &serviceError);
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
            if (sendViaP2P(request, result.messageId, &p2pError)) {
                result.success = true;
                result.channelUsed = TransportChannel::P2P;
                return result;
            }
            result.errorMessage =
                firstNonEmpty({p2pError, serviceError, QStringLiteral("direct message send failed")});
            return result;
        }

        result.errorMessage =
            firstNonEmpty({serviceError, QStringLiteral("message service send failed")});
        return result;
    }

    if (decision.primary == TransportChannel::P2P) {
        QString p2pError;
        result.channelUsed = TransportChannel::P2P;
        if (sendViaP2P(request, result.messageId, &p2pError)) {
            result.success = true;
            return result;
        }
        result.errorMessage =
            firstNonEmpty({p2pError, QStringLiteral("p2p direct message send failed")});
        return result;
    }

    result.errorMessage = QStringLiteral("no available direct message transport");
    return result;
}

ReliableDirectMessageSender::ServiceSendStatus
ReliableDirectMessageSender::sendViaMessageService(
    const ReliableDirectMessageSendRequest& request,
    const QString& messageId,
    QString* errorMessage) const
{
    if (!m_serverClient) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("message service client is not configured");
        }
        return ServiceSendStatus::FailedBeforeAck;
    }

    ServerMessageDraft draft;
    draft.clientMessageId = messageId;
    draft.conversationId = request.conversationId.trimmed();
    draft.workspaceId = request.settings.workspaceId.trimmed();
    draft.type = QStringLiteral("chat_text");
    draft.body = request.body.trimmed();
    draft.contentType = QStringLiteral("html");
    draft.replyToMessageId = request.replyToMessageId.trimmed();
    if (!request.replyToSenderId.trimmed().isEmpty()) {
        draft.payload.insert(QStringLiteral("reply_to_sender_id"),
                             request.replyToSenderId.trimmed());
    }
    if (!request.replyToBody.isEmpty()) {
        draft.payload.insert(QStringLiteral("reply_to_body"), request.replyToBody);
    }
    draft.recipientIds.push_back(request.targetId.trimmed());

    QString sendError;
    std::optional<ServerMessageAck> ack;
    try {
        ack = m_serverClient->sendMessage(draft, &sendError);
    } catch (const std::exception& e) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("message service send exception: %1")
                                .arg(QString::fromLocal8Bit(e.what()));
        }
        qWarning().noquote()
            << "[direct-text] message service exception msgId="
            << messageId.left(8)
            << "target=" << request.targetId.left(8)
            << "what=" << e.what();
        return ServiceSendStatus::FailedBeforeAck;
    } catch (...) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("message service send unknown exception");
        }
        qWarning().noquote()
            << "[direct-text] message service unknown exception msgId="
            << messageId.left(8)
            << "target=" << request.targetId.left(8);
        return ServiceSendStatus::FailedBeforeAck;
    }
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
                                                  messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to save message service id mapping");
        }
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }
    if (!ChatService::markMessageServerAcked(m_repository, messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to mark message server acknowledged");
        }
        return ServiceSendStatus::AcceptedButLocalFinalizeFailed;
    }
    return ServiceSendStatus::Succeeded;
}

bool ReliableDirectMessageSender::sendViaP2P(
    const ReliableDirectMessageSendRequest& request,
    const QString& messageId,
    QString* errorMessage) const
{
    if (!m_p2pSendCallback) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("p2p send callback is not configured");
        }
        return false;
    }

    ReliableDirectMessageP2PRequest p2pRequest;
    p2pRequest.messageId = messageId;
    p2pRequest.conversationId = request.conversationId.trimmed();
    p2pRequest.targetId = request.targetId.trimmed();
    p2pRequest.body = request.body.trimmed();
    p2pRequest.replyToMessageId = request.replyToMessageId.trimmed();
    p2pRequest.replyToSenderId = request.replyToSenderId.trimmed();
    p2pRequest.replyToBody = request.replyToBody;

    QString callbackError;
    bool accepted = false;
    try {
        accepted = m_p2pSendCallback(p2pRequest, &callbackError);
    } catch (const std::exception& e) {
        callbackError = QStringLiteral("p2p send exception: %1")
                            .arg(QString::fromLocal8Bit(e.what()));
        qWarning().noquote()
            << "[direct-text] p2p exception msgId=" << messageId.left(8)
            << "target=" << request.targetId.left(8)
            << "what=" << e.what();
    } catch (...) {
        callbackError = QStringLiteral("p2p send unknown exception");
        qWarning().noquote()
            << "[direct-text] p2p unknown exception msgId="
            << messageId.left(8)
            << "target=" << request.targetId.left(8);
    }
    if (!accepted) {
        if (errorMessage) {
            *errorMessage =
                firstNonEmpty({callbackError, QStringLiteral("p2p send failed")});
        }
        return false;
    }

    if (request.requireP2PDeliveryReceipt) {
        return true;
    }

    if (!ChatService::markMessageSent(m_repository, messageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to mark p2p message sent");
        }
        return false;
    }
    return true;
}
