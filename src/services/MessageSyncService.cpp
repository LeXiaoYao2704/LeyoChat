#include "services/MessageSyncService.h"

#include <algorithm>
#include <optional>
#include <utility>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QUuid>

#include "domain/MessageMutation.h"
#include "domain/ConversationSummary.h"
#include "services/MessageMutationService.h"
#include "storage/ConversationRepository.h"

namespace {

std::wstring toWide(const QString& value)
{
    return value.toStdWString();
}

QString compactPayloadJson(const QJsonObject& payload)
{
    if (payload.isEmpty()) {
        return {};
    }

    const QByteArray bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(bytes);
}

bool isMutationRecord(const ServerMessageRecord& record)
{
    const QString normalized = record.type.trimmed();
    return normalized == QStringLiteral("message_mutation")
        || normalized == QStringLiteral("mutation");
}

bool isReactionRecord(const ServerMessageRecord& record)
{
    const QString normalized = record.type.trimmed();
    return normalized == QStringLiteral("message_reaction")
        || normalized == QStringLiteral("reaction");
}

bool isPinRecord(const ServerMessageRecord& record)
{
    const QString normalized = record.type.trimmed();
    return normalized == QStringLiteral("pin_message")
        || normalized == QStringLiteral("group_pin");
}

bool isUserVisibleIncomingRecord(const ServerMessageRecord& record,
                                 const QString& localClientId)
{
    return record.senderId.trimmed() != localClientId.trimmed()
        && !isMutationRecord(record)
        && !isReactionRecord(record)
        && !isPinRecord(record);
}

QString firstNonEmpty(std::initializer_list<QString> values);

QString payloadString(const QJsonObject& payload,
                      std::initializer_list<QString> keys)
{
    for (const QString& key : keys) {
        const QString value = payload.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QJsonArray payloadArray(const QJsonObject& payload,
                        std::initializer_list<QString> keys)
{
    for (const QString& key : keys) {
        const QJsonValue value = payload.value(key);
        if (value.isArray()) {
            return value.toArray();
        }
        if (value.isString()) {
            const QJsonDocument parsed =
                QJsonDocument::fromJson(value.toString().toUtf8());
            if (parsed.isArray()) {
                return parsed.array();
            }
        }
    }
    return {};
}

QString compactArrayJson(const QJsonArray& array)
{
    return array.isEmpty()
        ? QString()
        : QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

bool arrayMentionsClient(const QJsonArray& mentionedIds,
                         const QString& localClientId)
{
    const QString normalizedLocalClientId = localClientId.trimmed();
    for (const QJsonValue& value : mentionedIds) {
        const QString mentionedId = value.toString().trimmed();
        if (mentionedId == QStringLiteral("__all__")
            || (!normalizedLocalClientId.isEmpty()
                && mentionedId == normalizedLocalClientId)) {
            return true;
        }
    }
    return false;
}

QString deterministicPinSystemMessageId(const ServerMessageRecord& record)
{
    const QString source = firstNonEmpty({
        record.clientMessageId,
        record.serverMessageId
    });
    return source.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : QStringLiteral("system-pin-%1").arg(source);
}

bool appendPinSystemMessage(ConversationRepository* repository,
                            const ServerMessageRecord& record,
                            const QString& conversationId,
                            const QString& pinnedBody,
                            const QString& pinnerName,
                            const QString& action,
                            QString* errorMessage)
{
    if (!repository) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("repository is required");
        }
        return false;
    }

    const QString actorName = pinnerName.trimmed().isEmpty()
        ? record.senderId.trimmed()
        : pinnerName.trimmed();
    const QString preview = pinnedBody.trimmed().left(30);
    const bool isUnpin = action == QStringLiteral("unpin");
    const QString body = isUnpin
        ? (preview.isEmpty()
               ? QStringLiteral("%1 \u53D6\u6D88\u4E86\u4E00\u6761\u7F6E\u9876\u6D88\u606F")
                     .arg(actorName)
               : QStringLiteral("%1 \u53D6\u6D88\u4E86\u4E00\u6761\u7F6E\u9876\u6D88\u606F\uFF1A%2")
                     .arg(actorName, preview))
        : QStringLiteral("%1 \u7F6E\u9876\u4E86\u4E00\u6761\u6D88\u606F\uFF1A%2")
              .arg(actorName, preview);

    const qint64 createdAtMs =
        record.createdAtMs > 0 ? record.createdAtMs : QDateTime::currentMSecsSinceEpoch();
    const QString systemMessageId = deterministicPinSystemMessageId(record);
    ChatMessage existing;
    if (repository->findMessageById(systemMessageId, &existing)) {
        return true;
    }

    ChatMessage systemMessage;
    systemMessage.messageId = systemMessageId.toStdWString();
    systemMessage.conversationId = conversationId.toStdWString();
    systemMessage.senderId = record.senderId.trimmed().toStdWString();
    systemMessage.body = body.toStdWString();
    systemMessage.createdAtMs = createdAtMs;
    systemMessage.deliveryState = MessageDeliveryState::Read;
    systemMessage.messageType = QStringLiteral("system").toStdWString();

    if (!repository->appendMessage(systemMessage, createdAtMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to append service pin system message");
        }
        return false;
    }
    return true;
}

MessageEnvelope mutationEnvelopeFor(const ServerMessageRecord& record)
{
    MessageEnvelope envelope;
    envelope.type = MessageType::MessageMutation;
    envelope.messageId = record.clientMessageId.trimmed().toStdString();
    envelope.senderId = record.senderId.trimmed().toStdString();
    envelope.conversationId = record.conversationId.trimmed().toStdString();
    envelope.createdAtMs = record.createdAtMs;
    envelope.payloadJson = compactPayloadJson(record.payload).toStdString();
    envelope.messageSubtype =
        record.payload.value(QStringLiteral("mutation_kind")).toString().toStdString();
    return envelope;
}

bool isAlreadyAppliedMutation(const ConversationRepository* repository,
                              const MessageEnvelope& envelope)
{
    if (!repository) {
        return false;
    }

    const std::optional<MessageMutation> mutation = parseMutationPayload(envelope);
    if (!mutation.has_value()) {
        return false;
    }

    ChatMessage current;
    if (!repository->findMessageMutationStateById(mutation->targetMessageId, &current)) {
        return false;
    }

    const QString actorId = QString::fromStdString(envelope.senderId);
    return current.lastMutationAtMs >= mutation->mutatedAtMs
        && QString::fromStdWString(current.lastEditorId) == actorId;
}

bool applyReactionRecord(ConversationRepository* repository,
                         const ServerMessageRecord& record,
                         QString* errorMessage)
{
    if (!repository) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("repository is required");
        }
        return false;
    }

    const QString targetMessageId = payloadString(
        record.payload,
        {QStringLiteral("target_message_id"), QStringLiteral("targetMessageId")});
    const QString emoji = payloadString(record.payload, {QStringLiteral("emoji")});
    const QString reactorId = record.senderId.trimmed();
    if (targetMessageId.isEmpty() || reactorId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("reaction record is incomplete");
        }
        return false;
    }

    if (!repository->applyReaction(targetMessageId, reactorId, emoji)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to apply service reaction");
        }
        return false;
    }
    return true;
}

bool applyPinRecord(ConversationRepository* repository,
                    const ServerMessageRecord& record,
                    QString* errorMessage)
{
    if (!repository) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("repository is required");
        }
        return false;
    }

    QString conversationId = payloadString(
        record.payload,
        {QStringLiteral("group_id"), QStringLiteral("conversation_id")});
    if (conversationId.isEmpty()) {
        conversationId = record.conversationId.trimmed();
    }
    const QString pinnedMessageId = payloadString(
        record.payload,
        {QStringLiteral("message_id"), QStringLiteral("target_message_id"), QStringLiteral("targetMessageId")});
    const QString action = payloadString(record.payload, {QStringLiteral("action")});
    if (conversationId.isEmpty() || pinnedMessageId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("pin record is incomplete");
        }
        return false;
    }

    if (action == QStringLiteral("unpin")) {
        if (!repository->unpinMessageForConversation(conversationId, pinnedMessageId)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("failed to apply service unpin");
            }
            return false;
        }
        return appendPinSystemMessage(
            repository,
            record,
            conversationId,
            record.payload.value(QStringLiteral("pinned_body")).toString(),
            payloadString(record.payload, {QStringLiteral("pinner_name")}),
            action,
            errorMessage);
    }

    const qint64 pinnedAtMs =
        record.createdAtMs > 0 ? record.createdAtMs : QDateTime::currentMSecsSinceEpoch();
    const QString pinnerName =
        payloadString(record.payload, {QStringLiteral("pinner_name")});
    const QString pinnedBody =
        record.payload.value(QStringLiteral("pinned_body")).toString();
    if (!repository->pinMessageForConversation(
            conversationId,
            pinnedMessageId,
            record.senderId.trimmed(),
            pinnerName,
            payloadString(record.payload, {QStringLiteral("author_name")}),
            pinnedBody,
            pinnedAtMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to apply service pin");
        }
        return false;
    }
    return appendPinSystemMessage(
        repository,
        record,
        conversationId,
        pinnedBody,
        pinnerName,
        action,
        errorMessage);
}

QString localMessageTypeFor(const QString& serviceType)
{
    const QString normalized = serviceType.trimmed();
    if (normalized == QStringLiteral("chat_text")) {
        return QStringLiteral("text");
    }
    if (normalized == QStringLiteral("resource_ref")
        || normalized == QStringLiteral("resource_reference")) {
        return QStringLiteral("resource_ref");
    }
    if (normalized == QStringLiteral("file")
        || normalized == QStringLiteral("file_attachment")) {
        return QStringLiteral("file");
    }
    if (normalized == QStringLiteral("group_file_card")) {
        return QStringLiteral("group_file_card");
    }
    return normalized.isEmpty() ? QStringLiteral("text") : normalized;
}

IncomingMessageNotificationEvent notificationEventFor(
    const ServerMessageRecord& record)
{
    IncomingMessageNotificationEvent event;
    event.conversationId = record.conversationId.trimmed();
    event.senderId = record.senderId.trimmed();
    event.messageId = record.clientMessageId.trimmed();
    event.messageType = localMessageTypeFor(record.type);
    if (record.contentType.trimmed() == QStringLiteral("nudge")) {
        event.messageType = QStringLiteral("nudge");
    }
    event.preview = record.body.trimmed();
    const QJsonArray mentionedIds = payloadArray(
        record.payload,
        {QStringLiteral("mentioned_ids"), QStringLiteral("mentionedIds")});
    for (const QJsonValue& value : mentionedIds) {
        const QString mentionedId = value.toString().trimmed();
        if (!mentionedId.isEmpty()) {
            event.mentionedIds.push_back(mentionedId);
        }
    }
    event.payload = record.payload;
    event.payload.remove(QStringLiteral("gif_base64"));
    return event;
}

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

std::optional<ConversationSummary> findConversationSummary(
    const ConversationRepository* repository,
    const QString& conversationId)
{
    if (!repository) {
        return std::nullopt;
    }

    const std::wstring wantedConversationId = conversationId.toStdWString();
    for (const ConversationSummary& summary : repository->loadConversationSummaries()) {
        if (summary.conversationId == wantedConversationId) {
            return summary;
        }
    }
    return std::nullopt;
}

}  // namespace

MessageSyncService::MessageSyncService(QString localClientId,
                                       ConversationRepository* repository,
                                       const IServerMessageClient* serverClient,
                                       int pageLimit)
    : MessageSyncService(std::move(localClientId),
                         repository,
                         serverClient,
                         pageLimit,
                         {})
{
}

MessageSyncService::MessageSyncService(QString localClientId,
                                       ConversationRepository* repository,
                                       const IServerMessageClient* serverClient,
                                       int pageLimit,
                                       IncomingStickerCacheCallback stickerCacheCallback)
    : m_localClientId(std::move(localClientId))
    , m_repository(repository)
    , m_serverClient(serverClient)
    , m_pageLimit(std::clamp(pageLimit, 1, 500))
    , m_stickerCacheCallback(std::move(stickerCacheCallback))
{
}

MessageSyncResult MessageSyncService::syncConversation(
    const QString& conversationId) const
{
    MessageSyncResult result;
    const QString trimmedConversationId = conversationId.trimmed();
    if (!m_repository || !m_serverClient || trimmedConversationId.isEmpty()) {
        result.errorMessage =
            QStringLiteral("repository, server client, and conversationId are required");
        return result;
    }

    result.previousCursor =
        m_repository->loadRemoteChatCursor(trimmedConversationId);
    result.nextCursor = result.previousCursor;

    QString listError;
    const std::optional<ServerMessagePage> page =
        m_serverClient->listMessages(trimmedConversationId,
                                     result.previousCursor,
                                     m_pageLimit,
                                     &listError);
    if (!page) {
        result.errorMessage = firstNonEmpty(
            {listError, QStringLiteral("message service sync failed")});
        return result;
    }

    QSqlDatabase db =
        QSqlDatabase::database(m_repository->connectionName(), false);
    const bool ownTransaction = db.isValid() && db.isOpen() && db.transaction();
    QStringList newIncomingConversationIds;
    QVector<IncomingMessageNotificationEvent> newIncomingNotifications;

    for (const ServerMessageRecord& record : page->messages) {
        if (record.conversationId.trimmed() != trimmedConversationId) {
            result.errorMessage =
                QStringLiteral("message service returned a different conversation");
            if (ownTransaction) {
                db.rollback();
            }
            return result;
        }

        QString persistError;
        const PersistOutcome outcome = persistRecord(record, &persistError);
        if (outcome == PersistOutcome::Failed) {
            result.errorMessage = firstNonEmpty(
                {persistError, QStringLiteral("failed to persist service message")});
            if (ownTransaction) {
                db.rollback();
            }
            return result;
        }

        if (outcome == PersistOutcome::Stored) {
            ++result.storedCount;
            if (isUserVisibleIncomingRecord(record, m_localClientId)) {
                newIncomingNotifications.push_back(notificationEventFor(record));
                if (!newIncomingConversationIds.contains(trimmedConversationId)) {
                    newIncomingConversationIds.push_back(trimmedConversationId);
                }
            }
        } else {
            ++result.skippedDuplicateCount;
        }

        QString ackError;
        if (!enqueueIncomingDeliveryAck(record, &ackError)) {
            result.errorMessage = firstNonEmpty(
                {ackError, QStringLiteral("failed to queue service message delivery ack")});
            if (ownTransaction) {
                db.rollback();
            }
            return result;
        }
    }

    const qint64 nextAfterSeq =
        std::max(result.previousCursor, page->nextAfterSeq);
    if (!m_repository->saveRemoteChatCursor(trimmedConversationId, nextAfterSeq)) {
        result.errorMessage = QStringLiteral("failed to save remote chat cursor");
        if (ownTransaction) {
            db.rollback();
        }
        return result;
    }

    if (ownTransaction && !db.commit()) {
        db.rollback();
        result.errorMessage = QStringLiteral("failed to commit message sync");
        return result;
    }

    result.success = true;
    result.nextCursor = nextAfterSeq;
    result.newIncomingConversationIds = std::move(newIncomingConversationIds);
    result.newIncomingNotifications = std::move(newIncomingNotifications);
    flushPendingDeliveryAcks(500);
    return result;
}

PendingDeliveryAckFlushResult
MessageSyncService::flushPendingDeliveryAcks(int limit) const
{
    PendingDeliveryAckFlushResult result;
    if (!m_repository || !m_serverClient) {
        result.errorMessage =
            QStringLiteral("repository and server client are required");
        return result;
    }

    const auto pending =
        m_repository->loadPendingRemoteDeliveryAcks(std::clamp(limit, 1, 500));
    for (const ConversationRepository::PendingRemoteDeliveryAck& ack : pending) {
        const QString serverMessageId = ack.serverMessageId.trimmed();
        if (serverMessageId.isEmpty()) {
            continue;
        }

        ++result.attemptedCount;
        const ServerAckAttemptResult ackResult =
            m_serverClient->acknowledgeDeliveredResult(serverMessageId,
                                                       qMax<qint64>(0, ack.receivedSeq));
        if (ackResult.outcome == ServerAckOutcome::MessageNotFound) {
            if (!m_repository->deletePendingRemoteDeliveryAck(serverMessageId)) {
                result.errorMessage =
                    QStringLiteral("failed to delete terminal delivery receipt");
                return result;
            }
            ++result.discardedTerminalCount;
            continue;
        }
        if (ackResult.outcome != ServerAckOutcome::Acknowledged) {
            result.errorMessage = firstNonEmpty(
                {ackResult.errorMessage,
                 QStringLiteral("failed to acknowledge pending delivery receipt")});
            return result;
        }
        if (!m_repository->deletePendingRemoteDeliveryAck(serverMessageId)) {
            result.errorMessage =
                QStringLiteral("failed to delete acknowledged delivery receipt");
            return result;
        }
        ++result.acknowledgedCount;
    }

    result.success = true;
    return result;
}

PendingReadAckFlushResult MessageSyncService::flushPendingReadAcks(int limit) const
{
    PendingReadAckFlushResult result;
    if (!m_repository || !m_serverClient) {
        result.errorMessage =
            QStringLiteral("repository and server client are required");
        return result;
    }

    const auto pending =
        m_repository->loadPendingRemoteReadAcks(std::clamp(limit, 1, 500));
    for (const ConversationRepository::PendingRemoteReadAck& ack : pending) {
        const QString serverMessageId = ack.serverMessageId.trimmed();
        if (serverMessageId.isEmpty()) {
            continue;
        }

        ++result.attemptedCount;
        const ServerAckAttemptResult ackResult =
            m_serverClient->acknowledgeReadResult(serverMessageId,
                                                  qMax<qint64>(0, ack.readSeq));
        if (ackResult.outcome == ServerAckOutcome::MessageNotFound) {
            if (!m_repository->deletePendingRemoteReadAck(serverMessageId)) {
                result.errorMessage =
                    QStringLiteral("failed to delete terminal read receipt");
                return result;
            }
            ++result.discardedTerminalCount;
            continue;
        }
        if (ackResult.outcome != ServerAckOutcome::Acknowledged) {
            result.errorMessage = firstNonEmpty(
                {ackResult.errorMessage,
                 QStringLiteral("failed to acknowledge pending read receipt")});
            return result;
        }
        if (!m_repository->deletePendingRemoteReadAck(serverMessageId)) {
            result.errorMessage =
                QStringLiteral("failed to delete acknowledged read receipt");
            return result;
        }
        ++result.acknowledgedCount;
    }

    result.success = true;
    return result;
}

bool MessageSyncService::enqueueIncomingDeliveryAck(
    const ServerMessageRecord& record,
    QString* errorMessage) const
{
    const QString localClientId = m_localClientId.trimmed();
    if (localClientId.isEmpty() || record.senderId.trimmed() == localClientId) {
        return true;
    }

    const QString serverMessageId = record.serverMessageId.trimmed();
    if (serverMessageId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("service message is missing server id");
        }
        return false;
    }
    if (record.serverSeq < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("service message has invalid server seq");
        }
        return false;
    }

    if (!m_repository->enqueuePendingRemoteDeliveryAck(
            serverMessageId,
            record.conversationId,
            record.serverSeq)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to queue delivery ack");
        }
        return false;
    }
    return true;
}

MessageSyncService::PersistOutcome MessageSyncService::persistRecord(
    const ServerMessageRecord& record,
    QString* errorMessage) const
{
    const QString localMessageId = record.clientMessageId.trimmed();
    const QString conversationId = record.conversationId.trimmed();
    const QString senderId = record.senderId.trimmed();
    if (localMessageId.isEmpty() || conversationId.isEmpty()
        || senderId.isEmpty() || record.serverSeq < 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("service message record is incomplete");
        }
        return PersistOutcome::Failed;
    }

    if (isMutationRecord(record)) {
        const MessageEnvelope envelope = mutationEnvelopeFor(record);
        const std::optional<MessageMutation> mutation =
            parseMutationPayload(envelope);
        if (!mutation.has_value()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("service message mutation is incomplete");
            }
            return PersistOutcome::Failed;
        }
        ChatMessage target;
        if (!m_repository->findMessageMutationStateById(
                mutation->targetMessageId, &target)) {
            return PersistOutcome::SkippedDuplicate;
        }
        if (MessageMutationService::applyIncomingMutation(m_repository, envelope)) {
            return PersistOutcome::Stored;
        }
        if (isAlreadyAppliedMutation(m_repository, envelope)) {
            return PersistOutcome::SkippedDuplicate;
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to apply service message mutation");
        }
        return PersistOutcome::Failed;
    }

    if (isReactionRecord(record)) {
        const QString targetMessageId = payloadString(
            record.payload,
            {QStringLiteral("target_message_id"), QStringLiteral("targetMessageId")});
        const QString reactorId = record.senderId.trimmed();
        if (targetMessageId.isEmpty() || reactorId.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("reaction record is incomplete");
            }
            return PersistOutcome::Failed;
        }
        ChatMessage target;
        if (!m_repository->findMessageById(targetMessageId, &target)) {
            return PersistOutcome::SkippedDuplicate;
        }
        QString applyError;
        if (applyReactionRecord(m_repository, record, &applyError)) {
            return PersistOutcome::Stored;
        }
        if (errorMessage) {
            *errorMessage = firstNonEmpty(
                {applyError, QStringLiteral("failed to apply service reaction")});
        }
        return PersistOutcome::Failed;
    }

    if (isPinRecord(record)) {
        QString applyError;
        if (applyPinRecord(m_repository, record, &applyError)) {
            return PersistOutcome::Stored;
        }
        if (errorMessage) {
            *errorMessage = firstNonEmpty(
                {applyError, QStringLiteral("failed to apply service pin")});
        }
        return PersistOutcome::Failed;
    }

    ChatMessage existing;
    if (m_repository->findMessageById(localMessageId, &existing)) {
        const QString serverMessageId = record.serverMessageId.trimmed();
        if (!serverMessageId.isEmpty()
            && !m_repository->saveRemoteMessageIdMapping(serverMessageId,
                                                         localMessageId)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("failed to save remote message id mapping");
            }
            return PersistOutcome::Failed;
        }
        return PersistOutcome::SkippedDuplicate;
    }

    ChatMessage message;
    message.messageId = toWide(localMessageId);
    message.conversationId = toWide(conversationId);
    message.senderId = toWide(senderId);
    message.body = toWide(record.body);
    message.createdAtMs =
        record.createdAtMs > 0 ? record.createdAtMs : QDateTime::currentMSecsSinceEpoch();
    message.deliveryState =
        senderId == m_localClientId.trimmed()
            ? MessageDeliveryState::ServerAcked
            : MessageDeliveryState::Received;
    QString localMessageType = localMessageTypeFor(record.type);
    if (record.contentType.trimmed() == QStringLiteral("nudge")) {
        localMessageType = QStringLiteral("nudge");
    }
    QJsonObject persistedPayload = record.payload;
    if (localMessageType == QStringLiteral("sticker")) {
        const QString packId = payloadString(
            persistedPayload, {QStringLiteral("pack_id"), QStringLiteral("packId")});
        const QString stickerId = payloadString(
            persistedPayload,
            {QStringLiteral("sticker_id"), QStringLiteral("stickerId")});
        const QString encodedGif =
            persistedPayload.value(QStringLiteral("gif_base64")).toString();
        constexpr qsizetype kMaxStickerBase64Chars = 12 * 1024 * 1024;
        if (!encodedGif.isEmpty()) {
            bool cached = false;
            if (!packId.isEmpty() && !stickerId.isEmpty()
                && encodedGif.size() <= kMaxStickerBase64Chars
                && m_stickerCacheCallback) {
                const QByteArray gifData =
                    QByteArray::fromBase64(encodedGif.toLatin1());
                if (!gifData.isEmpty()) {
                    cached = m_stickerCacheCallback(packId, stickerId, gifData);
                }
            }
            if (cached || encodedGif.size() > kMaxStickerBase64Chars) {
                persistedPayload.remove(QStringLiteral("gif_base64"));
            }
        }
    }
    const QString payloadJson = compactPayloadJson(persistedPayload);
    const QJsonArray mentionedIds = payloadArray(
        record.payload,
        {QStringLiteral("mentioned_ids"), QStringLiteral("mentionedIds")});
    message.messageType = toWide(localMessageType);
    if (localMessageType == QStringLiteral("group_file_card")) {
        message.fileCardJson = toWide(payloadJson);
    } else {
        message.payloadJson = toWide(payloadJson);
    }
    message.replyToMessageId = toWide(firstNonEmpty({
        record.replyToMessageId,
        payloadString(record.payload,
                      {QStringLiteral("reply_to_message_id"),
                       QStringLiteral("replyToMessageId")})
    }));
    message.replyToSenderId = toWide(payloadString(
        record.payload,
        {QStringLiteral("reply_to_sender_id"), QStringLiteral("replyToSenderId")}));
    message.replyToBody = toWide(payloadString(
        record.payload,
        {QStringLiteral("reply_to_body"), QStringLiteral("replyToBody")}));
    message.mentionedIds = toWide(compactArrayJson(mentionedIds));

    if (!m_repository->appendMessage(message, QDateTime::currentMSecsSinceEpoch())) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to append service message");
        }
        return PersistOutcome::Failed;
    }

    ChatMessage persisted;
    if (!m_repository->findMessageById(localMessageId, &persisted)
        || QString::fromStdWString(persisted.senderId) != senderId) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("service message was not persisted");
        }
        return PersistOutcome::Failed;
    }

    QString summaryTitle =
        senderId == m_localClientId.trimmed() ? conversationId : senderId;
    const std::optional<ConversationSummary> existingSummary =
        findConversationSummary(m_repository, conversationId);
    if (existingSummary) {
        const QString existingTitle =
            QString::fromStdWString(existingSummary->title).trimmed();
        if (!existingTitle.isEmpty()) {
            summaryTitle = existingTitle;
        }
    }

    if (!m_repository->upsertConversationWithType(
            ConversationSummary{
                toWide(conversationId),
                toWide(summaryTitle),
                toWide(record.body),
                message.createdAtMs
            },
            QStringLiteral("direct"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to update synced conversation");
        }
        return PersistOutcome::Failed;
    }

    if (senderId != m_localClientId.trimmed()) {
        m_repository->setConversationFlag(conversationId, ConversationFlag::Done, false);
        if (arrayMentionsClient(mentionedIds, m_localClientId)) {
            m_repository->setConversationFlag(
                conversationId, ConversationFlag::HasMentionMe, true);
        }
    }
    const QString serverMessageId = record.serverMessageId.trimmed();
    if (!serverMessageId.isEmpty()
        && !m_repository->saveRemoteMessageIdMapping(serverMessageId, localMessageId)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("failed to save remote message id mapping");
        }
        return PersistOutcome::Failed;
    }
    return PersistOutcome::Stored;
}
