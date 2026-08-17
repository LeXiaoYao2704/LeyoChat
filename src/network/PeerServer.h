#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTcpServer>

class PeerConnection;
class QHostAddress;

// 子类化 QTcpServer，在 incomingConnection 中直接创建 QSslSocket，
// 避免 socket 描述符在 QTcpSocket 和 QSslSocket 之间转移时被关闭。
class SslTcpServer : public QTcpServer {
protected:
    void incomingConnection(qintptr socketDescriptor) override;
};

class PeerServer : public QObject {
    Q_OBJECT

public:
    explicit PeerServer(QString localClientId, QObject* parent = nullptr);

    bool listen(const QHostAddress& address, quint16 port);
    quint16 serverPort() const;

signals:
    void connectionAccepted(PeerConnection* connection);

private:
    QString m_localClientId;
    SslTcpServer m_server;
    QHash<QString, qint64> m_inboundLastAcceptMs;  // 入站限速：同一 IP 3 秒内只接受一个新连接
};
