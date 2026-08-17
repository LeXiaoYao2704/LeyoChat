#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QString>
#include <QVector>

#include "MessageServiceDatabase.h"

struct MessageServiceEvent {
    qint64 eventId = 0;
    qint64 createdAtMs = 0;
    QString type;
    QString workspaceId;
    QString conversationId;
    QJsonObject data;
};

class MessageEventBus {
public:
    MessageServiceEvent publishMessageCreated(const StoredMessage& message);

    QVector<MessageServiceEvent> eventsAfter(const QString& workspaceId,
                                             qint64 afterEventId,
                                             int limit) const;

    void recordConsumed(int eventCount);
    QJsonObject metricsJson() const;

    QByteArray encodeStreamSnapshot(const QString& workspaceId,
                                    qint64 afterEventId,
                                    int limit) const;

    static QByteArray encodeSse(const QVector<MessageServiceEvent>& events);

private:
    mutable QMutex m_mutex;
    QVector<MessageServiceEvent> m_events;
    qint64 m_nextEventId = 1;
    int m_maxBufferedEvents = 1000;
    qint64 m_publishedCount = 0;
    qint64 m_consumedCount = 0;
};
