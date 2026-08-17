#pragma once

#include <functional>
#include <vector>

#include <QString>
#include <QVector>

#include "domain/MessageEnvelope.h"
#include "app/GroupFanOutDelivery.h"
#include "integrations/RemoteChatServiceSettings.h"
#include "services/TransportPolicy.h"

class ConversationRepository;
class IServerMessageClient;

struct ReliableGroupMessageSendRequest {
    QString groupId;
    QString groupTitle;
    QString body;
    std::vector<MessageEnvelope> envelopes;
    QVector<QString> serverRecipientIds;
    QVector<QString> p2pRecipientIds;
    RemoteChatServiceSettings settings;
    bool serviceReachable = false;
    bool p2pAvailable = false;
};

struct ReliableGroupMessageP2PRequest {
    QString messageId;
    QString groupId;
    QString groupTitle;
    QString body;
    std::vector<MessageEnvelope> envelopes;
    bool acceptQueuedOnlyDelivery = false;
};

using ReliableGroupMessageP2PStatusCallback =
    std::function<GroupFanOutDeliveryResult(
        const ReliableGroupMessageP2PRequest&, QString*)>;

struct ReliableGroupMessageSendResult {
    bool success = false;
    bool accepted = false;
    QString messageId;
    TransportChannel channelUsed = TransportChannel::None;
    int writtenRecipientCount = 0;
    int queuedRecipientCount = 0;
    int failedRecipientCount = 0;
    QString errorMessage;
};

class ReliableGroupMessageSender {
public:
    using P2PSendCallback =
        std::function<bool(const ReliableGroupMessageP2PRequest&, QString*)>;

    ReliableGroupMessageSender(QString localClientId,
                               ConversationRepository* repository,
                               const IServerMessageClient* serverClient,
                               P2PSendCallback p2pSendCallback);
    ReliableGroupMessageSender(QString localClientId,
                               ConversationRepository* repository,
                               const IServerMessageClient* serverClient,
                               ReliableGroupMessageP2PStatusCallback p2pStatusCallback);

    ReliableGroupMessageSendResult sendText(
        const ReliableGroupMessageSendRequest& request) const;

private:
    enum class ServiceSendStatus {
        FailedBeforeAck,
        Succeeded,
        AcceptedButLocalFinalizeFailed
    };

    struct ValidatedEnvelopes {
        QString messageId;
        QVector<QString> recipientIds;
        MessageEnvelope selfEnvelope;
    };

    bool validateEnvelopes(const ReliableGroupMessageSendRequest& request,
                           ValidatedEnvelopes* out,
                           QString* errorMessage) const;
    bool persistLocalPendingMessage(const ReliableGroupMessageSendRequest& request,
                                    const ValidatedEnvelopes& envelopes,
                                    QString* errorMessage) const;
    ServiceSendStatus sendViaMessageService(
        const ReliableGroupMessageSendRequest& request,
        const ValidatedEnvelopes& envelopes,
        QString* errorMessage) const;
    bool sendViaP2P(const ReliableGroupMessageSendRequest& request,
                    const ValidatedEnvelopes& envelopes,
                    bool updateDeliveryState,
                    GroupFanOutDeliveryResult* deliveryResult,
                    QString* errorMessage) const;

    QString m_localClientId;
    ConversationRepository* m_repository = nullptr;
    const IServerMessageClient* m_serverClient = nullptr;
    P2PSendCallback m_p2pSendCallback;
    ReliableGroupMessageP2PStatusCallback m_p2pStatusCallback;
};
