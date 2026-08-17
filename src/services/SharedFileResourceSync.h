#pragma once

#include <optional>

#include <QJsonObject>
#include <QString>

#include "architecture/ResourceReference.h"
#include "integrations/RemoteFileServiceSettings.h"

class ConversationRepository;
class ServiceResourceRepository;

namespace SharedFileResourceSync {

std::optional<ResourceReference> decodeSharedFileResource(
    const QJsonObject& payloadObject,
    const QString& expectedServiceId,
    const GroupFileServiceConfig& config);

bool syncIncomingSharedFileResource(
    const QJsonObject& payloadObject,
    const QString& expectedServiceId,
    const GroupFileServiceConfig& config,
    const ServiceResourceRepository& resourceRepository);

int replaySharedFileResourcesForConversation(
    const QString& conversationId,
    const QString& expectedServiceId,
    const GroupFileServiceConfig& config,
    const ConversationRepository& conversationRepository,
    const ServiceResourceRepository& resourceRepository);

}  // namespace SharedFileResourceSync
