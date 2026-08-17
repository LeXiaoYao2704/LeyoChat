#include "network/PeerHandshake.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {
std::string toUtf8(const QString& value) {
    const QByteArray bytes = value.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

QString fromUtf8(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QStringList splitBody(const QString& body) {
    return body.split(QChar(0x1F));
}

QStringList splitCapabilities(const QString& value)
{
    QStringList result;
    const QStringList raw = value.split(QChar(0x1E), Qt::SkipEmptyParts);
    for (const QString& item : raw) {
        const QString capability = item.trimmed();
        if (!capability.isEmpty()) {
            result.push_back(capability);
        }
    }
    return result;
}

QString peerField(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QString presenceToString(PeerPresenceStatus presence) {
    switch (presence) {
    case PeerPresenceStatus::Away:
        return QStringLiteral("away");
    case PeerPresenceStatus::Offline:
        return QStringLiteral("offline");
    case PeerPresenceStatus::Online:
    default:
        return QStringLiteral("online");
    }
}

PeerPresenceStatus presenceFromString(const QString& value) {
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("away")) {
        return PeerPresenceStatus::Away;
    }
    if (normalized == QStringLiteral("offline")) {
        return PeerPresenceStatus::Offline;
    }
    return PeerPresenceStatus::Online;
}
}

MessageEnvelope PeerHandshake::buildHelloEnvelope(const PeerHello& hello) {
    const QChar sep(0x1F);
    const QString payload =
        hello.displayName + sep
        + QString::number(hello.listenPort) + sep
        + hello.signature + sep
        + presenceToString(hello.presence) + sep
        + hello.avatarBase64 + sep
        + (hello.supportsTls ? QStringLiteral("1") : QStringLiteral("0")) + sep
        + hello.department + sep
        + hello.jobTitle + sep
        + hello.phoneNumber + sep
        + hello.gender + sep
        + hello.email + sep
        + hello.appVersion + sep
        + hello.capabilities.join(QChar(0x1E));

    MessageEnvelope envelope;
    envelope.messageId = QStringLiteral("hello-%1").arg(hello.clientId).toStdString();
    envelope.type = MessageType::HandshakeHello;
    envelope.senderId = toUtf8(hello.clientId);
    envelope.body = toUtf8(payload);
    envelope.createdAtMs = QDateTime::currentMSecsSinceEpoch();
    return envelope;
}

std::optional<PeerHello> PeerHandshake::parseHelloEnvelope(const MessageEnvelope& envelope) {
    if (envelope.type != MessageType::HandshakeHello || envelope.senderId.empty()) {
        return std::nullopt;
    }

    const QString body = fromUtf8(envelope.body);
    const QStringList parts = splitBody(body);
    if (parts.size() < 2) {
        return std::nullopt;
    }

    bool ok = false;
    const QString displayName = parts.at(0).trimmed();
    const int listenPort = parts.at(1).toInt(&ok);
    if (!ok || displayName.isEmpty() || listenPort < 0 || listenPort > 65535) {
        return std::nullopt;
    }

    const QString signature = parts.size() >= 3 ? parts.at(2) : QString();
    const PeerPresenceStatus presence =
        parts.size() >= 4 ? presenceFromString(parts.at(3)) : PeerPresenceStatus::Online;
    const QString avatarBase64 = parts.size() >= 5 ? parts.at(4).trimmed() : QString();
    const bool supportsTls = parts.size() >= 6 && parts.at(5).trimmed() == QStringLiteral("1");

    const QString department  = parts.size() >= 7 ? parts.at(6).trimmed() : QString();
    const QString jobTitle    = parts.size() >= 8 ? parts.at(7).trimmed() : QString();
    const QString phoneNumber = parts.size() >= 9 ? parts.at(8).trimmed() : QString();
    const QString gender      = parts.size() >= 10 ? parts.at(9).trimmed() : QString();
    const QString email       = parts.size() >= 11 ? parts.at(10).trimmed() : QString();
    const QString appVersion  = parts.size() >= 12 ? parts.at(11).trimmed() : QString();
    const QStringList capabilities =
        parts.size() >= 13 ? splitCapabilities(parts.at(12)) : QStringList();

    return PeerHello{
        fromUtf8(envelope.senderId),
        displayName,
        static_cast<quint16>(listenPort),
        signature,
        presence,
        avatarBase64,
        supportsTls,
        department,
        jobTitle,
        phoneNumber,
        gender,
        email,
        appVersion,
        capabilities
    };
}

MessageEnvelope PeerHandshake::buildDirectorySnapshotEnvelope(const QString& senderId,
                                                             const std::vector<PeerEndpoint>& peers) {
    QJsonArray peerItems;
    for (const auto& peer : peers) {
        if (peer.clientId.empty() || peer.host.empty() || peer.port == 0) {
            continue;
        }

        QJsonObject item;
        item.insert(QStringLiteral("client_id"), peerField(peer.clientId));
        item.insert(QStringLiteral("display_name"), peerField(peer.displayName));
        item.insert(QStringLiteral("host"), peerField(peer.host));
        item.insert(QStringLiteral("port"), static_cast<int>(peer.port));
        item.insert(QStringLiteral("presence"), presenceToString(peer.presence));
        peerItems.push_back(item);
    }

    QJsonObject root;
    root.insert(QStringLiteral("peers"), peerItems);

    MessageEnvelope envelope;
    envelope.messageId = QStringLiteral("peers-%1-%2")
                             .arg(senderId, QString::number(QDateTime::currentMSecsSinceEpoch()))
                             .toStdString();
    envelope.type = MessageType::PeerDirectorySnapshot;
    envelope.senderId = toUtf8(senderId);
    envelope.body = toUtf8(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact)));
    envelope.createdAtMs = QDateTime::currentMSecsSinceEpoch();
    return envelope;
}

std::optional<std::vector<PeerEndpoint>> PeerHandshake::parseDirectorySnapshotEnvelope(const MessageEnvelope& envelope) {
    if (envelope.type != MessageType::PeerDirectorySnapshot || envelope.senderId.empty()) {
        return std::nullopt;
    }

    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray::fromStdString(envelope.body));
    if (!document.isObject()) {
        return std::nullopt;
    }

    const QJsonValue peersValue = document.object().value(QStringLiteral("peers"));
    if (!peersValue.isArray()) {
        return std::nullopt;
    }

    std::vector<PeerEndpoint> peers;
    const QJsonArray peerItems = peersValue.toArray();
    peers.reserve(static_cast<std::size_t>(peerItems.size()));
    for (const QJsonValue& value : peerItems) {
        if (!value.isObject()) {
            return std::nullopt;
        }

        const QJsonObject item = value.toObject();
        const QString clientId = item.value(QStringLiteral("client_id")).toString().trimmed();
        const QString displayName = item.value(QStringLiteral("display_name")).toString().trimmed();
        const QString host = item.value(QStringLiteral("host")).toString().trimmed();
        const int port = item.value(QStringLiteral("port")).toInt();
        const PeerPresenceStatus presence =
            presenceFromString(item.value(QStringLiteral("presence")).toString());
        if (clientId.isEmpty() || host.isEmpty() || port <= 0 || port > 65535) {
            return std::nullopt;
        }

        peers.push_back(PeerEndpoint{
            toUtf8(clientId),
            toUtf8(displayName),
            toUtf8(host),
            static_cast<quint16>(port),
            false,
            presence,
            QDateTime::currentMSecsSinceEpoch()
        });
    }

    return peers;
}
