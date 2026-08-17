#pragma once

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QAudioSource>
#include <QAudioSink>
#include <QIODevice>
#include <QByteArray>

#include <memory>

// 音频通道：采集本地麦克风 → UDP 发送 → 接收对端音频 → 本地播放
// LAN 环境直接传 PCM：16kHz, 16bit, 单声道 = 32KB/s
class AudioChannel : public QObject {
    Q_OBJECT
public:
    explicit AudioChannel(QObject* parent = nullptr);
    ~AudioChannel() override;

    // 绑定本地 UDP 端口（0 = 系统分配），返回实际端口
    quint16 bind(quint16 port = 0);

    // 设置对端地址和端口
    void setRemoteEndpoint(const QHostAddress& host, quint16 port);

    // 开始/停止音频流
    void start();
    void stop();

    // 静音控制（停止采集但继续播放对端音频）
    void setMuted(bool muted);
    bool isMuted() const;

    quint16 localPort() const;

signals:
    void error(const QString& message);

private:
    void onCaptureReady();
    void onDatagramReady();

    QUdpSocket m_socket;
    QHostAddress m_remoteHost;
    quint16 m_remotePort = 0;
    bool m_muted = false;

    // Qt Multimedia 采集/播放
    std::unique_ptr<QAudioSource> m_audioSource;
    std::unique_ptr<QAudioSink> m_audioSink;
    QIODevice* m_captureDevice = nullptr;   // 由 QAudioSource::start() 返回
    QIODevice* m_playbackDevice = nullptr;  // 由 QAudioSink::start() 返回
};
