#pragma once

#include <optional>
#include <vector>

#include <QString>

#include "architecture/ResourceReferenceMessage.h"
#include "integrations/AzureDevOpsNotificationContracts.h"
#include "services/DirectResourceReferenceDraft.h"

class GroupService;

class AzureDevOpsNotificationDispatcher {
public:
    static ResourceRefPayload payloadForEvent(const AzureDevOpsNotificationEvent& event);

    static std::optional<DirectResourceReferenceDraft> buildDirectDraft(const QString& localClientId,
                                                                        const QString& targetClientId,
                                                                        const QString& conversationTitle,
                                                                        const AzureDevOpsNotificationEvent& event);

    static std::vector<MessageEnvelope> buildGroupFanOut(const QString& localClientId,
                                                         const QString& groupId,
                                                         const AzureDevOpsNotificationEvent& event,
                                                         const GroupService* groupService);
};
