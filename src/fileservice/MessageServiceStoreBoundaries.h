#pragma once

#include "MessageServiceDatabase.h"
#include "MessageServiceOperations.h"
#include "MessageSessionRegistry.h"

class IMessageEventStore {
public:
    virtual ~IMessageEventStore() = default;

    virtual StoredMessageEvent appendMessageCreatedEvent(
        const StoredMessage& message) = 0;
    virtual StoredMessageEvent appendSessionStatusEvent(
        const QString& workspaceId,
        const QString& eventType,
        const QString& sessionId,
        const QString& clientId,
        const QString& deviceId,
        qint64 connectedAtMs,
        qint64 lastSeenAtMs,
        qint64 lastEventId) = 0;
    virtual QVector<StoredMessageEvent> listMessageEventsAfter(
        const QString& workspaceId,
        qint64 afterEventId,
        int limit) const = 0;
};

class ISessionPresenceStore {
public:
    virtual ~ISessionPresenceStore() = default;

    virtual MessageSessionTouchResult touchSession(const QString& clientId,
                                                   const QString& deviceId,
                                                   const QString& workspaceId,
                                                   qint64 lastEventId,
                                                   qint64 nowMs = 0) = 0;
    virtual QVector<MessageSessionSnapshot> takeExpiredSessions(
        qint64 nowMs = 0) = 0;
    virtual QJsonObject metricsJson(qint64 nowMs = 0) const = 0;
};

class IRateLimitStore {
public:
    virtual ~IRateLimitStore() = default;

    virtual void setRateLimit(int maxRequestsPerWindow, qint64 windowMs) = 0;
    virtual MessageServiceRateLimitDecision accept(
        const QString& clientId,
        MessageServiceOperation operation) = 0;
    virtual QJsonObject metricsJson() const = 0;
};

class SqliteMessageEventStore final : public IMessageEventStore {
public:
    explicit SqliteMessageEventStore(MessageServiceDatabase* database);

    StoredMessageEvent appendMessageCreatedEvent(
        const StoredMessage& message) override;
    StoredMessageEvent appendSessionStatusEvent(
        const QString& workspaceId,
        const QString& eventType,
        const QString& sessionId,
        const QString& clientId,
        const QString& deviceId,
        qint64 connectedAtMs,
        qint64 lastSeenAtMs,
        qint64 lastEventId) override;
    QVector<StoredMessageEvent> listMessageEventsAfter(
        const QString& workspaceId,
        qint64 afterEventId,
        int limit) const override;

private:
    MessageServiceDatabase* m_database = nullptr;
};

class InMemorySessionPresenceStore final : public ISessionPresenceStore {
public:
    explicit InMemorySessionPresenceStore(qint64 ttlMs = 120000);

    MessageSessionTouchResult touchSession(const QString& clientId,
                                           const QString& deviceId,
                                           const QString& workspaceId,
                                           qint64 lastEventId,
                                           qint64 nowMs = 0) override;
    QVector<MessageSessionSnapshot> takeExpiredSessions(
        qint64 nowMs = 0) override;
    QJsonObject metricsJson(qint64 nowMs = 0) const override;

private:
    MessageSessionRegistry m_registry;
};

class InMemoryRateLimitStore final : public IRateLimitStore {
public:
    void setRateLimit(int maxRequestsPerWindow, qint64 windowMs) override;
    MessageServiceRateLimitDecision accept(
        const QString& clientId,
        MessageServiceOperation operation) override;
    QJsonObject metricsJson() const override;

private:
    MessageServiceOperations m_operations;
};
