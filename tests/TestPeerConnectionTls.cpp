#include <QtTest/QTest>
#include <QSignalSpy>

#include <memory>
#include <QSslSocket>
#include <QTcpServer>

#include "network/PeerConnection.h"
#include "network/PeerHandshake.h"
#include "services/MessageRoutingCapabilities.h"
#include "network/TlsHelper.h"

class TestPeerConnectionTls : public QObject {
    Q_OBJECT

private slots:
    void tlsAvailability()
    {
        // TLS is available only when the deployment supplies credentials;
        // this test also verifies that an unconfigured build stays stable.
        const bool available = TlsHelper::isAvailable();
        qInfo() << "TLS available:" << available;
        QVERIFY(QSslSocket::supportsSsl() == available || !available);
    }

    void upgradeToTls_bothSidesEncrypted()
    {
        if (!TlsHelper::isAvailable()) {
            QSKIP("TLS not available on this build");
        }

        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));

        // 客户端：用 QSslSocket 连接
        auto* clientSsl = new QSslSocket();
        clientSsl->connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(clientSsl->waitForConnected(3000));
        QVERIFY(server.waitForNewConnection(3000));

        // 服务端：从 QTcpServer 取出原始 socket，用 QSslSocket 替换
        QTcpSocket* rawServer = server.nextPendingConnection();
        QVERIFY(rawServer);
        auto* serverSsl = new QSslSocket();
        serverSsl->setSocketDescriptor(rawServer->socketDescriptor());

        PeerConnection clientConn(QStringLiteral("client"), clientSsl, ConnectionRole::Client);
        PeerConnection serverConn(QStringLiteral("server"), serverSsl, ConnectionRole::Server);

        QVERIFY(!clientConn.isEncrypted());
        QVERIFY(!serverConn.isEncrypted());

        // 先发起服务端加密（STARTTLS 顺序：服务端先准备好，客户端再发起）
        QSignalSpy serverTlsSpy(&serverConn, &PeerConnection::tlsUpgradeComplete);
        QSignalSpy clientTlsSpy(&clientConn, &PeerConnection::tlsUpgradeComplete);

        serverConn.upgradeToTls();
        clientConn.upgradeToTls();

        // 等待加密完成
        // Either side may complete while the other spy is waiting.
        QVERIFY(serverTlsSpy.count() > 0 || serverTlsSpy.wait(5000));
        QVERIFY(clientTlsSpy.count() > 0 || clientTlsSpy.wait(5000));

        QVERIFY(clientConn.isEncrypted());
        QVERIFY(serverConn.isEncrypted());

        // 验证加密后仍能正常收发数据
        QSignalSpy receiveSpy(&serverConn, &PeerConnection::payloadReceived);
        const QByteArray testData = QByteArrayLiteral("hello-tls-test");
        QVERIFY(clientConn.sendPayload(testData));
        QVERIFY(receiveSpy.wait(3000));
        QCOMPARE(receiveSpy.count(), 1);
        QCOMPARE(receiveSpy.at(0).at(0).toByteArray(), testData + '\n');
    }

    void connectionRole_defaultIsClient()
    {
        auto* socket = new QSslSocket();
        PeerConnection conn(QStringLiteral("test"), socket);
        QCOMPARE(conn.connectionRole(), ConnectionRole::Client);
        // socket 未连接，isEncrypted 应为 false
        QVERIFY(!conn.isEncrypted());
    }

    void peerHello_supportsTls_roundTrip()
    {
        // 新版本：hello 带 supportsTls=true
        PeerHello hello;
        hello.clientId = QStringLiteral("user1");
        hello.displayName = QStringLiteral("Alice");
        hello.listenPort = 12345;
        hello.supportsTls = true;

        const MessageEnvelope env = PeerHandshake::buildHelloEnvelope(hello);
        const auto parsed = PeerHandshake::parseHelloEnvelope(env);

        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->clientId, hello.clientId);
        QCOMPARE(parsed->displayName, hello.displayName);
        QCOMPARE(parsed->listenPort, hello.listenPort);
        QVERIFY(parsed->supportsTls);
    }

    void peerHello_noTlsField_parsedAsFalse()
    {
        // 模拟旧版本 hello（没有 supportsTls 字段）：只有 4 个分隔字段
        PeerHello hello;
        hello.clientId = QStringLiteral("user2");
        hello.displayName = QStringLiteral("Bob");
        hello.listenPort = 12345;
        hello.supportsTls = false;

        const MessageEnvelope env = PeerHandshake::buildHelloEnvelope(hello);
        const auto parsed = PeerHandshake::parseHelloEnvelope(env);

        QVERIFY(parsed.has_value());
        QVERIFY(!parsed->supportsTls);
    }

    void peerHello_twoFieldLegacyPayload_parsedWithEmptyCapabilities()
    {
        MessageEnvelope env;
        env.messageId = "hello-legacy-user";
        env.type = MessageType::HandshakeHello;
        env.senderId = "legacy-user";
        const QString payload =
            QStringLiteral("Bob") + QChar(0x1F) + QStringLiteral("12345");
        env.body = payload.toUtf8().toStdString();

        const auto parsed = PeerHandshake::parseHelloEnvelope(env);

        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->clientId, QStringLiteral("legacy-user"));
        QCOMPARE(parsed->displayName, QStringLiteral("Bob"));
        QCOMPARE(parsed->listenPort, static_cast<quint16>(12345));
        QVERIFY(!parsed->supportsTls);
        QVERIFY(parsed->appVersion.isEmpty());
        QVERIFY(parsed->capabilities.isEmpty());
    }

    void peerHello_preservesCapabilities()
    {
        PeerHello hello;
        hello.clientId = QStringLiteral("client-new");
        hello.displayName = QStringLiteral("Alice");
        hello.listenPort = 50001;
        hello.appVersion = QStringLiteral("0.2.0");
        hello.capabilities = QStringList{
            MessageRoutingCapabilities::remoteMessageV1(),
            MessageRoutingCapabilities::serverReceiveV1()
        };

        const MessageEnvelope env = PeerHandshake::buildHelloEnvelope(hello);
        const auto parsed = PeerHandshake::parseHelloEnvelope(env);

        QVERIFY(parsed.has_value());
        QCOMPARE(parsed->appVersion, hello.appVersion);
        QVERIFY(parsed->capabilities.contains(
            MessageRoutingCapabilities::serverReceiveV1()));
    }

    void peerHello_defaultHasNoCapabilities()
    {
        PeerHello hello;
        hello.clientId = QStringLiteral("legacy");
        hello.displayName = QStringLiteral("Bob");
        hello.listenPort = 50001;

        const MessageEnvelope env = PeerHandshake::buildHelloEnvelope(hello);
        const auto parsed = PeerHandshake::parseHelloEnvelope(env);

        QVERIFY(parsed.has_value());
        QVERIFY(parsed->appVersion.isEmpty());
        QVERIFY(parsed->capabilities.isEmpty());
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TestPeerConnectionTls testCase;
    const int result = QTest::qExec(&testCase, argc, argv);
    std::_Exit(result);
}

#include "TestPeerConnectionTls.moc"
