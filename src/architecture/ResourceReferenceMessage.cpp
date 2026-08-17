#include "architecture/ResourceReferenceMessage.h"

#include <QJsonDocument>
#include <QJsonObject>

#include "services/ResourceRefRouter.h"

namespace {
QString resourceOriginToString(ResourceOrigin origin)
{
    return origin == ResourceOrigin::Service
        ? QStringLiteral("service")
        : QStringLiteral("local");
}

QString fallbackBodySummary(const MessageEnvelope& envelope)
{
    return QString::fromUtf8(envelope.body.data(), static_cast<int>(envelope.body.size())).trimmed();
}

ResourceRefPayload toRouterPayload(const ResourceReferenceMessagePayload& payload)
{
    ResourceRefPayload routed;
    routed.serviceId = payload.resource.serviceId.trimmed();
    routed.workspaceId = payload.resource.workspaceId.trimmed();
    routed.origin = resourceOriginToString(payload.resource.origin);
    routed.kind = payload.resource.resourceKind.trimmed();
    routed.resourceId = payload.resource.resourceId.trimmed();
    routed.title = payload.resource.title.trimmed();
    routed.subtitle = payload.summary.trimmed();
    routed.snapshotVersion = payload.resource.version.trimmed();
    return routed;
}

std::optional<ResourceReferenceMessagePayload> toLegacyPayload(const std::optional<ResourceRefPayload>& payload)
{
    if (!payload.has_value()) {
        return std::nullopt;
    }

    ResourceReferenceMessagePayload legacy;
    legacy.resource.serviceId = payload->serviceId.trimmed();
    legacy.resource.workspaceId = payload->workspaceId.trimmed();
    legacy.resource.origin =
        payload->origin.compare(QStringLiteral("local"), Qt::CaseInsensitive) == 0
            ? ResourceOrigin::Local
            : ResourceOrigin::Service;
    legacy.resource.resourceKind = payload->kind.trimmed();
    legacy.resource.resourceId = payload->resourceId.trimmed();
    legacy.resource.title = payload->title.trimmed();
    legacy.resource.version = payload->snapshotVersion.trimmed();
    legacy.resource.summary = payload->subtitle.trimmed();
    legacy.resource.origin = ResourceOrigin::Service;
    legacy.summary = payload->subtitle.trimmed();
    return legacy;
}
}

MessageEnvelope buildResourceReferenceEnvelope(const QString& messageId,
                                               const QString& senderId,
                                               const QString& targetId,
                                               const QString& conversationId,
                                               const ResourceRefPayload& payload,
                                               qint64 createdAtMs)
{
    QJsonObject bodyObject;
    bodyObject.insert(QStringLiteral("message_kind"), QStringLiteral("resource_reference"));
    bodyObject.insert(QStringLiteral("resource_id"), payload.resourceId);
    bodyObject.insert(QStringLiteral("resource_kind"), payload.kind);
    bodyObject.insert(QStringLiteral("title"), payload.title);
    bodyObject.insert(QStringLiteral("summary"), payload.subtitle);
    bodyObject.insert(QStringLiteral("version"), payload.snapshotVersion);
    bodyObject.insert(QStringLiteral("origin"), payload.origin);
    bodyObject.insert(QStringLiteral("service_id"), payload.serviceId);
    bodyObject.insert(QStringLiteral("workspace_id"), payload.workspaceId);
    if (!payload.status.trimmed().isEmpty()) {
        bodyObject.insert(QStringLiteral("status"), payload.status);
    }

    MessageEnvelope envelope;
    envelope.messageId = messageId.toStdString();
    envelope.type = MessageType::ResourceReference;
    envelope.senderId = senderId.toStdString();
    envelope.targetId = targetId.toStdString();
    envelope.conversationId = conversationId.toStdString();
    envelope.body = QJsonDocument(bodyObject).toJson(QJsonDocument::Compact).toStdString();
    envelope.contentType = "resource_reference";
    envelope.messageSubtype = "resource_ref";
    envelope.payloadJson = ResourceRefRouter::serializePayload(payload).toStdString();
    envelope.resourceId = payload.resourceId.toStdString();
    envelope.resourceKind = payload.kind.toStdString();
    envelope.resourceTitle = payload.title.toStdString();
    envelope.workspaceId = payload.workspaceId.toStdString();
    envelope.serviceId = payload.serviceId.toStdString();
    envelope.createdAtMs = createdAtMs;
    return envelope;
}

MessageEnvelope buildResourceReferenceEnvelope(const QString& messageId,
                                               const QString& senderId,
                                               const QString& targetId,
                                               const QString& conversationId,
                                               const ResourceReferenceMessagePayload& payload,
                                               qint64 createdAtMs)
{
    return buildResourceReferenceEnvelope(messageId,
                                          senderId,
                                          targetId,
                                          conversationId,
                                          toRouterPayload(payload),
                                          createdAtMs);
}

std::optional<ResourceReferenceMessagePayload> parseResourceReferenceEnvelope(
    const MessageEnvelope& envelope)
{
    if (envelope.type != MessageType::ResourceReference) {
        return std::nullopt;
    }
    auto payload = toLegacyPayload(ResourceRefRouter::parseEnvelope(envelope));
    if (!payload.has_value()) {
        return std::nullopt;
    }
    if (payload->summary.isEmpty()) {
        payload->summary = fallbackBodySummary(envelope);
    }
    return payload;
}
