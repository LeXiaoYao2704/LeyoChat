#pragma once

#include <functional>

#include <QString>

#include "integrations/RemoteChatServiceSettings.h"
#include "services/TransportPolicy.h"

class ConversationRepository;
class IServerMessageClient;

struct ReliableDirectMessageSendRequest {
    QString conversationId;
    QString targetId;
    QString body;
    QString replyToMessageId;
    QString replyToSenderId;
    QString replyToBody;
    RemoteChatServiceSettings settings;
    bool serviceReachable = false;
    bool receiverServerCapable = false;
    bool p2pAvailable = false;
    // P2P socket write only means bytes entered the local socket buffer. When
    // true, keep the durable outbox row pending until ReceiptReceived proves
    // that the peer stored the message.
    bool requireP2PDeliveryReceipt = false;
};

struct ReliableDirectMessageP2PRequest {
    QString messageId;
    QString conversationId;
    QString targetId;
    QString body;
    QString replyToMessageId;
    QString replyToSenderId;
    QString replyToBody;
};

struct ReliableDirectMessageSendResult {
    bool success = false;
    QString messageId;
    TransportChannel channelUsed = TransportChannel::None;
    QString errorMessage;
};

qint64 reliableDirectMessageRetryDelayMs(int consecutiveFailures,
                                         qint64 baseDelayMs = 30000,
                                         qint64 maxDelayMs = 300000);

class ReliableDirectMessageSender {
public:
    using P2PSendCallback =
        std::function<bool(const ReliableDirectMessageP2PRequest&, QString*)>;

    ReliableDirectMessageSender(QString localClientId,
                                ConversationRepository* repository,
                                const IServerMessageClient* serverClient,
                                P2PSendCallback p2pSendCallback);

    ReliableDirectMessageSendResult sendText(
        const ReliableDirectMessageSendRequest& request) const;
    ReliableDirectMessageSendResult retryText(
        const QString& messageId,
        const ReliableDirectMessageSendRequest& request) const;

private:
    enum class ServiceSendStatus {
        FailedBeforeAck,
        Succeeded,
        AcceptedButLocalFinalizeFailed
    };

    ReliableDirectMessageSendResult sendPersistedText(
        const QString& messageId,
        const ReliableDirectMessageSendRequest& request) const;

    ServiceSendStatus sendViaMessageService(
        const ReliableDirectMessageSendRequest& request,
        const QString& messageId,
        QString* errorMessage) const;
    bool sendViaP2P(const ReliableDirectMessageSendRequest& request,
                    const QString& messageId,
                    QString* errorMessage) const;

    QString m_localClientId;
    ConversationRepository* m_repository = nullptr;
    const IServerMessageClient* m_serverClient = nullptr;
    P2PSendCallback m_p2pSendCallback;
};
