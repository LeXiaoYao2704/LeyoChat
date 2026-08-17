#pragma once

#include <atomic>
#include <memory>

#include <QObject>
#include <QPointer>
#include <QTcpSocket>
#include <QThread>

#include "domain/FileTransferChunk.h"

class QSslSocket;
class QTimer;

class FileTransferConnection : public QObject {
    Q_OBJECT

public:
    explicit FileTransferConnection(QTcpSocket* socket, bool isServerSide = false,
                                    QObject* parent = nullptr);
    ~FileTransferConnection() override;

    bool isConnected() const;
    bool isEncrypted() const;
    void sendChunk(const FileTransferChunkHeader& header, const QByteArray& payload);
    QString peerHost() const;
    quint16 peerPort() const;

    void upgradeToTls();

signals:
    void chunkReceived(FileTransferChunkHeader header, QByteArray payload);
    void connected();
    void disconnected();
    void errorOccurred(QString errorText);
    void tlsUpgradeComplete(bool success);

private:
    void drainBuffer();

    QPointer<QTcpSocket> m_socket;
    QByteArray m_buffer;
    QTimer* m_idleTimer = nullptr;
    bool m_isServerSide;
    bool m_encrypted = false;
    QThread* m_tlsThread = nullptr;
    std::shared_ptr<std::atomic<bool>> m_alive = std::make_shared<std::atomic<bool>>(true);
};
