#include "MessageServiceHttpContracts.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>

#include <algorithm>

namespace {
void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

std::optional<QString> requiredString(const QJsonObject& object,
                                      const QString& field,
                                      QString* error)
{
    const QJsonValue value = object.value(field);
    if (!value.isString()) {
        setError(error, QStringLiteral("%1 is required").arg(field));
        return std::nullopt;
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        setError(error, QStringLiteral("%1 is required").arg(field));
        return std::nullopt;
    }
    return text;
}

QString optionalString(const QJsonObject& object, const QString& field)
{
    const QJsonValue value = object.value(field);
    return value.isString() ? value.toString() : QString();
}

std::optional<QString> parsePayloadJson(const QJsonObject& object,
                                        QString* error)
{
    if (object.contains(QStringLiteral("payload"))) {
        const QJsonValue payload = object.value(QStringLiteral("payload"));
        if (!payload.isObject()) {
            setError(error, QStringLiteral("payload must be an object"));
            return std::nullopt;
        }
        return QString::fromUtf8(
            QJsonDocument(payload.toObject()).toJson(QJsonDocument::Compact));
    }

    if (object.contains(QStringLiteral("payloadJson"))) {
        const QJsonValue payloadJson = object.value(QStringLiteral("payloadJson"));
        if (!payloadJson.isString()) {
            setError(error, QStringLiteral("payloadJson must be a string"));
            return std::nullopt;
        }

        const QString text = payloadJson.toString().trimmed();
        if (text.isEmpty())
            return QString();

        const QJsonDocument document = QJsonDocument::fromJson(text.toUtf8());
        if (!document.isObject()) {
            setError(error, QStringLiteral("payloadJson must be a JSON object"));
            return std::nullopt;
        }
        return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
    }

    return QString();
}

QJsonObject payloadObjectFromJson(const QString& payloadJson)
{
    if (payloadJson.trimmed().isEmpty())
        return {};

    const QJsonDocument document = QJsonDocument::fromJson(payloadJson.toUtf8());
    return document.isObject() ? document.object() : QJsonObject{};
}
}

namespace MessageServiceHttpContracts {

std::optional<StoreMessageRequest> parseStoreMessageRequest(
    const QJsonObject& object,
    const QString& senderId,
    QString* error)
{
    const auto clientMessageId =
        requiredString(object, QStringLiteral("clientMessageId"), error);
    if (!clientMessageId)
        return std::nullopt;

    const auto conversationId =
        requiredString(object, QStringLiteral("conversationId"), error);
    if (!conversationId)
        return std::nullopt;

    const auto workspaceId =
        requiredString(object, QStringLiteral("workspaceId"), error);
    if (!workspaceId)
        return std::nullopt;

    const auto type = requiredString(object, QStringLiteral("type"), error);
    if (!type)
        return std::nullopt;

    if (!object.value(QStringLiteral("recipientIds")).isArray()) {
        setError(error, QStringLiteral("recipientIds is required"));
        return std::nullopt;
    }

    QStringList recipientIds;
    const QJsonArray recipients = object.value(QStringLiteral("recipientIds")).toArray();
    for (const QJsonValue& value : recipients) {
        if (!value.isString()) {
            setError(error, QStringLiteral("recipientIds must contain strings"));
            return std::nullopt;
        }
        const QString recipientId = value.toString().trimmed();
        if (!recipientId.isEmpty())
            recipientIds.push_back(recipientId);
    }
    if (recipientIds.isEmpty()) {
        setError(error, QStringLiteral("recipientIds must not be empty"));
        return std::nullopt;
    }

    const auto payloadJson = parsePayloadJson(object, error);
    if (!payloadJson)
        return std::nullopt;

    StoreMessageRequest request;
    request.clientMessageId = *clientMessageId;
    request.conversationId = *conversationId;
    request.workspaceId = *workspaceId;
    request.senderId = senderId.trimmed();
    request.type = *type;
    request.body = optionalString(object, QStringLiteral("body"));
    request.payloadJson = *payloadJson;
    request.fileId = optionalString(object, QStringLiteral("fileId"));
    request.contentType = optionalString(object, QStringLiteral("contentType"));
    request.replyToMessageId =
        optionalString(object, QStringLiteral("replyToMessageId"));
    request.recipientIds = recipientIds;
    request.createdAtMs =
        object.value(QStringLiteral("createdAtMs")).toInteger(0);

    if (request.senderId.isEmpty()) {
        setError(error, QStringLiteral("authenticated sender is required"));
        return std::nullopt;
    }

    return request;
}

QJsonObject storeMessageResultToJson(const StoreMessageResult& result)
{
    QJsonObject object;
    object[QStringLiteral("ok")] = result.ok;
    object[QStringLiteral("duplicate")] = result.duplicate;
    if (!result.error.isEmpty())
        object[QStringLiteral("error")] = result.error;

    if (result.ok) {
        object[QStringLiteral("serverMessageId")] =
            result.message.serverMessageId;
        object[QStringLiteral("conversationId")] =
            result.message.conversationId;
        object[QStringLiteral("serverSeq")] = result.message.serverSeq;
        object[QStringLiteral("createdAtMs")] = result.message.createdAtMs;
    }
    return object;
}

QJsonObject storedMessageToJson(const StoredMessage& message)
{
    QJsonObject object;
    object[QStringLiteral("serverMessageId")] = message.serverMessageId;
    object[QStringLiteral("clientMessageId")] = message.clientMessageId;
    object[QStringLiteral("conversationId")] = message.conversationId;
    object[QStringLiteral("workspaceId")] = message.workspaceId;
    object[QStringLiteral("senderId")] = message.senderId;
    object[QStringLiteral("serverSeq")] = message.serverSeq;
    object[QStringLiteral("type")] = message.type;
    object[QStringLiteral("body")] = message.body;
    object[QStringLiteral("payload")] = payloadObjectFromJson(message.payloadJson);
    object[QStringLiteral("fileId")] = message.fileId;
    object[QStringLiteral("contentType")] = message.contentType;
    object[QStringLiteral("replyToMessageId")] = message.replyToMessageId;
    object[QStringLiteral("createdAtMs")] = message.createdAtMs;
    return object;
}

QJsonObject messageListToJson(const QVector<StoredMessage>& messages)
{
    QJsonArray array;
    qint64 nextAfterSeq = 0;
    for (const StoredMessage& message : messages) {
        array.append(storedMessageToJson(message));
        nextAfterSeq = std::max(nextAfterSeq, message.serverSeq);
    }

    QJsonObject object;
    object[QStringLiteral("messages")] = array;
    object[QStringLiteral("nextAfterSeq")] = nextAfterSeq;
    return object;
}

QJsonObject conversationListToJson(
    const QVector<StoredConversation>& conversations)
{
    QJsonArray array;
    for (const StoredConversation& conversation : conversations) {
        QJsonObject object;
        object[QStringLiteral("conversationId")] = conversation.conversationId;
        object[QStringLiteral("workspaceId")] = conversation.workspaceId;
        object[QStringLiteral("latestServerSeq")] = conversation.latestServerSeq;
        object[QStringLiteral("updatedAtMs")] = conversation.updatedAtMs;
        array.append(object);
    }

    return QJsonObject{
        {QStringLiteral("conversations"), array}
    };
}

std::optional<qint64> parseAckSeq(const QJsonObject& object,
                                  const QString& fieldName,
                                  QString* error)
{
    const QJsonValue value = object.value(fieldName);
    if (!value.isDouble()) {
        setError(error, QStringLiteral("%1 is required").arg(fieldName));
        return std::nullopt;
    }

    const qint64 seq = value.toInteger(-1);
    if (seq < 0) {
        setError(error, QStringLiteral("%1 must be a non-negative integer")
                            .arg(fieldName));
        return std::nullopt;
    }
    return seq;
}

}
