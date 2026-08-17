#include <QtTest/QTest>
#include <QSignalSpy>
#include <QUdpSocket>

#include "network/LanDiscoveryService.h"
#include "services/MessageRoutingCapabilities.h"

// ---------------------------------------------------------------------------
// 测试：LanDiscoveryService 广播包的序列化与解析
// 用单进程内两个 socket 互发来测试，避免依赖真实网络接口。
// ---------------------------------------------------------------------------
class TestLanDiscovery : public QObject {
    Q_OBJECT

private slots:
    void encodeDecodeRoundTrip() {
        LanDiscoveryService::Announcement ann;
        ann.clientId    = QStringLiteral("user-abc");
        ann.displayName = QStringLiteral("张三");
        ann.tcpPort     = 52301;

        const QByteArray encoded = LanDiscoveryService::encodeAnnouncement(ann);
        QVERIFY(!encoded.isEmpty());

        bool ok = false;
        const auto decoded = LanDiscoveryService::decodeAnnouncement(encoded, &ok);
        QVERIFY(ok);
        QCOMPARE(decoded.clientId,    ann.clientId);
        QCOMPARE(decoded.displayName, ann.displayName);
        QCOMPARE(decoded.tcpPort,     ann.tcpPort);
    }

    void encodeDecode_preservesCapabilities() {
        LanDiscoveryService::Announcement ann;
        ann.clientId = QStringLiteral("client-new");
        ann.displayName = QStringLiteral("Alice");
        ann.tcpPort = 50123;
        ann.appVersion = QStringLiteral("0.2.0");
        ann.capabilities = QStringList{
            MessageRoutingCapabilities::remoteMessageV1(),
            MessageRoutingCapabilities::serverReceiveV1()
        };

        bool ok = false;
        const auto decoded =
            LanDiscoveryService::decodeAnnouncement(
                LanDiscoveryService::encodeAnnouncement(ann), &ok);

        QVERIFY(ok);
        QCOMPARE(decoded.clientId, ann.clientId);
        QCOMPARE(decoded.appVersion, ann.appVersion);
        QVERIFY(decoded.capabilities.contains(
            MessageRoutingCapabilities::serverReceiveV1()));
    }

    void decode_oldV1AnnouncementHasNoCapabilities() {
        const QByteArray oldPacket =
            R"({"v":1,"clientId":"legacy","displayName":"Bob","tcpPort":50001})";

        bool ok = false;
        const auto decoded =
            LanDiscoveryService::decodeAnnouncement(oldPacket, &ok);

        QVERIFY(ok);
        QCOMPARE(decoded.clientId, QStringLiteral("legacy"));
        QVERIFY(decoded.appVersion.isEmpty());
        QVERIFY(decoded.capabilities.isEmpty());
    }

    void decodeInvalidData_returnsFalse() {
        bool ok = true;
        LanDiscoveryService::decodeAnnouncement(QByteArray("garbage{}data"), &ok);
        QVERIFY(!ok);
    }

    void decodeEmptyData_returnsFalse() {
        bool ok = true;
        LanDiscoveryService::decodeAnnouncement(QByteArray(), &ok);
        QVERIFY(!ok);
    }

    void service_emitsPeerDiscovered_whenReceivingBroadcast() {
        // 两台 service，用本地回环互发
        LanDiscoveryService sender;
        LanDiscoveryService receiver;

        // 选不同端口，避免绑定冲突
        const quint16 receiverUdpPort = 47202;

        QVERIFY(receiver.start(QStringLiteral("peer-recv"), QStringLiteral("接收方"), 9001, receiverUdpPort));

        QSignalSpy spy(&receiver, &LanDiscoveryService::peerDiscovered);

        // 手动模拟 sender 发一包到 receiver 的端口
        QUdpSocket probe;
        LanDiscoveryService::Announcement ann;
        ann.clientId    = QStringLiteral("peer-send");
        ann.displayName = QStringLiteral("发送方");
        ann.tcpPort     = 9002;
        const QByteArray payload = LanDiscoveryService::encodeAnnouncement(ann);
        probe.writeDatagram(payload, QHostAddress::LocalHost, receiverUdpPort);

        // 等最多 500ms
        spy.wait(500);

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("peer-send"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("发送方"));
        QCOMPARE(args.at(2).value<quint16>(), quint16(9002));

        receiver.stop();
    }

    void service_emitsPeerDiscovered_withCapabilities() {
        LanDiscoveryService receiver;
        const quint16 receiverUdpPort = 47204;

        QVERIFY(receiver.start(QStringLiteral("peer-recv"),
                               QStringLiteral("receiver"),
                               9001,
                               receiverUdpPort));

        QSignalSpy spy(&receiver, &LanDiscoveryService::peerDiscovered);

        QUdpSocket probe;
        LanDiscoveryService::Announcement ann;
        ann.clientId = QStringLiteral("peer-new");
        ann.displayName = QStringLiteral("sender");
        ann.tcpPort = 9002;
        ann.appVersion = QStringLiteral("0.2.0");
        ann.capabilities = QStringList{
            MessageRoutingCapabilities::remoteMessageV1(),
            MessageRoutingCapabilities::serverReceiveV1()
        };
        probe.writeDatagram(LanDiscoveryService::encodeAnnouncement(ann),
                            QHostAddress::LocalHost,
                            receiverUdpPort);

        spy.wait(500);

        QCOMPARE(spy.count(), 1);
        const auto args = spy.takeFirst();
        QVERIFY(args.size() >= 6);
        QCOMPARE(args.at(0).toString(), QStringLiteral("peer-new"));
        QCOMPARE(args.at(4).toString(), QStringLiteral("0.2.0"));
        QVERIFY(args.at(5).toStringList().contains(
            MessageRoutingCapabilities::serverReceiveV1()));

        receiver.stop();
    }

    void service_ignoresOwnAnnouncement() {
        // service 不应把自己广播的包当成发现事件
        LanDiscoveryService service;
        const quint16 udpPort = 47203;
        QVERIFY(service.start(QStringLiteral("self-id"), QStringLiteral("我自己"), 9003, udpPort));

        QSignalSpy spy(&service, &LanDiscoveryService::peerDiscovered);

        // 发一个 clientId == 自己的包
        QUdpSocket probe;
        LanDiscoveryService::Announcement ann;
        ann.clientId    = QStringLiteral("self-id");  // 与 service 的 clientId 相同
        ann.displayName = QStringLiteral("我自己");
        ann.tcpPort     = 9003;
        probe.writeDatagram(LanDiscoveryService::encodeAnnouncement(ann),
                            QHostAddress::LocalHost, udpPort);

        spy.wait(300);
        QCOMPARE(spy.count(), 0);  // 不应触发信号

        service.stop();
    }
};

QTEST_MAIN(TestLanDiscovery)
#include "TestLanDiscovery.moc"
