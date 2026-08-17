#include "storage/ConversationRepository.h"

#include "network/MessageCodec.h"

#include <algorithm>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {
QString toStorageValue(MessageDeliveryState state) {
    switch (state) {
    case MessageDeliveryState::Pending:
        return QStringLiteral("pending");
    case MessageDeliveryState::Sent:
        return QStringLiteral("sent");
    case MessageDeliveryState::ServerAcked:
        return QStringLiteral("server_acked");
    case MessageDeliveryState::Received:
        return QStringLiteral("received");
    case MessageDeliveryState::Read:
        return QStringLiteral("read");
    case MessageDeliveryState::Failed:
        return QStringLiteral("failed");
    }

    return QStringLiteral("pending");
}

MessageDeliveryState fromStorageValue(const QString& value) {
    if (value == QStringLiteral("sent")) {
        return MessageDeliveryState::Sent;
    }
    if (value == QStringLiteral("server_acked")) {
        return MessageDeliveryState::ServerAcked;
    }
    if (value == QStringLiteral("received")) {
        return MessageDeliveryState::Received;
    }
    if (value == QStringLiteral("read")) {
        return MessageDeliveryState::Read;
    }
    if (value == QStringLiteral("failed")) {
        return MessageDeliveryState::Failed;
    }
    return MessageDeliveryState::Pending;
}

ChatMessage messageFromQuery(const QSqlQuery& query) {
    ChatMessage msg{
        query.value(0).toString().toStdWString(),
        query.value(1).toString().toStdWString(),
        query.value(2).toString().toStdWString(),
        query.value(3).toString().toStdWString(),
        query.value(4).toLongLong(),
        fromStorageValue(query.value(5).toString()),
        query.value(6).toString().toStdWString(),
        query.value(7).toString().toStdWString(),
        query.value(8).toString().toStdWString(),
        query.value(9).toString().toStdWString()
    };
    msg.isRecalled        = query.value(10).toInt() != 0;
    msg.recalledAtMs      = query.value(11).toLongLong();
    msg.editedAtMs        = query.value(12).toLongLong();
    msg.lastMutationAtMs  = query.value(13).toLongLong();
    msg.lastEditorId      = query.value(14).toString().toStdWString();
    msg.replyToMessageId  = query.value(15).toString().toStdWString();
    msg.replyToSenderId   = query.value(16).toString().toStdWString();
    msg.replyToBody       = query.value(17).toString().toStdWString();
    msg.fileCardJson      = query.value(18).toString().toStdWString();
    msg.mentionedIds      = query.value(19).toString().toStdWString();
    msg.reactionsJson     = query.value(20).toString().toStdWString();
    return msg;
}

QString fromWideForAppendTrace(const char* field,
                               const std::wstring& value,
                               bool traceOutgoingText) {
    if (traceOutgoingText) {
        qInfo().noquote()
            << "[appendMessage] stage=bind-field"
            << "field=" << field
            << "wideLen=" << static_cast<qulonglong>(value.size());
    }
    return QString::fromStdWString(value);
}

}

ConversationRepository::ConversationRepository(QString connectionName)
    : m_connectionName(connectionName) {}

bool ConversationRepository::appendMessage(const ChatMessage& message, qint64 receivedAtMs) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    const bool traceOutgoingText =
        message.deliveryState == MessageDeliveryState::Pending
        && message.messageType == L"text";
    if (traceOutgoingText) {
        qInfo().noquote()
            << "[appendMessage] stage=enter"
            << "msgIdLen=" << static_cast<qulonglong>(message.messageId.size())
            << "convLen=" << static_cast<qulonglong>(message.conversationId.size())
            << "senderLen=" << static_cast<qulonglong>(message.senderId.size())
            << "bodyLen=" << static_cast<qulonglong>(message.body.size())
            << "replyMsgLen=" << static_cast<qulonglong>(message.replyToMessageId.size())
            << "replySenderLen=" << static_cast<qulonglong>(message.replyToSenderId.size())
            << "replyBodyLen=" << static_cast<qulonglong>(message.replyToBody.size());
    }
    // 使用 INSERT ... ON CONFLICT 代替 INSERT OR REPLACE，
    // 保护 sender_id 不被覆盖：冲突时保留原始 sender_id，只更新其他字段。
    // 防止瞬态 SQLITE_BUSY 导致 dedup 检查失效时，回执/重发消息覆盖原发送者。
    query.prepare(QStringLiteral(R"(
        INSERT INTO messages
        (message_id, conversation_id, sender_id, body, created_at_ms, delivery_state,
         attachment_name, local_file_path, message_type, payload_json, received_at_ms,
         is_recalled, recalled_at_ms, edited_at_ms, last_mutation_at_ms, last_editor_id,
         reply_to_message_id, reply_to_sender_id, reply_to_body, file_card_json,
         mentioned_ids)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(message_id) DO UPDATE SET
            body              = excluded.body,
            delivery_state    = excluded.delivery_state,
            attachment_name   = excluded.attachment_name,
            local_file_path   = excluded.local_file_path,
            message_type      = excluded.message_type,
            payload_json      = excluded.payload_json,
            received_at_ms    = excluded.received_at_ms,
            is_recalled       = excluded.is_recalled,
            recalled_at_ms    = excluded.recalled_at_ms,
            edited_at_ms      = excluded.edited_at_ms,
            last_mutation_at_ms = excluded.last_mutation_at_ms,
            last_editor_id    = excluded.last_editor_id,
            reply_to_message_id = excluded.reply_to_message_id,
            reply_to_sender_id  = excluded.reply_to_sender_id,
            reply_to_body       = excluded.reply_to_body,
            file_card_json    = excluded.file_card_json,
            mentioned_ids     = excluded.mentioned_ids
        WHERE sender_id = excluded.sender_id
    )"));
    query.addBindValue(fromWideForAppendTrace("messageId", message.messageId, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("conversationId", message.conversationId, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("senderId", message.senderId, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("body", message.body, traceOutgoingText));
    query.addBindValue(message.createdAtMs);
    query.addBindValue(toStorageValue(message.deliveryState));
    query.addBindValue(fromWideForAppendTrace("attachmentName", message.attachmentName, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("localFilePath", message.localFilePath, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("messageType", message.messageType, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("payloadJson", message.payloadJson, traceOutgoingText));
    query.addBindValue(receivedAtMs);
    query.addBindValue(message.isRecalled ? 1 : 0);
    query.addBindValue(message.recalledAtMs);
    query.addBindValue(message.editedAtMs);
    query.addBindValue(message.lastMutationAtMs);
    query.addBindValue(fromWideForAppendTrace("lastEditorId", message.lastEditorId, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("replyToMessageId", message.replyToMessageId, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("replyToSenderId", message.replyToSenderId, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("replyToBody", message.replyToBody, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("fileCardJson", message.fileCardJson, traceOutgoingText));
    query.addBindValue(fromWideForAppendTrace("mentionedIds", message.mentionedIds, traceOutgoingText));
    if (traceOutgoingText) {
        qInfo().noquote() << "[appendMessage] stage=exec-begin";
    }
    if (!query.exec()) {
        qWarning().noquote() << "[appendMessage] SQL exec failed msgId="
                             << QString::fromStdWString(message.messageId)
                             << "type=" << QString::fromStdWString(message.messageType)
                             << "error=" << query.lastError().text();
        return false;
    }
    if (traceOutgoingText) {
        qInfo().noquote()
            << "[appendMessage] stage=exec-ok"
            << "rows=" << query.numRowsAffected();
    }
    // 诊断：ON CONFLICT ... WHERE sender_id = excluded.sender_id 阻止了 senderId 不同的覆盖时，
    // numRowsAffected() == 0 且 exec() 成功。记录 CRITICAL 日志帮助定位触发源。
    const bool messageStoredOrUpdated = query.numRowsAffected() > 0;
    if (!messageStoredOrUpdated) {
        qCritical().noquote() << "[appendMessage-SENDER-GUARD] BLOCKED senderId overwrite! msgId="
                              << QString::fromStdWString(message.messageId)
                              << "attemptedSender=" << QString::fromStdWString(message.senderId)
                              << "type=" << QString::fromStdWString(message.messageType);
        return true;
    }
    if (!applyPendingDeliveryReceiptForMessage(QString::fromStdWString(message.messageId))) {
        qWarning().noquote() << "[appendMessage] failed to apply pending delivery receipt msgId="
                             << QString::fromStdWString(message.messageId);
        return false;
    }
    return true;
}

bool ConversationRepository::upsertConversation(const ConversationSummary& summary) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO conversations (conversation_id, title, last_message_preview, last_message_at_ms,
                                  is_pinned, is_starred, is_muted, is_done, is_manually_unread, has_mention_me)
        VALUES (?, ?, ?, ?, 0, 0, 0, 0, 0, 0)
        ON CONFLICT(conversation_id) DO UPDATE SET
            title                = excluded.title,
            last_message_preview = CASE
                WHEN excluded.last_message_at_ms >= conversations.last_message_at_ms
                THEN excluded.last_message_preview
                ELSE conversations.last_message_preview END,
            last_message_at_ms   = CASE
                WHEN excluded.last_message_at_ms >= conversations.last_message_at_ms
                THEN excluded.last_message_at_ms
                ELSE conversations.last_message_at_ms END,
            is_done              = 0
    )"));
    query.addBindValue(QString::fromStdWString(summary.conversationId));
    query.addBindValue(QString::fromStdWString(summary.title));
    query.addBindValue(QString::fromStdWString(summary.lastMessagePreview));
    query.addBindValue(summary.lastMessageAtMs);
    return query.exec();
}

bool ConversationRepository::upsertConversationWithType(const ConversationSummary& summary,
                                                        const QString& conversationType) const {
    if (conversationType.trimmed().isEmpty()) {
        return false;
    }

    return upsertConversation(summary);
}

bool ConversationRepository::updateDeliveryState(const QString& messageId, MessageDeliveryState state) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE messages
        SET delivery_state = ?
        WHERE message_id = ?
    )"));
    query.addBindValue(toStorageValue(state));
    query.addBindValue(messageId);
    return query.exec() && query.numRowsAffected() > 0;
}

bool ConversationRepository::updateDeliveryStatePreservingRead(const QString& messageId,
                                                               MessageDeliveryState state) const {
    ChatMessage existingMessage;
    if (findMessageById(messageId, &existingMessage)
        && existingMessage.deliveryState == MessageDeliveryState::Read
        && state != MessageDeliveryState::Failed) {
        state = MessageDeliveryState::Read;
    }

    return updateDeliveryState(messageId, state);
}

qint64 ConversationRepository::loadRemoteChatCursor(
    const QString& conversationId) const
{
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedConversationId.isEmpty()) {
        return 0;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT last_received_seq
        FROM remote_chat_cursors
        WHERE conversation_id = ?
        LIMIT 1
    )"));
    query.addBindValue(trimmedConversationId);
    if (!query.exec() || !query.next()) {
        return 0;
    }

    return qMax<qint64>(0, query.value(0).toLongLong());
}

bool ConversationRepository::saveRemoteChatCursor(
    const QString& conversationId,
    qint64 lastReceivedSeq) const
{
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedConversationId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO remote_chat_cursors
            (conversation_id, last_received_seq, updated_at_ms)
        VALUES (?, ?, ?)
        ON CONFLICT(conversation_id) DO UPDATE SET
            last_received_seq = excluded.last_received_seq,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(trimmedConversationId);
    query.addBindValue(qMax<qint64>(0, lastReceivedSeq));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

qint64 ConversationRepository::loadRemoteChatDeviceCursor(
    const QString& conversationId,
    const QString& deviceId) const
{
    const QString trimmedConversationId = conversationId.trimmed();
    const QString trimmedDeviceId = deviceId.trimmed();
    if (trimmedConversationId.isEmpty() || trimmedDeviceId.isEmpty()) {
        return 0;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT last_received_seq
        FROM remote_chat_device_cursors
        WHERE conversation_id = ?
          AND device_id = ?
        LIMIT 1
    )"));
    query.addBindValue(trimmedConversationId);
    query.addBindValue(trimmedDeviceId);
    if (!query.exec() || !query.next()) {
        return 0;
    }

    return qMax<qint64>(0, query.value(0).toLongLong());
}

bool ConversationRepository::saveRemoteChatDeviceCursor(
    const QString& conversationId,
    const QString& deviceId,
    qint64 lastReceivedSeq) const
{
    const QString trimmedConversationId = conversationId.trimmed();
    const QString trimmedDeviceId = deviceId.trimmed();
    if (trimmedConversationId.isEmpty() || trimmedDeviceId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO remote_chat_device_cursors
            (conversation_id, device_id, last_received_seq, updated_at_ms)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(conversation_id, device_id) DO UPDATE SET
            last_received_seq = excluded.last_received_seq,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(trimmedConversationId);
    query.addBindValue(trimmedDeviceId);
    query.addBindValue(qMax<qint64>(0, lastReceivedSeq));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

qint64 ConversationRepository::loadRemoteMessageEventCursor(
    const QString& workspaceId,
    const QString& deviceId) const
{
    const QString trimmedWorkspaceId = workspaceId.trimmed();
    const QString trimmedDeviceId = deviceId.trimmed();
    if (trimmedWorkspaceId.isEmpty() || trimmedDeviceId.isEmpty()) {
        return 0;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT last_event_id
        FROM remote_message_event_cursors
        WHERE workspace_id = ?
          AND device_id = ?
        LIMIT 1
    )"));
    query.addBindValue(trimmedWorkspaceId);
    query.addBindValue(trimmedDeviceId);
    if (!query.exec() || !query.next()) {
        return 0;
    }

    return qMax<qint64>(0, query.value(0).toLongLong());
}

bool ConversationRepository::saveRemoteMessageEventCursor(
    const QString& workspaceId,
    const QString& deviceId,
    qint64 lastEventId) const
{
    const QString trimmedWorkspaceId = workspaceId.trimmed();
    const QString trimmedDeviceId = deviceId.trimmed();
    if (trimmedWorkspaceId.isEmpty() || trimmedDeviceId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO remote_message_event_cursors
            (workspace_id, device_id, last_event_id, updated_at_ms)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(workspace_id, device_id) DO UPDATE SET
            last_event_id = excluded.last_event_id,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(trimmedWorkspaceId);
    query.addBindValue(trimmedDeviceId);
    query.addBindValue(qMax<qint64>(0, lastEventId));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

bool ConversationRepository::saveRemoteMessageIdMapping(
    const QString& serverMessageId,
    const QString& localMessageId) const
{
    const QString trimmedServerMessageId = serverMessageId.trimmed();
    const QString trimmedLocalMessageId = localMessageId.trimmed();
    if (trimmedServerMessageId.isEmpty() || trimmedLocalMessageId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO remote_message_id_map
            (server_message_id, local_message_id, updated_at_ms)
        VALUES (?, ?, ?)
        ON CONFLICT(server_message_id) DO UPDATE SET
            local_message_id = excluded.local_message_id,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(trimmedServerMessageId);
    query.addBindValue(trimmedLocalMessageId);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

QString ConversationRepository::loadLocalMessageIdForRemoteServerId(
    const QString& serverMessageId) const
{
    const QString trimmedServerMessageId = serverMessageId.trimmed();
    if (trimmedServerMessageId.isEmpty()) {
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT local_message_id
        FROM remote_message_id_map
        WHERE server_message_id = ?
        LIMIT 1
    )"));
    query.addBindValue(trimmedServerMessageId);
    if (!query.exec() || !query.next()) {
        return {};
    }

    return query.value(0).toString().trimmed();
}

QString ConversationRepository::loadRemoteServerIdForLocalMessageId(
    const QString& localMessageId) const
{
    const QString trimmedLocalMessageId = localMessageId.trimmed();
    if (trimmedLocalMessageId.isEmpty()) {
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT server_message_id
        FROM remote_message_id_map
        WHERE local_message_id = ?
        ORDER BY updated_at_ms DESC
        LIMIT 1
    )"));
    query.addBindValue(trimmedLocalMessageId);
    if (!query.exec() || !query.next()) {
        return {};
    }

    return query.value(0).toString().trimmed();
}

bool ConversationRepository::enqueuePendingRemoteReadAck(
    const QString& serverMessageId,
    const QString& conversationId,
    qint64 readSeq) const
{
    const QString trimmedServerMessageId = serverMessageId.trimmed();
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedServerMessageId.isEmpty() || trimmedConversationId.isEmpty()) {
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO pending_remote_read_acks
            (server_message_id, conversation_id, read_seq, created_at_ms, updated_at_ms)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(server_message_id) DO UPDATE SET
            conversation_id = excluded.conversation_id,
            read_seq = excluded.read_seq,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(trimmedServerMessageId);
    query.addBindValue(trimmedConversationId);
    query.addBindValue(qMax<qint64>(0, readSeq));
    query.addBindValue(nowMs);
    query.addBindValue(nowMs);
    return query.exec();
}

std::vector<ConversationRepository::PendingRemoteReadAck>
ConversationRepository::loadPendingRemoteReadAcks(int limit) const
{
    std::vector<PendingRemoteReadAck> results;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT server_message_id, conversation_id, read_seq, created_at_ms, updated_at_ms
        FROM pending_remote_read_acks
        ORDER BY created_at_ms ASC, server_message_id ASC
        LIMIT ?
    )"));
    query.addBindValue(qMax(1, limit));
    if (!query.exec()) {
        return results;
    }

    while (query.next()) {
        PendingRemoteReadAck ack;
        ack.serverMessageId = query.value(0).toString().trimmed();
        ack.conversationId = query.value(1).toString().trimmed();
        ack.readSeq = qMax<qint64>(0, query.value(2).toLongLong());
        ack.createdAtMs = query.value(3).toLongLong();
        ack.updatedAtMs = query.value(4).toLongLong();
        results.push_back(std::move(ack));
    }
    return results;
}

bool ConversationRepository::deletePendingRemoteReadAck(
    const QString& serverMessageId) const
{
    const QString trimmedServerMessageId = serverMessageId.trimmed();
    if (trimmedServerMessageId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "DELETE FROM pending_remote_read_acks WHERE server_message_id = ?"));
    query.addBindValue(trimmedServerMessageId);
    return query.exec();
}

bool ConversationRepository::enqueuePendingRemoteDeliveryAck(
    const QString& serverMessageId,
    const QString& conversationId,
    qint64 receivedSeq) const
{
    const QString trimmedServerMessageId = serverMessageId.trimmed();
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedServerMessageId.isEmpty() || trimmedConversationId.isEmpty()) {
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO pending_remote_delivery_acks
            (server_message_id, conversation_id, received_seq, created_at_ms, updated_at_ms)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(server_message_id) DO UPDATE SET
            conversation_id = excluded.conversation_id,
            received_seq = excluded.received_seq,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(trimmedServerMessageId);
    query.addBindValue(trimmedConversationId);
    query.addBindValue(qMax<qint64>(0, receivedSeq));
    query.addBindValue(nowMs);
    query.addBindValue(nowMs);
    return query.exec();
}

std::vector<ConversationRepository::PendingRemoteDeliveryAck>
ConversationRepository::loadPendingRemoteDeliveryAcks(int limit) const
{
    std::vector<PendingRemoteDeliveryAck> results;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT server_message_id, conversation_id, received_seq, created_at_ms, updated_at_ms
        FROM pending_remote_delivery_acks
        ORDER BY created_at_ms ASC, server_message_id ASC
        LIMIT ?
    )"));
    query.addBindValue(qMax(1, limit));
    if (!query.exec()) {
        return results;
    }

    while (query.next()) {
        PendingRemoteDeliveryAck ack;
        ack.serverMessageId = query.value(0).toString().trimmed();
        ack.conversationId = query.value(1).toString().trimmed();
        ack.receivedSeq = qMax<qint64>(0, query.value(2).toLongLong());
        ack.createdAtMs = query.value(3).toLongLong();
        ack.updatedAtMs = query.value(4).toLongLong();
        results.push_back(std::move(ack));
    }
    return results;
}

bool ConversationRepository::deletePendingRemoteDeliveryAck(
    const QString& serverMessageId) const
{
    const QString trimmedServerMessageId = serverMessageId.trimmed();
    if (trimmedServerMessageId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "DELETE FROM pending_remote_delivery_acks WHERE server_message_id = ?"));
    query.addBindValue(trimmedServerMessageId);
    return query.exec();
}

bool ConversationRepository::saveRemoteSessionPresence(
    const RemoteSessionPresence& presence) const
{
    const QString workspaceId = presence.workspaceId.trimmed();
    const QString clientId = presence.clientId.trimmed();
    const QString deviceId = presence.deviceId.trimmed();
    const QString sessionId = presence.sessionId.trimmed();
    if (workspaceId.isEmpty() || clientId.isEmpty()
        || deviceId.isEmpty() || sessionId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO remote_session_presence
            (workspace_id, client_id, device_id, session_id, is_online,
             connected_at_ms, last_seen_at_ms, last_event_id, updated_at_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(workspace_id, client_id, device_id) DO UPDATE SET
            session_id = excluded.session_id,
            is_online = excluded.is_online,
            connected_at_ms = excluded.connected_at_ms,
            last_seen_at_ms = excluded.last_seen_at_ms,
            last_event_id = excluded.last_event_id,
            updated_at_ms = excluded.updated_at_ms
    )"));
    query.addBindValue(workspaceId);
    query.addBindValue(clientId);
    query.addBindValue(deviceId);
    query.addBindValue(sessionId);
    query.addBindValue(presence.online ? 1 : 0);
    query.addBindValue(qMax<qint64>(0, presence.connectedAtMs));
    query.addBindValue(qMax<qint64>(0, presence.lastSeenAtMs));
    query.addBindValue(qMax<qint64>(0, presence.lastEventId));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

bool ConversationRepository::replaceRemoteSessionPresenceForWorkspace(
    const QString& workspaceId,
    const QVector<RemoteSessionPresence>& onlinePresences) const
{
    const QString trimmedWorkspaceId = workspaceId.trimmed();
    if (trimmedWorkspaceId.isEmpty()) {
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isValid() || !db.isOpen() || !db.transaction()) {
        return false;
    }

    QSqlQuery markOffline(db);
    markOffline.prepare(QStringLiteral(R"(
        UPDATE remote_session_presence
        SET is_online = 0,
            updated_at_ms = ?
        WHERE workspace_id = ?
    )"));
    markOffline.addBindValue(QDateTime::currentMSecsSinceEpoch());
    markOffline.addBindValue(trimmedWorkspaceId);
    if (!markOffline.exec()) {
        db.rollback();
        return false;
    }

    for (const RemoteSessionPresence& rawPresence : onlinePresences) {
        RemoteSessionPresence presence = rawPresence;
        presence.workspaceId = trimmedWorkspaceId;
        presence.online = true;
        if (!saveRemoteSessionPresence(presence)) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        db.rollback();
        return false;
    }
    return true;
}

std::optional<ConversationRepository::RemoteSessionPresence>
ConversationRepository::loadRemoteSessionPresence(
    const QString& workspaceId,
    const QString& clientId,
    const QString& deviceId) const
{
    const QString trimmedWorkspaceId = workspaceId.trimmed();
    const QString trimmedClientId = clientId.trimmed();
    const QString trimmedDeviceId = deviceId.trimmed();
    if (trimmedWorkspaceId.isEmpty() || trimmedClientId.isEmpty()
        || trimmedDeviceId.isEmpty()) {
        return std::nullopt;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT workspace_id, client_id, device_id, session_id, is_online,
               connected_at_ms, last_seen_at_ms, last_event_id
        FROM remote_session_presence
        WHERE workspace_id = ?
          AND client_id = ?
          AND device_id = ?
        LIMIT 1
    )"));
    query.addBindValue(trimmedWorkspaceId);
    query.addBindValue(trimmedClientId);
    query.addBindValue(trimmedDeviceId);
    if (!query.exec() || !query.next()) {
        return std::nullopt;
    }

    RemoteSessionPresence presence;
    presence.workspaceId = query.value(0).toString().trimmed();
    presence.clientId = query.value(1).toString().trimmed();
    presence.deviceId = query.value(2).toString().trimmed();
    presence.sessionId = query.value(3).toString().trimmed();
    presence.online = query.value(4).toInt() != 0;
    presence.connectedAtMs = query.value(5).toLongLong();
    presence.lastSeenAtMs = query.value(6).toLongLong();
    presence.lastEventId = query.value(7).toLongLong();
    return presence;
}

QSet<QString> ConversationRepository::loadOnlineRemoteSessionClientIds(
    const QString& workspaceId) const
{
    QSet<QString> clientIds;
    const QString trimmedWorkspaceId = workspaceId.trimmed();
    if (trimmedWorkspaceId.isEmpty()) {
        return clientIds;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT DISTINCT client_id
        FROM remote_session_presence
        WHERE workspace_id = ?
          AND is_online = 1
    )"));
    query.addBindValue(trimmedWorkspaceId);
    if (!query.exec()) {
        return clientIds;
    }

    while (query.next()) {
        const QString clientId = query.value(0).toString().trimmed();
        if (!clientId.isEmpty()) {
            clientIds.insert(clientId);
        }
    }
    return clientIds;
}

bool ConversationRepository::updateAttachmentMetadata(const QString& messageId,
                                                      const QString& attachmentName,
                                                      const QString& localFilePath) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE messages
        SET attachment_name = ?,
            local_file_path = ?
        WHERE message_id = ?
    )"));
    query.addBindValue(attachmentName);
    query.addBindValue(localFilePath);
    query.addBindValue(messageId);
    return query.exec() && query.numRowsAffected() > 0;
}

bool ConversationRepository::updateMessageBody(const QString& messageId, const QString& body) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE messages
        SET body = ?
        WHERE message_id = ?
    )"));
    query.addBindValue(body);
    query.addBindValue(messageId);
    return query.exec() && query.numRowsAffected() > 0;
}

bool ConversationRepository::findMessageById(const QString& messageId, ChatMessage* outMessage) const {
    if (!outMessage) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT message_id, conversation_id, sender_id, body, created_at_ms, delivery_state,
               attachment_name, local_file_path, message_type, payload_json,
               is_recalled, recalled_at_ms, edited_at_ms, last_mutation_at_ms, last_editor_id,
               reply_to_message_id, reply_to_sender_id, reply_to_body, file_card_json,
               mentioned_ids, reactions_json
        FROM messages
        WHERE message_id = ?
        LIMIT 1
    )"));
    query.addBindValue(messageId);
    if (!query.exec()) {
        qWarning().noquote()
            << "[conversation-repository] message storage lookup failed"
            << "msgId=" << messageId.left(8)
            << "error=" << query.lastError().text();
        return false;
    }
    if (!query.next()) {
        return false;
    }
    *outMessage = messageFromQuery(query);
    return true;
}

bool ConversationRepository::findMessageStorageRecordById(const QString& messageId,
                                                          QString* outConversationId,
                                                          QString* outBody,
                                                          qint64* outCreatedAtMs,
                                                          QString* outAttachmentName,
                                                          QString* outLocalFilePath) const {
    if (!outConversationId || !outBody || !outCreatedAtMs || !outAttachmentName || !outLocalFilePath) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT conversation_id, body, created_at_ms, attachment_name, local_file_path
        FROM messages
        WHERE message_id = ?
        LIMIT 1
    )"));
    query.addBindValue(messageId);
    if (!query.exec()) {
        qWarning().noquote()
            << "[conversation-repository] message envelope lookup failed"
            << "msgId=" << messageId.left(8)
            << "error=" << query.lastError().text();
        return false;
    }
    if (!query.next()) {
        return false;
    }

    *outConversationId = query.value(0).toString();
    *outBody = query.value(1).toString();
    *outCreatedAtMs = query.value(2).toLongLong();
    *outAttachmentName = query.value(3).toString();
    *outLocalFilePath = query.value(4).toString();
    query.finish();
    return true;
}

QString ConversationRepository::loadLatestMessageIdForConversation(const QString& conversationId) const {
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedConversationId.isEmpty()) {
        return {};
    }

    struct Candidate {
        QString messageId;
        qint64 sortAtMs = 0;
        qint64 rowId = -1;
    };

    const auto loadCandidate = [&](const QString& statement,
                                   const QString& bindConversationId) -> Candidate {
        QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
        query.prepare(statement);
        query.addBindValue(bindConversationId);
        if (!query.exec() || !query.next()) {
            return {};
        }

        Candidate candidate;
        candidate.messageId = query.value(0).toString();
        candidate.sortAtMs = query.value(1).toLongLong();
        candidate.rowId = query.value(2).toLongLong();
        return candidate;
    };

    const Candidate receivedCandidate = loadCandidate(
        QStringLiteral(R"(
            SELECT message_id, received_at_ms, rowid
            FROM messages
            WHERE conversation_id = ?
              AND received_at_ms > 0
            ORDER BY received_at_ms DESC, rowid DESC
            LIMIT 1
        )"),
        trimmedConversationId);

    const Candidate createdCandidate = loadCandidate(
        QStringLiteral(R"(
            SELECT message_id, created_at_ms, rowid
            FROM messages
            WHERE conversation_id = ?
              AND received_at_ms = 0
            ORDER BY created_at_ms DESC, rowid DESC
            LIMIT 1
        )"),
        trimmedConversationId);

    if (receivedCandidate.messageId.isEmpty()) {
        return createdCandidate.messageId;
    }
    if (createdCandidate.messageId.isEmpty()) {
        return receivedCandidate.messageId;
    }
    if (receivedCandidate.sortAtMs != createdCandidate.sortAtMs) {
        return receivedCandidate.sortAtMs > createdCandidate.sortAtMs
            ? receivedCandidate.messageId
            : createdCandidate.messageId;
    }
    return receivedCandidate.rowId >= createdCandidate.rowId
        ? receivedCandidate.messageId
        : createdCandidate.messageId;
}

std::vector<ChatMessage> ConversationRepository::loadPendingOutgoingMessages(const std::wstring& conversationId,
                                                                             const std::wstring& senderId) const {
    std::vector<ChatMessage> messages;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT message_id, conversation_id, sender_id, body, created_at_ms, delivery_state,
               attachment_name, local_file_path, message_type, payload_json,
               is_recalled, recalled_at_ms, edited_at_ms, last_mutation_at_ms, last_editor_id,
               reply_to_message_id, reply_to_sender_id, reply_to_body, file_card_json,
               mentioned_ids, reactions_json
        FROM messages
        WHERE conversation_id = ?
          AND sender_id = ?
          AND delivery_state = 'pending'
        ORDER BY created_at_ms ASC, rowid ASC
    )"));
    query.addBindValue(QString::fromStdWString(conversationId));
    query.addBindValue(QString::fromStdWString(senderId));
    if (!query.exec()) {
        return messages;
    }

    while (query.next()) {
        messages.push_back(messageFromQuery(query));
    }

    return messages;
}

std::vector<ChatMessage> ConversationRepository::loadMessages(const std::wstring& conversationId) const {
    return loadRecentMessagesPage(conversationId, 200).messages;
}

ConversationRepository::MessagePage ConversationRepository::loadRecentMessagesPage(
    const std::wstring& conversationId, int limit) const
{
    MessagePage page;
    const int boundedLimit = qBound(1, limit, 500);

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT m.message_id, m.conversation_id, m.sender_id, m.body,
               m.created_at_ms, m.delivery_state, m.attachment_name,
               m.local_file_path, m.message_type, m.payload_json,
               m.is_recalled, m.recalled_at_ms, m.edited_at_ms, m.last_mutation_at_ms, m.last_editor_id,
               m.reply_to_message_id, m.reply_to_sender_id, m.reply_to_body,
               m.file_card_json, m.mentioned_ids, m.reactions_json,
               COALESCE(r.cnt, 0) AS group_read_count
        FROM messages m
        LEFT JOIN (
            SELECT message_id, COUNT(*) AS cnt
            FROM message_read_receipts
            GROUP BY message_id
        ) r ON m.message_id = r.message_id
        WHERE m.conversation_id = ?
        ORDER BY COALESCE(NULLIF(m.received_at_ms, 0), m.created_at_ms) DESC, m.rowid DESC
        LIMIT ?
    )"));
    query.addBindValue(QString::fromStdWString(conversationId));
    query.addBindValue(boundedLimit + 1);
    if (!query.exec()) {
        return page;
    }

    while (query.next()) {
        ChatMessage msg = messageFromQuery(query);
        msg.groupReadCount = query.value(21).toInt();
        page.messages.push_back(std::move(msg));
    }
    if (static_cast<int>(page.messages.size()) > boundedLimit) {
        page.hasMoreBefore = true;
        page.messages.resize(static_cast<size_t>(boundedLimit));
    }
    // SQL fetches newest-first; reverse to chronological ASC
    // for UI display.
    std::reverse(page.messages.begin(), page.messages.end());

    return page;
}

ConversationRepository::MessagePage ConversationRepository::loadMessagesBeforePage(
    const std::wstring& conversationId, const QString& beforeMessageId, int limit) const
{
    MessagePage page;
    const QString trimmedBeforeId = beforeMessageId.trimmed();
    if (trimmedBeforeId.isEmpty()) {
        return page;
    }
    const int boundedLimit = qBound(1, limit, 500);

    QSqlQuery cursorQuery(QSqlDatabase::database(m_connectionName, false));
    cursorQuery.prepare(QStringLiteral(R"(
        SELECT COALESCE(NULLIF(received_at_ms, 0), created_at_ms) AS sort_at,
               rowid
        FROM messages
        WHERE conversation_id = ? AND message_id = ?
        LIMIT 1
    )"));
    cursorQuery.addBindValue(QString::fromStdWString(conversationId));
    cursorQuery.addBindValue(trimmedBeforeId);
    if (!cursorQuery.exec() || !cursorQuery.next()) {
        return page;
    }
    const qint64 beforeSortAt = cursorQuery.value(0).toLongLong();
    const qint64 beforeRowId = cursorQuery.value(1).toLongLong();

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT m.message_id, m.conversation_id, m.sender_id, m.body,
               m.created_at_ms, m.delivery_state, m.attachment_name,
               m.local_file_path, m.message_type, m.payload_json,
               m.is_recalled, m.recalled_at_ms, m.edited_at_ms, m.last_mutation_at_ms, m.last_editor_id,
               m.reply_to_message_id, m.reply_to_sender_id, m.reply_to_body,
               m.file_card_json, m.mentioned_ids, m.reactions_json,
               COALESCE(r.cnt, 0) AS group_read_count
        FROM messages m
        LEFT JOIN (
            SELECT message_id, COUNT(*) AS cnt
            FROM message_read_receipts
            GROUP BY message_id
        ) r ON m.message_id = r.message_id
        WHERE m.conversation_id = ?
          AND (
              COALESCE(NULLIF(m.received_at_ms, 0), m.created_at_ms) < ?
              OR (
                  COALESCE(NULLIF(m.received_at_ms, 0), m.created_at_ms) = ?
                  AND m.rowid < ?
              )
          )
        ORDER BY COALESCE(NULLIF(m.received_at_ms, 0), m.created_at_ms) DESC, m.rowid DESC
        LIMIT ?
    )"));
    query.addBindValue(QString::fromStdWString(conversationId));
    query.addBindValue(beforeSortAt);
    query.addBindValue(beforeSortAt);
    query.addBindValue(beforeRowId);
    query.addBindValue(boundedLimit + 1);
    if (!query.exec()) {
        return page;
    }

    while (query.next()) {
        ChatMessage msg = messageFromQuery(query);
        msg.groupReadCount = query.value(21).toInt();
        page.messages.push_back(std::move(msg));
    }
    if (static_cast<int>(page.messages.size()) > boundedLimit) {
        page.hasMoreBefore = true;
        page.messages.resize(static_cast<size_t>(boundedLimit));
    }
    std::reverse(page.messages.begin(), page.messages.end());
    return page;
}

std::vector<ChatMessage> ConversationRepository::searchMessagesByContent(
    const QString& keyword, int limit) const
{
    std::vector<ChatMessage> results;
    if (keyword.trimmed().isEmpty()) return results;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT m.message_id, m.conversation_id, m.sender_id, m.body,
               m.created_at_ms, m.delivery_state, m.attachment_name,
               m.local_file_path, m.message_type, m.payload_json,
               m.is_recalled, m.recalled_at_ms, m.edited_at_ms, m.last_mutation_at_ms, m.last_editor_id,
               m.reply_to_message_id, m.reply_to_sender_id, m.reply_to_body,
               m.file_card_json, m.mentioned_ids, m.reactions_json,
               0 AS group_read_count
        FROM messages m
        WHERE m.body LIKE ? AND m.is_recalled = 0
          AND m.message_type IN ('text', 'chat_text')
        ORDER BY m.created_at_ms DESC
        LIMIT ?
    )"));
    query.addBindValue(QStringLiteral("%%1%").arg(keyword.trimmed()));
    query.addBindValue(limit);
    if (!query.exec()) return results;

    while (query.next()) {
        ChatMessage msg = messageFromQuery(query);
        msg.groupReadCount = query.value(21).toInt();
        results.push_back(std::move(msg));
    }
    return results;
}

std::vector<ChatMessage> ConversationRepository::loadResourceRefMessages(
    const std::wstring& conversationId) const
{
    std::vector<ChatMessage> messages;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT m.message_id, m.conversation_id, m.sender_id, m.body,
               m.created_at_ms, m.delivery_state, m.attachment_name,
               m.local_file_path, m.message_type, m.payload_json,
               m.is_recalled, m.recalled_at_ms, m.edited_at_ms, m.last_mutation_at_ms, m.last_editor_id,
               m.reply_to_message_id, m.reply_to_sender_id, m.reply_to_body,
               m.file_card_json, m.mentioned_ids, m.reactions_json,
               0 AS group_read_count
        FROM messages m
        WHERE m.conversation_id = ? AND m.message_type = 'resource_ref'
        ORDER BY COALESCE(NULLIF(m.received_at_ms, 0), m.created_at_ms) ASC, m.rowid ASC
    )"));
    query.addBindValue(QString::fromStdWString(conversationId));
    if (!query.exec())
        return messages;
    while (query.next()) {
        ChatMessage msg = messageFromQuery(query);
        msg.groupReadCount = query.value(21).toInt();
        messages.push_back(std::move(msg));
    }
    return messages;
}

std::vector<ConversationSummary> ConversationRepository::loadConversationSummaries() const {
    std::vector<ConversationSummary> summaries;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT conversation_id, title, last_message_preview, last_message_at_ms,
               is_pinned, is_starred, is_muted, is_done, is_manually_unread,
               has_mention_me
        FROM conversations
        ORDER BY is_pinned DESC,
                 CASE WHEN TRIM(last_message_preview) = '' THEN 0 ELSE last_message_at_ms END DESC,
                 last_message_at_ms DESC,
                 rowid DESC
    )"))) {
        return summaries;
    }

    while (query.next()) {
        ConversationSummary s;
        s.conversationId      = query.value(0).toString().toStdWString();
        s.title               = query.value(1).toString().toStdWString();
        s.lastMessagePreview  = query.value(2).toString().toStdWString();
        s.lastMessageAtMs     = query.value(3).toLongLong();
        s.isPinned            = query.value(4).toInt() != 0;
        s.isStarred           = query.value(5).toInt() != 0;
        s.isMuted             = query.value(6).toInt() != 0;
        s.isDone              = query.value(7).toInt() != 0;
        s.isManuallyUnread    = query.value(8).toInt() != 0;
        s.hasMentionMe        = query.value(9).toInt() != 0;
        summaries.push_back(std::move(s));
    }

    return summaries;
}

bool ConversationRepository::remapConversationId(const QString& oldConversationId,
                                                 const QString& newConversationId) const {
    const QString oldId = oldConversationId.trimmed();
    const QString newId = newConversationId.trimmed();
    if (oldId.isEmpty() || newId.isEmpty() || oldId == newId) {
        return false;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid()) {
        return false;
    }

    if (!database.transaction()) {
        return false;
    }

    QSqlQuery updateMessages(database);
    updateMessages.prepare(QStringLiteral(R"(
        UPDATE messages
        SET conversation_id = ?
        WHERE conversation_id = ?
    )"));
    updateMessages.addBindValue(newId);
    updateMessages.addBindValue(oldId);
    if (!updateMessages.exec()) {
        database.rollback();
        return false;
    }

    if (!database.commit()) {
        database.rollback();
        return false;
    }

    return true;
}

bool ConversationRepository::deleteConversation(const QString& conversationId) const {
    const QString trimmedId = conversationId.trimmed();
    if (trimmedId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        DELETE FROM conversations
        WHERE conversation_id = ?
    )"));
    query.addBindValue(trimmedId);
    return query.exec();
}

bool ConversationRepository::saveKnownPeer(const PeerEndpoint& peer) const {
    if (peer.clientId.empty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO known_peers (client_id, display_name, host, port, last_seen_at_ms)
        VALUES (:clientId, :displayName, :host, :port, :lastSeenAtMs)
        ON CONFLICT(client_id) DO UPDATE SET
            display_name  = excluded.display_name,
            host          = excluded.host,
            port          = excluded.port,
            last_seen_at_ms = excluded.last_seen_at_ms
    )"));
    query.bindValue(QStringLiteral(":clientId"),
                    QString::fromUtf8(peer.clientId.data(),
                                      static_cast<int>(peer.clientId.size())));
    query.bindValue(QStringLiteral(":displayName"),
                    QString::fromUtf8(peer.displayName.data(),
                                      static_cast<int>(peer.displayName.size())));
    query.bindValue(QStringLiteral(":host"),
                    QString::fromUtf8(peer.host.data(),
                                      static_cast<int>(peer.host.size())));
    query.bindValue(QStringLiteral(":port"), peer.port);
    query.bindValue(QStringLiteral(":lastSeenAtMs"),
                    QDateTime::currentMSecsSinceEpoch());
    return query.exec();
}

std::vector<PeerEndpoint> ConversationRepository::loadKnownPeers() const {
    std::vector<PeerEndpoint> peers;

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(QStringLiteral(R"(
        SELECT client_id, display_name, host, port
        FROM known_peers
        ORDER BY last_seen_at_ms DESC
    )"))) {
        return peers;
    }

    while (query.next()) {
        PeerEndpoint peer;
        peer.clientId    = query.value(0).toString().toUtf8().toStdString();
        peer.displayName = query.value(1).toString().toUtf8().toStdString();
        peer.host        = query.value(2).toString().toUtf8().toStdString();
        peer.port        = static_cast<quint16>(query.value(3).toUInt());
        peers.push_back(std::move(peer));
    }

    return peers;
}

bool ConversationRepository::deleteKnownPeer(const QString& clientId) const {
    const QString trimmedClientId = clientId.trimmed();
    if (trimmedClientId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        DELETE FROM known_peers
        WHERE client_id = ?
    )"));
    query.addBindValue(trimmedClientId);
    return query.exec() && query.numRowsAffected() > 0;
}

bool ConversationRepository::isKnownActiveGroupConversation(const QString& conversationId) const
{
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedConversationId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT 1
        FROM groups
        WHERE group_id = ?
          AND is_active != 0
        LIMIT 1
    )"));
    query.addBindValue(trimmedConversationId);
    return query.exec() && query.next();
}

bool ConversationRepository::insertReadReceipt(const QString& messageId,
                                               const QString& readerId,
                                               qint64 readAtMs) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "INSERT OR IGNORE INTO message_read_receipts (message_id, reader_id, read_at_ms) "
        "VALUES (?, ?, ?)"));
    query.addBindValue(messageId);
    query.addBindValue(readerId);
    query.addBindValue(readAtMs);
    return query.exec();
}

QVector<QPair<QString, qint64>> ConversationRepository::loadReadReceiptsForMessage(
    const QString& messageId) const
{
    QVector<QPair<QString, qint64>> result;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(
        "SELECT reader_id, read_at_ms FROM message_read_receipts "
        "WHERE message_id = ? ORDER BY read_at_ms ASC"));
    query.addBindValue(messageId);
    if (query.exec()) {
        while (query.next()) {
            result.append({query.value(0).toString(), query.value(1).toLongLong()});
        }
    }
    return result;
}

bool ConversationRepository::enqueuePendingDeliveryReceipt(const QString& messageId,
                                                           const QString& senderId,
                                                           const QString& targetId,
                                                           const QString& conversationId,
                                                           qint64 receivedAtMs) const
{
    const QString trimmedMessageId = messageId.trimmed();
    const QString trimmedSenderId = senderId.trimmed();
    const QString trimmedTargetId = targetId.trimmed();
    if (trimmedMessageId.isEmpty() || trimmedSenderId.isEmpty() || trimmedTargetId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO pending_delivery_receipts
            (message_id, sender_id, target_id, conversation_id, received_at_ms)
        VALUES (?, ?, ?, ?, ?)
        ON CONFLICT(message_id) DO UPDATE SET
            sender_id = excluded.sender_id,
            target_id = excluded.target_id,
            conversation_id = excluded.conversation_id,
            received_at_ms = MAX(pending_delivery_receipts.received_at_ms,
                                 excluded.received_at_ms)
    )"));
    query.addBindValue(trimmedMessageId);
    query.addBindValue(trimmedSenderId);
    query.addBindValue(trimmedTargetId);
    query.addBindValue(conversationId.trimmed());
    query.addBindValue(receivedAtMs);
    return query.exec();
}

bool ConversationRepository::applyPendingDeliveryReceiptForMessage(const QString& messageId) const
{
    const QString trimmedMessageId = messageId.trimmed();
    if (trimmedMessageId.isEmpty()) {
        return false;
    }

    QSqlQuery exists(QSqlDatabase::database(m_connectionName, false));
    exists.prepare(QStringLiteral(R"(
        SELECT 1
        FROM pending_delivery_receipts
        WHERE message_id = ?
        LIMIT 1
    )"));
    exists.addBindValue(trimmedMessageId);
    if (!exists.exec()) {
        return false;
    }
    if (!exists.next()) {
        return true;
    }

    QSqlQuery update(QSqlDatabase::database(m_connectionName, false));
    update.prepare(QStringLiteral(R"(
        UPDATE messages
        SET delivery_state = CASE
                WHEN delivery_state = 'read' THEN 'read'
                ELSE 'received'
            END
        WHERE message_id = ?
          AND sender_id = (
              SELECT target_id
              FROM pending_delivery_receipts
              WHERE message_id = ?
          )
          AND delivery_state IN ('pending', 'sent', 'server_acked', 'received', 'failed', 'read')
    )"));
    update.addBindValue(trimmedMessageId);
    update.addBindValue(trimmedMessageId);
    if (!update.exec()) {
        return false;
    }
    if (update.numRowsAffected() <= 0) {
        return true;
    }

    QSqlQuery remove(QSqlDatabase::database(m_connectionName, false));
    remove.prepare(QStringLiteral(R"(
        DELETE FROM pending_delivery_receipts
        WHERE message_id = ?
    )"));
    remove.addBindValue(trimmedMessageId);
    return remove.exec();
}

QSet<QString> ConversationRepository::loadConversationsWithUnreadMessages(
    const QString& localClientId) const
{
    QSet<QString> result;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));

    // 有来自他人且 state='received' 的消息（非手动标记会话）
    query.prepare(QStringLiteral(R"(
        SELECT DISTINCT m.conversation_id
        FROM messages m
        INNER JOIN conversations c ON c.conversation_id = m.conversation_id
        WHERE m.sender_id != ?
          AND m.delivery_state = 'received'
          AND c.is_manually_unread = 0
    )"));
    query.addBindValue(localClientId);
    if (!query.exec()) {
        qWarning() << "[ConversationRepository] loadConversationsWithUnreadMessages:"
                   << query.lastError().text();
    } else {
        while (query.next()) {
            result.insert(query.value(0).toString());
        }
    }

    // 加上所有手动标记未读的会话
    QSqlQuery manualQuery(QSqlDatabase::database(m_connectionName, false));
    if (!manualQuery.exec(QStringLiteral(
            "SELECT conversation_id FROM conversations WHERE is_manually_unread = 1"))) {
        qWarning() << "[ConversationRepository] loadConversationsWithUnreadMessages:"
                   << manualQuery.lastError().text();
    } else {
        while (manualQuery.next()) {
            result.insert(manualQuery.value(0).toString());
        }
    }

    return result;
}

bool ConversationRepository::consumeConversationUnread(const QString& conversationId,
                                                       const QString& localClientId,
                                                       bool includeSentState) const
{
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedConversationId.isEmpty()) {
        return false;
    }

    QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
    if (!database.isValid()) {
        return false;
    }

    if (!database.transaction()) {
        return false;
    }

    bool changed = false;
    QStringList allowedStates;
    allowedStates << QStringLiteral("received");
    if (includeSentState) {
        allowedStates << QStringLiteral("sent");
    }

    QStringList placeholders;
    placeholders.reserve(allowedStates.size());
    for (int i = 0; i < allowedStates.size(); ++i) {
        placeholders << QStringLiteral("?");
    }

    QSqlQuery updateMessages(database);
    updateMessages.prepare(QStringLiteral(
        "UPDATE messages "
        "SET delivery_state = 'read' "
        "WHERE conversation_id = ? "
        "  AND sender_id != ? "
        "  AND delivery_state IN (%1)")
                               .arg(placeholders.join(QStringLiteral(", "))));
    updateMessages.addBindValue(trimmedConversationId);
    updateMessages.addBindValue(localClientId);
    for (const QString& state : std::as_const(allowedStates)) {
        updateMessages.addBindValue(state);
    }
    if (!updateMessages.exec()) {
        database.rollback();
        return false;
    }
    changed = updateMessages.numRowsAffected() > 0;

    QSqlQuery clearManualUnread(database);
    clearManualUnread.prepare(QStringLiteral(
        "UPDATE conversations "
        "SET is_manually_unread = 0 "
        "WHERE conversation_id = ? AND is_manually_unread != 0"));
    clearManualUnread.addBindValue(trimmedConversationId);
    if (!clearManualUnread.exec()) {
        database.rollback();
        return false;
    }
    changed = changed || clearManualUnread.numRowsAffected() > 0;

    if (!database.commit()) {
        database.rollback();
        return false;
    }

    return changed;
}

bool ConversationRepository::setConversationFlag(const QString& conversationId,
                                                  ConversationFlag flag, bool value) const {
    const QString column = [flag]() -> QString {
        switch (flag) {
        case ConversationFlag::Pinned:         return QStringLiteral("is_pinned");
        case ConversationFlag::Starred:        return QStringLiteral("is_starred");
        case ConversationFlag::Muted:          return QStringLiteral("is_muted");
        case ConversationFlag::Done:           return QStringLiteral("is_done");
        case ConversationFlag::ManuallyUnread: return QStringLiteral("is_manually_unread");
        case ConversationFlag::HasMentionMe:   return QStringLiteral("has_mention_me");
        }
        return {};
    }();
    if (column.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("UPDATE conversations SET %1 = ? WHERE conversation_id = ?").arg(column));
    query.addBindValue(value ? 1 : 0);
    query.addBindValue(conversationId);
    return query.exec();
}

bool ConversationRepository::applyMessageRecall(const QString& messageId,
                                                const QString& actorId,
                                                qint64 recalledAtMs) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE messages
        SET is_recalled = 1,
            recalled_at_ms = ?,
            last_mutation_at_ms = ?,
            last_editor_id = ?
        WHERE message_id = ?
          AND last_mutation_at_ms < ?
    )"));
    query.addBindValue(recalledAtMs);
    query.addBindValue(recalledAtMs);
    query.addBindValue(actorId);
    query.addBindValue(messageId);
    query.addBindValue(recalledAtMs);
    return query.exec() && query.numRowsAffected() > 0;
}

bool ConversationRepository::applyMessageEdit(const QString& messageId,
                                              const QString& actorId,
                                              qint64 editedAtMs,
                                              const QString& newBody) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        UPDATE messages
        SET body = ?,
            edited_at_ms = ?,
            last_mutation_at_ms = ?,
            last_editor_id = ?
        WHERE message_id = ?
          AND is_recalled = 0
          AND last_mutation_at_ms < ?
    )"));
    query.addBindValue(newBody);
    query.addBindValue(editedAtMs);
    query.addBindValue(editedAtMs);
    query.addBindValue(actorId);
    query.addBindValue(messageId);
    query.addBindValue(editedAtMs);
    return query.exec() && query.numRowsAffected() > 0;
}

bool ConversationRepository::applyReaction(const QString& messageId,
                                           const QString& reactorClientId,
                                           const QString& emoji) const
{
    if (messageId.isEmpty() || reactorClientId.isEmpty()) return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.isValid()) return false;

    // 读取当前 reactions_json
    QSqlQuery selectQuery(db);
    selectQuery.prepare(QStringLiteral(
        "SELECT reactions_json FROM messages WHERE message_id = ?"));
    selectQuery.addBindValue(messageId);
    if (!selectQuery.exec() || !selectQuery.next()) return false;

    const QString currentJson = selectQuery.value(0).toString();
    QJsonObject reactions = currentJson.isEmpty()
        ? QJsonObject{}
        : QJsonDocument::fromJson(currentJson.toUtf8()).object();

    // 1) 从所有 key 中移除该 reactor（互斥）
    for (auto it = reactions.begin(); it != reactions.end(); ) {
        QJsonArray arr = it.value().toArray();
        QJsonArray filtered;
        for (const auto& v : arr) {
            if (v.toString() != reactorClientId) filtered.append(v);
        }
        if (filtered.isEmpty()) {
            it = reactions.erase(it);
        } else {
            *it = filtered;
            ++it;
        }
    }

    // 2) 如果 emoji 非空，添加到对应列表（toggle 取消时传空 emoji）
    if (!emoji.isEmpty()) {
        QJsonArray arr = reactions.value(emoji).toArray();
        arr.append(reactorClientId);
        reactions.insert(emoji, arr);
    }

    // 3) 序列化写回
    const QString newJson = reactions.isEmpty()
        ? QString()
        : QString::fromUtf8(QJsonDocument(reactions).toJson(QJsonDocument::Compact));

    QSqlQuery updateQuery(db);
    updateQuery.prepare(QStringLiteral(
        "UPDATE messages SET reactions_json = ? WHERE message_id = ?"));
    updateQuery.addBindValue(newJson);
    updateQuery.addBindValue(messageId);
    return updateQuery.exec();
}

bool ConversationRepository::updateMessageFileCardJson(const QString& messageId,
                                                       const QString& fileCardJson) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("UPDATE messages SET file_card_json = ? WHERE message_id = ?"));
    query.addBindValue(fileCardJson);
    query.addBindValue(messageId);
    return query.exec();
}

bool ConversationRepository::updateMessageFields(const QString& messageId,
                                                  const QString& messageType,
                                                  const QString& payloadJson) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("UPDATE messages SET message_type = ?, payload_json = ? WHERE message_id = ?"));
    query.addBindValue(messageType);
    query.addBindValue(payloadJson.isNull() ? QStringLiteral("") : payloadJson);
    query.addBindValue(messageId);
    return query.exec();
}

bool ConversationRepository::findMessageMutationStateById(const QString& messageId,
                                                          ChatMessage* outMessage) const {
    return findMessageById(messageId, outMessage);
}

bool ConversationRepository::refreshConversationPreviewFromLatestVisibleMessage(
    const QString& conversationId) const {
    QSqlQuery selectQuery(QSqlDatabase::database(m_connectionName, false));
    selectQuery.prepare(QStringLiteral(R"(
        SELECT body, is_recalled, message_type, COALESCE(NULLIF(received_at_ms, 0), created_at_ms)
        FROM messages
        WHERE conversation_id = ?
        ORDER BY COALESCE(NULLIF(received_at_ms, 0), created_at_ms) DESC, rowid DESC
        LIMIT 1
    )"));
    selectQuery.addBindValue(conversationId);
    if (!selectQuery.exec() || !selectQuery.next()) {
        return false;
    }

    const QString body = selectQuery.value(0).toString();
    const bool isRecalled = selectQuery.value(1).toInt() != 0;
    const QString messageType = selectQuery.value(2).toString();
    const qint64 previewAtMs = selectQuery.value(3).toLongLong();

    QString preview;
    if (isRecalled) {
        preview = QStringLiteral("消息已撤回");
    } else if (messageType == QStringLiteral("file") || messageType == QStringLiteral("file_attachment")) {
        preview = QStringLiteral("[文件]");
    } else if (messageType == QStringLiteral("resource_ref")) {
        preview = QStringLiteral("[资源引用]");
    } else {
        preview = body.left(100);
    }

    QSqlQuery updateQuery(QSqlDatabase::database(m_connectionName, false));
    updateQuery.prepare(QStringLiteral(R"(
        UPDATE conversations
        SET last_message_preview = ?,
            last_message_at_ms = ?
        WHERE conversation_id = ?
    )"));
    updateQuery.addBindValue(preview);
    updateQuery.addBindValue(previewAtMs);
    updateQuery.addBindValue(conversationId);
    return updateQuery.exec() && updateQuery.numRowsAffected() > 0;
}

bool ConversationRepository::pinMessageForConversation(const QString& conversationId,
                                                        const QString& messageId,
                                                        const QString& pinnerId,
                                                        const QString& pinnerName,
                                                        const QString& authorName,
                                                        const QString& pinnedBody,
                                                        qint64 pinnedAtMs) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT OR REPLACE INTO pinned_messages
            (conversation_id, message_id, pinner_id, pinner_name, author_name, pinned_body, pinned_at_ms)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )"));
    query.addBindValue(conversationId);
    query.addBindValue(messageId);
    query.addBindValue(pinnerId);
    query.addBindValue(pinnerName);
    query.addBindValue(authorName);
    query.addBindValue(pinnedBody);
    query.addBindValue(pinnedAtMs);
    return query.exec();
}

bool ConversationRepository::unpinMessageForConversation(const QString& conversationId, const QString& messageId) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("DELETE FROM pinned_messages WHERE conversation_id = ? AND message_id = ?"));
    query.addBindValue(conversationId);
    query.addBindValue(messageId);
    return query.exec();
}

int ConversationRepository::pinnedMessageCount(const QString& conversationId) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("SELECT COUNT(*) FROM pinned_messages WHERE conversation_id = ?"));
    query.addBindValue(conversationId);
    if (!query.exec() || !query.next()) { return 0; }
    return query.value(0).toInt();
}

std::vector<ConversationRepository::PinnedMessageInfo>
ConversationRepository::loadPinnedMessages(const QString& conversationId) const {
    std::vector<PinnedMessageInfo> results;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT message_id, pinner_id, pinner_name, author_name, pinned_body, pinned_at_ms
        FROM pinned_messages
        WHERE conversation_id = ?
        ORDER BY pinned_at_ms ASC
        LIMIT 3
    )"));
    query.addBindValue(conversationId);
    if (!query.exec()) { return results; }
    while (query.next()) {
        PinnedMessageInfo info;
        info.messageId  = query.value(0).toString();
        info.pinnerId   = query.value(1).toString();
        info.pinnerName = query.value(2).toString();
        info.authorName = query.value(3).toString();
        info.pinnedBody = query.value(4).toString();
        info.pinnedAtMs = query.value(5).toLongLong();
        results.push_back(std::move(info));
    }
    return results;
}

// ── 群消息离线补发队列 ──────────────────────────────────────────────

bool ConversationRepository::enqueuePendingGroupEnvelope(const QString& targetId,
                                                          const QString& groupId,
                                                          const QByteArray& envelopeBlob,
                                                          qint64 createdAtMs) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        INSERT INTO pending_group_envelopes (target_id, group_id, envelope_blob, created_at_ms)
        VALUES (?, ?, ?, ?)
    )"));
    query.addBindValue(targetId);
    query.addBindValue(groupId);
    query.addBindValue(envelopeBlob);
    query.addBindValue(createdAtMs);
    return query.exec();
}

std::vector<ConversationRepository::PendingGroupEnvelope>
ConversationRepository::loadPendingGroupEnvelopes(const QString& targetId, int limit) const {
    return loadPendingGroupEnvelopesAfterId(targetId, 0, limit);
}

std::vector<ConversationRepository::PendingGroupEnvelope>
ConversationRepository::loadPendingGroupEnvelopesAfterId(const QString& targetId,
                                                         qint64 afterId,
                                                         int limit) const {
    std::vector<PendingGroupEnvelope> results;
    const QString normalizedTargetId = targetId.trimmed();
    if (normalizedTargetId.isEmpty()) {
        return results;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT id, target_id, group_id, envelope_blob, created_at_ms
        FROM pending_group_envelopes
        WHERE target_id = ? AND id > ?
        ORDER BY id ASC
        LIMIT ?
    )"));
    query.addBindValue(normalizedTargetId);
    query.addBindValue(std::max<qint64>(0, afterId));
    query.addBindValue(qBound(1, limit, 2000));
    if (!query.exec()) { return results; }
    while (query.next()) {
        PendingGroupEnvelope env;
        env.id          = query.value(0).toLongLong();
        env.targetId    = query.value(1).toString();
        env.groupId     = query.value(2).toString();
        env.envelopeBlob = query.value(3).toByteArray();
        env.createdAtMs = query.value(4).toLongLong();
        results.push_back(std::move(env));
    }
    return results;
}

std::vector<ConversationRepository::PendingGroupEnvelope>
ConversationRepository::loadAllPendingGroupEnvelopes() const {
    std::vector<PendingGroupEnvelope> results;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT id, target_id, group_id, envelope_blob, created_at_ms
        FROM pending_group_envelopes
        ORDER BY id ASC
    )"));
    if (!query.exec()) {
        return results;
    }
    while (query.next()) {
        PendingGroupEnvelope env;
        env.id = query.value(0).toLongLong();
        env.targetId = query.value(1).toString();
        env.groupId = query.value(2).toString();
        env.envelopeBlob = query.value(3).toByteArray();
        env.createdAtMs = query.value(4).toLongLong();
        results.push_back(std::move(env));
    }
    return results;
}

bool ConversationRepository::deletePendingGroupEnvelopes(const QVector<qint64>& ids) const {
    if (ids.isEmpty()) return true;
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    QStringList placeholders;
    placeholders.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i) {
        placeholders.append(QStringLiteral("?"));
    }
    query.prepare(QStringLiteral("DELETE FROM pending_group_envelopes WHERE id IN (%1)")
                      .arg(placeholders.join(QStringLiteral(","))));
    for (const qint64 id : ids) {
        query.addBindValue(id);
    }
    return query.exec();
}

bool ConversationRepository::deletePendingGroupEnvelopeForTargetMessage(const QString& targetId,
                                                                        const QString& messageId) const {
    const QString trimmedTargetId = targetId.trimmed();
    const QString trimmedMessageId = messageId.trimmed();
    if (trimmedTargetId.isEmpty() || trimmedMessageId.isEmpty()) {
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral(R"(
        SELECT id, envelope_blob
        FROM pending_group_envelopes
        WHERE target_id = ?
    )"));
    query.addBindValue(trimmedTargetId);
    if (!query.exec()) {
        return false;
    }

    QVector<qint64> matchedIds;
    while (query.next()) {
        const QByteArray blob = query.value(1).toByteArray();
        const auto envelope = MessageCodec::decode(
            std::string_view(blob.constData(), static_cast<std::size_t>(blob.size())));
        if (!envelope.has_value()) {
            continue;
        }
        if (QString::fromStdString(envelope->messageId) == trimmedMessageId) {
            matchedIds.append(query.value(0).toLongLong());
        }
    }

    return deletePendingGroupEnvelopes(matchedIds);
}

bool ConversationRepository::deletePendingGroupEnvelopesForTarget(const QString& targetId) const {
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(QStringLiteral("DELETE FROM pending_group_envelopes WHERE target_id = ?"));
    query.addBindValue(targetId);
    return query.exec();
}
