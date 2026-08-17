#pragma once

#include <string>

#include <QtGlobal>

enum class PeerPresenceStatus {
    Online,
    Away,
    Offline
};

struct PeerEndpoint {
    std::string clientId;
    std::string displayName;
    std::string host;
    quint16 port = 0;
    bool isConnected = false;
    PeerPresenceStatus presence = PeerPresenceStatus::Offline;
    qint64 lastPresenceAtMs = 0;
};
