#pragma once

#include <QObject>
#include <QPointer>
#include <QTcpSocket>
#include <QThread>

#include <atomic>
#include <memory>

class QSslSocket;

enum class ConnectionRole { Client, Server };

class PeerConnection : public QObject {
    Q_OBJECT

public:
    explicit PeerConnection(QString localClientId, QTcpSocket* socket,
                            ConnectionRole role = ConnectionRole::Client,
                            QObject* parent = nullptr);
    ~PeerConnection() override;

    bool isConnected() const;
    bool isConnecting() const;
    bool isEncrypted() const;
    bool sendPayload(const QByteArray& payload, bool deferFlush = false);
    qint64 pendingBytes() const;
    void markHelloReceived();
    QString peerHost() const;
    quint16 peerPort() const;
    ConnectionRole connectionRole() const;

    void upgradeToTls();

signals:
    void payloadReceived(const QByteArray& payload);
    void connected();
    void disconnected();
    void tlsUpgradeComplete(bool success);

private:
    QString m_localClientId;
    QPointer<QTcpSocket> m_socket;
    QByteArray m_buffer;
    ConnectionRole m_role;
    bool m_encrypted = false;
    bool m_tlsUpgradeInProgress = false;
    bool m_disconnectEmitted = false;
    bool m_helloReceived = false;
    QThread* m_tlsThread = nullptr;
    // 跨线程生命周期标志：TLS 线程的 DirectConnection lambda 执行前检查此标志，
    // 防止 PeerConnection 析构后 lambda 仍访问已释放的 this 指针。
    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);
};
