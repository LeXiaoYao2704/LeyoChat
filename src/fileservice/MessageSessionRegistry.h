#pragma once

#include <QHash>
#include <QJsonObject>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QVector>

struct MessageSessionSnapshot {
    QString sessionId;
    QString clientId;
    QString deviceId;
    QString workspaceId;
    qint64 connectedAtMs = 0;
    qint64 lastSeenAtMs = 0;
    qint64 lastEventId = 0;
    QString appVersion;
    QStringList capabilities;
};

struct MessageSessionTouchResult {
    MessageSessionSnapshot session;
    bool created = false;
};

class MessageSessionRegistry {
public:
    explicit MessageSessionRegistry(qint64 ttlMs = 120000);

    MessageSessionSnapshot touch(const QString& clientId,
                                 const QString& deviceId,
                                 const QString& workspaceId,
                                 qint64 lastEventId,
                                 qint64 nowMs = 0);
    MessageSessionSnapshot touch(const QString& clientId,
                                 const QString& deviceId,
                                 const QString& workspaceId,
                                 qint64 lastEventId,
                                 const QString& appVersion,
                                 const QStringList& capabilities,
                                 qint64 nowMs = 0);
    MessageSessionTouchResult touchWithStatus(const QString& clientId,
                                              const QString& deviceId,
                                              const QString& workspaceId,
                                              qint64 lastEventId,
                                              qint64 nowMs = 0);
    MessageSessionTouchResult touchWithStatus(const QString& clientId,
                                              const QString& deviceId,
                                              const QString& workspaceId,
                                              qint64 lastEventId,
                                              const QString& appVersion,
                                              const QStringList& capabilities,
                                              qint64 nowMs = 0);

    int cleanupExpired(qint64 nowMs = 0);
    QVector<MessageSessionSnapshot> takeExpiredSessions(qint64 nowMs = 0);
    QVector<MessageSessionSnapshot> onlineSessions(const QString& workspaceId,
                                                   qint64 nowMs = 0) const;
    QJsonObject metricsJson(qint64 nowMs = 0) const;

private:
    static QString sessionKey(const QString& clientId,
                              const QString& deviceId,
                              const QString& workspaceId);
    static qint64 effectiveNowMs(qint64 nowMs);
    MessageSessionTouchResult touchWithStatusInternal(
        const QString& clientId,
        const QString& deviceId,
        const QString& workspaceId,
        qint64 lastEventId,
        const QString& appVersion,
        const QStringList& capabilities,
        bool updateCapabilities,
        qint64 nowMs);
    bool isExpired(const MessageSessionSnapshot& session, qint64 nowMs) const;

    mutable QMutex m_mutex;
    qint64 m_ttlMs = 120000;
    QHash<QString, MessageSessionSnapshot> m_sessions;
    qint64 m_expiredSessions = 0;
};
