#pragma once

#include <QObject>

class PeerConnection;
class QHostAddress;

class PeerSessionManager : public QObject {
    Q_OBJECT

public:
    explicit PeerSessionManager(QString localClientId, QObject* parent = nullptr);

    PeerConnection* connectToPeer(const QHostAddress& address, quint16 port);

private:
    QString m_localClientId;
};
