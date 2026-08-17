#pragma once

#include <QWidget>
#include <QTcpSocket>
#include <QImage>
#include <QByteArray>

// 桌面共享观看端：TCP 连接到共享端 → 接收 JPEG 帧流 → 显示
// 支持远程控制时转发鼠标/键盘事件
class ScreenViewerWidget : public QWidget {
    Q_OBJECT
public:
    explicit ScreenViewerWidget(QWidget* parent = nullptr);
    ~ScreenViewerWidget() override;

    void connectToHost(const QString& host, quint16 port);
    void disconnectFromHost();

    void setRemoteControlEnabled(bool enabled);
    bool isRemoteControlEnabled() const;

signals:
    void connected();
    void disconnected();
    void error(const QString& message);

    // 远程控制事件（发送给共享端）
    void remoteMouseEvent(int type, int x, int y, int button);
    void remoteKeyEvent(int type, int key, int modifiers);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void onReadyRead();
    void processFrame(const QByteArray& jpegData);
    QPoint mapToRemote(const QPoint& localPos) const;

    QTcpSocket m_socket;
    QImage m_currentFrame;
    QImage m_scaledFrame;          // 预缩放缓存，避免 paintEvent 重复缩放
    QSize m_scaledForSize;         // 缓存对应的窗口尺寸
    QByteArray m_receiveBuffer;
    int m_readOffset = 0;          // 读偏移量，避免频繁 remove(0,n) O(n) memmove
    quint32 m_expectedFrameSize = 0;
    bool m_remoteControlEnabled = false;
    QSize m_remoteScreenSize;
};
