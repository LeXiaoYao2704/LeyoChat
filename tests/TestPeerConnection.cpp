#include <QtTest/QTest>

#include <memory>
#include <QElapsedTimer>
#include <QScopedPointer>
#include <QTcpServer>
#include <QTcpSocket>

#include "network/PeerConnection.h"

class TestPeerConnection : public QObject {
    Q_OBJECT

private slots:
    void sendPayload_sendsFullFrameOverTcp()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));

        auto clientSocket = std::make_unique<QTcpSocket>();
        clientSocket->connectToHost(QHostAddress::LocalHost, server.serverPort());
        QVERIFY(clientSocket->waitForConnected(3000));
        QVERIFY(server.waitForNewConnection(3000));

        QScopedPointer<QTcpSocket> serverSocket(server.nextPendingConnection());
        QVERIFY(serverSocket);
        QVERIFY(serverSocket->waitForReadyRead(100) || serverSocket->isValid());

        PeerConnection connection(QStringLiteral("local"), clientSocket.release());
        const QByteArray payload(128 * 1024, 'A');

        QVERIFY(connection.sendPayload(payload));

        QByteArray received;
        QElapsedTimer timer;
        timer.start();
        while (!received.endsWith('\n') && timer.elapsed() < 5000) {
            if (!serverSocket->waitForReadyRead(200)) {
                continue;
            }
            received.append(serverSocket->readAll());
        }

        QCOMPARE(received, payload + '\n');
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TestPeerConnection testCase;
    const int result = QTest::qExec(&testCase, argc, argv);
    std::_Exit(result);
}

#include "TestPeerConnection.moc"
