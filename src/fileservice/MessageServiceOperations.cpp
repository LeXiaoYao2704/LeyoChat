#include "MessageServiceOperations.h"

#include <QDateTime>
#include <QJsonArray>
#include <QMutexLocker>

#include <algorithm>

namespace {

QString bucketKey(const QString& clientId, const QString& operation)
{
    return clientId.trimmed() + QLatin1Char('|') + operation;
}

qint64 nonNegative(qint64 value)
{
    return value > 0 ? value : 0;
}

}  // namespace

void MessageServiceOperations::setRateLimit(int maxRequestsPerWindow,
                                            qint64 windowMs)
{
    QMutexLocker locker(&m_mutex);
    m_maxRequestsPerWindow = std::max(1, maxRequestsPerWindow);
    m_windowMs = std::max<qint64>(1000, windowMs);
    m_buckets.clear();
}

MessageServiceRateLimitDecision MessageServiceOperations::accept(
    const QString& clientId,
    MessageServiceOperation operation)
{
    const QString normalizedClientId = clientId.trimmed();
    const QString operationName = operationKey(operation);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    QMutexLocker locker(&m_mutex);
    Bucket& bucket = m_buckets[bucketKey(normalizedClientId, operationName)];
    if (bucket.windowStartMs <= 0 || nowMs - bucket.windowStartMs >= m_windowMs) {
        bucket.windowStartMs = nowMs;
        bucket.count = 0;
    }

    if (bucket.count >= m_maxRequestsPerWindow) {
        const qint64 retryAfterMs =
            nonNegative(m_windowMs - (nowMs - bucket.windowStartMs));
        ++m_rejectedCounters[operationName];
        ++m_rejectedCounters[QStringLiteral("total")];
        appendAuditLocked(nowMs,
                          normalizedClientId,
                          operationName,
                          QStringLiteral("rate_limited"),
                          retryAfterMs);
        return MessageServiceRateLimitDecision{false, retryAfterMs};
    }

    ++bucket.count;
    ++m_acceptedCounters[operationName];
    ++m_acceptedCounters[QStringLiteral("total")];
    appendAuditLocked(nowMs,
                      normalizedClientId,
                      operationName,
                      QStringLiteral("accepted"),
                      0);
    return MessageServiceRateLimitDecision{true, 0};
}

QJsonObject MessageServiceOperations::metricsJson() const
{
    QMutexLocker locker(&m_mutex);
    QJsonArray audit;
    for (const QJsonObject& entry : m_recentAudit) {
        audit.append(entry);
    }

    return QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("rateLimit"), QJsonObject{
            {QStringLiteral("maxRequestsPerWindow"), m_maxRequestsPerWindow},
            {QStringLiteral("windowMs"), m_windowMs}
        }},
        {QStringLiteral("counters"), QJsonObject{
            {QStringLiteral("accepted"), countersObject(m_acceptedCounters)},
            {QStringLiteral("rejected"), countersObject(m_rejectedCounters)}
        }},
        {QStringLiteral("audit"), QJsonObject{
            {QStringLiteral("recent"), audit}
        }}
    };
}

QString MessageServiceOperations::operationKey(MessageServiceOperation operation)
{
    switch (operation) {
    case MessageServiceOperation::PostMessage:
        return QStringLiteral("post_message");
    case MessageServiceOperation::SyncMessages:
        return QStringLiteral("sync_messages");
    case MessageServiceOperation::DeliveryAck:
        return QStringLiteral("delivery_ack");
    case MessageServiceOperation::ReadAck:
        return QStringLiteral("read_ack");
    case MessageServiceOperation::EventsStream:
        return QStringLiteral("events_stream");
    case MessageServiceOperation::SessionHeartbeat:
        return QStringLiteral("session_heartbeat");
    }
    return QStringLiteral("unknown");
}

void MessageServiceOperations::appendAuditLocked(qint64 nowMs,
                                                 const QString& clientId,
                                                 const QString& operation,
                                                 const QString& outcome,
                                                 qint64 retryAfterMs)
{
    QJsonObject entry{
        {QStringLiteral("atMs"), nowMs},
        {QStringLiteral("clientId"), clientId},
        {QStringLiteral("operation"), operation},
        {QStringLiteral("outcome"), outcome}
    };
    if (retryAfterMs > 0) {
        entry[QStringLiteral("retryAfterMs")] = retryAfterMs;
    }

    m_recentAudit.push_back(entry);
    while (m_recentAudit.size() > m_maxRecentAudit) {
        m_recentAudit.pop_front();
    }
}

QJsonObject MessageServiceOperations::countersObject(
    const QHash<QString, qint64>& counters)
{
    QJsonObject object;
    const QStringList keys = {
        QStringLiteral("total"),
        QStringLiteral("post_message"),
        QStringLiteral("sync_messages"),
        QStringLiteral("delivery_ack"),
        QStringLiteral("read_ack"),
        QStringLiteral("events_stream"),
        QStringLiteral("session_heartbeat")
    };
    for (const QString& key : keys) {
        object[key] = counters.value(key, 0);
    }
    return object;
}
