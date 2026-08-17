#include "network/PeerSessionManager.h"

#include "network/PeerConnection.h"

#include <QHostAddress>
#include <QNetworkProxy>
#include <QSslSocket>

PeerSessionManager::PeerSessionManager(QString localClientId, QObject* parent)
    : QObject(parent),
      m_localClientId(std::move(localClientId)) {}

PeerConnection* PeerSessionManager::connectToPeer(const QHostAddress& address, quint16 port) {
    auto* socket = new QSslSocket(this);
    socket->setProxy(QNetworkProxy::NoProxy);  // 跳过代理解析，避免 WPAD 阻塞
    auto* connection = new PeerConnection(m_localClientId, socket, ConnectionRole::Client, this);
    socket->connectToHost(address, port);
    return connection;
}
