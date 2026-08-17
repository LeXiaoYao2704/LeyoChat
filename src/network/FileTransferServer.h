#pragma once

#include <QObject>
#include <QTcpServer>

class FileTransferConnection;
class QHostAddress;

class FileTransferServer : public QObject {
    Q_OBJECT

public:
    explicit FileTransferServer(QObject* parent = nullptr);

    bool listen(const QHostAddress& address, quint16 port);
    quint16 serverPort() const;

signals:
    void connectionAccepted(FileTransferConnection* connection);

private:
    QTcpServer m_server;
};
