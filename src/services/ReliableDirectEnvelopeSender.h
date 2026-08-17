#pragma once

#include <functional>

#include <QString>

#include "domain/MessageEnvelope.h"
#include "integrations/RemoteChatServiceSettings.h"
#include "services/TransportPolicy.h"

class IServerMessageClient;

struct ReliableDirectEnvelopeSendRequest {
    MessageEnvelope envelope;
    RemoteChatServiceSettings settings;
    bool serviceReachable = false;
    bool receiverServerCapable = false;
    bool p2pAvailable = false;
};

struct ReliableDirectEnvelopeP2PRequest {
    QString messageId;
    MessageEnvelope envelope;
};

struct ReliableDirectEnvelopeSendResult {
    bool success = false;
    QString messageId;
    QString serverMessageId;
    TransportChannel channelUsed = TransportChannel::None;
    QString errorMessage;
};

class ReliableDirectEnvelopeSender {
public:
    using P2PSendCallback =
        std::function<bool(const ReliableDirectEnvelopeP2PRequest&, QString*)>;

    ReliableDirectEnvelopeSender(QString localClientId,
                                 const IServerMessageClient* serverClient,
                                 P2PSendCallback p2pSendCallback);

    ReliableDirectEnvelopeSendResult send(
        const ReliableDirectEnvelopeSendRequest& request) const;

private:
    enum class ServiceSendStatus {
        FailedBeforeAck,
        Succeeded,
        AcceptedButLocalFinalizeFailed
    };

    bool validateEnvelope(const MessageEnvelope& envelope,
                          QString* errorMessage) const;
    ServiceSendStatus sendViaMessageService(
        const ReliableDirectEnvelopeSendRequest& request,
        QString* outServerMessageId,
        QString* errorMessage) const;
    bool sendViaP2P(const MessageEnvelope& envelope,
                    QString* errorMessage) const;

    QString m_localClientId;
    const IServerMessageClient* m_serverClient = nullptr;
    P2PSendCallback m_p2pSendCallback;
};
