#pragma once

#include <optional>
#include <vector>

#include <QString>
#include <QStringList>

#include "domain/MessageEnvelope.h"
#include "domain/PeerEndpoint.h"

struct PeerHello {
    QString clientId;
    QString displayName;
    quint16 listenPort = 0;
    QString signature;
    PeerPresenceStatus presence = PeerPresenceStatus::Online;
    QString avatarBase64;
    bool supportsTls = false;
    // 扩展个人资料字段
    QString department;
    QString jobTitle;
    QString phoneNumber;
    QString gender;
    QString email;
    QString appVersion;
    QStringList capabilities;
};

class PeerHandshake {
public:
    static MessageEnvelope buildHelloEnvelope(const PeerHello& hello);
    static std::optional<PeerHello> parseHelloEnvelope(const MessageEnvelope& envelope);
    static MessageEnvelope buildDirectorySnapshotEnvelope(const QString& senderId,
                                                          const std::vector<PeerEndpoint>& peers);
    static std::optional<std::vector<PeerEndpoint>> parseDirectorySnapshotEnvelope(const MessageEnvelope& envelope);
};
