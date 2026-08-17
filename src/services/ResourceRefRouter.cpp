#include "services/ResourceRefRouter.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QString toQString(const std::string& value)
{
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

QJsonObject actionToJson(const ResourceRefAction& action)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), action.actionId);
    object.insert(QStringLiteral("label"), action.label);
    object.insert(QStringLiteral("target"), action.target);
    object.insert(QStringLiteral("primary"), action.primary);
    return object;
}

ResourceRefAction actionFromJson(const QJsonObject& object)
{
    ResourceRefAction action;
    action.actionId = object.value(QStringLiteral("id")).toString().trimmed();
    action.label = object.value(QStringLiteral("label")).toString().trimmed();
    action.target = object.value(QStringLiteral("target")).toString().trimmed();
    action.primary = object.value(QStringLiteral("primary")).toBool(false);
    return action;
}

ResourceRefPayload payloadFromObject(const QJsonObject& object)
{
    ResourceRefPayload payload;
    payload.serviceId = object.value(QStringLiteral("service_id")).toString().trimmed();
    payload.workspaceId = object.value(QStringLiteral("workspace_id")).toString().trimmed();
    payload.origin = object.value(QStringLiteral("origin")).toString().trimmed();
    payload.kind = object.value(QStringLiteral("kind")).toString().trimmed();
    payload.resourceId = object.value(QStringLiteral("resource_id")).toString().trimmed();
    payload.title = object.value(QStringLiteral("title")).toString().trimmed();
    payload.subtitle = object.value(QStringLiteral("subtitle")).toString().trimmed();
    payload.status = object.value(QStringLiteral("status")).toString().trimmed();
    payload.snapshotVersion = object.value(QStringLiteral("snapshot_version")).toString().trimmed();
    payload.updatedAtMs =
        static_cast<qint64>(object.value(QStringLiteral("updated_at_ms")).toDouble(0));

    const QJsonArray actionsArray = object.value(QStringLiteral("actions")).toArray();
    payload.actions.reserve(actionsArray.size());
    for (const QJsonValue& value : actionsArray) {
        if (!value.isObject()) {
            continue;
        }
        const ResourceRefAction action = actionFromJson(value.toObject());
        if (!action.actionId.isEmpty() || !action.label.isEmpty() || !action.target.isEmpty()) {
            payload.actions.push_back(action);
        }
    }

    return payload;
}

std::optional<ResourceRefPayload> parseLegacyEnvelopeBody(const MessageEnvelope& envelope)
{
    const QByteArray rawBody(envelope.body.data(), static_cast<int>(envelope.body.size()));
    const QJsonObject object = QJsonDocument::fromJson(rawBody).object();
    if (object.isEmpty() && toQString(envelope.resourceId).trimmed().isEmpty()
        && toQString(envelope.resourceKind).trimmed().isEmpty()) {
        return std::nullopt;
    }

    ResourceRefPayload payload;
    payload.serviceId = !envelope.serviceId.empty()
        ? toQString(envelope.serviceId).trimmed()
        : object.value(QStringLiteral("service_id")).toString().trimmed();
    payload.workspaceId = !envelope.workspaceId.empty()
        ? toQString(envelope.workspaceId).trimmed()
        : object.value(QStringLiteral("workspace_id")).toString().trimmed();
    payload.origin = object.value(QStringLiteral("origin")).toString().trimmed();
    payload.kind = !envelope.resourceKind.empty()
        ? toQString(envelope.resourceKind).trimmed()
        : object.value(QStringLiteral("resource_kind")).toString().trimmed();
    payload.resourceId = !envelope.resourceId.empty()
        ? toQString(envelope.resourceId).trimmed()
        : object.value(QStringLiteral("resource_id")).toString().trimmed();
    payload.title = object.value(QStringLiteral("title")).toString().trimmed();
    payload.subtitle = object.value(QStringLiteral("summary")).toString().trimmed();
    payload.status = object.value(QStringLiteral("status")).toString().trimmed();
    payload.snapshotVersion = object.value(QStringLiteral("version")).toString().trimmed();

    if (payload.title.isEmpty() && !envelope.resourceTitle.empty()) {
        payload.title = toQString(envelope.resourceTitle).trimmed();
    }
    if (payload.resourceId.isEmpty() || payload.kind.isEmpty()) {
        return std::nullopt;
    }
    if (payload.title.isEmpty()) {
        payload.title = payload.resourceId;
    }

    return payload;
}

QString previewPrefixForKind(const QString& kind)
{
    const QString normalizedKind = kind.trimmed().toLower();
    if (normalizedKind == QStringLiteral("shared_file")) {
        return QStringLiteral("[共享文件]");
    }
    if (normalizedKind == QStringLiteral("shared_doc")) {
        return QStringLiteral("[共享文档]");
    }
    if (normalizedKind == QStringLiteral("devops_work_item")) {
        return QStringLiteral("[DevOps 工作项]");
    }
    if (normalizedKind == QStringLiteral("devops_pull_request")) {
        return QStringLiteral("[DevOps 合并请求]");
    }
    if (normalizedKind == QStringLiteral("devops_build")) {
        return QStringLiteral("[DevOps 构建]");
    }
    if (normalizedKind == QStringLiteral("outlook_mail")) {
        return QStringLiteral("[Outlook 邮件]");
    }
    if (normalizedKind == QStringLiteral("outlook_event")) {
        return QStringLiteral("[Outlook 日程]");
    }
    if (normalizedKind == QStringLiteral("connector")) {
        return QStringLiteral("[连接器]");
    }
    if (normalizedKind == QStringLiteral("bot")) {
        return QStringLiteral("[机器人]");
    }
    return QStringLiteral("[共享资源]");
}

}  // namespace

bool ResourceRefRouter::isResourceReferenceEnvelope(const MessageEnvelope& envelope)
{
    return envelope.type == MessageType::ResourceReference
        || envelope.messageSubtype == "resource_ref"
        || envelope.contentType == "resource_reference";
}

QByteArray ResourceRefRouter::serializePayload(const ResourceRefPayload& payload)
{
    QJsonObject object;
    object.insert(QStringLiteral("service_id"), payload.serviceId);
    object.insert(QStringLiteral("workspace_id"), payload.workspaceId);
    object.insert(QStringLiteral("origin"), payload.origin);
    object.insert(QStringLiteral("kind"), payload.kind);
    object.insert(QStringLiteral("resource_id"), payload.resourceId);
    object.insert(QStringLiteral("title"), payload.title);
    object.insert(QStringLiteral("subtitle"), payload.subtitle);
    object.insert(QStringLiteral("status"), payload.status);
    object.insert(QStringLiteral("snapshot_version"), payload.snapshotVersion);
    object.insert(QStringLiteral("updated_at_ms"), payload.updatedAtMs);

    QJsonArray actionsArray;
    for (const ResourceRefAction& action : payload.actions) {
        actionsArray.push_back(actionToJson(action));
    }
    object.insert(QStringLiteral("actions"), actionsArray);

    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

std::optional<ResourceRefPayload> ResourceRefRouter::parsePayload(const QByteArray& payloadJson)
{
    if (payloadJson.trimmed().isEmpty()) {
        return std::nullopt;
    }

    const QJsonObject object = QJsonDocument::fromJson(payloadJson).object();
    if (object.isEmpty()) {
        return std::nullopt;
    }

    ResourceRefPayload payload = payloadFromObject(object);
    if (payload.resourceId.isEmpty() || payload.kind.isEmpty()) {
        return std::nullopt;
    }
    if (payload.title.isEmpty()) {
        payload.title = payload.resourceId;
    }
    return payload;
}

std::optional<ResourceRefPayload> ResourceRefRouter::parseEnvelope(const MessageEnvelope& envelope)
{
    if (!isResourceReferenceEnvelope(envelope)) {
        return std::nullopt;
    }

    if (!envelope.payloadJson.empty()) {
        const auto parsed = parsePayload(
            QByteArray(envelope.payloadJson.data(), static_cast<int>(envelope.payloadJson.size())));
        if (parsed.has_value()) {
            return parsed;
        }
    }

    return parseLegacyEnvelopeBody(envelope);
}

QString ResourceRefRouter::previewLabel(const MessageEnvelope& envelope)
{
    const auto payload = parseEnvelope(envelope);
    if (!payload.has_value()) {
        return QStringLiteral("[共享资源]");
    }

    const QString label = !payload->title.trimmed().isEmpty() ? payload->title.trimmed()
                                                              : payload->resourceId.trimmed();
    const QString prefix = previewPrefixForKind(payload->kind);
    return label.isEmpty() ? prefix : QStringLiteral("%1 %2").arg(prefix, label);
}
