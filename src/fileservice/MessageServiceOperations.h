#pragma once

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QString>
#include <QVector>

enum class MessageServiceOperation {
    PostMessage,
    SyncMessages,
    DeliveryAck,
    ReadAck,
    EventsStream,
    SessionHeartbeat
};

struct MessageServiceRateLimitDecision {
    bool allowed = true;
    qint64 retryAfterMs = 0;
};

class MessageServiceOperations {
public:
    void setRateLimit(int maxRequestsPerWindow, qint64 windowMs);

    MessageServiceRateLimitDecision accept(const QString& clientId,
                                           MessageServiceOperation operation);

    QJsonObject metricsJson() const;

    static QString operationKey(MessageServiceOperation operation);

private:
    struct Bucket {
        qint64 windowStartMs = 0;
        int count = 0;
    };

    void appendAuditLocked(qint64 nowMs,
                           const QString& clientId,
                           const QString& operation,
                           const QString& outcome,
                           qint64 retryAfterMs);
    static QJsonObject countersObject(const QHash<QString, qint64>& counters);

    mutable QMutex m_mutex;
    int m_maxRequestsPerWindow = 600;
    qint64 m_windowMs = 60000;
    QHash<QString, Bucket> m_buckets;
    QHash<QString, qint64> m_acceptedCounters;
    QHash<QString, qint64> m_rejectedCounters;
    QVector<QJsonObject> m_recentAudit;
    int m_maxRecentAudit = 100;
};
