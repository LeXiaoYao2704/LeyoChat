#include "services/RemoteMessageEventConsumer.h"

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QDebug>
#include <QSet>

#include "integrations/ServerMessageClient.h"
#include "services/MessageSyncService.h"
#include "storage/ConversationRepository.h"

namespace {

QString firstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return {};
}

QString eventConversationId(const ServerMessageEvent& event)
{
    const QString direct = event.conversationId.trimmed();
    if (!direct.isEmpty()) {
        return direct;
    }
    return event.data.value(QStringLiteral("conversationId")).toString().trimmed();
}

QString eventWorkspaceId(const ServerMessageEvent& event)
{
    return firstNonEmpty({
        event.workspaceId,
        event.data.value(QStringLiteral("workspaceId")).toString()
    });
}

QString eventServerMessageId(const ServerMessageEvent& event)
{
    return firstNonEmpty({
        event.data.value(QStringLiteral("serverMessageId")).toString(),
        event.data.value(QStringLiteral("messageId")).toString()
    });
}

qint64 eventCreatedAtMs(const ServerMessageEvent& event)
{
    const qint64 createdAtMs =
        event.data.value(QStringLiteral("createdAtMs")).toInteger(0);
    return createdAtMs > 0 ? createdAtMs : QDateTime::currentMSecsSinceEpoch();
}

bool applyDeliveredEvent(const ConversationRepository& repository,
                         const ServerMessageEvent& event,
                         QString* errorMessage)
{
    const QString serverMessageId = eventServerMessageId(event);
    if (serverMessageId.isEmpty()) {
        return true;
    }

    const QString localMessageId =
        repository.loadLocalMessageIdForRemoteServerId(serverMessageId);
    if (localMessageId.isEmpty()) {
        return true;
    }

    if (!repository.updateDeliveryStatePreservingRead(
            localMessageId, MessageDeliveryState::Received)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to apply message delivered event");
        }
        return false;
    }
    return true;
}

bool applyReadEvent(const ConversationRepository& repository,
                    const ServerMessageEvent& event,
                    QString* errorMessage)
{
    const QString serverMessageId = eventServerMessageId(event);
    if (serverMessageId.isEmpty()) {
        return true;
    }

    const QString localMessageId =
        repository.loadLocalMessageIdForRemoteServerId(serverMessageId);
    if (localMessageId.isEmpty()) {
        return true;
    }

    const QString readerId =
        event.data.value(QStringLiteral("recipientId")).toString().trimmed();
    if (!readerId.isEmpty()
        && !repository.insertReadReceipt(localMessageId,
                                         readerId,
                                         eventCreatedAtMs(event))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to save message read receipt");
        }
        return false;
    }

    ChatMessage existing;
    if (repository.findMessageById(localMessageId, &existing)
        && repository.isKnownActiveGroupConversation(QString::fromStdWString(existing.conversationId))) {
        return true;
    }

    if (!repository.updateDeliveryStatePreservingRead(
            localMessageId, MessageDeliveryState::Read)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to apply message read event");
        }
        return false;
    }
    return true;
}

bool applySessionEvent(const ConversationRepository& repository,
                       const ServerMessageEvent& event,
                       bool online,
                       QString* errorMessage)
{
    ConversationRepository::RemoteSessionPresence presence;
    presence.workspaceId = eventWorkspaceId(event);
    presence.clientId =
        event.data.value(QStringLiteral("clientId")).toString().trimmed();
    presence.deviceId =
        event.data.value(QStringLiteral("deviceId")).toString().trimmed();
    presence.sessionId =
        event.data.value(QStringLiteral("sessionId")).toString().trimmed();
    presence.online = online;
    presence.connectedAtMs =
        event.data.value(QStringLiteral("connectedAtMs")).toInteger(0);
    presence.lastSeenAtMs =
        event.data.value(QStringLiteral("lastSeenAtMs")).toInteger(0);
    presence.lastEventId =
        event.data.value(QStringLiteral("lastEventId"))
            .toInteger(std::max<qint64>(0, event.eventId - 1));

    if (presence.workspaceId.isEmpty() || presence.clientId.isEmpty()
        || presence.deviceId.isEmpty() || presence.sessionId.isEmpty()) {
        return true;
    }

    if (!repository.saveRemoteSessionPresence(presence)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to save remote session presence");
        }
        return false;
    }
    return true;
}

}  // namespace

RemoteMessageEventConsumer::RemoteMessageEventConsumer(
    QString localClientId,
    QString workspaceId,
    QString deviceId,
    ConversationRepository* repository,
    const IServerMessageClient* serverClient,
    int eventLimit,
    int messagePageLimit,
    QString appVersion,
    QStringList capabilities)
    : RemoteMessageEventConsumer(std::move(localClientId),
                                 std::move(workspaceId),
                                 std::move(deviceId),
                                 repository,
                                 serverClient,
                                 eventLimit,
                                 messagePageLimit,
                                 std::move(appVersion),
                                 std::move(capabilities),
                                 {})
{
}

RemoteMessageEventConsumer::RemoteMessageEventConsumer(
    QString localClientId,
    QString workspaceId,
    QString deviceId,
    ConversationRepository* repository,
    const IServerMessageClient* serverClient,
    int eventLimit,
    int messagePageLimit,
    QString appVersion,
    QStringList capabilities,
    IncomingStickerCacheCallback stickerCacheCallback)
    : m_localClientId(std::move(localClientId))
    , m_workspaceId(std::move(workspaceId))
    , m_deviceId(std::move(deviceId))
    , m_repository(repository)
    , m_serverClient(serverClient)
    , m_eventLimit(std::clamp(eventLimit, 1, 500))
    , m_messagePageLimit(std::clamp(messagePageLimit, 1, 500))
    , m_appVersion(std::move(appVersion))
    , m_capabilities(std::move(capabilities))
    , m_stickerCacheCallback(std::move(stickerCacheCallback))
{
}

RemoteMessageEventConsumerResult RemoteMessageEventConsumer::consumeOnce() const
{
    RemoteMessageEventConsumerResult result;
    const QString localClientId = m_localClientId.trimmed();
    const QString workspaceId = m_workspaceId.trimmed();
    const QString deviceId = m_deviceId.trimmed();
    if (!m_repository || !m_serverClient || localClientId.isEmpty()
        || workspaceId.isEmpty() || deviceId.isEmpty()) {
        result.errorMessage =
            QStringLiteral("local client, workspace, device, repository, and server client are required");
        return result;
    }

    result.previousEventId =
        std::max<qint64>(0, m_repository->loadRemoteMessageEventCursor(
                                workspaceId, deviceId));
    result.nextEventId = result.previousEventId;

    QString eventError;
    const std::optional<ServerMessageEventPage> page =
        m_serverClient->listEvents(workspaceId,
                                   deviceId,
                                   result.previousEventId,
                                   m_eventLimit,
                                   &eventError);
    if (!page) {
        result.errorMessage = firstNonEmpty(
            {eventError, QStringLiteral("message event stream fetch failed")});
        return result;
    }

    qint64 nextEventId = std::max(result.previousEventId, page->nextAfterEventId);
    QSet<QString> seenConversationIds;
    QStringList conversationIds;
    result.eventsSeen = page->events.size();
    QString stateEventError;
    for (const ServerMessageEvent& event : page->events) {
        nextEventId = std::max(nextEventId, event.eventId);
        const QString eventType = event.type.trimmed();
        if (eventType == QStringLiteral("session.online")) {
            if (!applySessionEvent(*m_repository, event, true, &stateEventError)) {
                result.nextEventId = result.previousEventId;
                result.errorMessage = firstNonEmpty(
                    {stateEventError,
                     QStringLiteral("failed to apply session online event")});
                return result;
            }
            continue;
        }
        if (eventType == QStringLiteral("session.offline")) {
            if (!applySessionEvent(*m_repository, event, false, &stateEventError)) {
                result.nextEventId = result.previousEventId;
                result.errorMessage = firstNonEmpty(
                    {stateEventError,
                     QStringLiteral("failed to apply session offline event")});
                return result;
            }
            continue;
        }
        if (eventType == QStringLiteral("message.delivered")) {
            if (!applyDeliveredEvent(*m_repository, event, &stateEventError)) {
                result.nextEventId = result.previousEventId;
                result.errorMessage = firstNonEmpty(
                    {stateEventError,
                     QStringLiteral("failed to apply message delivered event")});
                return result;
            }
            continue;
        }
        if (eventType == QStringLiteral("message.read")) {
            if (!applyReadEvent(*m_repository, event, &stateEventError)) {
                result.nextEventId = result.previousEventId;
                result.errorMessage = firstNonEmpty(
                    {stateEventError,
                     QStringLiteral("failed to apply message read event")});
                return result;
            }
            continue;
        }
        if (eventType != QStringLiteral("message.created")) {
            continue;
        }

        const QString conversationId = eventConversationId(event);
        if (conversationId.isEmpty() || seenConversationIds.contains(conversationId)) {
            continue;
        }
        seenConversationIds.insert(conversationId);
        conversationIds.push_back(conversationId);
    }

    result.triggeredConversationIds = conversationIds;
    result.conversationsTriggered = conversationIds.size();

    MessageSyncService syncService(localClientId,
                                   m_repository,
                                   m_serverClient,
                                   m_messagePageLimit,
                                   m_stickerCacheCallback);
    QString firstSyncError;
    for (const QString& conversationId : conversationIds) {
        const MessageSyncResult syncResult =
            syncService.syncConversation(conversationId);
        if (syncResult.success) {
            ++result.conversationsSynced;
            for (const QString& incomingConversationId :
                 syncResult.newIncomingConversationIds) {
                const QString normalized = incomingConversationId.trimmed();
                if (!normalized.isEmpty()
                    && !result.newIncomingConversationIds.contains(normalized)) {
                    result.newIncomingConversationIds.push_back(normalized);
                }
            }
            result.newIncomingNotifications.append(
                syncResult.newIncomingNotifications);
            continue;
        }
        ++result.conversationsFailed;
        if (firstSyncError.isEmpty()) {
            firstSyncError = firstNonEmpty(
                {syncResult.errorMessage,
                 QStringLiteral("sync failed for %1").arg(conversationId)});
        }
    }

    if (result.conversationsFailed > 0) {
        result.errorMessage = firstNonEmpty(
            {firstSyncError, QStringLiteral("message event consumer sync failed")});
    }

    const PendingDeliveryAckFlushResult deliveryAckResult =
        syncService.flushPendingDeliveryAcks(100);
    result.pendingDeliveryAcksAttempted = deliveryAckResult.attemptedCount;
    result.pendingDeliveryAcksAcknowledged = deliveryAckResult.acknowledgedCount;
    if (!deliveryAckResult.success && deliveryAckResult.attemptedCount > 0) {
        qWarning().noquote()
            << "[remote-delivery-ack] pending flush failed during event consume"
            << "attempted=" << deliveryAckResult.attemptedCount
            << "acked=" << deliveryAckResult.acknowledgedCount
            << "error=" << deliveryAckResult.errorMessage;
    }

    const PendingReadAckFlushResult readAckResult =
        syncService.flushPendingReadAcks(100);
    result.pendingReadAcksAttempted = readAckResult.attemptedCount;
    result.pendingReadAcksAcknowledged = readAckResult.acknowledgedCount;
    if (!readAckResult.success && readAckResult.attemptedCount > 0) {
        qWarning().noquote()
            << "[remote-read-ack] pending flush failed during event consume"
            << "attempted=" << readAckResult.attemptedCount
            << "acked=" << readAckResult.acknowledgedCount
            << "error=" << readAckResult.errorMessage;
    }

    if (!m_repository->saveRemoteMessageEventCursor(
            workspaceId, deviceId, nextEventId)) {
        result.nextEventId = result.previousEventId;
        result.errorMessage =
            QStringLiteral("failed to save remote message event cursor");
        return result;
    }

    result.nextEventId = nextEventId;
    QString heartbeatError;
    const std::optional<ServerMessageSessionAck> ack =
        m_serverClient->sendSessionHeartbeat(workspaceId,
                                             deviceId,
                                             nextEventId,
                                             m_appVersion,
                                             m_capabilities,
                                             &heartbeatError);
    if (!ack || !ack->ok) {
        result.errorMessage = firstNonEmpty(
            {heartbeatError, QStringLiteral("message session heartbeat failed")});
        return result;
    }

    const std::optional<QVector<ServerMessageSessionSnapshot>> onlineSessions =
        m_serverClient->listOnlineSessions(workspaceId, nullptr);
    if (onlineSessions) {
        QVector<ConversationRepository::RemoteSessionPresence> presences;
        presences.reserve(onlineSessions->size());
        for (const ServerMessageSessionSnapshot& session : *onlineSessions) {
            ConversationRepository::RemoteSessionPresence presence;
            presence.workspaceId = firstNonEmpty({session.workspaceId, workspaceId});
            presence.clientId = session.clientId.trimmed();
            presence.deviceId = session.deviceId.trimmed();
            presence.sessionId = session.sessionId.trimmed();
            presence.online = true;
            presence.connectedAtMs = std::max<qint64>(0, session.connectedAtMs);
            presence.lastSeenAtMs = std::max<qint64>(0, session.lastSeenAtMs);
            presence.lastEventId = std::max<qint64>(0, session.lastEventId);
            if (!presence.workspaceId.isEmpty()
                && !presence.clientId.isEmpty()
                && !presence.deviceId.isEmpty()
                && !presence.sessionId.isEmpty()) {
                presences.push_back(presence);
            }
        }
        if (!m_repository->replaceRemoteSessionPresenceForWorkspace(
                workspaceId, presences)) {
            result.errorMessage =
                QStringLiteral("failed to save online session snapshot");
            return result;
        }
        result.sessionsSynced = presences.size();
    }

    // A single conversation can become inaccessible after a membership
    // change, or an older server can leak a foreign workspace event. Advancing
    // the durable event cursor prevents that one event from permanently
    // blocking every later conversation. The caller still receives a failed
    // result and runs the conversation-cursor reconciliation path.
    result.success = result.conversationsFailed == 0;
    return result;
}
