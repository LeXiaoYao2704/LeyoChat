#include "call/AudioChannel.h"

#include <QAudioFormat>
#include <QMediaDevices>
#include <QAudioDevice>

// LAN 环境直接传 PCM：16kHz, 16bit, 单声道 = 32KB/s，LAN 完全无压力
static QAudioFormat defaultAudioFormat()
{
    QAudioFormat fmt;
    fmt.setSampleRate(16000);
    fmt.setChannelCount(1);
    fmt.setSampleFormat(QAudioFormat::Int16);
    return fmt;
}

// 每 20ms 发送一帧：16000 * 2 * 1 * 0.02 = 640 bytes
static constexpr int kFrameBytes = 640;

AudioChannel::AudioChannel(QObject* parent)
    : QObject(parent)
{
    connect(&m_socket, &QUdpSocket::readyRead, this, &AudioChannel::onDatagramReady);
}

AudioChannel::~AudioChannel()
{
    stop();
}

quint16 AudioChannel::bind(quint16 port)
{
    if (!m_socket.bind(QHostAddress::AnyIPv4, port)) {
        emit error(QStringLiteral("UDP 绑定失败: %1").arg(m_socket.errorString()));
        return 0;
    }
    return m_socket.localPort();
}

void AudioChannel::setRemoteEndpoint(const QHostAddress& host, quint16 port)
{
    m_remoteHost = host;
    m_remotePort = port;
}

void AudioChannel::start()
{
    const QAudioFormat fmt = defaultAudioFormat();

    // 采集
    const QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();
    if (inputDevice.isNull()) {
        emit error(QStringLiteral("未找到麦克风设备"));
        return;
    }
    m_audioSource = std::make_unique<QAudioSource>(inputDevice, fmt, this);
    m_audioSource->setBufferSize(kFrameBytes * 4);
    m_captureDevice = m_audioSource->start();
    if (!m_captureDevice) {
        emit error(QStringLiteral("麦克风设备打开失败"));
        m_audioSource.reset();
        return;
    }
    connect(m_captureDevice, &QIODevice::readyRead, this, &AudioChannel::onCaptureReady);

    // 播放
    const QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (outputDevice.isNull()) {
        emit error(QStringLiteral("未找到音频输出设备"));
        m_audioSource->stop();
        m_audioSource.reset();
        m_captureDevice = nullptr;
        return;
    }
    m_audioSink = std::make_unique<QAudioSink>(outputDevice, fmt, this);
    m_audioSink->setBufferSize(kFrameBytes * 8);
    m_playbackDevice = m_audioSink->start();
}

void AudioChannel::stop()
{
    if (m_audioSource) {
        m_audioSource->stop();
        m_audioSource.reset();
    }
    if (m_audioSink) {
        m_audioSink->stop();
        m_audioSink.reset();
    }
    m_captureDevice = nullptr;
    m_playbackDevice = nullptr;
}

void AudioChannel::setMuted(bool muted)
{
    m_muted = muted;
}

bool AudioChannel::isMuted() const
{
    return m_muted;
}

quint16 AudioChannel::localPort() const
{
    return m_socket.localPort();
}

void AudioChannel::onCaptureReady()
{
    if (!m_captureDevice || m_muted || m_remotePort == 0) return;

    while (m_captureDevice->bytesAvailable() >= kFrameBytes) {
        const QByteArray frame = m_captureDevice->read(kFrameBytes);
        if (frame.size() == kFrameBytes) {
            m_socket.writeDatagram(frame, m_remoteHost, m_remotePort);
        }
    }
}

void AudioChannel::onDatagramReady()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(static_cast<int>(m_socket.pendingDatagramSize()));
        m_socket.readDatagram(datagram.data(), datagram.size());

        if (m_playbackDevice && datagram.size() > 0) {
            m_playbackDevice->write(datagram);
        }
    }
}
