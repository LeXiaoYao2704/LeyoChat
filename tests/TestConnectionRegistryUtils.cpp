#include <QtTest>

#include <algorithm>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include "ConnectionRegistryUtils.h"

class FakePeerConnection : public QObject {
public:
    explicit FakePeerConnection(bool connected, QObject* parent = nullptr)
        : QObject(parent)
        , connected_(connected)
    {
    }

    bool isConnected() const { return connected_; }
    void setConnected(bool connected) { connected_ = connected; }

private:
    bool connected_ = false;
};

class TestConnectionRegistryUtils : public QObject {
    Q_OBJECT

private slots:
    void removePointerEntries_removesAllAliasesForTarget();
    void removePointerEntries_cleansUpNullQPointers();
    void shouldKeepExistingPeerConnection_keepsCanonicalOutboundFromLowerId();
    void shouldKeepExistingPeerConnection_keepsCanonicalInboundFromLowerId();
    void duplicatePeerConnectionAction_keepsBothConnectionsForLegacyPeer();
    void duplicatePeerConnectionAction_usesCanonicalSingleConnectionForServerCapablePeer();
    void connectedConnectionForTarget_returnsRetainedConnectedIdentity();
    void connectedConnectionForTarget_ignoresDestroyedIdentity();
    void hasConnectedConnectionForTarget_detectsRetainedLegacyConnection();
    void hasConnectedConnectionForTarget_ignoresExcludedOrDisconnectedConnections();
    void shouldThrottlePeerHello_onlyForRepeatedRegisteredConnection();
    void shouldThrottlePeerHello_allowsFreshOrExpiredHello();
};

void TestConnectionRegistryUtils::removePointerEntries_removesAllAliasesForTarget()
{
    QObject first;
    QObject target;

    QHash<QString, QPointer<QObject>> entries;
    entries.insert(QStringLiteral("alpha"), &first);
    entries.insert(QStringLiteral("beta"), &target);
    entries.insert(QStringLiteral("gamma"), &target);

    QStringList removed = ConnectionRegistryUtils::removePointerEntries(entries, &target);
    std::sort(removed.begin(), removed.end());

    QCOMPARE(removed, (QStringList{QStringLiteral("beta"), QStringLiteral("gamma")}));
    QCOMPARE(entries.size(), 1);
    QVERIFY(entries.contains(QStringLiteral("alpha")));
}

void TestConnectionRegistryUtils::removePointerEntries_cleansUpNullQPointers()
{
    auto* obj = new QObject;
    QObject survivor;

    QHash<QString, QPointer<QObject>> entries;
    entries.insert(QStringLiteral("alive"), &survivor);
    entries.insert(QStringLiteral("doomed"), obj);

    // Destroying the object makes QPointer null
    delete obj;
    QVERIFY(entries.value(QStringLiteral("doomed")).isNull());

    // removePointerEntries with nullptr target should still clean null entries
    QStringList removed = ConnectionRegistryUtils::removePointerEntries(entries, static_cast<QObject*>(nullptr));
    QCOMPARE(removed, (QStringList{QStringLiteral("doomed")}));
    QCOMPARE(entries.size(), 1);
    QVERIFY(entries.contains(QStringLiteral("alive")));
}

void TestConnectionRegistryUtils::shouldKeepExistingPeerConnection_keepsCanonicalOutboundFromLowerId()
{
    QVERIFY(ConnectionRegistryUtils::shouldKeepExistingPeerConnection(
        QStringLiteral("client-a"),
        QStringLiteral("client-b"),
        true));
    QVERIFY(!ConnectionRegistryUtils::shouldKeepExistingPeerConnection(
        QStringLiteral("client-a"),
        QStringLiteral("client-b"),
        false));
}

void TestConnectionRegistryUtils::shouldKeepExistingPeerConnection_keepsCanonicalInboundFromLowerId()
{
    QVERIFY(ConnectionRegistryUtils::shouldKeepExistingPeerConnection(
        QStringLiteral("client-b"),
        QStringLiteral("client-a"),
        false));
    QVERIFY(!ConnectionRegistryUtils::shouldKeepExistingPeerConnection(
        QStringLiteral("client-b"),
        QStringLiteral("client-a"),
        true));
}

void TestConnectionRegistryUtils::duplicatePeerConnectionAction_keepsBothConnectionsForLegacyPeer()
{
    QCOMPARE(ConnectionRegistryUtils::duplicatePeerConnectionAction(
                 QStringLiteral("client-a"),
                 QStringLiteral("client-b"),
                 true,
                 QStringList{}),
             ConnectionRegistryUtils::DuplicatePeerConnectionAction::KeepBothPreferExisting);

    QCOMPARE(ConnectionRegistryUtils::duplicatePeerConnectionAction(
                 QStringLiteral("client-a"),
                 QStringLiteral("client-b"),
                 false,
                 QStringList{}),
             ConnectionRegistryUtils::DuplicatePeerConnectionAction::KeepBothPreferNew);
}

void TestConnectionRegistryUtils::duplicatePeerConnectionAction_usesCanonicalSingleConnectionForServerCapablePeer()
{
    const QStringList capabilities{
        QStringLiteral("remote_message_v1"),
        QStringLiteral("server_receive_v1"),
    };

    QCOMPARE(ConnectionRegistryUtils::duplicatePeerConnectionAction(
                 QStringLiteral("client-a"),
                 QStringLiteral("client-b"),
                 true,
                 capabilities),
             ConnectionRegistryUtils::DuplicatePeerConnectionAction::KeepExistingDropNew);

    QCOMPARE(ConnectionRegistryUtils::duplicatePeerConnectionAction(
                 QStringLiteral("client-a"),
                 QStringLiteral("client-b"),
                 false,
                 capabilities),
             ConnectionRegistryUtils::DuplicatePeerConnectionAction::DropExistingUseNew);
}

void TestConnectionRegistryUtils::connectedConnectionForTarget_returnsRetainedConnectedIdentity()
{
    FakePeerConnection disconnecting(false);
    FakePeerConnection retained(true);
    FakePeerConnection other(true);

    QHash<QString, QPointer<FakePeerConnection>> primaryConnections;
    primaryConnections.insert(QStringLiteral("peer-a"), &disconnecting);
    primaryConnections.insert(QStringLiteral("peer-b"), &other);

    ConnectionRegistryUtils::ConnectionIdentityRegistry<FakePeerConnection> identities;
    identities.insert(&disconnecting, QStringLiteral("peer-a"));
    identities.insert(&retained, QStringLiteral("peer-a"));
    identities.insert(&other, QStringLiteral("peer-b"));

    QCOMPARE(ConnectionRegistryUtils::connectedConnectionForTarget(
                 primaryConnections,
                 identities,
                 QStringLiteral("peer-a"),
                 &disconnecting),
             &retained);
}

void TestConnectionRegistryUtils::connectedConnectionForTarget_ignoresDestroyedIdentity()
{
    auto* destroyed = new FakePeerConnection(true);

    QHash<QString, QPointer<FakePeerConnection>> primaryConnections;
    ConnectionRegistryUtils::ConnectionIdentityRegistry<FakePeerConnection> identities;
    identities.insert(destroyed, QStringLiteral("peer-a"));

    delete destroyed;

    QCOMPARE(ConnectionRegistryUtils::connectedConnectionForTarget(
                 primaryConnections,
                 identities,
                 QStringLiteral("peer-a")),
             nullptr);
    QCOMPARE(identities.size(), 0);
}

void TestConnectionRegistryUtils::hasConnectedConnectionForTarget_detectsRetainedLegacyConnection()
{
    FakePeerConnection disconnecting(true);
    FakePeerConnection retained(true);

    QHash<QString, QPointer<FakePeerConnection>> primaryConnections;
    primaryConnections.insert(QStringLiteral("peer-a"), &disconnecting);

    ConnectionRegistryUtils::ConnectionIdentityRegistry<FakePeerConnection> identities;
    identities.insert(&disconnecting, QStringLiteral("peer-a"));
    identities.insert(&retained, QStringLiteral("peer-a"));

    QVERIFY(ConnectionRegistryUtils::hasConnectedConnectionForTarget(
        primaryConnections,
        identities,
        QStringLiteral("peer-a"),
        &disconnecting));
}

void TestConnectionRegistryUtils::hasConnectedConnectionForTarget_ignoresExcludedOrDisconnectedConnections()
{
    FakePeerConnection disconnecting(true);
    FakePeerConnection disconnected(false);

    QHash<QString, QPointer<FakePeerConnection>> primaryConnections;
    primaryConnections.insert(QStringLiteral("peer-a"), &disconnecting);

    ConnectionRegistryUtils::ConnectionIdentityRegistry<FakePeerConnection> identities;
    identities.insert(&disconnecting, QStringLiteral("peer-a"));
    identities.insert(&disconnected, QStringLiteral("peer-a"));

    QVERIFY(!ConnectionRegistryUtils::hasConnectedConnectionForTarget(
        primaryConnections,
        identities,
        QStringLiteral("peer-a"),
        &disconnecting));
}

void TestConnectionRegistryUtils::shouldThrottlePeerHello_onlyForRepeatedRegisteredConnection()
{
    QVERIFY(ConnectionRegistryUtils::shouldThrottlePeerHello(
        true,
        true,
        10'000,
        9'500,
        5'000));
    QVERIFY(!ConnectionRegistryUtils::shouldThrottlePeerHello(
        false,
        true,
        10'000,
        9'500,
        5'000));
}

void TestConnectionRegistryUtils::shouldThrottlePeerHello_allowsFreshOrExpiredHello()
{
    QVERIFY(!ConnectionRegistryUtils::shouldThrottlePeerHello(
        true,
        false,
        10'000,
        0,
        5'000));
    QVERIFY(!ConnectionRegistryUtils::shouldThrottlePeerHello(
        true,
        true,
        10'000,
        4'000,
        5'000));
}

QTEST_MAIN(TestConnectionRegistryUtils)

#include "TestConnectionRegistryUtils.moc"
