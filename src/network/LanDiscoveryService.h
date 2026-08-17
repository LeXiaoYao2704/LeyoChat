#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QStringList>

class QTimer;
class QUdpSocket;

// ---------------------------------------------------------------------------
// LanDiscoveryService
//
// 通过 UDP 组播在局域网内自动发现在线对等端。
//
// 协议：
//   - 广播包格式（JSON UTF-8）：
//       {"v":1,"clientId":"...","displayName":"...","tcpPort":12345}
//   - 优先使用 UDP 组播（239.255.45.45），支持跨子网发现
//   - 同时向本机各网卡的定向广播地址发送（备用）
//   - 每隔 kAnnounceIntervalMs 广播一次自身信息
//   - 收到广播后若 clientId != 本机则 emit peerDiscovered
// ---------------------------------------------------------------------------
class LanDiscoveryService : public QObject {
    Q_OBJECT

public:
    static constexpr quint16 kDefaultUdpPort    = 45454;
    static constexpr int     kAnnounceIntervalMs = 30000;  // 千人规模需降低广播频率
    // 组播组地址（239.255.x.x 为组织本地范围，不会出网）
    static constexpr const char* kMulticastGroup = "239.255.45.45";

    struct Announcement {
        QString clientId;
        QString displayName;
        quint16 tcpPort = 0;
        QString appVersion;
        QStringList capabilities;
    };

    explicit LanDiscoveryService(QObject* parent = nullptr);
    ~LanDiscoveryService() override;

    // 启动广播与监听
    // udpPort == 0 时使用 kDefaultUdpPort
    bool start(const QString& clientId,
               const QString& displayName,
               quint16 tcpPort,
               quint16 udpPort = 0,
               const QString& appVersion = {},
               const QStringList& capabilities = {});

    void stop();
    bool isRunning() const;

    // 静态工具：序列化 / 反序列化广播包（暴露给测试）
    static QByteArray   encodeAnnouncement(const Announcement& ann);
    static Announcement decodeAnnouncement(const QByteArray& data, bool* ok = nullptr);

signals:
    // 发现一个新的或更新的对等端
    void peerDiscovered(const QString& clientId,
                        const QString& displayName,
                        quint16        tcpPort,
                        const QString& senderHost,
                        const QString& appVersion,
                        const QStringList& capabilities);

private slots:
    void onBroadcastTimer();
    void onDatagramReady();

private:
    void sendAnnouncement();

    QString    m_clientId;
    QString    m_displayName;
    QString    m_appVersion;
    QStringList m_capabilities;
    quint16    m_tcpPort  = 0;
    quint16    m_udpPort  = 0;
    QUdpSocket* m_socket  = nullptr;
    QTimer*    m_timer    = nullptr;
};
