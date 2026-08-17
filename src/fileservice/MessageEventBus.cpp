#include "MessageEventBus.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QMutexLocker>

#include <algorithm>

namespace {

int boundedEventLimit(int limit)
{
    return std::max(1, std::min(limit, 500));
}

QJsonObject messageCreatedData(const StoredMessage& message,
                               qint64 eventId,
                               qint64 eventCreatedAtMs)
{
    return QJsonObject{
        {QStringLiteral("eventId"), eventId},
        {QStringLiteral("type"), QStringLiteral("message.created")},
        {QStringLiteral("createdAtMs"), eventCreatedAtMs},
        {QStringLiteral("serverMessageId"), message.serverMessageId},
        {QStringLiteral("clientMessageId"), message.clientMessageId},
        {QStringLiteral("conversationId"), message.conversationId},
        {QStringLiteral("workspaceId"), message.workspaceId},
        {QStringLiteral("senderId"), message.senderId},
        {QStringLiteral("serverSeq"), message.serverSeq},
        {QStringLiteral("messageType"), message.type}
    };
}

QByteArray sseLine(const QByteArray& name, const QByteArray& value)
{
    QByteArray line;
    line.reserve(name.size() + value.size() + 3);
    line.append(name);
    line.append(": ");
    line.append(value);
    line.append('\n');
    return line;
}

}  // namespace

MessageServiceEvent MessageEventBus::publishMessageCreated(
    const StoredMessage& message)
{
    QMutexLocker locker(&m_mutex);
    const qint64 eventId = m_nextEventId++;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    MessageServiceEvent event;
    event.eventId = eventId;
    event.createdAtMs = nowMs;
    event.type = QStringLiteral("message.created");
    event.workspaceId = message.workspaceId;
    event.conversationId = message.conversationId;
    event.data = messageCreatedData(message, eventId, nowMs);

    m_events.push_back(event);
    ++m_publishedCount;
    while (m_events.size() > m_maxBufferedEvents) {
        m_events.pop_front();
    }
    return event;
}

QVector<MessageServiceEvent> MessageEventBus::eventsAfter(
    const QString& workspaceId,
    qint64 afterEventId,
    int limit) const
{
    const QString normalizedWorkspaceId = workspaceId.trimmed();
    const int boundedLimitValue = boundedEventLimit(limit);

    QMutexLocker locker(&m_mutex);
    QVector<MessageServiceEvent> result;
    result.reserve(std::min(boundedLimitValue, static_cast<int>(m_events.size())));
    for (const MessageServiceEvent& event : m_events) {
        if (event.workspaceId != normalizedWorkspaceId
            || event.eventId <= afterEventId) {
            continue;
        }
        result.push_back(event);
        if (result.size() >= boundedLimitValue) {
            break;
        }
    }
    return result;
}

void MessageEventBus::recordConsumed(int eventCount)
{
    if (eventCount <= 0) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    m_consumedCount += eventCount;
}

QJsonObject MessageEventBus::metricsJson() const
{
    QMutexLocker locker(&m_mutex);
    return QJsonObject{
        {QStringLiteral("published"), m_publishedCount},
        {QStringLiteral("consumed"), m_consumedCount},
        {QStringLiteral("buffered"), m_events.size()},
        {QStringLiteral("nextEventId"), m_nextEventId}
    };
}

QByteArray MessageEventBus::encodeStreamSnapshot(const QString& workspaceId,
                                                 qint64 afterEventId,
                                                 int limit) const
{
    return encodeSse(eventsAfter(workspaceId, afterEventId, limit));
}

QByteArray MessageEventBus::encodeSse(
    const QVector<MessageServiceEvent>& events)
{
    QByteArray stream;
    stream.append(": leyochat heartbeat\n");
    stream.append("retry: 5000\n\n");

    for (const MessageServiceEvent& event : events) {
        stream.append(sseLine(QByteArrayLiteral("id"),
                              QByteArray::number(event.eventId)));
        stream.append(sseLine(QByteArrayLiteral("event"),
                              event.type.toUtf8()));
        stream.append(sseLine(
            QByteArrayLiteral("data"),
            QJsonDocument(event.data).toJson(QJsonDocument::Compact)));
        stream.append('\n');
    }

    return stream;
}
