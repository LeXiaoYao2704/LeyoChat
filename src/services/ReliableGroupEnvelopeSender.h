#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QVector>

#include "domain/ChatMessage.h"
#include "domain/MessageEnvelope.h"
#include "integrations/RemoteChatServiceSettings.h"
#include "services/TransportPolicy.h"

class ConversationRepository;
class IServerMessageClient;

struct ReliableGroupEnvelopeSendRequest {
    QString groupId;
    QString groupTitle;
    std::vector<MessageEnvelope> envelopes;
    QVector<QString> serverRecipientIds;
    QVector<QString> p2pRecipientIds;
    RemoteChatServiceSettings settings;
    bool serviceReachable = false;
    bool p2pAvailable = false;
};

struct ReliableGroupEnvelopeP2PRequest {
    QString messageId;
    QString groupId;
    QString groupTitle;
    std::vector<MessageEnvelope> envelopes;
    bool acceptQueuedOnlyDelivery = false;
};

struct ReliableGroupEnvelopeSendResult {
    bool success = false;
    QString messageId;
    TransportChannel channelUsed = TransportChannel::None;
    QString errorMessage;
};

class ReliableGroupEnvelopeSender {
public:
    using P2PSendCallback =
        std::function<bool(const ReliableGroupEnvelopeP2PRequest&, QString*)>;

    ReliableGroupEnvelopeSender(QString localClientId,
                                ConversationRepository* repository,
                                const IServerMessageClient* serverClient,
                                P2PSendCallback p2pSendCallback);

    ReliableGroupEnvelopeSendResult send(
        const ReliableGroupEnvelopeSendRequest& request) const;

private:
    enum class ServiceSendStatus {
        FailedBeforeAck,
        Succeeded,
        AcceptedButLocalFinalizeFailed
    };

    struct ValidatedEnvelopes {
        QString messageId;
        QVector<QString> recipientIds;
        MessageEnvelope firstEnvelope;
    };

    bool validateEnvelopes(const ReliableGroupEnvelopeSendRequest& request,
                           ValidatedEnvelopes* out,
                           QString* errorMessage) const;
    ServiceSendStatus sendViaMessageService(
        const ReliableGroupEnvelopeSendRequest& request,
        const ValidatedEnvelopes& envelopes,
        QString* errorMessage) const;
    bool sendViaP2P(const ReliableGroupEnvelopeSendRequest& request,
                    const ValidatedEnvelopes& envelopes,
                    bool updateDeliveryState,
                    QString* errorMessage) const;
    bool markExistingMessageState(const QString& messageId,
                                  MessageDeliveryState state,
                                  const QString& serverMessageId,
                                  QString* errorMessage) const;
    void clearPendingFanOutForRecipients(const QString& messageId,
                                         const QVector<QString>& recipientIds) const;

    QString m_localClientId;
    ConversationRepository* m_repository = nullptr;
    const IServerMessageClient* m_serverClient = nullptr;
    P2PSendCallback m_p2pSendCallback;
};
