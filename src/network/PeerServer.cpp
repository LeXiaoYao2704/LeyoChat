#include "network/PeerServer.h"

#include "network/PeerConnection.h"

#include <QDateTime>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QSslSocket>

#ifdef Q_OS_WIN
#include <winsock2.h>
#endif

void SslTcpServer::incomingConnection(qintptr socketDescriptor) {
    auto* sslSocket = new QSslSocket;
    sslSocket->setProxy(QNetworkProxy::NoProxy);
    if (sslSocket->setSocketDescriptor(socketDescriptor)) {
        addPendingConnection(sslSocket);
    } else {
        delete sslSocket;
    }
}

PeerServer::PeerServer(QString localClientId, QObject* parent)
    : QObject(parent),
      m_localClientId(std::move(localClientId)) {
    connect(&m_server, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket* socket = m_server.nextPendingConnection()) {
            auto* sslSocket = qobject_cast<QSslSocket*>(socket);
            if (!sslSocket) {
                qWarning() << "PeerServer: unexpected non-QSslSocket, dropping";
                socket->deleteLater();
                continue;
            }

            // 入站限速：同一 IP 3 秒内只接受一个新 TCP 连接，
            // 防止连接震荡（dedup 裁决断开 → 对端立刻重连 → 再 dedup）形成洪水
            const QString peerIp = sslSocket->peerAddress().toString();
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            // 从 3s 放宽到 500ms：NAT/代理环境下多用户共享出口IP，过严会互相阻塞
            constexpr qint64 kInboundCooldownMs = 500;
            const auto it = m_inboundLastAcceptMs.constFind(peerIp);
            if (it != m_inboundLastAcceptMs.constEnd()
                && (now - it.value()) < kInboundCooldownMs) {
                sslSocket->abort();
                sslSocket->deleteLater();
                continue;
            }
            m_inboundLastAcceptMs.insert(peerIp, now);

            sslSocket->setParent(this);
            emit connectionAccepted(
                new PeerConnection(m_localClientId, sslSocket, ConnectionRole::Server, this));
        }
    });
}

bool PeerServer::listen(const QHostAddress& address, quint16 port) {
#ifdef Q_OS_WIN
    // 启用 SO_REUSEADDR：允许绑定处于 TIME_WAIT 状态的端口，
    // 避免安装后重启因旧进程端口未释放而 fallback 到随机端口。
    SOCKET sock = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock != INVALID_SOCKET) {
        int optval = 1;
        ::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&optval), sizeof(optval));
        if (m_server.setSocketDescriptor(static_cast<qintptr>(sock))) {
            return m_server.listen(address, port);
        }
        ::closesocket(sock);
    }
#endif
    return m_server.listen(address, port);
}

quint16 PeerServer::serverPort() const {
    return m_server.serverPort();
}
