#pragma once

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

namespace ConnectionRegistryUtils {

enum class DuplicatePeerConnectionAction {
    KeepExistingDropNew,
    DropExistingUseNew,
    KeepBothPreferExisting,
    KeepBothPreferNew,
};

inline bool shouldKeepExistingPeerConnection(const QString& localClientId,
                                             const QString& targetClientId,
                                             bool existingIsLocalOutbound)
{
    return (localClientId < targetClientId) == existingIsLocalOutbound;
}

inline bool hasServerReceiveCapability(const QStringList& capabilities)
{
    return capabilities.contains(QStringLiteral("server_receive_v1"), Qt::CaseInsensitive);
}

inline DuplicatePeerConnectionAction duplicatePeerConnectionAction(
    const QString& localClientId,
    const QString& targetClientId,
    bool existingIsLocalOutbound,
    const QStringList& peerCapabilities)
{
    const bool keepExisting =
        shouldKeepExistingPeerConnection(localClientId, targetClientId, existingIsLocalOutbound);
    if (!hasServerReceiveCapability(peerCapabilities)) {
        return keepExisting ? DuplicatePeerConnectionAction::KeepBothPreferExisting
                            : DuplicatePeerConnectionAction::KeepBothPreferNew;
    }
    return keepExisting ? DuplicatePeerConnectionAction::KeepExistingDropNew
                        : DuplicatePeerConnectionAction::DropExistingUseNew;
}

inline const char* duplicatePeerConnectionActionName(DuplicatePeerConnectionAction action)
{
    switch (action) {
    case DuplicatePeerConnectionAction::KeepExistingDropNew:
        return "keep-existing-drop-new";
    case DuplicatePeerConnectionAction::DropExistingUseNew:
        return "drop-existing-use-new";
    case DuplicatePeerConnectionAction::KeepBothPreferExisting:
        return "keep-both-prefer-existing";
    case DuplicatePeerConnectionAction::KeepBothPreferNew:
        return "keep-both-prefer-new";
    }
    return "unknown";
}

template <typename T>
class ConnectionIdentityRegistry {
public:
    struct Entry {
        QPointer<T> connection;
        QString identity;
    };

    void insert(T* connection, const QString& identity)
    {
        remove(connection);
        if (connection) {
            entries_.push_back(Entry{connection, identity});
        }
    }

    void remove(T* connection)
    {
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->connection.isNull() || it->connection == connection) {
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
    }

    QString value(T* connection) const
    {
        for (const Entry& entry : entries_) {
            if (entry.connection && entry.connection == connection) {
                return entry.identity;
            }
        }
        return {};
    }

    qsizetype size() const
    {
        qsizetype liveCount = 0;
        for (const Entry& entry : entries_) {
            if (entry.connection) {
                ++liveCount;
            }
        }
        return liveCount;
    }

    const QVector<Entry>& entries() const { return entries_; }

private:
    QVector<Entry> entries_;
};

template <typename T>
T* connectedConnectionForTarget(const QHash<QString, QPointer<T>>& entries,
                                const ConnectionIdentityRegistry<T>& identities,
                                const QString& targetClientId,
                                T* excluded = nullptr)
{
    const QString normalizedTargetId = targetClientId.trimmed();
    if (normalizedTargetId.isEmpty()) {
        return nullptr;
    }
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        T* connection = it.value();
        if (it.key() == normalizedTargetId
            && connection
            && connection != excluded
            && connection->isConnected()) {
            return connection;
        }
    }
    for (const auto& entry : identities.entries()) {
        T* connection = entry.connection.data();
        if (entry.identity.trimmed() == normalizedTargetId
            && connection
            && connection != excluded
            && connection->isConnected()) {
            return connection;
        }
    }
    return nullptr;
}

template <typename T>
bool hasConnectedConnectionForTarget(const QHash<QString, QPointer<T>>& entries,
                                     const ConnectionIdentityRegistry<T>& identities,
                                     const QString& targetClientId,
                                     T* excluded)
{
    return connectedConnectionForTarget(entries, identities, targetClientId, excluded) != nullptr;
}

inline bool shouldThrottlePeerHello(bool repeatedHelloOnRegisteredConnection,
                                    bool hasLastProcessedHello,
                                    qint64 nowMs,
                                    qint64 lastProcessedMs,
                                    qint64 intervalMs)
{
    return repeatedHelloOnRegisteredConnection
        && hasLastProcessedHello
        && (nowMs - lastProcessedMs) < intervalMs;
}

template <typename T>
QStringList removePointerEntries(QHash<QString, QPointer<T>>& entries, T* target)
{
    QStringList removedKeys;
    if (entries.isEmpty()) {
        return removedKeys;
    }
    for (auto it = entries.begin(); it != entries.end();) {
        if (it.value().isNull() || it.value() == target) {
            removedKeys.push_back(it.key());
            it = entries.erase(it);
        } else {
            ++it;
        }
    }
    return removedKeys;
}

}  // namespace ConnectionRegistryUtils
