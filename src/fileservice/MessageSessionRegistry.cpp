#include "MessageSessionRegistry.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QSet>
#include <QUuid>

#include <algorithm>
#include <tuple>

namespace {

QString normalizedDeviceId(const QString& clientId, const QString& deviceId)
{
    const QString trimmedDeviceId = deviceId.trimmed();
    return trimmedDeviceId.isEmpty() ? clientId.trimmed() : trimmedDeviceId;
}

QStringList normalizedCapabilities(const QStringList& capabilities)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString& raw : capabilities) {
        const QString capability = raw.trimmed();
        const QString key = capability.toLower();
        if (capability.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        result.push_back(capability);
    }
    return result;
}

}  // namespace

MessageSessionRegistry::MessageSessionRegistry(qint64 ttlMs)
    : m_ttlMs(std::max<qint64>(1, ttlMs))
{
}

MessageSessionSnapshot MessageSessionRegistry::touch(
    const QString& clientId,
    const QString& deviceId,
    const QString& workspaceId,
    qint64 lastEventId,
    qint64 nowMs)
{
    return touchWithStatus(clientId,
                           deviceId,
                           workspaceId,
                           lastEventId,
                           nowMs).session;
}

MessageSessionSnapshot MessageSessionRegistry::touch(
    const QString& clientId,
    const QString& deviceId,
    const QString& workspaceId,
    qint64 lastEventId,
    const QString& appVersion,
    const QStringList& capabilities,
    qint64 nowMs)
{
    return touchWithStatus(clientId,
                           deviceId,
                           workspaceId,
                           lastEventId,
                           appVersion,
                           capabilities,
                           nowMs).session;
}

MessageSessionTouchResult MessageSessionRegistry::touchWithStatus(
    const QString& clientId,
    const QString& deviceId,
    const QString& workspaceId,
    qint64 lastEventId,
    qint64 nowMs)
{
    return touchWithStatusInternal(clientId,
                                   deviceId,
                                   workspaceId,
                                   lastEventId,
                                   QString(),
                                   QStringList(),
                                   false,
                                   nowMs);
}

MessageSessionTouchResult MessageSessionRegistry::touchWithStatus(
    const QString& clientId,
    const QString& deviceId,
    const QString& workspaceId,
    qint64 lastEventId,
    const QString& appVersion,
    const QStringList& capabilities,
    qint64 nowMs)
{
    return touchWithStatusInternal(clientId,
                                   deviceId,
                                   workspaceId,
                                   lastEventId,
                                   appVersion,
                                   capabilities,
                                   true,
                                   nowMs);
}

MessageSessionTouchResult MessageSessionRegistry::touchWithStatusInternal(
    const QString& clientId,
    const QString& deviceId,
    const QString& workspaceId,
    qint64 lastEventId,
    const QString& appVersion,
    const QStringList& capabilities,
    bool updateCapabilities,
    qint64 nowMs)
{
    const QString normalizedClientId = clientId.trimmed();
    const QString normalizedWorkspaceId = workspaceId.trimmed();
    const QString normalizedDevice =
        normalizedDeviceId(normalizedClientId, deviceId);
    if (normalizedClientId.isEmpty() || normalizedWorkspaceId.isEmpty()) {
        return {};
    }

    const qint64 now = effectiveNowMs(nowMs);
    const QString key = sessionKey(normalizedClientId,
                                   normalizedDevice,
                                   normalizedWorkspaceId);

    QMutexLocker locker(&m_mutex);
    MessageSessionSnapshot& session = m_sessions[key];
    const bool created = session.sessionId.isEmpty();
    if (session.sessionId.isEmpty()) {
        session.sessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        session.clientId = normalizedClientId;
        session.deviceId = normalizedDevice;
        session.workspaceId = normalizedWorkspaceId;
        session.connectedAtMs = now;
    }

    session.lastSeenAtMs = now;
    session.lastEventId = std::max<qint64>(0, lastEventId);
    if (updateCapabilities) {
        session.appVersion = appVersion.trimmed();
        session.capabilities = normalizedCapabilities(capabilities);
    }
    return MessageSessionTouchResult{session, created};
}

int MessageSessionRegistry::cleanupExpired(qint64 nowMs)
{
    return takeExpiredSessions(nowMs).size();
}

QVector<MessageSessionSnapshot> MessageSessionRegistry::takeExpiredSessions(
    qint64 nowMs)
{
    const qint64 now = effectiveNowMs(nowMs);

    QMutexLocker locker(&m_mutex);
    QVector<MessageSessionSnapshot> removed;
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
        if (isExpired(it.value(), now)) {
            removed.push_back(it.value());
            it = m_sessions.erase(it);
            continue;
        }
        ++it;
    }
    m_expiredSessions += removed.size();
    return removed;
}

QVector<MessageSessionSnapshot> MessageSessionRegistry::onlineSessions(
    const QString& workspaceId,
    qint64 nowMs) const
{
    const QString normalizedWorkspaceId = workspaceId.trimmed();
    if (normalizedWorkspaceId.isEmpty()) {
        return {};
    }

    const qint64 now = effectiveNowMs(nowMs);

    QMutexLocker locker(&m_mutex);
    QVector<MessageSessionSnapshot> result;
    for (const MessageSessionSnapshot& session : m_sessions) {
        if (session.workspaceId == normalizedWorkspaceId
            && !isExpired(session, now)) {
            result.push_back(session);
        }
    }
    std::sort(result.begin(), result.end(), [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.clientId, lhs.deviceId, lhs.sessionId)
            < std::tie(rhs.clientId, rhs.deviceId, rhs.sessionId);
    });
    return result;
}

QJsonObject MessageSessionRegistry::metricsJson(qint64 nowMs) const
{
    const qint64 now = effectiveNowMs(nowMs);

    QMutexLocker locker(&m_mutex);
    QSet<QString> clients;
    QSet<QString> devices;
    int onlineSessions = 0;
    for (const MessageSessionSnapshot& session : m_sessions) {
        if (isExpired(session, now)) {
            continue;
        }
        ++onlineSessions;
        clients.insert(session.clientId);
        devices.insert(session.clientId + QLatin1Char('|') + session.deviceId);
    }

    return QJsonObject{
        {QStringLiteral("onlineSessions"), onlineSessions},
        {QStringLiteral("onlineClients"), clients.size()},
        {QStringLiteral("onlineDevices"), devices.size()},
        {QStringLiteral("expiredSessions"), m_expiredSessions},
        {QStringLiteral("ttlMs"), m_ttlMs}
    };
}

QString MessageSessionRegistry::sessionKey(const QString& clientId,
                                           const QString& deviceId,
                                           const QString& workspaceId)
{
    return workspaceId + QLatin1Char('|')
        + clientId + QLatin1Char('|')
        + deviceId;
}

qint64 MessageSessionRegistry::effectiveNowMs(qint64 nowMs)
{
    return nowMs > 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();
}

bool MessageSessionRegistry::isExpired(const MessageSessionSnapshot& session,
                                       qint64 nowMs) const
{
    return session.lastSeenAtMs > 0 && nowMs - session.lastSeenAtMs > m_ttlMs;
}
