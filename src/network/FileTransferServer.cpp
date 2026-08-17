#include "network/FileTransferServer.h"

#include "network/FileTransferConnection.h"

#include <QSslSocket>

FileTransferServer::FileTransferServer(QObject* parent)
    : QObject(parent) {
    connect(&m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            QSslSocket* sslSocket = qobject_cast<QSslSocket*>(socket);
            if (!sslSocket) {
                const qintptr descriptor = socket->socketDescriptor();
                socket->setSocketDescriptor(-1);
                sslSocket = new QSslSocket(this);
                sslSocket->setSocketDescriptor(descriptor);
                socket->deleteLater();
            }
            emit connectionAccepted(new FileTransferConnection(sslSocket, true, this));
        }
    });
}

bool FileTransferServer::listen(const QHostAddress& address, quint16 port) {
    return m_server.listen(address, port);
}

quint16 FileTransferServer::serverPort() const {
    return m_server.serverPort();
}
