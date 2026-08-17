#pragma once

#include <optional>
#include <string>

#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "domain/MessageEnvelope.h"

enum class MessageMutationKind { Recall, Edit };

struct MessageMutation {
    QString mutationMessageId;
    QString targetMessageId;
    QString conversationId;
    QString actorId;
    MessageMutationKind kind = MessageMutationKind::Recall;
    QString newBody;
    QString newContentType;   // "plain" | "html"
    qint64 mutatedAtMs = 0;
};

inline std::string buildRecallPayloadJson(const QString& targetMessageId, qint64 mutatedAtMs)
{
    QJsonObject obj;
    obj[QStringLiteral("target_message_id")] = targetMessageId;
    obj[QStringLiteral("mutation_kind")] = QStringLiteral("recall");
    obj[QStringLiteral("mutated_at_ms")] = mutatedAtMs;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

inline std::string buildEditPayloadJson(const QString& targetMessageId,
                                        const QString& newBody,
                                        const QString& newContentType,
                                        qint64 mutatedAtMs)
{
    QJsonObject obj;
    obj[QStringLiteral("target_message_id")] = targetMessageId;
    obj[QStringLiteral("mutation_kind")] = QStringLiteral("edit");
    obj[QStringLiteral("new_body")] = newBody;
    obj[QStringLiteral("new_content_type")] = newContentType;
    obj[QStringLiteral("mutated_at_ms")] = mutatedAtMs;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();
}

inline std::optional<MessageMutation> parseMutationPayload(const MessageEnvelope& envelope)
{
    const QByteArray payloadBytes = QByteArray::fromStdString(envelope.payloadJson);
    const QJsonDocument doc = QJsonDocument::fromJson(payloadBytes);
    if (!doc.isObject()) {
        return std::nullopt;
    }

    const QJsonObject obj = doc.object();

    const QString targetMessageId = obj.value(QStringLiteral("target_message_id")).toString();
    if (targetMessageId.isEmpty()) {
        return std::nullopt;
    }

    const QString mutationKindStr = obj.value(QStringLiteral("mutation_kind")).toString();
    MessageMutationKind kind{};
    if (mutationKindStr == QLatin1String("recall")) {
        kind = MessageMutationKind::Recall;
    } else if (mutationKindStr == QLatin1String("edit")) {
        kind = MessageMutationKind::Edit;
    } else {
        return std::nullopt;
    }

    MessageMutation mutation;
    mutation.mutationMessageId = QString::fromStdString(envelope.messageId);
    mutation.targetMessageId = targetMessageId;
    mutation.conversationId = QString::fromStdString(envelope.conversationId);
    mutation.actorId = QString::fromStdString(envelope.senderId);
    mutation.kind = kind;
    mutation.newBody = obj.value(QStringLiteral("new_body")).toString();
    mutation.newContentType = obj.value(QStringLiteral("new_content_type")).toString();
    mutation.mutatedAtMs = obj.value(QStringLiteral("mutated_at_ms")).toInteger();

    if (kind == MessageMutationKind::Edit && mutation.newBody.isEmpty()) return std::nullopt;
    if (mutation.mutatedAtMs <= 0) return std::nullopt;

    return mutation;
}
