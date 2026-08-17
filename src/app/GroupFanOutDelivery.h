#pragma once

#include <functional>
#include <vector>

#include <QObject>
#include <QString>

#include "app/GroupFanOutPayloadBuilder.h"

using GroupFanOutDeliveredCallback =
    std::function<void(const GroupFanOutPayload& payload)>;

enum class GroupFanOutPayloadDisposition {
    Written,
    Queued,
    Failed
};

struct GroupFanOutDeliveryOptions {
    int batchSize = 20;
    QString logPrefix = QStringLiteral("group-send");
    GroupFanOutDeliveredCallback onDelivered;
};

struct GroupFanOutDeliveryResult {
    int attemptedCount = 0;
    int writtenCount = 0;
    int queuedCount = 0;
    // Kept as a source-compatible alias for callers that predate the explicit
    // written/queued split. New code should use writtenCount.
    int deliveredCount = 0;
    int failedCount = 0;

    bool anyWritten() const
    {
        return writtenCount > 0 || (writtenCount == 0 && deliveredCount > 0);
    }
    bool allWritten() const
    {
        const int effectiveWritten = writtenCount > 0 ? writtenCount : deliveredCount;
        return attemptedCount > 0 && failedCount == 0
            && effectiveWritten == attemptedCount;
    }
    bool accepted() const { return attemptedCount > 0 && failedCount == 0; }
    bool anyDelivered() const { return anyWritten(); }
    bool allDelivered() const { return allWritten(); }
};

inline bool isGroupFanOutDeliveryAccepted(bool acceptQueuedOnlyDelivery,
                                          const GroupFanOutDeliveryResult& result)
{
    if (result.allDelivered()) {
        return true;
    }
    return acceptQueuedOnlyDelivery && result.accepted();
}

using GroupFanOutPayloadSender =
    std::function<bool(const GroupFanOutPayload& payload)>;
using GroupFanOutPayloadStatusSender =
    std::function<GroupFanOutPayloadDisposition(const GroupFanOutPayload& payload)>;

GroupFanOutDeliveryResult deliverGroupFanOutPayloads(
    const std::vector<GroupFanOutPayload>& payloads,
    QObject* context,
    GroupFanOutPayloadSender sender,
    GroupFanOutDeliveryOptions options = {});

GroupFanOutDeliveryResult deliverGroupFanOutPayloadsWithStatus(
    const std::vector<GroupFanOutPayload>& payloads,
    QObject* context,
    GroupFanOutPayloadStatusSender sender,
    GroupFanOutDeliveryOptions options = {});
