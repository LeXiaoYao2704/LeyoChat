#include "app/DeliveryReceiptHelpers.h"

#include "app/PeerPresentationHelpers.h"
#include "network/MessageCodec.h"
#include "network/PeerConnection.h"

#include <QByteArray>
#include <QDateTime>

bool sendDeliveryReceipt(PeerConnection* connection,
                         const QString& localClientId,
                         const MessageEnvelope& delivered)
{
    if (!connection || !connection->isConnected() || delivered.messageId.empty()) {
        return false;
    }

    MessageEnvelope receipt;
    receipt.messageId = delivered.messageId;
    receipt.type = MessageType::ReceiptReceived;
    receipt.senderId = toUtf8(localClientId);
    receipt.targetId = delivered.senderId;
    receipt.conversationId = delivered.conversationId;
    receipt.createdAtMs = QDateTime::currentMSecsSinceEpoch();
    return connection->sendPayload(QByteArray::fromStdString(MessageCodec::encode(receipt)));
}
