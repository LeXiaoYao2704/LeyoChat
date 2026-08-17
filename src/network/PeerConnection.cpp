#include "network/PeerConnection.h"

#include <QDebug>
#include <QSslSocket>
#include <QTimer>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <mstcpip.h>
#endif

#include "network/TlsHelper.h"

PeerConnection::PeerConnection(QString localClientId, QTcpSocket* socket,
                               ConnectionRole role, QObject* parent)
    : QObject(parent),
      m_localClientId(std::move(localClientId)),
      m_socket(socket),
      m_role(role) {
    Q_ASSERT(m_socket != nullptr);
    // 始终接管 socket 所有权：确保 PeerConnection 析构时 socket 一同被清理，
    // 避免 DUPLICATE 仲裁 / STALE-CLEANUP / 正常断开路径中 socket 泄漏。
    // （原来仅在 socket 无 parent 时才接管，导致 Server/Client 路径的 socket
    //   parent 留在 PeerServer/PeerSessionManager 上，PeerConnection 析构后
    //   zombie socket 永久残留。）
    m_socket->setParent(this);

    // 启用 TCP Keep-Alive，让操作系统层面检测半开连接（对端崩溃/断网等）。
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

#ifdef Q_OS_WIN
    // Windows 默认 keepalive 首次探测 2 小时太久；缩短到 2 分钟首探 + 10 秒重试，
    // 3 次失败（约 150 秒）即断开死连接。
    // 权衡：间隔过短会产生大量小包（千人部署每人 80+ 连接），过长则死连接检测慢。
    {
        const auto fd = m_socket->socketDescriptor();
        if (fd != (qintptr)-1) {
            tcp_keepalive alive{};
            alive.onoff = 1;
            alive.keepalivetime = 120000;    // 120 秒首次探测
            alive.keepaliveinterval = 10000; // 10 秒重试间隔
            DWORD bytesReturned = 0;
            ::WSAIoctl(static_cast<SOCKET>(fd), SIO_KEEPALIVE_VALS,
                       &alive, sizeof(alive), nullptr, 0, &bytesReturned, nullptr, nullptr);
        }
    }
#endif

    connect(m_socket, &QTcpSocket::connected, this, &PeerConnection::connected);
    // 使用 m_disconnectEmitted 标志防止 disconnected 信号被多次 emit
    // （Qt 自身的 disconnected + 5s 超时手动 emit + deleteLater 延迟销毁期间可能重入）
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        if (!m_disconnectEmitted) {
            m_disconnectEmitted = true;
            emit disconnected();
        }
    });

    // Client 角色：5 秒内未建立 TCP 连接则 abort，避免离线 peer 占用 socket 长达 21 秒
    // abort() 后必须确保 emit disconnected()，因为：
    //   1. ConnectingState → abort()：Qt 不触发 disconnected（只触发 stateChanged）
    //   2. UnconnectedState（TCP 被立即拒绝/网络不可达）：Qt 既不触发 connected 也不触发 disconnected
    // 如果不发 disconnected，globalPendingConnections 计数器永久泄漏 → 队列卡死
    if (m_role == ConnectionRole::Client) {
        QTimer::singleShot(5000, this, [this]() {
            if (m_socket && m_socket->state() != QAbstractSocket::ConnectedState) {
                qInfo().noquote() << "[peer-diag] 5s-timeout: state=" << m_socket->state()
                                  << "peer=" << m_socket->peerAddress().toString()
                                  << ":" << m_socket->peerPort()
                                  << " → abort + emit disconnected";
                m_socket->abort();
                if (!m_disconnectEmitted) {
                    m_disconnectEmitted = true;
                    emit disconnected();
                }
            }
        });
    }
    // Server 角色：15 秒内未收到任何数据（HELLO）则断开，防止僵尸入站连接
    if (m_role == ConnectionRole::Server) {
        QTimer::singleShot(15000, this, [this]() {
            if (m_socket && !m_helloReceived && !m_tlsUpgradeInProgress) {
                qInfo().noquote() << "[peer-diag] 15s-no-hello: peer="
                                  << m_socket->peerAddress().toString()
                                  << ":" << m_socket->peerPort()
                                  << " → abort + emit disconnected";
                m_socket->abort();
                if (!m_disconnectEmitted) {
                    m_disconnectEmitted = true;
                    emit disconnected();
                }
            }
        });
    }
    connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
        if (!m_socket) { return; }
        m_buffer.append(m_socket->readAll());
        while (true) {
            const qsizetype newLine = m_buffer.indexOf('\n');
            if (newLine < 0) {
                break;
            }
            const QByteArray frame = m_buffer.left(newLine + 1);
            m_buffer.remove(0, newLine + 1);
            emit payloadReceived(frame);
        }
        // 防御性保护：提取完所有完整消息后，剩余缓冲仍超限说明
        // 单条消息异常大（bug 或恶意），断开连接防止内存撑爆。
        // 阈值 16MB：文件 chunk(1MB) base64 编码后约 1.4MB，
        // 加 JSON 包装 + 多条交错到达需要足够余量。
        if (m_buffer.size() > 16 * 1024 * 1024) {
            qWarning().noquote() << "[peer-diag] buffer overflow (" << m_buffer.size()
                                 << " bytes after frame extraction), aborting connection to"
                                 << peerHost() << ":" << peerPort();
            m_socket->abort();
            if (!m_disconnectEmitted) {
                m_disconnectEmitted = true;
                emit disconnected();
            }
        }
    });
}

PeerConnection::~PeerConnection() {
    // 先标记生命周期结束：TLS 线程的 DirectConnection lambda 在执行前会检查此标志。
    // 必须在任何成员被销毁之前设置，确保跨线程 lambda 不会访问半析构的对象。
    m_alive->store(false);

    if (m_tlsThread && m_tlsThread->isRunning()) {
        // TLS 握手正在工作线程中执行，必须先安全停止线程再操作 socket。
        // 否则两个线程同时读写 socket 内部缓冲区会导致堆损坏
        // (STATUS_HEAP_CORRUPTION c0000374)。
        m_tlsThread->quit();
        if (!m_tlsThread->wait(5000)) {
            // 工作线程在 5 秒内未停止（可能阻塞在系统 Schannel 调用中）。
            // 断开所有从 socket 到本对象的信号连接，防止线程恢复后
            // DirectConnection 回调访问已销毁的 this。
            if (m_socket) {
                disconnect(m_socket, nullptr, this, nullptr);
                m_socket->setParent(nullptr);  // 解除父子关系，防止跟随 this 析构
            }
            qWarning().noquote()
                << "[PeerConnection] destructor: TLS thread stuck after 5s,"
                   " scheduling deferred cleanup. peer="
                << (m_socket ? m_socket->peerAddress().toString()
                             : QStringLiteral("null"));

            // 延迟清理：当线程最终结束时，自动清理 socket 和线程对象。
            // 用局部变量捕获指针，避免访问已析构的 this。
            QThread* stuckThread = m_tlsThread;
            QTcpSocket* orphanSocket = m_socket.data();
            stuckThread->setParent(nullptr);  // 阻止父子树 delete 运行中的线程
            QObject::connect(stuckThread, &QThread::finished, stuckThread, [stuckThread, orphanSocket]() {
                // 此 lambda 通过 QueuedConnection 在主线程执行（stuckThread 的亲和线程）。
                // 此时 TLS 线程已退出，socket 的原线程已死，需先迁移再销毁。
                if (orphanSocket) {
                    orphanSocket->moveToThread(QThread::currentThread());
                    orphanSocket->abort();
                    delete orphanSocket;
                }
                delete stuckThread;
            });
            m_socket = nullptr;
            m_tlsThread = nullptr;
            return;
        }
        // 工作线程已安全退出，socket 不再被并发访问
        if (m_socket && m_socket->thread() != this->thread()) {
            m_socket->moveToThread(this->thread());
        }
    }
    // 在 delete 前先 abort 确保 TCP 连接立即关闭，
    // 避免 socket 在析构过程中仍处于 ConnectedState 占用 fd。
    if (m_socket) {
        m_socket->setParent(this);
        m_socket->abort();
    }
}

bool PeerConnection::isConnected() const {
    return m_socket && m_socket->state() == QAbstractSocket::ConnectedState;
}

bool PeerConnection::isConnecting() const {
    if (!m_socket) return false;
    const auto s = m_socket->state();
    return s == QAbstractSocket::ConnectingState
        || s == QAbstractSocket::HostLookupState;
}

void PeerConnection::markHelloReceived() {
    m_helloReceived = true;
}

bool PeerConnection::sendPayload(const QByteArray& payload, bool deferFlush) {
    if (!m_socket || !m_socket->isOpen()
        || m_socket->state() != QAbstractSocket::ConnectedState) {
        return false;
    }

    // TLS 升级进行中时拒绝写入：socket 已移到工作线程，
    // 主线程写入会导致跨线程竞争和 QSocketNotifier 异常。
    if (m_tlsUpgradeInProgress) {
        qWarning().noquote()
            << "PeerConnection sendPayload rejected (TLS upgrade in progress) for"
            << m_localClientId << "peer=" << peerHost();
        return false;
    }

    QByteArray frame = payload;
    if (!frame.endsWith('\n')) {
        frame.append('\n');
    }

    const qint64 written = m_socket->write(frame);
    if (written != frame.size()) {
        qWarning().noquote()
            << "PeerConnection sendPayload write failed for" << m_localClientId
            << "wrote" << written << "of" << frame.size() << "bytes";
        return false;
    }

    // Keep control messages moving even when the caller is not returning to the
    // event loop immediately. Batched file chunks can still defer the flush.
    if (!deferFlush) {
        m_socket->flush();
    }
    return true;
}

qint64 PeerConnection::pendingBytes() const {
    return m_socket ? m_socket->bytesToWrite() : 0;
}

QString PeerConnection::peerHost() const {
    return m_socket ? m_socket->peerAddress().toString() : QString();
}

quint16 PeerConnection::peerPort() const {
    return m_socket ? m_socket->peerPort() : 0;
}

ConnectionRole PeerConnection::connectionRole() const {
    return m_role;
}

bool PeerConnection::isEncrypted() const {
    return m_encrypted;
}

void PeerConnection::upgradeToTls() {
    auto* sslSocket = qobject_cast<QSslSocket*>(m_socket);
    if (!sslSocket) {
        qWarning() << "PeerConnection::upgradeToTls: socket is not QSslSocket";
        emit tlsUpgradeComplete(false);
        return;
    }

    if (m_tlsUpgradeInProgress || m_encrypted) {
        return;
    }
    m_tlsUpgradeInProgress = true;

    // 清空应用层缓冲区，防止明文残留数据干扰 TLS 握手
    m_buffer.clear();

    TlsHelper::configureSslSocket(sslSocket);

    // 将 socket 移到工作线程执行 TLS 握手。
    // Schannel 后端在 TLS 握手期间可能同步调用 CertGetCertificateChain / AuthRoot Update，
    // 当 CRL 服务器不可达时阻塞 15-30 秒。在工作线程中执行可避免冻结主线程 UI。
    QThread* mainThread = this->thread();
    m_tlsThread = new QThread(this);
    m_tlsThread->setObjectName(QStringLiteral("TLS-%1").arg(peerHost()));

    // 断开主线程的 readyRead 处理（TLS 握手期间由 Schannel 内部驱动）
    disconnect(m_socket, &QTcpSocket::readyRead, this, nullptr);

    // moveToThread 要求对象无 parent；暂时解除，完成后恢复
    sslSocket->setParent(nullptr);
    sslSocket->moveToThread(m_tlsThread);

    // 捕获生命周期标志的副本：即使 PeerConnection 已析构，
    // shared_ptr 仍持有 atomic<bool>，lambda 可安全读取。
    auto alive = m_alive;

    connect(sslSocket, &QSslSocket::encrypted, this, [this, alive, sslSocket, mainThread]() {
        if (!alive->load()) return;  // PeerConnection 已析构，放弃
        sslSocket->moveToThread(mainThread);
        // 立即退出 TLS 线程事件循环：防止 moveToThread 后残留 socket 事件
        // 在工作线程上被 dispatch，导致跨线程 notifier 竞争 / 主线程死锁。
        QThread::currentThread()->quit();
        QMetaObject::invokeMethod(this, [this, alive, sslSocket]() {
            if (!alive->load()) return;  // 二次检查：排队期间可能被析构
            sslSocket->setParent(this);
            m_encrypted = true;
            m_tlsUpgradeInProgress = false;
            connect(m_socket, &QTcpSocket::readyRead, this, [this]() {
                if (!m_socket) { return; }
                m_buffer.append(m_socket->readAll());
                while (true) {
                    const int idx = m_buffer.indexOf('\n');
                    if (idx < 0) { break; }
                    const QByteArray line = m_buffer.left(idx + 1);
                    m_buffer.remove(0, idx + 1);
                    if (line.size() > 1) {
                        emit payloadReceived(line);
                    }
                }
                if (m_buffer.size() > 16 * 1024 * 1024) {
                    qWarning().noquote() << "[peer-diag] buffer overflow (" << m_buffer.size()
                                         << " bytes after frame extraction), aborting connection to"
                                         << peerHost() << ":" << peerPort();
                    m_socket->abort();
                    if (!m_disconnectEmitted) {
                        m_disconnectEmitted = true;
                        emit disconnected();
                    }
                }
            });
            if (m_socket && m_socket->bytesAvailable() > 0) {
                m_buffer.append(m_socket->readAll());
                while (true) {
                    const int idx = m_buffer.indexOf('\n');
                    if (idx < 0) { break; }
                    const QByteArray line = m_buffer.left(idx + 1);
                    m_buffer.remove(0, idx + 1);
                    if (line.size() > 1) {
                        emit payloadReceived(line);
                    }
                }
            }
            // 刷出 TLS 升级期间可能残留在 Qt 写缓冲区的数据。
            // moveToThread 后 write notifier 可能因跨线程竞争而失效，
            // 显式 flush 确保之前排队的数据立即发送。
            if (m_socket && m_socket->bytesToWrite() > 0) {
                m_socket->flush();
            }
            qInfo() << "PeerConnection: TLS upgrade complete for" << peerHost();
            emit tlsUpgradeComplete(true);
            if (m_tlsThread) {
                if (!m_tlsThread->wait(3000)) {
                    qWarning() << "PeerConnection: TLS thread did not exit within 3s, terminating"
                               << peerHost();
                    m_tlsThread->terminate();
                    m_tlsThread->wait(1000);
                }
                m_tlsThread->deleteLater();
                m_tlsThread = nullptr;
            }
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);

    connect(sslSocket, &QSslSocket::errorOccurred, this, [this, alive, sslSocket, mainThread](QAbstractSocket::SocketError err) {
        if (!alive->load()) return;  // PeerConnection 已析构，放弃
        if (m_tlsUpgradeInProgress) {
            sslSocket->abort();
            sslSocket->moveToThread(mainThread);
            // 立即退出 TLS 线程事件循环（同 encrypted 路径的理由）
            QThread::currentThread()->quit();
            QMetaObject::invokeMethod(this, [this, alive, sslSocket, err]() {
                if (!alive->load()) return;  // 二次检查
                sslSocket->setParent(this);
                m_tlsUpgradeInProgress = false;
                qWarning().noquote() << "PeerConnection: TLS upgrade failed:" << err
                                     << "peer=" << peerHost() << ":" << peerPort()
                                     << "role=" << (m_role == ConnectionRole::Client ? "Client" : "Server");
                emit tlsUpgradeComplete(false);
                if (m_socket) {
                    m_socket->abort();
                }
                if (!m_disconnectEmitted) {
                    m_disconnectEmitted = true;
                    emit disconnected();
                }
                if (m_tlsThread) {
                    if (!m_tlsThread->wait(3000)) {
                        qWarning() << "PeerConnection: TLS thread (error path) did not exit within 3s, terminating"
                                   << peerHost();
                        m_tlsThread->terminate();
                        m_tlsThread->wait(1000);
                    }
                    m_tlsThread->deleteLater();
                    m_tlsThread = nullptr;
                }
            }, Qt::QueuedConnection);
        }
    }, Qt::DirectConnection);

    // 在工作线程启动后执行 TLS 握手
    connect(m_tlsThread, &QThread::started, sslSocket, [this, alive, sslSocket]() {
        if (!alive->load()) return;  // PeerConnection 已析构，放弃握手
        if (m_role == ConnectionRole::Server) {
            sslSocket->startServerEncryption();
        } else {
            sslSocket->startClientEncryption();
        }
    });

    m_tlsThread->start();
}
