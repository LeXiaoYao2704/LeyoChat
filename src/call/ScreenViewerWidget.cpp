#include "call/ScreenViewerWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QtEndian>

ScreenViewerWidget::ScreenViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(640, 480);
    setWindowTitle(QStringLiteral("桌面共享"));
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    connect(&m_socket, &QTcpSocket::readyRead, this, &ScreenViewerWidget::onReadyRead);
    connect(&m_socket, &QTcpSocket::connected, this, &ScreenViewerWidget::connected);
    connect(&m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_currentFrame = QImage();
        update();
        emit disconnected();
    });
    connect(&m_socket, &QTcpSocket::errorOccurred, this, [this]() {
        emit error(m_socket.errorString());
    });
}

ScreenViewerWidget::~ScreenViewerWidget()
{
    disconnectFromHost();
}

void ScreenViewerWidget::connectToHost(const QString& host, quint16 port)
{
    m_socket.connectToHost(host, port);
}

void ScreenViewerWidget::disconnectFromHost()
{
    m_socket.disconnectFromHost();
    m_receiveBuffer.clear();
    m_readOffset = 0;
    m_expectedFrameSize = 0;
}

void ScreenViewerWidget::setRemoteControlEnabled(bool enabled)
{
    m_remoteControlEnabled = enabled;
    setCursor(enabled ? Qt::CrossCursor : Qt::ArrowCursor);
}

bool ScreenViewerWidget::isRemoteControlEnabled() const
{
    return m_remoteControlEnabled;
}

void ScreenViewerWidget::onReadyRead()
{
    m_receiveBuffer.append(m_socket.readAll());

    // 安全上限：buffer 总量不超过 50MB，防止发送端速率高于解码速率导致 OOM
    constexpr int kMaxBufferBytes = 50 * 1024 * 1024;
    if (m_receiveBuffer.size() > kMaxBufferBytes) {
        emit error(QStringLiteral("\u63a5\u6536\u7f13\u51b2\u533a\u6ea2\u51fa"));
        m_socket.disconnectFromHost();
        m_receiveBuffer.clear();
        m_expectedFrameSize = 0;
        m_readOffset = 0;
        return;
    }

    while (true) {
        const int available = m_receiveBuffer.size() - m_readOffset;
        if (m_expectedFrameSize == 0) {
            if (available < 4) break;
            quint32 sizeBE;
            memcpy(&sizeBE, m_receiveBuffer.constData() + m_readOffset, 4);
            m_expectedFrameSize = qFromBigEndian(sizeBE);
            m_readOffset += 4;
            // 安全上限 10MB
            if (m_expectedFrameSize > 10 * 1024 * 1024) {
                emit error(QStringLiteral("帧大小异常"));
                m_socket.disconnectFromHost();
                return;
            }
        }

        const int availableAfterHeader = m_receiveBuffer.size() - m_readOffset;
        if (availableAfterHeader < static_cast<int>(m_expectedFrameSize)) break;

        QByteArray frameData = m_receiveBuffer.mid(m_readOffset, static_cast<int>(m_expectedFrameSize));
        m_readOffset += static_cast<int>(m_expectedFrameSize);
        m_expectedFrameSize = 0;

        processFrame(frameData);
    }

    // 已消费的数据超过 64KB 时做一次 compact，避免 buffer 前端空洞累积
    if (m_readOffset > 65536) {
        m_receiveBuffer.remove(0, m_readOffset);
        m_readOffset = 0;
    }
}

void ScreenViewerWidget::processFrame(const QByteArray& jpegData)
{
    QImage frame;
    if (frame.loadFromData(jpegData, "JPEG")) {
        m_currentFrame = frame;
        m_remoteScreenSize = frame.size();
        // FastTransformation: 每帧都会执行，用快速缩放避免主线程阻塞（~1ms vs ~8ms）
        m_scaledFrame = frame.scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation);
        m_scaledForSize = size();
        update();
    }
}

void ScreenViewerWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_currentFrame.isNull()) {
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("等待共享画面..."));
        return;
    }

    // 窗口尺寸变化时重新缩放，否则用缓存
    if (m_scaledFrame.isNull() || m_scaledForSize != size()) {
        m_scaledFrame = m_currentFrame.scaled(size(), Qt::KeepAspectRatio, Qt::FastTransformation);
        m_scaledForSize = size();
    }
    const int x = (width() - m_scaledFrame.width()) / 2;
    const int y = (height() - m_scaledFrame.height()) / 2;
    painter.drawImage(x, y, m_scaledFrame);
}

QPoint ScreenViewerWidget::mapToRemote(const QPoint& localPos) const
{
    if (m_currentFrame.isNull() || m_remoteScreenSize.isEmpty() || m_scaledFrame.isNull()) return localPos;

    // 使用缓存的缩放图尺寸，与 paintEvent 保持一致
    const int offsetX = (width() - m_scaledFrame.width()) / 2;
    const int offsetY = (height() - m_scaledFrame.height()) / 2;

    const double scaleX = static_cast<double>(m_remoteScreenSize.width()) / m_scaledFrame.width();
    const double scaleY = static_cast<double>(m_remoteScreenSize.height()) / m_scaledFrame.height();

    return QPoint(
        static_cast<int>((localPos.x() - offsetX) * scaleX),
        static_cast<int>((localPos.y() - offsetY) * scaleY)
    );
}

void ScreenViewerWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_remoteControlEnabled) {
        const QPoint remote = mapToRemote(event->pos());
        emit remoteMouseEvent(0 /*press*/, remote.x(), remote.y(), static_cast<int>(event->button()));
    }
}

void ScreenViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_remoteControlEnabled) {
        const QPoint remote = mapToRemote(event->pos());
        emit remoteMouseEvent(1 /*release*/, remote.x(), remote.y(), static_cast<int>(event->button()));
    }
}

void ScreenViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_remoteControlEnabled) {
        const QPoint remote = mapToRemote(event->pos());
        emit remoteMouseEvent(2 /*move*/, remote.x(), remote.y(), 0);
    }
}

void ScreenViewerWidget::keyPressEvent(QKeyEvent* event)
{
    if (m_remoteControlEnabled) {
        emit remoteKeyEvent(0 /*press*/, event->key(), static_cast<int>(event->modifiers()));
    }
}

void ScreenViewerWidget::keyReleaseEvent(QKeyEvent* event)
{
    if (m_remoteControlEnabled) {
        emit remoteKeyEvent(1 /*release*/, event->key(), static_cast<int>(event->modifiers()));
    }
}

void ScreenViewerWidget::wheelEvent(QWheelEvent* event)
{
    if (m_remoteControlEnabled) {
        emit remoteMouseEvent(3 /*wheel*/, event->angleDelta().x(), event->angleDelta().y(), 0);
    }
}
