#pragma once

#include <optional>
#include <vector>

#include "integrations/OutlookNotificationContracts.h"
#include "services/DirectResourceReferenceDraft.h"

class GroupService;

class OutlookNotificationDispatcher {
public:
    static ResourceRefPayload payloadForEvent(const OutlookNotificationEvent& event);

    static std::optional<DirectResourceReferenceDraft> buildDirectDraft(
        const QString& localClientId,
        const QString& targetClientId,
        const QString& conversationTitle,
        const OutlookNotificationEvent& event);

    static std::vector<MessageEnvelope> buildGroupFanOut(const QString& localClientId,
                                                         const QString& groupId,
                                                         const OutlookNotificationEvent& event,
                                                         const GroupService* groupService);
};
