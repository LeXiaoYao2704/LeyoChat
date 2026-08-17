#include "network/FileTransferConnection.h"

#include <cstring>

#include <QtEndian>
#include <QSslSocket>
#include <QTimer>

#include "network/TlsHelper.h"

namespace {
constexpr int kTaskIdLengthBytes = 4;
constexpr int kChunkIndexBytes = 4;
constexpr int kPayloadSizeBytes = 8;
constexpr int kHeaderBytes = kTaskIdLengthBytes + kChunkIndexBytes + kPayloadSizeBytes;
}

FileTransferConnection::~FileTransferConnection() {
    m_alive->store(false);
    if (m_tlsThread && m_tlsThread->isRunning()) {
        m_tlsThread->quit();
        if (!m_tlsThread->wait(5000)) {
            // TLS 线程卡死（Schannel CRL 超时等），放弃 socket 防止堆损坏
            if (m_socket) {
                disconnect(m_socket, nullptr, this, nullptr);
            }
            qWarning().noquote()
                << "[FileTransferConnection] destructor: TLS thread stuck after 5s,"
                   " abandoning thread. peer="
                << (m_socket ? m_socket->peerAddress().toString()
                             : QStringLiteral("null"));
            m_socket = nullptr;
            m_tlsThread->setParent(nullptr);
            m_tlsThread = nullptr;
            return;
        }
        if (m_socket && m_socket->thread() != this->thread()) {
            m_socket->moveToThread(this->thread());
        }
    }
    if (m_socket) {
        m_socket->abort();
    }
}

FileTransferConnection::FileTransferConnection(QTcpSocket* socket, bool isServerSide,
                                               QObject* parent)
    : QObject(parent),
      m_socket(socket),
      m_isServerSide(isServerSide) {
    Q_ASSERT(m_socket != nullptr);
    if (!m_socket->parent()) {
        m_socket->setParent(this);
    }

    connect(m_socket, &QTcpSocket::connected, this, &FileTransferConnection::connected);
    connect(m_socket, &QTcpSocket::disconnected, this, &FileTransferConnection::disconnected);
    connect(m_socket,
            &QTcpSocket::errorOccurred,
            this,
            [this](QAbstractSocket::SocketError) {
                if (m_socket) { emit errorOccurred(m_socket->errorString()); }
            });
    connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
        if (!m_socket) { return; }
        m_buffer.append(m_socket->readAll());
        // 防御性保护：文件传输帧不应超过 64MB（chunk 最大几 MB + header）。
        // 避免异常对端数据撑爆内存。
        if (m_buffer.size() > 64 * 1024 * 1024) {
            qWarning().noquote() << "[file-transfer] buffer overflow (" << m_buffer.size()
                                 << " bytes), aborting connection to"
                                 << peerHost() << ":" << peerPort();
            m_socket->abort();
            return;
        }
        drainBuffer();
        // 收到数据时重置空闲超时
        if (m_idleTimer) {
            m_idleTimer->start();
        }
    });

    // 60 秒无数据交换则断开连接，防止对端崩溃后 zombie 连接永久挂起。
    // 文件传输没有 keepalive 心跳，只能靠应用层超时检测。
    m_idleTimer = new QTimer(this);
    m_idleTimer->setSingleShot(true);
    m_idleTimer->setInterval(60000);
    connect(m_idleTimer, &QTimer::timeout, this, [this]() {
        if (m_socket && m_socket->state() == QAbstractSocket::ConnectedState) {
            qInfo().noquote() << "[file-transfer] idle timeout 60s, disconnecting peer="
                              << peerHost() << ":" << peerPort();
            m_socket->abort();
        }
    });
    m_idleTimer->start();
}

bool FileTransferConnection::isConnected() const {
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

void FileTransferConnection::sendChunk(const FileTransferChunkHeader& header, const QByteArray& payload) {
    if (!m_socket) {
        return;
    }

    // 发送数据时重置空闲超时
    if (m_idleTimer) {
        m_idleTimer->start();
    }

    const QByteArray taskIdBytes(header.taskId.data(), static_cast<int>(header.taskId.size()));
    QByteArray frame;
    frame.resize(kHeaderBytes + taskIdBytes.size() + payload.size());

    char* writePtr = frame.data();
    qToBigEndian<quint32>(static_cast<quint32>(taskIdBytes.size()), writePtr);
    writePtr += kTaskIdLengthBytes;
    qToBigEndian<qint32>(header.chunkIndex, writePtr);
    writePtr += kChunkIndexBytes;
    qToBigEndian<qint64>(header.payloadSize, writePtr);
    writePtr += kPayloadSizeBytes;

    if (!taskIdBytes.isEmpty()) {
        std::memcpy(writePtr, taskIdBytes.constData(), static_cast<std::size_t>(taskIdBytes.size()));
        writePtr += taskIdBytes.size();
    }
    if (!payload.isEmpty()) {
        std::memcpy(writePtr, payload.constData(), static_cast<std::size_t>(payload.size()));
    }

    m_socket->write(frame);
}

QString FileTransferConnection::peerHost() const {
    return m_socket ? m_socket->peerAddress().toString() : QString();
}

quint16 FileTransferConnection::peerPort() const {
    return m_socket ? m_socket->peerPort() : 0;
}

void FileTransferConnection::drainBuffer() {
    while (true) {
        if (m_buffer.size() < kHeaderBytes) {
            return;
        }

        const char* readPtr = m_buffer.constData();
        const quint32 taskIdLength = qFromBigEndian<quint32>(readPtr);
        readPtr += kTaskIdLengthBytes;
        const qint32 chunkIndex = qFromBigEndian<qint32>(readPtr);
        readPtr += kChunkIndexBytes;
        const qint64 payloadSize = qFromBigEndian<qint64>(readPtr);
        readPtr += kPayloadSizeBytes;

        if (taskIdLength > static_cast<quint32>(m_buffer.size())) {
            return;
        }
        const qint64 totalFrameBytes =
            static_cast<qint64>(kHeaderBytes) + static_cast<qint64>(taskIdLength) + payloadSize;
        if (payloadSize < 0 || totalFrameBytes < 0 || totalFrameBytes > m_buffer.size()) {
            return;
        }

        const QByteArray taskIdBytes = QByteArray(readPtr, static_cast<int>(taskIdLength));
        readPtr += taskIdLength;
        const QByteArray payload(readPtr, static_cast<int>(payloadSize));

        FileTransferChunkHeader header;
        header.taskId = std::string(taskIdBytes.constData(), static_cast<std::size_t>(taskIdBytes.size()));
        header.chunkIndex = chunkIndex;
        header.payloadSize = payloadSize;
        emit chunkReceived(header, payload);

        m_buffer.remove(0, static_cast<int>(totalFrameBytes));
    }
}

bool FileTransferConnection::isEncrypted() const {
    return m_encrypted;
}

void FileTransferConnection::upgradeToTls() {
    auto* sslSocket = qobject_cast<QSslSocket*>(m_socket);
    if (!sslSocket || m_encrypted) {
        emit tlsUpgradeComplete(false);
        return;
    }

    TlsHelper::configureSslSocket(sslSocket);

    // 将 TLS 握手移到工作线程，避免 Schannel CRL 阻塞冻结主线程
    QThread* mainThread = this->thread();
    m_tlsThread = new QThread(this);
    m_tlsThread->setObjectName(QStringLiteral("FT-TLS-%1").arg(peerHost()));

    disconnect(m_socket, &QTcpSocket::readyRead, this, nullptr);
    sslSocket->setParent(nullptr);
    sslSocket->moveToThread(m_tlsThread);

    auto alive = m_alive;

    connect(sslSocket, &QSslSocket::encrypted, this, [this, alive, sslSocket, mainThread]() {
        if (!alive->load()) return;
        sslSocket->moveToThread(mainThread);
        QThread::currentThread()->quit();
        QMetaObject::invokeMethod(this, [this, alive, sslSocket]() {
            if (!alive->load()) return;
            sslSocket->setParent(this);
            m_encrypted = true;
            // 重连 readyRead
            connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
                if (!m_socket) { return; }
                m_buffer.append(m_socket->readAll());
                if (m_buffer.size() > 64 * 1024 * 1024) {
                    qWarning().noquote() << "[file-transfer] buffer overflow (" << m_buffer.size()
                                         << " bytes), aborting connection to"
                                         << peerHost() << ":" << peerPort();
                    m_socket->abort();
                    return;
                }
                drainBuffer();
                if (m_idleTimer) { m_idleTimer->start(); }
            });
            if (m_socket && m_socket->bytesAvailable() > 0) {
                m_buffer.append(m_socket->readAll());
                drainBuffer();
            }
            emit tlsUpgradeComplete(true);
            if (m_tlsThread) {
                m_tlsThread->wait();
                m_tlsThread->deleteLater();
                m_tlsThread = nullptr;
            }
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);

    connect(sslSocket, &QSslSocket::errorOccurred, this, [this, alive, sslSocket, mainThread](QAbstractSocket::SocketError) {
        if (!alive->load()) return;
        sslSocket->moveToThread(mainThread);
        QThread::currentThread()->quit();
        QMetaObject::invokeMethod(this, [this, alive, sslSocket]() {
            if (!alive->load()) return;
            sslSocket->setParent(this);
            emit tlsUpgradeComplete(false);
            if (m_tlsThread) {
                m_tlsThread->wait();
                m_tlsThread->deleteLater();
                m_tlsThread = nullptr;
            }
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);

    connect(m_tlsThread, &QThread::started, sslSocket, [this, sslSocket]() {
        if (m_isServerSide) {
            sslSocket->startServerEncryption();
        } else {
            sslSocket->startClientEncryption();
        }
    });

    m_tlsThread->start();
}
