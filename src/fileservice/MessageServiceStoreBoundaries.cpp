#include "MessageServiceStoreBoundaries.h"

SqliteMessageEventStore::SqliteMessageEventStore(
    MessageServiceDatabase* database)
    : m_database(database)
{
}

StoredMessageEvent SqliteMessageEventStore::appendMessageCreatedEvent(
    const StoredMessage& message)
{
    return m_database ? m_database->appendMessageCreatedEvent(message)
                      : StoredMessageEvent{};
}

StoredMessageEvent SqliteMessageEventStore::appendSessionStatusEvent(
    const QString& workspaceId,
    const QString& eventType,
    const QString& sessionId,
    const QString& clientId,
    const QString& deviceId,
    qint64 connectedAtMs,
    qint64 lastSeenAtMs,
    qint64 lastEventId)
{
    return m_database
        ? m_database->appendSessionStatusEvent(workspaceId,
                                               eventType,
                                               sessionId,
                                               clientId,
                                               deviceId,
                                               connectedAtMs,
                                               lastSeenAtMs,
                                               lastEventId)
        : StoredMessageEvent{};
}

QVector<StoredMessageEvent> SqliteMessageEventStore::listMessageEventsAfter(
    const QString& workspaceId,
    qint64 afterEventId,
    int limit) const
{
    return m_database ? m_database->listMessageEventsAfter(workspaceId,
                                                           afterEventId,
                                                           limit)
                      : QVector<StoredMessageEvent>{};
}

InMemorySessionPresenceStore::InMemorySessionPresenceStore(qint64 ttlMs)
    : m_registry(ttlMs)
{
}

MessageSessionTouchResult InMemorySessionPresenceStore::touchSession(
    const QString& clientId,
    const QString& deviceId,
    const QString& workspaceId,
    qint64 lastEventId,
    qint64 nowMs)
{
    return m_registry.touchWithStatus(clientId,
                                      deviceId,
                                      workspaceId,
                                      lastEventId,
                                      nowMs);
}

QVector<MessageSessionSnapshot>
InMemorySessionPresenceStore::takeExpiredSessions(qint64 nowMs)
{
    return m_registry.takeExpiredSessions(nowMs);
}

QJsonObject InMemorySessionPresenceStore::metricsJson(qint64 nowMs) const
{
    return m_registry.metricsJson(nowMs);
}

void InMemoryRateLimitStore::setRateLimit(int maxRequestsPerWindow,
                                          qint64 windowMs)
{
    m_operations.setRateLimit(maxRequestsPerWindow, windowMs);
}

MessageServiceRateLimitDecision InMemoryRateLimitStore::accept(
    const QString& clientId,
    MessageServiceOperation operation)
{
    return m_operations.accept(clientId, operation);
}

QJsonObject InMemoryRateLimitStore::metricsJson() const
{
    return m_operations.metricsJson();
}
