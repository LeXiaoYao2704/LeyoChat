#include "services/SharedFileResourceSync.h"

#include <QJsonDocument>

#include "storage/ConversationRepository.h"
#include "storage/ServiceResourceRepository.h"

namespace {
QString normalizedKind(const QJsonObject& payloadObject)
{
    const QString kind = payloadObject.value(QStringLiteral("kind")).toString();
    return kind.isEmpty()
               ? payloadObject.value(QStringLiteral("resource_kind")).toString()
               : kind;
}

QString normalizedVersion(const QJsonObject& payloadObject)
{
    const QString version = payloadObject.value(QStringLiteral("version")).toString();
    return version.isEmpty()
               ? payloadObject.value(QStringLiteral("snapshot_version")).toString()
               : version;
}

bool isSharedFileKind(const QString& kind)
{
    return kind == QStringLiteral("shared_file")
        || kind == QStringLiteral("group_file");
}
}

namespace SharedFileResourceSync {

std::optional<ResourceReference> decodeSharedFileResource(
    const QJsonObject& payloadObject,
    const QString& expectedServiceId,
    const GroupFileServiceConfig& config)
{
    const QString serviceId = payloadObject.value(QStringLiteral("service_id")).toString();
    const QString workspaceId = payloadObject.value(QStringLiteral("workspace_id")).toString();
    const QString resourceId = payloadObject.value(QStringLiteral("resource_id")).toString();
    const QString kind = normalizedKind(payloadObject);

    if (!config.enabled
        || expectedServiceId.isEmpty()
        || serviceId != expectedServiceId
        || !isSharedFileKind(kind)
        || workspaceId.isEmpty()
        || config.workspaceId.isEmpty()
        || workspaceId != config.workspaceId
        || resourceId.isEmpty()) {
        return std::nullopt;
    }

    return ResourceReference{
        serviceId,
        workspaceId,
        resourceId,
        kind,
        payloadObject.value(QStringLiteral("title")).toString(),
        normalizedVersion(payloadObject),
        payloadObject.value(QStringLiteral("summary")).toString(),
        ResourceOrigin::Service};
}

bool syncIncomingSharedFileResource(
    const QJsonObject& payloadObject,
    const QString& expectedServiceId,
    const GroupFileServiceConfig& config,
    const ServiceResourceRepository& resourceRepository)
{
    const auto reference =
        decodeSharedFileResource(payloadObject, expectedServiceId, config);
    if (!reference.has_value()) {
        return false;
    }
    return resourceRepository.upsertResource(*reference);
}

int replaySharedFileResourcesForConversation(
    const QString& conversationId,
    const QString& expectedServiceId,
    const GroupFileServiceConfig& config,
    const ConversationRepository& conversationRepository,
    const ServiceResourceRepository& resourceRepository)
{
    int upsertCount = 0;
    const auto messages =
        conversationRepository.loadResourceRefMessages(conversationId.toStdWString());
    for (const auto& message : messages) {
        const QJsonObject payloadObject =
            QJsonDocument::fromJson(QString::fromStdWString(message.payloadJson).toUtf8())
                .object();
        if (syncIncomingSharedFileResource(payloadObject,
                                           expectedServiceId,
                                           config,
                                           resourceRepository)) {
            ++upsertCount;
        }
    }
    return upsertCount;
}

}  // namespace SharedFileResourceSync
