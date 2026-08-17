#include "network/LanDiscoveryService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QTimer>
#include <QUdpSocket>

namespace {

QStringList stringArray(const QJsonValue& value)
{
    QStringList result;
    if (!value.isArray()) {
        return result;
    }

    const QJsonArray array = value.toArray();
    for (const QJsonValue& item : array) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            result.push_back(text);
        }
    }
    return result;
}

QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : values) {
        const QString text = value.trimmed();
        if (!text.isEmpty()) {
            array.push_back(text);
        }
    }
    return array;
}

}  // namespace

LanDiscoveryService::LanDiscoveryService(QObject* parent)
    : QObject(parent) {}

LanDiscoveryService::~LanDiscoveryService() {
    stop();
}

bool LanDiscoveryService::start(const QString& clientId,
                                 const QString& displayName,
                                 quint16         tcpPort,
                                 quint16         udpPort,
                                 const QString& appVersion,
                                 const QStringList& capabilities) {
    if (isRunning()) {
        stop();
    }

    m_clientId    = clientId;
    m_displayName = displayName;
    m_appVersion  = appVersion.trimmed();
    m_capabilities = capabilities;
    m_tcpPort     = tcpPort;
    m_udpPort     = (udpPort == 0) ? kDefaultUdpPort : udpPort;

    m_socket = new QUdpSocket(this);

    // 允许多个进程绑定同一端口（同机器多实例）
    m_socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);
    if (!m_socket->bind(QHostAddress::AnyIPv4, m_udpPort,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    // 加入组播组，使 socket 能收到组播包（跨子网发现的关键）
    const QHostAddress multicastGroup(QString::fromLatin1(kMulticastGroup));
    // 在所有可用接口上加入组播组，确保每个网卡都能收
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        m_socket->joinMulticastGroup(multicastGroup, iface);
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &LanDiscoveryService::onDatagramReady);

    m_timer = new QTimer(this);
    m_timer->setInterval(kAnnounceIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &LanDiscoveryService::onBroadcastTimer);
    m_timer->start();

    // 随机抖动：首次启动延迟 0~3s，避免批量上线时所有客户端同步广播
    m_timer->setInterval(kAnnounceIntervalMs + (QRandomGenerator::global()->bounded(3000)));

    // 立刻广播一次，无需等待第一个 interval
    sendAnnouncement();
    return true;
}

void LanDiscoveryService::stop() {
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }
}

bool LanDiscoveryService::isRunning() const {
    return m_socket != nullptr;
}

// ---------------------------------------------------------------------------
// 静态工具
// ---------------------------------------------------------------------------
QByteArray LanDiscoveryService::encodeAnnouncement(const Announcement& ann) {
    QJsonObject obj;
    obj[QStringLiteral("v")]           = 2;
    obj[QStringLiteral("clientId")]    = ann.clientId;
    obj[QStringLiteral("displayName")] = ann.displayName;
    obj[QStringLiteral("tcpPort")]     = static_cast<int>(ann.tcpPort);
    if (!ann.appVersion.trimmed().isEmpty()) {
        obj[QStringLiteral("appVersion")] = ann.appVersion.trimmed();
    }
    const QJsonArray capabilities = toJsonArray(ann.capabilities);
    if (!capabilities.isEmpty()) {
        obj[QStringLiteral("capabilities")] = capabilities;
    }
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

LanDiscoveryService::Announcement LanDiscoveryService::decodeAnnouncement(const QByteArray& data, bool* ok) {
    Announcement ann;
    if (data.isEmpty()) {
        if (ok) *ok = false;
        return ann;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        if (ok) *ok = false;
        return ann;
    }

    const QJsonObject obj = doc.object();
    if (!obj.contains(QStringLiteral("clientId"))
        || !obj.contains(QStringLiteral("tcpPort"))) {
        if (ok) *ok = false;
        return ann;
    }

    ann.clientId    = obj[QStringLiteral("clientId")].toString();
    ann.displayName = obj[QStringLiteral("displayName")].toString();
    ann.tcpPort     = static_cast<quint16>(obj[QStringLiteral("tcpPort")].toInt());
    ann.appVersion  = obj[QStringLiteral("appVersion")].toString().trimmed();
    ann.capabilities = stringArray(obj.value(QStringLiteral("capabilities")));

    if (ann.clientId.isEmpty() || ann.tcpPort == 0) {
        if (ok) *ok = false;
        return ann;
    }

    if (ok) *ok = true;
    return ann;
}

// ---------------------------------------------------------------------------
// 私有槽
// ---------------------------------------------------------------------------
void LanDiscoveryService::onBroadcastTimer() {
    sendAnnouncement();
}

void LanDiscoveryService::onDatagramReady() {
    if (!m_socket) {
        return;
    }

    while (m_socket->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress senderAddr;
        quint16      senderPort = 0;
        m_socket->readDatagram(buf.data(), buf.size(), &senderAddr, &senderPort);

        bool ok = false;
        const Announcement ann = decodeAnnouncement(buf, &ok);
        if (!ok) {
            continue;
        }

        // 忽略自己的广播
        if (ann.clientId == m_clientId) {
            continue;
        }

        // 取纯 IPv4 地址（去除 ::ffff: 前缀）
        QString host = senderAddr.toString();
        if (host.startsWith(QStringLiteral("::ffff:"))) {
            host = host.mid(7);
        }

        emit peerDiscovered(ann.clientId,
                            ann.displayName,
                            ann.tcpPort,
                            host,
                            ann.appVersion,
                            ann.capabilities);
    }
}

void LanDiscoveryService::sendAnnouncement() {
    if (!m_socket) {
        return;
    }

    Announcement ann;
    ann.clientId    = m_clientId;
    ann.displayName = m_displayName;
    ann.tcpPort     = m_tcpPort;
    ann.appVersion  = m_appVersion;
    ann.capabilities = m_capabilities;

    const QByteArray payload = encodeAnnouncement(ann);
    const QHostAddress multicastGroup(QString::fromLatin1(kMulticastGroup));

    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& iface : interfaces) {
        const auto flags = iface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }

        bool hasIPv4 = false;
        QHostAddress bcast;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                hasIPv4 = true;
                if (!entry.broadcast().isNull()) {
                    bcast = entry.broadcast();
                }
            }
        }
        if (!hasIPv4) {
            continue;
        }

        // ① 对每张网卡单独设置组播出口后发包
        //    Windows 上不设 setMulticastInterface 则只从默认路由出口发，
        //    其他网卡上的对端根本收不到
        m_socket->setMulticastInterface(iface);
        m_socket->writeDatagram(payload, multicastGroup, m_udpPort);

        // ② 定向广播备用（同子网内兼容）
        if (!bcast.isNull()) {
            m_socket->writeDatagram(payload, bcast, m_udpPort);
        }
    }

    // 重置组播出口为默认
    m_socket->setMulticastInterface(QNetworkInterface());

    // 每次广播后重新施加随机抖动 (±2s)，避免千人场景下广播同步
    if (m_timer) {
        const int jitter = static_cast<int>(QRandomGenerator::global()->bounded(4001)) - 2000;
        m_timer->setInterval(kAnnounceIntervalMs + jitter);
    }
}
