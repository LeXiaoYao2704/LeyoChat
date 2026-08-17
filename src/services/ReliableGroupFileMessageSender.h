#pragma once

#include <functional>
#include <vector>

#include <QJsonObject>
#include <QString>
#include <QVector>

#include "domain/MessageEnvelope.h"
#include "integrations/RemoteChatServiceSettings.h"
#include "services/TransportPolicy.h"

class ConversationRepository;
class IServerMessageClient;

struct ReliableGroupFileMessageSendRequest {
    QString messageId;
    qint64 createdAtMs = 0;
    QString groupId;
    QString groupTitle;
    QString body;
    QString fileId;
    QString localFilePath;
    QJsonObject localFileCard;
    QJsonObject broadcastFileCard;
    std::vector<MessageEnvelope> envelopes;
    QVector<QString> serverRecipientIds;
    QVector<QString> p2pRecipientIds;
    RemoteChatServiceSettings settings;
    bool serviceReachable = false;
    bool p2pAvailable = false;
};

struct ReliableGroupFileMessageP2PRequest {
    QString messageId;
    QString groupId;
    QString groupTitle;
    QString body;
    std::vector<MessageEnvelope> envelopes;
    bool acceptQueuedOnlyDelivery = false;
};

struct ReliableGroupFileMessageSendResult {
    bool success = false;
    QString messageId;
    TransportChannel channelUsed = TransportChannel::None;
    QString errorMessage;
};

class ReliableGroupFileMessageSender {
public:
    using P2PSendCallback =
        std::function<bool(const ReliableGroupFileMessageP2PRequest&, QString*)>;

    ReliableGroupFileMessageSender(QString localClientId,
                                   ConversationRepository* repository,
                                   const IServerMessageClient* serverClient,
                                   P2PSendCallback p2pSendCallback);

    ReliableGroupFileMessageSendResult sendFileCard(
        const ReliableGroupFileMessageSendRequest& request) const;

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

    bool validateEnvelopes(const ReliableGroupFileMessageSendRequest& request,
                           ValidatedEnvelopes* out,
                           QString* errorMessage) const;
    bool persistLocalPendingMessage(const ReliableGroupFileMessageSendRequest& request,
                                    const ValidatedEnvelopes& envelopes,
                                    QString* errorMessage) const;
    ServiceSendStatus sendViaMessageService(
        const ReliableGroupFileMessageSendRequest& request,
        const ValidatedEnvelopes& envelopes,
        QString* errorMessage) const;
    bool sendViaP2P(const ReliableGroupFileMessageSendRequest& request,
                    const ValidatedEnvelopes& envelopes,
                    bool updateDeliveryState,
                    QString* errorMessage) const;

    QString m_localClientId;
    ConversationRepository* m_repository = nullptr;
    const IServerMessageClient* m_serverClient = nullptr;
    P2PSendCallback m_p2pSendCallback;
};
