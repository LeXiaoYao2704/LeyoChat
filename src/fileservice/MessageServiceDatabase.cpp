#include "MessageServiceDatabase.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

#include <algorithm>

namespace {
QString normalizedConnectionName(const QString& connectionName)
{
    return connectionName.isEmpty()
        ? QStringLiteral("leyo-message-service")
        : connectionName;
}

qint64 nowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

QString cleaned(const QString& value)
{
    return value.trimmed();
}

bool isSensitiveAuditKey(const QString& key)
{
    const QString normalized = key.trimmed().toLower();
    return normalized == QStringLiteral("body")
        || normalized == QStringLiteral("messagebody")
        || normalized == QStringLiteral("payloadjson");
}

QJsonValue sanitizedAuditValue(const QJsonValue& value)
{
    if (value.isObject()) {
        QJsonObject sanitized;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!isSensitiveAuditKey(it.key()))
                sanitized.insert(it.key(), sanitizedAuditValue(it.value()));
        }
        return sanitized;
    }

    if (value.isArray()) {
        QJsonArray sanitized;
        const QJsonArray array = value.toArray();
        for (const auto& item : array)
            sanitized.append(sanitizedAuditValue(item));
        return sanitized;
    }

    return value;
}

QJsonObject sanitizedAuditMetadata(const QJsonObject& metadata)
{
    return sanitizedAuditValue(metadata).toObject();
}

StoredMessage messageFromQuery(const QSqlQuery& query)
{
    StoredMessage message;
    message.serverMessageId = query.value(QStringLiteral("server_message_id")).toString();
    message.clientMessageId = query.value(QStringLiteral("client_message_id")).toString();
    message.conversationId = query.value(QStringLiteral("conversation_id")).toString();
    message.workspaceId = query.value(QStringLiteral("workspace_id")).toString();
    message.senderId = query.value(QStringLiteral("sender_id")).toString();
    message.serverSeq = query.value(QStringLiteral("server_seq")).toLongLong();
    message.type = query.value(QStringLiteral("type")).toString();
    message.body = query.value(QStringLiteral("body")).toString();
    message.payloadJson = query.value(QStringLiteral("payload_json")).toString();
    message.fileId = query.value(QStringLiteral("file_id")).toString();
    message.contentType = query.value(QStringLiteral("content_type")).toString();
    message.replyToMessageId = query.value(QStringLiteral("reply_to_message_id")).toString();
    message.createdAtMs = query.value(QStringLiteral("created_at_ms")).toLongLong();
    message.updatedAtMs = query.value(QStringLiteral("updated_at_ms")).toLongLong();
    return message;
}

StoredMessageEvent eventFromQuery(const QSqlQuery& query)
{
    StoredMessageEvent event;
    event.eventId = query.value(QStringLiteral("event_id")).toLongLong();
    event.workspaceId = query.value(QStringLiteral("workspace_id")).toString();
    event.conversationId = query.value(QStringLiteral("conversation_id")).toString();
    event.eventType = query.value(QStringLiteral("event_type")).toString();
    event.payloadJson = query.value(QStringLiteral("payload_json")).toString();
    event.createdAtMs = query.value(QStringLiteral("created_at_ms")).toLongLong();
    return event;
}

MessageDeliveryRecord deliveryFromQuery(const QSqlQuery& query)
{
    MessageDeliveryRecord delivery;
    delivery.serverMessageId = query.value(QStringLiteral("server_message_id")).toString();
    delivery.recipientId = query.value(QStringLiteral("recipient_id")).toString();
    delivery.state = query.value(QStringLiteral("state")).toString();
    delivery.deliveredAtMs = query.value(QStringLiteral("delivered_at_ms")).toLongLong();
    delivery.readAtMs = query.value(QStringLiteral("read_at_ms")).toLongLong();
    delivery.retryCount = query.value(QStringLiteral("retry_count")).toLongLong();
    return delivery;
}

MessageWorkspaceRecord workspaceFromQuery(const QSqlQuery& query)
{
    MessageWorkspaceRecord workspace;
    workspace.workspaceId = query.value(QStringLiteral("workspace_id")).toString();
    workspace.displayName = query.value(QStringLiteral("display_name")).toString();
    workspace.createdById = query.value(QStringLiteral("created_by_id")).toString();
    workspace.enabled = query.value(QStringLiteral("enabled")).toInt() != 0;
    workspace.createdAtMs = query.value(QStringLiteral("created_at_ms")).toLongLong();
    workspace.updatedAtMs = query.value(QStringLiteral("updated_at_ms")).toLongLong();
    return workspace;
}

MessageAuditRecord auditFromQuery(const QSqlQuery& query)
{
    MessageAuditRecord audit;
    audit.auditId = query.value(QStringLiteral("audit_id")).toLongLong();
    audit.workspaceId = query.value(QStringLiteral("workspace_id")).toString();
    audit.actorClientId = query.value(QStringLiteral("actor_client_id")).toString();
    audit.action = query.value(QStringLiteral("action")).toString();
    audit.outcome = query.value(QStringLiteral("outcome")).toString();
    audit.metadataJson = query.value(QStringLiteral("metadata_json")).toString();
    audit.createdAtMs = query.value(QStringLiteral("created_at_ms")).toLongLong();
    return audit;
}

QStringList normalizedRecipients(const QStringList& recipientIds,
                                 const QString& senderId)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString& raw : recipientIds) {
        const QString recipientId = cleaned(raw);
        if (recipientId.isEmpty() || recipientId == senderId || seen.contains(recipientId))
            continue;
        seen.insert(recipientId);
        result.push_back(recipientId);
    }
    return result;
}

int boundedLimit(int limit)
{
    return std::max(1, std::min(limit, 500));
}

QString compactJson(const QJsonObject& object)
{
    return QString::fromUtf8(
        QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QStringList normalizedCapabilities(const QStringList& capabilities)
{
    QStringList result;
    QSet<QString> seen;
    for (const QString& raw : capabilities) {
        const QString capability = cleaned(raw);
        const QString key = capability.toLower();
        if (capability.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        result.push_back(capability);
    }
    return result;
}

QString compactCapabilityArray(const QStringList& capabilities)
{
    QJsonArray array;
    for (const QString& capability : normalizedCapabilities(capabilities)) {
        array.append(capability);
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QStringList capabilityArrayFromJson(const QString& json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        return {};
    }

    QStringList capabilities;
    const QJsonArray array = document.array();
    for (const QJsonValue& value : array) {
        capabilities.push_back(value.toString());
    }
    return normalizedCapabilities(capabilities);
}

}

bool MessageClientCapabilityProfile::supports(const QString& capability) const
{
    return capabilities.contains(capability.trimmed(), Qt::CaseInsensitive);
}

MessageServiceDatabase::MessageServiceDatabase(const QString& databasePath,
                                               const QString& connectionName)
    : m_databasePath(databasePath),
      m_connectionName(normalizedConnectionName(connectionName))
{
}

MessageServiceDatabase::~MessageServiceDatabase()
{
    if (!QSqlDatabase::contains(m_connectionName))
        return;

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool MessageServiceDatabase::open()
{
    QSqlDatabase db = QSqlDatabase::contains(m_connectionName)
        ? QSqlDatabase::database(m_connectionName)
        : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_databasePath);
    if (!db.open()) {
        qWarning() << "MessageServiceDatabase: failed to open:"
                   << db.lastError().text();
        return false;
    }

    const auto execPragma = [&](const QString& sql) {
        QSqlQuery query(db);
        if (!query.exec(sql)) {
            qWarning() << "MessageServiceDatabase: failed pragma:"
                       << query.lastError().text() << sql;
        }
    };

    execPragma(QStringLiteral("PRAGMA journal_mode = WAL"));
    execPragma(QStringLiteral("PRAGMA foreign_keys = ON"));
    execPragma(QStringLiteral("PRAGMA busy_timeout = 5000"));

    return runMigrations();
}

bool MessageServiceDatabase::runMigrations() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    const auto exec = [&](const QString& sql) {
        if (!query.exec(sql)) {
            qWarning() << "MessageServiceDatabase migration error:"
                       << query.lastError().text() << "\nSQL:" << sql;
            return false;
        }
        return true;
    };

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS service_migrations ("
            "  module TEXT PRIMARY KEY,"
            "  version INTEGER NOT NULL,"
            "  updated_at_ms INTEGER NOT NULL"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS messages ("
            "  server_message_id TEXT PRIMARY KEY,"
            "  client_message_id TEXT NOT NULL,"
            "  conversation_id TEXT NOT NULL,"
            "  workspace_id TEXT NOT NULL,"
            "  sender_id TEXT NOT NULL,"
            "  server_seq INTEGER NOT NULL,"
            "  type TEXT NOT NULL,"
            "  body TEXT,"
            "  payload_json TEXT,"
            "  file_id TEXT,"
            "  content_type TEXT,"
            "  reply_to_message_id TEXT,"
            "  created_at_ms INTEGER NOT NULL,"
            "  updated_at_ms INTEGER NOT NULL,"
            "  deleted_at_ms INTEGER,"
            "  UNIQUE(sender_id, client_message_id),"
            "  UNIQUE(conversation_id, server_seq)"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_deliveries ("
            "  server_message_id TEXT NOT NULL,"
            "  recipient_id TEXT NOT NULL,"
            "  state TEXT NOT NULL,"
            "  delivered_at_ms INTEGER,"
            "  read_at_ms INTEGER,"
            "  retry_count INTEGER NOT NULL DEFAULT 0,"
            "  last_attempt_at_ms INTEGER,"
            "  PRIMARY KEY(server_message_id, recipient_id),"
            "  FOREIGN KEY(server_message_id) REFERENCES messages(server_message_id)"
            "    ON DELETE CASCADE"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversation_members ("
            "  conversation_id TEXT NOT NULL,"
            "  client_id TEXT NOT NULL,"
            "  role TEXT NOT NULL,"
            "  joined_at_ms INTEGER NOT NULL,"
            "  updated_at_ms INTEGER NOT NULL,"
            "  PRIMARY KEY(conversation_id, client_id)"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS conversation_cursors ("
            "  conversation_id TEXT NOT NULL,"
            "  client_id TEXT NOT NULL,"
            "  last_received_seq INTEGER NOT NULL DEFAULT 0,"
            "  last_read_seq INTEGER NOT NULL DEFAULT 0,"
            "  updated_at_ms INTEGER NOT NULL,"
            "  PRIMARY KEY(conversation_id, client_id)"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_events ("
            "  event_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  workspace_id TEXT NOT NULL,"
            "  conversation_id TEXT NOT NULL,"
            "  event_type TEXT NOT NULL,"
            "  payload_json TEXT NOT NULL,"
            "  created_at_ms INTEGER NOT NULL"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_workspaces ("
            "  workspace_id TEXT PRIMARY KEY,"
            "  display_name TEXT NOT NULL DEFAULT '',"
            "  created_by_id TEXT NOT NULL DEFAULT '',"
            "  enabled INTEGER NOT NULL DEFAULT 1,"
            "  created_at_ms INTEGER NOT NULL,"
            "  updated_at_ms INTEGER NOT NULL"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_audit_events ("
            "  audit_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  workspace_id TEXT NOT NULL DEFAULT '',"
            "  actor_client_id TEXT NOT NULL DEFAULT '',"
            "  action TEXT NOT NULL,"
            "  outcome TEXT NOT NULL,"
            "  metadata_json TEXT NOT NULL DEFAULT '{}',"
            "  created_at_ms INTEGER NOT NULL"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS message_client_capabilities ("
            "  workspace_id TEXT NOT NULL,"
            "  client_id TEXT NOT NULL,"
            "  app_version TEXT NOT NULL DEFAULT '',"
            "  capabilities_json TEXT NOT NULL DEFAULT '[]',"
            "  updated_at_ms INTEGER NOT NULL,"
            "  PRIMARY KEY(workspace_id, client_id)"
            ")")))
        return false;

    if (!exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_messages_conversation_seq "
            "ON messages(conversation_id, server_seq)")))
        return false;
    if (!exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_messages_workspace "
            "ON messages(workspace_id)")))
        return false;
    if (!exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_deliveries_recipient_state "
            "ON message_deliveries(recipient_id, state)")))
        return false;
    if (!exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_message_events_workspace_event "
            "ON message_events(workspace_id, event_id)")))
        return false;
    if (!exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_message_events_conversation_event "
            "ON message_events(conversation_id, event_id)")))
        return false;
    if (!exec(QStringLiteral(
            "CREATE INDEX IF NOT EXISTS idx_message_audit_workspace_id "
            "ON message_audit_events(workspace_id, audit_id DESC)")))
        return false;

    QSqlQuery migration(db);
    migration.prepare(QStringLiteral(
        "INSERT INTO service_migrations (module, version, updated_at_ms) "
        "VALUES ('message_service', 4, ?) "
        "ON CONFLICT(module) DO UPDATE SET "
        "  version = MAX(version, excluded.version),"
        "  updated_at_ms = excluded.updated_at_ms"));
    migration.addBindValue(nowMs());
    if (!migration.exec()) {
        qWarning() << "MessageServiceDatabase migration marker error:"
                   << migration.lastError().text();
        return false;
    }

    return true;
}

StoreMessageResult MessageServiceDatabase::storeMessage(
    const StoreMessageRequest& request) const
{
    StoreMessageResult result;
    const QString senderId = cleaned(request.senderId);
    const QString clientMessageId = cleaned(request.clientMessageId);
    const QString conversationId = cleaned(request.conversationId);
    const QString workspaceId = cleaned(request.workspaceId);
    const QString type = cleaned(request.type);
    const QStringList recipients = normalizedRecipients(request.recipientIds, senderId);

    if (senderId.isEmpty() || clientMessageId.isEmpty()
        || conversationId.isEmpty() || workspaceId.isEmpty() || type.isEmpty()) {
        result.error = QStringLiteral("missing required message fields");
        return result;
    }
    if (recipients.isEmpty()) {
        result.error = QStringLiteral("recipientIds must contain at least one recipient");
        return result;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        result.error = db.lastError().text();
        return result;
    }

    if (const auto duplicate = findMessageByClientId(senderId, clientMessageId)) {
        // Idempotent retry normally returns the original ACK. Mixed-version
        // groups add one important case: a recipient that was previously P2P
        // only can later upgrade and become service-capable. When the immutable
        // message payload is identical, merge missing delivery rows instead of
        // acknowledging the retry and silently omitting the newly capable peer.
        const bool sameImmutableMessage =
            duplicate->conversationId == conversationId
            && duplicate->workspaceId == workspaceId
            && duplicate->senderId == senderId
            && duplicate->type == type
            && duplicate->body == request.body
            && duplicate->payloadJson == request.payloadJson
            && duplicate->fileId == request.fileId
            && duplicate->contentType == request.contentType
            && duplicate->replyToMessageId == request.replyToMessageId;

        int addedRecipientCount = 0;
        if (sameImmutableMessage) {
            const qint64 timestamp = nowMs();
            for (const QString& recipientId : recipients) {
                if (!upsertConversationMember(conversationId,
                                              recipientId,
                                              QStringLiteral("member"),
                                              timestamp)) {
                    result.error = db.lastError().text();
                    db.rollback();
                    return result;
                }

                QSqlQuery delivery(db);
                delivery.prepare(QStringLiteral(
                    "INSERT OR IGNORE INTO message_deliveries ("
                    "  server_message_id, recipient_id, state, delivered_at_ms,"
                    "  read_at_ms, retry_count, last_attempt_at_ms"
                    ") VALUES (?, ?, 'pending', 0, 0, 0, 0)"));
                delivery.addBindValue(duplicate->serverMessageId);
                delivery.addBindValue(recipientId);
                if (!delivery.exec()) {
                    result.error = delivery.lastError().text();
                    db.rollback();
                    return result;
                }
                if (delivery.numRowsAffected() > 0) {
                    ++addedRecipientCount;
                }
            }

            if (addedRecipientCount > 0
                && appendMessageCreatedEvent(*duplicate).eventId <= 0) {
                result.error = QStringLiteral(
                    "failed to append recipient expansion event");
                db.rollback();
                return result;
            }
        }

        if (!db.commit()) {
            result.error = db.lastError().text();
            return result;
        }
        result.ok = true;
        result.duplicate = true;
        result.message = *duplicate;
        return result;
    }

    const qint64 timestamp = request.createdAtMs > 0 ? request.createdAtMs : nowMs();
    const qint64 serverSeq = nextConversationSeq(conversationId);
    const QString serverMessageId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO messages ("
        "  server_message_id, client_message_id, conversation_id, workspace_id,"
        "  sender_id, server_seq, type, body, payload_json, file_id,"
        "  content_type, reply_to_message_id, created_at_ms, updated_at_ms"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    insert.addBindValue(serverMessageId);
    insert.addBindValue(clientMessageId);
    insert.addBindValue(conversationId);
    insert.addBindValue(workspaceId);
    insert.addBindValue(senderId);
    insert.addBindValue(serverSeq);
    insert.addBindValue(type);
    insert.addBindValue(request.body);
    insert.addBindValue(request.payloadJson);
    insert.addBindValue(request.fileId);
    insert.addBindValue(request.contentType);
    insert.addBindValue(request.replyToMessageId);
    insert.addBindValue(timestamp);
    insert.addBindValue(timestamp);
    if (!insert.exec()) {
        result.error = insert.lastError().text();
        db.rollback();
        return result;
    }

    if (!upsertConversationMember(conversationId, senderId,
                                  QStringLiteral("sender"), timestamp)) {
        result.error = db.lastError().text();
        db.rollback();
        return result;
    }

    for (const QString& recipientId : recipients) {
        if (!upsertConversationMember(conversationId, recipientId,
                                      QStringLiteral("member"), timestamp)) {
            result.error = db.lastError().text();
            db.rollback();
            return result;
        }

        QSqlQuery delivery(db);
        delivery.prepare(QStringLiteral(
            "INSERT INTO message_deliveries ("
            "  server_message_id, recipient_id, state, delivered_at_ms,"
            "  read_at_ms, retry_count, last_attempt_at_ms"
            ") VALUES (?, ?, 'pending', 0, 0, 0, 0)"));
        delivery.addBindValue(serverMessageId);
        delivery.addBindValue(recipientId);
        if (!delivery.exec()) {
            result.error = delivery.lastError().text();
            db.rollback();
            return result;
        }
    }

    StoredMessage insertedMessage;
    insertedMessage.serverMessageId = serverMessageId;
    insertedMessage.clientMessageId = clientMessageId;
    insertedMessage.conversationId = conversationId;
    insertedMessage.workspaceId = workspaceId;
    insertedMessage.senderId = senderId;
    insertedMessage.serverSeq = serverSeq;
    insertedMessage.type = type;
    insertedMessage.body = request.body;
    insertedMessage.payloadJson = request.payloadJson;
    insertedMessage.fileId = request.fileId;
    insertedMessage.contentType = request.contentType;
    insertedMessage.replyToMessageId = request.replyToMessageId;
    insertedMessage.createdAtMs = timestamp;
    insertedMessage.updatedAtMs = timestamp;
    const StoredMessageEvent messageEvent =
        appendMessageCreatedEvent(insertedMessage);
    if (messageEvent.eventId <= 0) {
        result.error = QStringLiteral("failed to append message event");
        db.rollback();
        return result;
    }

    if (!db.commit()) {
        result.error = db.lastError().text();
        return result;
    }

    result.ok = true;
    result.message = *findMessageByServerId(serverMessageId);
    return result;
}

std::optional<StoredMessage> MessageServiceDatabase::findMessageByServerId(
    const QString& serverMessageId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT * FROM messages WHERE server_message_id = ?"));
    query.addBindValue(serverMessageId);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase findMessageByServerId error:"
                   << query.lastError().text();
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return messageFromQuery(query);
}

StoredMessageEvent MessageServiceDatabase::appendMessageCreatedEvent(
    const StoredMessage& message) const
{
    return appendMessageEventWithPayload(
        message.workspaceId,
        message.conversationId,
        QStringLiteral("message.created"),
        QJsonObject{
            {QStringLiteral("serverMessageId"), message.serverMessageId},
            {QStringLiteral("clientMessageId"), message.clientMessageId},
            {QStringLiteral("senderId"), message.senderId},
            {QStringLiteral("serverSeq"), message.serverSeq},
            {QStringLiteral("messageType"), message.type}
        });
}

StoredMessageEvent MessageServiceDatabase::appendSessionStatusEvent(
    const QString& workspaceId,
    const QString& eventType,
    const QString& sessionId,
    const QString& clientId,
    const QString& deviceId,
    qint64 connectedAtMs,
    qint64 lastSeenAtMs,
    qint64 lastEventId) const
{
    const QString normalizedEventType = cleaned(eventType);
    const QString normalizedSessionId = cleaned(sessionId);
    const QString normalizedClientId = cleaned(clientId);
    const QString normalizedDeviceId = cleaned(deviceId);
    if ((normalizedEventType != QStringLiteral("session.online")
         && normalizedEventType != QStringLiteral("session.offline"))
        || normalizedSessionId.isEmpty() || normalizedClientId.isEmpty()
        || normalizedDeviceId.isEmpty()) {
        return {};
    }

    return appendMessageEventWithPayload(
        workspaceId,
        QStringLiteral("__sessions__"),
        normalizedEventType,
        QJsonObject{
            {QStringLiteral("sessionId"), normalizedSessionId},
            {QStringLiteral("clientId"), normalizedClientId},
            {QStringLiteral("deviceId"), normalizedDeviceId},
            {QStringLiteral("connectedAtMs"), std::max<qint64>(0, connectedAtMs)},
            {QStringLiteral("lastSeenAtMs"), std::max<qint64>(0, lastSeenAtMs)},
            {QStringLiteral("lastEventId"), std::max<qint64>(0, lastEventId)}
        });
}

StoredMessageEvent MessageServiceDatabase::appendMessageEventWithPayload(
    const QString& workspaceId,
    const QString& conversationId,
    const QString& eventType,
    QJsonObject payload) const
{
    StoredMessageEvent event;
    const QString normalizedWorkspaceId = cleaned(workspaceId);
    const QString normalizedConversationId = cleaned(conversationId);
    const QString normalizedEventType = cleaned(eventType);
    if (normalizedWorkspaceId.isEmpty() || normalizedConversationId.isEmpty()
        || normalizedEventType.isEmpty()) {
        return event;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    const qint64 timestamp = nowMs();

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO message_events ("
        "  workspace_id, conversation_id, event_type, payload_json, created_at_ms"
        ") VALUES (?, ?, ?, ?, ?)"));
    insert.addBindValue(normalizedWorkspaceId);
    insert.addBindValue(normalizedConversationId);
    insert.addBindValue(normalizedEventType);
    insert.addBindValue(QStringLiteral("{}"));
    insert.addBindValue(timestamp);
    if (!insert.exec()) {
        qWarning() << "MessageServiceDatabase appendMessageEventWithPayload error:"
                   << insert.lastError().text();
        return event;
    }

    const qint64 eventId = insert.lastInsertId().toLongLong();
    if (eventId <= 0) {
        qWarning() << "MessageServiceDatabase appendMessageEventWithPayload missing event id";
        return event;
    }

    payload[QStringLiteral("eventId")] = eventId;
    payload[QStringLiteral("type")] = normalizedEventType;
    payload[QStringLiteral("createdAtMs")] = timestamp;
    payload[QStringLiteral("conversationId")] = normalizedConversationId;
    payload[QStringLiteral("workspaceId")] = normalizedWorkspaceId;

    event.eventId = eventId;
    event.workspaceId = normalizedWorkspaceId;
    event.conversationId = normalizedConversationId;
    event.eventType = normalizedEventType;
    event.createdAtMs = timestamp;
    event.payloadJson = compactJson(payload);

    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE message_events SET payload_json = ? WHERE event_id = ?"));
    update.addBindValue(event.payloadJson);
    update.addBindValue(eventId);
    if (!update.exec()) {
        qWarning() << "MessageServiceDatabase appendMessageEventWithPayload payload error:"
                   << update.lastError().text();
        QSqlQuery cleanup(db);
        cleanup.prepare(QStringLiteral(
            "DELETE FROM message_events WHERE event_id = ?"));
        cleanup.addBindValue(eventId);
        cleanup.exec();
        return {};
    }

    return event;
}

StoredMessageEvent MessageServiceDatabase::appendMessageStateEvent(
    const StoredMessage& message,
    const QString& eventType,
    const QString& recipientId,
    const QString& cursorField,
    qint64 cursorSeq) const
{
    const QString normalizedRecipientId = cleaned(recipientId);
    const QString normalizedCursorField = cleaned(cursorField);
    if (normalizedRecipientId.isEmpty() || normalizedCursorField.isEmpty()) {
        return {};
    }

    QJsonObject payload{
        {QStringLiteral("serverMessageId"), message.serverMessageId},
        {QStringLiteral("recipientId"), normalizedRecipientId},
        {QStringLiteral("serverSeq"), message.serverSeq}
    };
    payload[normalizedCursorField] = std::max<qint64>(0, cursorSeq);

    return appendMessageEventWithPayload(message.workspaceId,
                                         message.conversationId,
                                         eventType,
                                         payload);
}

QVector<StoredMessageEvent> MessageServiceDatabase::listMessageEventsAfter(
    const QString& workspaceId,
    qint64 afterEventId,
    int limit) const
{
    const QString normalizedWorkspaceId = cleaned(workspaceId);
    if (normalizedWorkspaceId.isEmpty()) {
        return {};
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT * FROM message_events "
        "WHERE workspace_id = ? AND event_id > ? "
        "ORDER BY event_id ASC "
        "LIMIT ?"));
    query.addBindValue(normalizedWorkspaceId);
    query.addBindValue(std::max<qint64>(0, afterEventId));
    query.addBindValue(boundedLimit(limit));
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase listMessageEventsAfter error:"
                   << query.lastError().text();
        return {};
    }

    QVector<StoredMessageEvent> events;
    while (query.next()) {
        events.push_back(eventFromQuery(query));
    }
    return events;
}

QVector<StoredMessageEvent>
MessageServiceDatabase::listMessageEventsAfterForClient(
    const QString& workspaceId,
    const QString& clientId,
    qint64 afterEventId,
    int limit) const
{
    const QString normalizedWorkspaceId = cleaned(workspaceId);
    const QString normalizedClientId = cleaned(clientId);
    if (normalizedWorkspaceId.isEmpty() || normalizedClientId.isEmpty()) {
        return {};
    }

    // Message events are conversation-private. Session presence events are
    // workspace-wide and intentionally remain visible to every authenticated
    // member of the workspace.
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT event.* FROM message_events AS event "
        "WHERE event.workspace_id = ? AND event.event_id > ? "
        "  AND (event.conversation_id = '__sessions__' "
        "       OR EXISTS ("
        "           SELECT 1 FROM conversation_members AS member "
        "           WHERE member.conversation_id = event.conversation_id "
        "             AND member.client_id = ?"
        "       )) "
        "ORDER BY event.event_id ASC "
        "LIMIT ?"));
    query.addBindValue(normalizedWorkspaceId);
    query.addBindValue(std::max<qint64>(0, afterEventId));
    query.addBindValue(normalizedClientId);
    query.addBindValue(boundedLimit(limit));
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase listMessageEventsAfterForClient error:"
                   << query.lastError().text();
        return {};
    }

    QVector<StoredMessageEvent> events;
    while (query.next()) {
        events.push_back(eventFromQuery(query));
    }
    return events;
}

std::optional<StoredMessage> MessageServiceDatabase::findMessageByClientId(
    const QString& senderId,
    const QString& clientMessageId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT * FROM messages WHERE sender_id = ? AND client_message_id = ?"));
    query.addBindValue(senderId);
    query.addBindValue(clientMessageId);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase findMessageByClientId error:"
                   << query.lastError().text();
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return messageFromQuery(query);
}

QVector<StoredMessage> MessageServiceDatabase::listMessagesAfterSeq(
    const QString& conversationId,
    qint64 afterSeq,
    int limit) const
{
    QVector<StoredMessage> messages;
    const int boundedLimit = std::max(1, std::min(limit, 500));

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT * FROM messages "
        "WHERE conversation_id = ? AND server_seq > ? AND deleted_at_ms IS NULL "
        "ORDER BY server_seq ASC "
        "LIMIT ?"));
    query.addBindValue(conversationId);
    query.addBindValue(afterSeq);
    query.addBindValue(boundedLimit);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase listMessagesAfterSeq error:"
                   << query.lastError().text();
        return messages;
    }
    while (query.next())
        messages.push_back(messageFromQuery(query));
    return messages;
}

QVector<StoredConversation> MessageServiceDatabase::listConversationsForMember(
    const QString& workspaceId,
    const QString& clientId,
    int limit) const
{
    QVector<StoredConversation> conversations;
    const QString normalizedWorkspaceId = cleaned(workspaceId);
    const QString normalizedClientId = cleaned(clientId);
    if (normalizedWorkspaceId.isEmpty() || normalizedClientId.isEmpty()) {
        return conversations;
    }

    const int boundedLimit = std::max(1, std::min(limit, 500));
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(R"(
        SELECT m.conversation_id,
               m.workspace_id,
               MAX(m.server_seq) AS latest_server_seq,
               MAX(m.updated_at_ms) AS latest_updated_at_ms
        FROM messages m
        INNER JOIN conversation_members cm
            ON cm.conversation_id = m.conversation_id
        WHERE m.workspace_id = ?
          AND cm.client_id = ?
          AND m.deleted_at_ms IS NULL
        GROUP BY m.conversation_id, m.workspace_id
        ORDER BY latest_updated_at_ms DESC, m.conversation_id ASC
        LIMIT ?
    )"));
    query.addBindValue(normalizedWorkspaceId);
    query.addBindValue(normalizedClientId);
    query.addBindValue(boundedLimit);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase listConversationsForMember error:"
                   << query.lastError().text();
        return conversations;
    }

    while (query.next()) {
        StoredConversation conversation;
        conversation.conversationId = query.value(0).toString();
        conversation.workspaceId = query.value(1).toString();
        conversation.latestServerSeq = query.value(2).toLongLong();
        conversation.updatedAtMs = query.value(3).toLongLong();
        conversations.push_back(conversation);
    }
    return conversations;
}

QVector<MessageDeliveryRecord> MessageServiceDatabase::listDeliveries(
    const QString& serverMessageId) const
{
    QVector<MessageDeliveryRecord> deliveries;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT * FROM message_deliveries "
        "WHERE server_message_id = ? "
        "ORDER BY recipient_id ASC"));
    query.addBindValue(serverMessageId);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase listDeliveries error:"
                   << query.lastError().text();
        return deliveries;
    }
    while (query.next())
        deliveries.push_back(deliveryFromQuery(query));
    return deliveries;
}

bool MessageServiceDatabase::upsertWorkspace(
    const MessageWorkspaceRecord& workspace) const
{
    const QString workspaceId = cleaned(workspace.workspaceId);
    if (workspaceId.isEmpty())
        return false;

    const qint64 timestamp = nowMs();
    const qint64 createdAtMs =
        workspace.createdAtMs > 0 ? workspace.createdAtMs : timestamp;
    const qint64 updatedAtMs =
        workspace.updatedAtMs > 0 ? workspace.updatedAtMs : timestamp;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO message_workspaces "
        "(workspace_id, display_name, created_by_id, enabled, created_at_ms, updated_at_ms) "
        "VALUES (?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(workspace_id) DO UPDATE SET "
        "  display_name = excluded.display_name,"
        "  enabled = excluded.enabled,"
        "  updated_at_ms = excluded.updated_at_ms"));
    query.addBindValue(workspaceId);
    query.addBindValue(workspace.displayName.trimmed());
    query.addBindValue(workspace.createdById.trimmed());
    query.addBindValue(workspace.enabled ? 1 : 0);
    query.addBindValue(createdAtMs);
    query.addBindValue(updatedAtMs);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase upsertWorkspace error:"
                   << query.lastError().text();
        return false;
    }
    return true;
}

QVector<MessageWorkspaceRecord> MessageServiceDatabase::listWorkspaces() const
{
    QVector<MessageWorkspaceRecord> workspaces;
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (!query.exec(QStringLiteral(
            "SELECT * FROM message_workspaces ORDER BY workspace_id ASC"))) {
        qWarning() << "MessageServiceDatabase listWorkspaces error:"
                   << query.lastError().text();
        return workspaces;
    }
    while (query.next())
        workspaces.push_back(workspaceFromQuery(query));
    return workspaces;
}

MessageAuditRecord MessageServiceDatabase::appendAuditEvent(
    const QString& workspaceId,
    const QString& actorClientId,
    const QString& action,
    const QString& outcome,
    QJsonObject metadata) const
{
    MessageAuditRecord audit;
    const QString normalizedAction = cleaned(action);
    const QString normalizedOutcome = cleaned(outcome);
    if (normalizedAction.isEmpty() || normalizedOutcome.isEmpty())
        return audit;

    const qint64 timestamp = nowMs();
    audit.workspaceId = cleaned(workspaceId);
    audit.actorClientId = cleaned(actorClientId);
    audit.action = normalizedAction;
    audit.outcome = normalizedOutcome;
    audit.metadataJson =
        QString::fromUtf8(QJsonDocument(sanitizedAuditMetadata(metadata))
                              .toJson(QJsonDocument::Compact));
    audit.createdAtMs = timestamp;

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO message_audit_events "
        "(workspace_id, actor_client_id, action, outcome, metadata_json, created_at_ms) "
        "VALUES (?, ?, ?, ?, ?, ?)"));
    query.addBindValue(audit.workspaceId);
    query.addBindValue(audit.actorClientId);
    query.addBindValue(audit.action);
    query.addBindValue(audit.outcome);
    query.addBindValue(audit.metadataJson);
    query.addBindValue(audit.createdAtMs);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase appendAuditEvent error:"
                   << query.lastError().text();
        return {};
    }
    audit.auditId = query.lastInsertId().toLongLong();
    return audit;
}

QVector<MessageAuditRecord> MessageServiceDatabase::listAuditEvents(
    const QString& workspaceId,
    int limit) const
{
    QVector<MessageAuditRecord> audits;
    const int boundedLimit = std::max(1, std::min(limit, 500));
    const QString normalizedWorkspaceId = cleaned(workspaceId);

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    if (normalizedWorkspaceId.isEmpty()) {
        query.prepare(QStringLiteral(
            "SELECT * FROM message_audit_events "
            "ORDER BY audit_id DESC LIMIT ?"));
        query.addBindValue(boundedLimit);
    } else {
        query.prepare(QStringLiteral(
            "SELECT * FROM message_audit_events "
            "WHERE workspace_id = ? "
            "ORDER BY audit_id DESC LIMIT ?"));
        query.addBindValue(normalizedWorkspaceId);
        query.addBindValue(boundedLimit);
    }
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase listAuditEvents error:"
                   << query.lastError().text();
        return audits;
    }
    while (query.next())
        audits.push_back(auditFromQuery(query));
    return audits;
}

std::optional<MessageDeliveryRecord> MessageServiceDatabase::findDeliveryRecord(
    const QString& serverMessageId,
    const QString& recipientId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT * FROM message_deliveries "
        "WHERE server_message_id = ? AND recipient_id = ?"));
    query.addBindValue(serverMessageId);
    query.addBindValue(recipientId);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase findDeliveryRecord error:"
                   << query.lastError().text();
        return std::nullopt;
    }
    if (!query.next()) {
        return std::nullopt;
    }
    return deliveryFromQuery(query);
}

bool MessageServiceDatabase::isConversationMember(
    const QString& conversationId,
    const QString& clientId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT 1 FROM conversation_members "
        "WHERE conversation_id = ? AND client_id = ?"));
    query.addBindValue(conversationId);
    query.addBindValue(clientId);
    return query.exec() && query.next();
}

bool MessageServiceDatabase::markDelivered(const QString& serverMessageId,
                                           const QString& recipientId,
                                           qint64 receivedSeq) const
{
    const auto message = findMessageByServerId(serverMessageId);
    if (!message)
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction())
        return false;

    const auto deliveryBefore = findDeliveryRecord(serverMessageId, recipientId);
    if (!deliveryBefore) {
        db.rollback();
        return false;
    }
    const bool shouldAppendEvent =
        deliveryBefore->deliveredAtMs <= 0 && deliveryBefore->readAtMs <= 0;

    const qint64 timestamp = nowMs();
    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE message_deliveries "
        "SET state = CASE WHEN state = 'read' THEN state ELSE 'delivered' END,"
        "    delivered_at_ms = CASE "
        "      WHEN delivered_at_ms IS NULL OR delivered_at_ms = 0 THEN ? "
        "      ELSE delivered_at_ms END "
        "WHERE server_message_id = ? AND recipient_id = ?"));
    update.addBindValue(timestamp);
    update.addBindValue(serverMessageId);
    update.addBindValue(recipientId);
    if (!update.exec() || update.numRowsAffected() <= 0) {
        db.rollback();
        return false;
    }

    if (!upsertCursor(message->conversationId, recipientId, receivedSeq, 0, timestamp)) {
        db.rollback();
        return false;
    }

    if (shouldAppendEvent
        && appendMessageStateEvent(*message,
                                   QStringLiteral("message.delivered"),
                                   recipientId,
                                   QStringLiteral("receivedSeq"),
                                   receivedSeq).eventId <= 0) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool MessageServiceDatabase::markRead(const QString& serverMessageId,
                                      const QString& recipientId,
                                      qint64 readSeq) const
{
    const auto message = findMessageByServerId(serverMessageId);
    if (!message)
        return false;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction())
        return false;

    const auto deliveryBefore = findDeliveryRecord(serverMessageId, recipientId);
    if (!deliveryBefore) {
        db.rollback();
        return false;
    }
    const bool shouldAppendEvent = deliveryBefore->readAtMs <= 0;

    const qint64 timestamp = nowMs();
    QSqlQuery update(db);
    update.prepare(QStringLiteral(
        "UPDATE message_deliveries "
        "SET state = 'read',"
        "    delivered_at_ms = CASE "
        "      WHEN delivered_at_ms IS NULL OR delivered_at_ms = 0 THEN ? "
        "      ELSE delivered_at_ms END,"
        "    read_at_ms = ? "
        "WHERE server_message_id = ? AND recipient_id = ?"));
    update.addBindValue(timestamp);
    update.addBindValue(timestamp);
    update.addBindValue(serverMessageId);
    update.addBindValue(recipientId);
    if (!update.exec() || update.numRowsAffected() <= 0) {
        db.rollback();
        return false;
    }

    if (!upsertCursor(message->conversationId, recipientId, readSeq, readSeq, timestamp)) {
        db.rollback();
        return false;
    }

    if (shouldAppendEvent
        && appendMessageStateEvent(*message,
                                   QStringLiteral("message.read"),
                                   recipientId,
                                   QStringLiteral("readSeq"),
                                   readSeq).eventId <= 0) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool MessageServiceDatabase::upsertClientCapabilities(
    const QString& workspaceId,
    const QString& clientId,
    const QString& appVersion,
    const QStringList& capabilities,
    qint64 updatedAtMs) const
{
    const QString normalizedWorkspaceId = cleaned(workspaceId);
    const QString normalizedClientId = cleaned(clientId);
    if (normalizedWorkspaceId.isEmpty() || normalizedClientId.isEmpty()) {
        return false;
    }

    const qint64 timestamp = updatedAtMs > 0 ? updatedAtMs : nowMs();
    QString normalizedAppVersion = appVersion.trimmed();
    if (normalizedAppVersion.isNull()) {
        normalizedAppVersion = QStringLiteral("");
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO message_client_capabilities ("
        "  workspace_id, client_id, app_version, capabilities_json, updated_at_ms"
        ") VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(workspace_id, client_id) DO UPDATE SET "
        "  app_version = excluded.app_version,"
        "  capabilities_json = excluded.capabilities_json,"
        "  updated_at_ms = excluded.updated_at_ms"));
    query.addBindValue(normalizedWorkspaceId);
    query.addBindValue(normalizedClientId);
    query.addBindValue(normalizedAppVersion);
    query.addBindValue(compactCapabilityArray(capabilities));
    query.addBindValue(timestamp);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase upsertClientCapabilities error:"
                   << query.lastError().text();
        return false;
    }
    return true;
}

QVector<MessageClientCapabilityProfile>
MessageServiceDatabase::loadClientCapabilities(
    const QString& workspaceId,
    const QStringList& clientIds) const
{
    const QString normalizedWorkspaceId = cleaned(workspaceId);
    if (normalizedWorkspaceId.isEmpty()) {
        return {};
    }

    QStringList orderedClientIds;
    QSet<QString> seen;
    for (const QString& raw : clientIds) {
        const QString clientId = cleaned(raw);
        if (clientId.isEmpty() || seen.contains(clientId)) {
            continue;
        }
        seen.insert(clientId);
        orderedClientIds.push_back(clientId);
    }

    QVector<MessageClientCapabilityProfile> profiles;
    profiles.reserve(orderedClientIds.size());

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT workspace_id, client_id, app_version, capabilities_json, updated_at_ms "
        "FROM message_client_capabilities "
        "WHERE workspace_id = ? AND client_id = ?"));

    for (const QString& clientId : orderedClientIds) {
        query.bindValue(0, normalizedWorkspaceId);
        query.bindValue(1, clientId);

        MessageClientCapabilityProfile profile;
        profile.workspaceId = normalizedWorkspaceId;
        profile.clientId = clientId;

        if (query.exec() && query.next()) {
            profile.appVersion =
                query.value(QStringLiteral("app_version")).toString();
            profile.capabilities = capabilityArrayFromJson(
                query.value(QStringLiteral("capabilities_json")).toString());
            profile.updatedAtMs =
                query.value(QStringLiteral("updated_at_ms")).toLongLong();
        } else if (query.lastError().isValid()) {
            qWarning() << "MessageServiceDatabase loadClientCapabilities error:"
                       << query.lastError().text();
        }

        profiles.push_back(profile);
        query.finish();
    }

    return profiles;
}

qint64 MessageServiceDatabase::nextConversationSeq(
    const QString& conversationId) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "SELECT COALESCE(MAX(server_seq), 0) + 1 FROM messages "
        "WHERE conversation_id = ?"));
    query.addBindValue(conversationId);
    if (!query.exec() || !query.next())
        return 1;
    return query.value(0).toLongLong();
}

bool MessageServiceDatabase::upsertConversationMember(
    const QString& conversationId,
    const QString& clientId,
    const QString& role,
    qint64 timestamp) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO conversation_members ("
        "  conversation_id, client_id, role, joined_at_ms, updated_at_ms"
        ") VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(conversation_id, client_id) DO UPDATE SET "
        "  role = CASE "
        "    WHEN conversation_members.role = 'sender' THEN 'sender' "
        "    ELSE excluded.role END,"
        "  updated_at_ms = excluded.updated_at_ms"));
    query.addBindValue(conversationId);
    query.addBindValue(clientId);
    query.addBindValue(role);
    query.addBindValue(timestamp);
    query.addBindValue(timestamp);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase upsertConversationMember error:"
                   << query.lastError().text();
        return false;
    }
    return true;
}

bool MessageServiceDatabase::upsertCursor(const QString& conversationId,
                                          const QString& clientId,
                                          qint64 receivedSeq,
                                          qint64 readSeq,
                                          qint64 timestamp) const
{
    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    query.prepare(QStringLiteral(
        "INSERT INTO conversation_cursors ("
        "  conversation_id, client_id, last_received_seq, last_read_seq, updated_at_ms"
        ") VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(conversation_id, client_id) DO UPDATE SET "
        "  last_received_seq = MAX(conversation_cursors.last_received_seq,"
        "                          excluded.last_received_seq),"
        "  last_read_seq = MAX(conversation_cursors.last_read_seq,"
        "                      excluded.last_read_seq),"
        "  updated_at_ms = excluded.updated_at_ms"));
    query.addBindValue(conversationId);
    query.addBindValue(clientId);
    query.addBindValue(std::max<qint64>(0, receivedSeq));
    query.addBindValue(std::max<qint64>(0, readSeq));
    query.addBindValue(timestamp);
    if (!query.exec()) {
        qWarning() << "MessageServiceDatabase upsertCursor error:"
                   << query.lastError().text();
        return false;
    }
    return true;
}
