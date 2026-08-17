#include <QApplication>
#include <QtTest/QTest>

#include <QDateTime>

#include "domain/PeerEndpoint.h"
#include "ui/ContactListModel.h"

namespace {
QModelIndex firstContactIndex(const ContactListModel& model)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (!index.data(ContactListModel::IsSectionHeaderRole).toBool()) {
            return index;
        }
    }
    return {};
}
}

class TestContactListModel : public QObject {
    Q_OBJECT

private slots:
    void recentUdpHeartbeatShowsOnlineWithoutTcpConnection()
    {
        ContactListModel model;
        PeerEndpoint peer;
        peer.clientId = "peer-udp";
        peer.displayName = "Udp";
        peer.host = "192.0.2.30";
        peer.port = 45454;
        peer.isConnected = false;
        peer.presence = PeerPresenceStatus::Online;
        peer.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch();

        model.setItems({peer});

        const QModelIndex index = firstContactIndex(model);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(ContactListModel::StatusTextRole).toString(),
                 QStringLiteral("\u5728\u7EBF"));
    }

    void staleConnectedPeerFallsBackFromOnline()
    {
        ContactListModel model;
        PeerEndpoint peer;
        peer.clientId = "peer-1";
        peer.displayName = "Zhang";
        peer.host = "192.0.2.31";
        peer.port = 45454;
        peer.isConnected = true;
        peer.presence = PeerPresenceStatus::Online;
        peer.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch() - 5 * 60 * 1000;

        model.setItems({peer});

        const QModelIndex index = firstContactIndex(model);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(ContactListModel::StatusTextRole).toString(),
                 QStringLiteral("\u79BB\u7EBF"));
    }

    void recentlyActivePeerKeepsOnlineStatus()
    {
        ContactListModel model;
        PeerEndpoint peer;
        peer.clientId = "peer-2";
        peer.displayName = "Li";
        peer.host = "192.0.2.10";
        peer.port = 45454;
        peer.isConnected = true;
        peer.presence = PeerPresenceStatus::Online;
        peer.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch();

        model.setItems({peer});

        const QModelIndex index = firstContactIndex(model);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(ContactListModel::StatusTextRole).toString(),
                 QStringLiteral("\u5728\u7EBF"));
    }

    void recentlyDisconnectedPeerFallsBackToAway()
    {
        ContactListModel model;
        PeerEndpoint peer;
        peer.clientId = "peer-3";
        peer.displayName = "Wang";
        peer.host = "192.0.2.11";
        peer.port = 45454;
        peer.isConnected = false;
        peer.presence = PeerPresenceStatus::Offline;
        peer.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch() - 1000;

        model.setItems({peer});

        const QModelIndex index = firstContactIndex(model);
        QVERIFY(index.isValid());
        QCOMPARE(index.data(ContactListModel::StatusTextRole).toString(),
                 QStringLiteral("\u79BB\u5F00"));
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestContactListModel tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestContactListModel.moc"
