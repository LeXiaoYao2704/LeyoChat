#include "services/ReliableDirectEnvelopeSender.h"

#include <initializer_list>
#include <optional>
#include <utility>

#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScopeGuard>

#include "integrations/ServerMessageClient.h"

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

QString serviceTypeFor(const MessageEnvelope& envelope)
{
    const QString subtype = fromEnvelopeUtf8(envelope.messageSubtype);
    if (envelope.type == MessageType::MessageMutation) {
        return QStringLiteral("message_mutation");
    }
    if (envelope.type == MessageType::MessageReaction) {
        return QStringLiteral("message_reaction");
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

QString bodyFor(const MessageEnvelope& envelope)
{
    if (envelope.type == MessageType::MessageMutation
        || envelope.type == MessageType::MessageReaction) {
        return {};
    }
    return QString::fromUtf8(envelope.body.data(), static_cast<int>(envelope.body.size()));
}

QString contentTypeFor(const MessageEnvelope& envelope)
{
    if (envelope.type == MessageType::MessageMutation) {
        return QStringLiteral("mutation");
    }
    if (envelope.type == MessageType::MessageReaction) {
        return QStringLiteral("reaction");
    }
    const QString direct = fromEnvelopeUtf8(envelope.contentType);
    if (!direct.isEmpty()) {
        return direct;
    }
    const QJsonObject body = objectFromJsonString(envelope.body);
    const QString fromBody = body.value(QStringLiteral("content_type")).toString().trimmed();
    return fromBody.isEmpty() ? QStringLiteral("plain") : fromBody;
}

QJsonObject payloadFor(const MessageEnvelope& envelope)
{
    QJsonObject payload = objectFromJsonString(envelope.payloadJson);
    if (!payload.isEmpty()) {
        return payload;
    }
    return objectFromJsonString(envelope.body);
}

}  // namespace

ReliableDirectEnvelopeSender::ReliableDirectEnvelopeSender(
    QString localClientId,
    const IServerMessageClient* serverClient,
    P2PSendCallback p2pSendCallback)
    : m_localClientId(std::move(localClientId))
    , m_serverClient(serverClient)
    , m_p2pSendCallback(std::move(p2pSendCallback))
{
}

ReliableDirectEnvelopeSendResult ReliableDirectEnvelopeSender::send(
    const ReliableDirectEnvelopeSendRequest& request) const
{
    ReliableDirectEnvelopeSendResult result;
    QString validationError;
    if (!validateEnvelope(request.envelope, &validationError)) {
        result.errorMessage =
            firstNonEmpty({validationError, QStringLiteral("direct envelope request is invalid")});
        return result;
    }
    result.messageId = fromEnvelopeUtf8(request.envelope.messageId);
    const QString conversationId = fromEnvelopeUtf8(request.envelope.conversationId);
    const QString targetId = fromEnvelopeUtf8(request.envelope.targetId);
    [[maybe_unused]] const auto routeResultLog = qScopeGuard([&]() {
        qInfo().noquote()
            << "[route-result] type=direct-envelope"
            << "msgId=" << result.messageId.left(8)
            << "serverMessageId=" << result.serverMessageId.left(8)
            << "conversationId=" << conversationId
            << "target=" << targetId.left(8)
            << "success=" << result.success
            << "channel=" << transportChannelName(result.channelUsed)
            << "error=" << result.errorMessage;
    });

    TransportPolicyInput input;
    input.mode = request.settings.mode;
    input.serviceConfigured = request.settings.canUseMessageService();
    input.serviceReachable = request.serviceReachable;
    input.receiverServerCapable = request.receiverServerCapable;
    input.p2pAvailable = request.p2pAvailable;
    input.allowP2PFallback = request.settings.allowP2PFallback;
    const TransportDecision decision = TransportPolicy::chooseDirectTextChannel(input);
    qInfo().noquote()
        << "[route-decision] type=direct-envelope"
        << "msgId=" << result.messageId.left(8)
        << "conversationId=" << conversationId
        << "target=" << targetId.left(8)
        << "envelopeType=" << static_cast<int>(request.envelope.type)
        << "mode=" << transportModeName(request.settings.mode)
        << "serviceConfigured=" << input.serviceConfigured
        << "serviceReachable=" << input.serviceReachable
        << "receiverServerCapable=" << input.receiverServerCapable
        << "p2pAvailable=" << input.p2pAvailable
        << "selected=" << transportChannelName(decision.primary)
        << "mayFallbackToP2P=" << decision.mayFallbackToP2P;

    if (decision.primary == TransportChannel::MessageService) {
        QString serviceError;
        QString serverMessageId;
        result.channelUsed = TransportChannel::MessageService;
        const ServiceSendStatus serviceStatus =
            sendViaMessageService(request, &serverMessageId, &serviceError);
        if (serviceStatus == ServiceSendStatus::Succeeded) {
            result.success = true;
            result.serverMessageId = serverMessageId;
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
            if (sendViaP2P(request.envelope, &p2pError)) {
                result.success = true;
                result.channelUsed = TransportChannel::P2P;
                return result;
            }
            result.errorMessage =
                firstNonEmpty({p2pError, serviceError, QStringLiteral("direct envelope send failed")});
            return result;
        }
        result.errorMessage =
            firstNonEmpty({serviceError, QStringLiteral("message service send failed")});
        return result;
    }

    if (decision.primary == TransportChannel::P2P) {
        QString p2pError;
        result.channelUsed = TransportChannel::P2P;
        if (sendViaP2P(request.envelope, &p2pError)) {
            result.success = true;
            return result;
        }
        result.errorMessage =
            firstNonEmpty({p2pError, QStringLiteral("p2p direct envelope send failed")});
        return result;
    }

    result.errorMessage = QStringLiteral("no available direct envelope transport");
    return result;
}

bool ReliableDirectEnvelopeSender::validateEnvelope(
    const MessageEnvelope& envelope,
    QString* errorMessage) const
{
    const QString messageId = fromEnvelopeUtf8(envelope.messageId);
    const QString senderId = fromEnvelopeUtf8(envelope.senderId);
    const QString targetId = fromEnvelopeUtf8(envelope.targetId);
    const QString conversationId = fromEnvelopeUtf8(envelope.conversationId);
    const bool payloadOnly =
        envelope.type == MessageType::MessageMutation
        || envelope.type == MessageType::MessageReaction;
    const bool supported =
        envelope.type == MessageType::ChatText
        || envelope.type == MessageType::ResourceReference
        || payloadOnly;
    const bool hasContent =
        payloadOnly ? !envelope.payloadJson.empty() : !envelope.body.empty();

    if (!supported || messageId.isEmpty()
        || senderId != m_localClientId.trimmed()
        || targetId.isEmpty() || conversationId.isEmpty()
        || !hasContent) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("direct envelope has inconsistent message, sender, target, conversation, or content");
        }
        return false;
    }
    return true;
}

ReliableDirectEnvelopeSender::ServiceSendStatus
ReliableDirectEnvelopeSender::sendViaMessageService(
    const ReliableDirectEnvelopeSendRequest& request,
    QString* outServerMessageId,
    QString* errorMessage) const
{
    if (!m_serverClient) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("message service client is not configured");
        }
        return ServiceSendStatus::FailedBeforeAck;
    }

    const MessageEnvelope& envelope = request.envelope;
    ServerMessageDraft draft;
    draft.clientMessageId = fromEnvelopeUtf8(envelope.messageId);
    draft.conversationId = fromEnvelopeUtf8(envelope.conversationId);
    draft.workspaceId = request.settings.workspaceId.trimmed();
    draft.type = serviceTypeFor(envelope);
    draft.body = bodyFor(envelope);
    draft.payload = payloadFor(envelope);
    draft.contentType = contentTypeFor(envelope);
    draft.replyToMessageId = fromEnvelopeUtf8(envelope.replyToMessageId);
    draft.recipientIds.push_back(fromEnvelopeUtf8(envelope.targetId));

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
    if (outServerMessageId) {
        *outServerMessageId = ack->serverMessageId.trimmed();
    }
    return ServiceSendStatus::Succeeded;
}

bool ReliableDirectEnvelopeSender::sendViaP2P(
    const MessageEnvelope& envelope,
    QString* errorMessage) const
{
    if (!m_p2pSendCallback) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("p2p send callback is not configured");
        }
        return false;
    }

    ReliableDirectEnvelopeP2PRequest request;
    request.messageId = fromEnvelopeUtf8(envelope.messageId);
    request.envelope = envelope;

    QString callbackError;
    if (!m_p2pSendCallback(request, &callbackError)) {
        if (errorMessage) {
            *errorMessage =
                firstNonEmpty({callbackError, QStringLiteral("p2p send failed")});
        }
        return false;
    }
    return true;
}
