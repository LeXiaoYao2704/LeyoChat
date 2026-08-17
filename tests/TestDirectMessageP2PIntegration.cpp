#include <QtTest/QTest>

#include <memory>
#include <string_view>

#include <QDateTime>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "integrations/RemoteChatServiceSettings.h"
#include "network/MessageCodec.h"
#include "network/PeerConnection.h"
#include "services/ChatService.h"
#include "services/DirectConversationAddressing.h"
#include "services/ReliableDirectMessageSender.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

namespace {

RemoteChatServiceSettings p2pOnlySettings()
{
    RemoteChatServiceSettings settings;
    settings.mode = RemoteChatTransportMode::P2POnly;
    settings.allowP2PFallback = true;
    return settings;
}

}  // namespace

class TestDirectMessageP2PIntegration : public QObject {
    Q_OBJECT

private slots:
    void persistedMessageTraversesTcpAndReceiptAdvancesSender();
};

void TestDirectMessageP2PIntegration::persistedMessageTraversesTcpAndReceiptAdvancesSender()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString senderConnectionName = QStringLiteral("direct-p2p-integration-sender");
    const QString receiverConnectionName = QStringLiteral("direct-p2p-integration-receiver");
    DatabaseManager senderDatabase(
        directory.filePath(QStringLiteral("sender.db")), senderConnectionName);
    DatabaseManager receiverDatabase(
        directory.filePath(QStringLiteral("receiver.db")), receiverConnectionName);
    QVERIFY(senderDatabase.open());
    QVERIFY(receiverDatabase.open());

    ConversationRepository senderRepository(senderConnectionName);
    ConversationRepository receiverRepository(receiverConnectionName);
    const QString senderId = QStringLiteral("integration-sender");
    const QString receiverId = QStringLiteral("integration-receiver");
    const QString conversationId =
        DirectConversationAddressing::conversationIdForPeers(senderId, receiverId);
    QVERIFY(!conversationId.isEmpty());

    QTcpServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));

    auto senderSocket = std::make_unique<QTcpSocket>();
    senderSocket->connectToHost(QHostAddress::LocalHost, server.serverPort());
    QVERIFY(senderSocket->waitForConnected(3000));
    QVERIFY(server.waitForNewConnection(3000));

    QTcpSocket* receiverSocket = server.nextPendingConnection();
    QVERIFY(receiverSocket != nullptr);
    PeerConnection senderConnection(senderId,
                                    senderSocket.release(),
                                    ConnectionRole::Client);
    PeerConnection receiverConnection(receiverId,
                                      receiverSocket,
                                      ConnectionRole::Server);
    receiverConnection.markHelloReceived();

    bool receiverStoredMessage = false;
    bool receiverSentReceipt = false;
    bool senderAppliedReceipt = false;
    QString transportError;

    connect(&receiverConnection,
            &PeerConnection::payloadReceived,
            this,
            [&](const QByteArray& payload) {
                const auto decoded = MessageCodec::decode(
                    std::string_view(payload.constData(),
                                     static_cast<std::size_t>(payload.size())));
                if (!decoded.has_value() || decoded->type != MessageType::ChatText) {
                    transportError = QStringLiteral("receiver failed to decode chat text");
                    return;
                }

                receiverStoredMessage = ChatService::storeIncomingEnvelope(
                    receiverId, &receiverRepository, *decoded);
                if (!receiverStoredMessage) {
                    transportError = QStringLiteral("receiver failed to store chat text");
                    return;
                }

                MessageEnvelope receipt;
                receipt.messageId = decoded->messageId;
                receipt.type = MessageType::ReceiptReceived;
                receipt.senderId = receiverId.toStdString();
                receipt.targetId = senderId.toStdString();
                receipt.conversationId = decoded->conversationId;
                receipt.createdAtMs = QDateTime::currentMSecsSinceEpoch();
                receiverSentReceipt = receiverConnection.sendPayload(
                    QByteArray::fromStdString(MessageCodec::encode(receipt)));
                if (!receiverSentReceipt) {
                    transportError = QStringLiteral("receiver failed to send receipt");
                }
            });

    connect(&senderConnection,
            &PeerConnection::payloadReceived,
            this,
            [&](const QByteArray& payload) {
                const auto decoded = MessageCodec::decode(
                    std::string_view(payload.constData(),
                                     static_cast<std::size_t>(payload.size())));
                if (!decoded.has_value()
                    || decoded->type != MessageType::ReceiptReceived) {
                    transportError = QStringLiteral("sender failed to decode receipt");
                    return;
                }
                senderAppliedReceipt = ChatService::storeIncomingEnvelope(
                    senderId, &senderRepository, *decoded);
                if (!senderAppliedReceipt) {
                    transportError = QStringLiteral("sender failed to apply receipt");
                }
            });

    ReliableDirectMessageSender sender(
        senderId,
        &senderRepository,
        nullptr,
        [&](const ReliableDirectMessageP2PRequest& p2pRequest,
            QString* errorMessage) {
            MessageEnvelope envelope;
            if (!ChatService::buildEnvelope(senderId,
                                            &senderRepository,
                                            p2pRequest.messageId,
                                            p2pRequest.targetId,
                                            &envelope)) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral("failed to build persisted envelope");
                }
                return false;
            }
            return senderConnection.sendPayload(
                QByteArray::fromStdString(MessageCodec::encode(envelope)));
        });

    ReliableDirectMessageSendRequest request;
    request.conversationId = conversationId;
    request.targetId = receiverId;
    request.body = QStringLiteral("<p>tcp integration message</p>");
    request.settings = p2pOnlySettings();
    request.p2pAvailable = true;
    request.requireP2PDeliveryReceipt = true;

    const ReliableDirectMessageSendResult result = sender.sendText(request);
    QVERIFY2(result.success, qPrintable(result.errorMessage));
    QCOMPARE(result.channelUsed, TransportChannel::P2P);
    QVERIFY(!result.messageId.isEmpty());

    ChatMessage senderMessage;
    QVERIFY(senderRepository.findMessageById(result.messageId, &senderMessage));
    QCOMPARE(senderMessage.deliveryState, MessageDeliveryState::Pending);

    QTRY_VERIFY_WITH_TIMEOUT(receiverStoredMessage || !transportError.isEmpty(), 3000);
    QVERIFY2(receiverStoredMessage, qPrintable(transportError));
    QTRY_VERIFY_WITH_TIMEOUT(receiverSentReceipt || !transportError.isEmpty(), 3000);
    QVERIFY2(receiverSentReceipt, qPrintable(transportError));
    QTRY_VERIFY_WITH_TIMEOUT(senderAppliedReceipt || !transportError.isEmpty(), 3000);
    QVERIFY2(senderAppliedReceipt, qPrintable(transportError));

    ChatMessage receiverMessage;
    QVERIFY(receiverRepository.findMessageById(result.messageId, &receiverMessage));
    QCOMPARE(QString::fromStdWString(receiverMessage.senderId), senderId);
    QCOMPARE(QString::fromStdWString(receiverMessage.body), request.body);
    QCOMPARE(receiverMessage.deliveryState, MessageDeliveryState::Received);

    QVERIFY(senderRepository.findMessageById(result.messageId, &senderMessage));
    QCOMPARE(senderMessage.deliveryState, MessageDeliveryState::Received);
    QCOMPARE(QString::fromStdWString(senderMessage.messageId), result.messageId);
}

QTEST_GUILESS_MAIN(TestDirectMessageP2PIntegration)
#include "TestDirectMessageP2PIntegration.moc"
