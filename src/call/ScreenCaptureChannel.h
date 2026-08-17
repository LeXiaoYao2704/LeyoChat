#pragma once

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QImage>
#include <QByteArray>
#include <QPointer>

#include <atomic>
#include <memory>

// 桌面共享发送端：定时捕获屏幕 → JPEG 压缩 → TCP 发送帧流
// 帧格式：[4字节帧长(big-endian)] [JPEG数据]
// 捕获和编码在线程池异步执行，不阻塞主线程
class ScreenCaptureChannel : public QObject {
    Q_OBJECT
public:
    explicit ScreenCaptureChannel(QObject* parent = nullptr);
    ~ScreenCaptureChannel() override;

    // 启动 TCP 服务器监听，返回端口（0=失败）
    quint16 startServer(quint16 port = 0);
    void stop();

    // 捕获参数
    void setFrameRate(int fps);          // 默认 10fps
    void setJpegQuality(int quality);    // 默认 50 (0-100)
    void setTargetScreen(int screenIndex); // 默认 0（主屏幕）

    bool isRunning() const;
    quint16 serverPort() const;

signals:
    void clientConnected();
    void clientDisconnected();
    void error(const QString& message);

private:
    void onNewConnection();
    void scheduleCaptureAsync();
    void sendEncodedFrame(const QByteArray& jpegData);

    QTcpServer m_server;
    QPointer<QTcpSocket> m_client;      // QPointer 防止悬挂指针
    QTimer m_captureTimer;
    int m_fps = 15;
    int m_jpegQuality = 85;
    int m_screenIndex = 0;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_captureInFlight{false};  // 防止捕获任务堆积
};
