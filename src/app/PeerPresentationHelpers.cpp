#include "app/PeerPresentationHelpers.h"

#include <QByteArray>
#include <QDateTime>
#include <QStringList>

#include "domain/PeerPresenceEvaluator.h"

QString toQString(const std::wstring& value) {
    return QString::fromStdWString(value);
}

std::string toUtf8(const QString& value) {
    const QByteArray utf8 = value.toUtf8();
    return std::string(utf8.constData(), static_cast<std::size_t>(utf8.size()));
}

QVector<PeerEndpoint> toPeerVector(const std::vector<PeerEndpoint>& peers) {
    QVector<PeerEndpoint> items;
    items.reserve(static_cast<qsizetype>(peers.size()));
    for (const auto& peer : peers) {
        items.push_back(peer);
    }
    return items;
}

std::vector<ChatMessage> toMessageVector(const std::vector<ChatMessage>& messages) {
    return messages;
}

QString endpointKey(const QString& host, quint16 port) {
    return QStringLiteral("%1:%2").arg(host, QString::number(port));
}

QString normalizeHost(const QString& host) {
    return host.startsWith(QStringLiteral("::ffff:")) ? host.mid(7) : host;
}

QString displayNameForPeer(const PeerEndpoint& peer) {
    const QString displayName = QString::fromStdString(peer.displayName).trimmed();
    if (!displayName.isEmpty()) {
        return displayName;
    }

    return QString::fromStdString(peer.clientId);
}

QString presenceTextForPeer(const PeerEndpoint& peer) {
    const PeerPresenceStatus presence =
        PeerPresenceEvaluator::effectivePresence(
            peer,
            QDateTime::currentMSecsSinceEpoch());
    if (presence == PeerPresenceStatus::Offline) {
        return QStringLiteral("\u79BB\u7EBF");
    }
    if (presence == PeerPresenceStatus::Away) {
        return QStringLiteral("\u79BB\u5F00");
    }
    return QStringLiteral("\u5728\u7EBF");
}
