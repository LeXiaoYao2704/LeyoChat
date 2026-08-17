#include "call/ScreenCaptureChannel.h"

#include <QApplication>
#include <QScreen>
#include <QBuffer>
#include <QtConcurrent>
#include <QtEndian>

ScreenCaptureChannel::ScreenCaptureChannel(QObject* parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &ScreenCaptureChannel::onNewConnection);
    connect(&m_captureTimer, &QTimer::timeout, this, &ScreenCaptureChannel::scheduleCaptureAsync);
}

ScreenCaptureChannel::~ScreenCaptureChannel()
{
    stop();
}

quint16 ScreenCaptureChannel::startServer(quint16 port)
{
    if (!m_server.listen(QHostAddress::AnyIPv4, port)) {
        emit error(QStringLiteral("桌面共享服务监听失败: %1").arg(m_server.errorString()));
        return 0;
    }
    m_running.store(true);
    return m_server.serverPort();
}

void ScreenCaptureChannel::stop()
{
    m_running.store(false);
    m_captureTimer.stop();
    m_captureInFlight.store(false);
    if (m_client) {
        QTcpSocket* client = m_client;
        m_client = nullptr;           // 先清空，防止 disconnected 信号 lambda 重入
        client->disconnectFromHost();
        client->deleteLater();
    }
    m_server.close();
}

void ScreenCaptureChannel::setFrameRate(int fps)
{
    m_fps = qBound(1, fps, 30);
    if (m_captureTimer.isActive()) {
        m_captureTimer.setInterval(1000 / m_fps);
    }
}

void ScreenCaptureChannel::setJpegQuality(int quality)
{
    m_jpegQuality = qBound(10, quality, 100);
}

void ScreenCaptureChannel::setTargetScreen(int screenIndex)
{
    m_screenIndex = screenIndex;
}

bool ScreenCaptureChannel::isRunning() const { return m_running.load(); }
quint16 ScreenCaptureChannel::serverPort() const { return m_server.serverPort(); }

void ScreenCaptureChannel::onNewConnection()
{
    QTcpSocket* socket = m_server.nextPendingConnection();
    if (!socket) return;

    // 只支持一个观看者
    if (m_client) {
        socket->disconnectFromHost();
        socket->deleteLater();
        return;
    }

    m_client = socket;
    connect(m_client, &QTcpSocket::disconnected, this, [this]() {
        m_captureTimer.stop();
        if (m_client) {
            m_client->deleteLater();
            m_client = nullptr;
        }
        emit clientDisconnected();
    });

    emit clientConnected();
    m_captureTimer.start(1000 / m_fps);
}

void ScreenCaptureChannel::scheduleCaptureAsync()
{
    if (!m_client || !m_running.load()) return;

    // 上一帧还没编码完，跳过本帧，防止线程池任务堆积
    if (m_captureInFlight.exchange(true)) return;

    // grabWindow + toImage 必须在主线程调用（Qt 限制: QPixmap 不可跨线程）
    // toImage 后产生的 QImage 是线程安全的，可以安全传入线程池
    const auto screens = QApplication::screens();
    if (m_screenIndex < 0 || m_screenIndex >= screens.size()) {
        m_captureInFlight.store(false);
        return;
    }
    QScreen* screen = screens[m_screenIndex];
    QImage image = screen->grabWindow(0).toImage();  // 主线程完成 QPixmap→QImage

    const int quality = m_jpegQuality;
    QPointer<ScreenCaptureChannel> self = this;

    // 缩放 + JPEG 编码在线程池异步执行，不阻塞主线程
    (void)QtConcurrent::run([image = std::move(image), quality, self]() mutable {
        QByteArray jpegData;
        QBuffer buffer(&jpegData);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "JPEG", quality);
        buffer.close();

        // 回到主线程发送（先检查 QPointer 再 invokeMethod，避免 nullptr 上下文）
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, jpegData = std::move(jpegData)]() {
            if (!self) return;
            self->m_captureInFlight.store(false);
            self->sendEncodedFrame(jpegData);
        });
    });
}

void ScreenCaptureChannel::sendEncodedFrame(const QByteArray& jpegData)
{
    if (!m_client || !m_client->isOpen() || jpegData.isEmpty()) return;

    // 背压控制：如果 TCP 写缓冲积压超过 2MB，跳过本帧防止内存持续增长
    constexpr qint64 kMaxPendingBytes = 2 * 1024 * 1024;
    if (m_client->bytesToWrite() > kMaxPendingBytes) return;

    // 帧格式：[4字节大端帧长][JPEG数据]
    quint32 frameSize = static_cast<quint32>(jpegData.size());
    quint32 frameSizeBE = qToBigEndian(frameSize);
    m_client->write(reinterpret_cast<const char*>(&frameSizeBE), 4);
    m_client->write(jpegData);
}
