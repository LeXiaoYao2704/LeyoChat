#include "app/GroupFanOutDelivery.h"

#include <memory>

#include <QDebug>
#include <QPointer>
#include <QTimer>
#include <QtGlobal>

namespace {

GroupFanOutDeliveryResult deliverRange(
    const std::vector<GroupFanOutPayload>& payloads,
    size_t begin,
    size_t end,
    const GroupFanOutPayloadSender& sender,
    const GroupFanOutDeliveryOptions& options)
{
    GroupFanOutDeliveryResult result;
    for (size_t i = begin; i < end; ++i) {
        const GroupFanOutPayload& payload = payloads[i];
        ++result.attemptedCount;
        if (sender(payload)) {
            ++result.deliveredCount;
            if (options.onDelivered) {
                options.onDelivered(payload);
            }
        } else {
            ++result.failedCount;
            qInfo().noquote()
                << QStringLiteral("[%1] sendPayload failed for").arg(options.logPrefix)
                << payload.targetId.left(8)
                << ", keeping pending";
        }
    }
    return result;
}

GroupFanOutDeliveryResult deliverStatusRange(
    const std::vector<GroupFanOutPayload>& payloads,
    size_t begin,
    size_t end,
    const GroupFanOutPayloadStatusSender& sender,
    const GroupFanOutDeliveryOptions& options)
{
    GroupFanOutDeliveryResult result;
    for (size_t i = begin; i < end; ++i) {
        const GroupFanOutPayload& payload = payloads[i];
        ++result.attemptedCount;
        const GroupFanOutPayloadDisposition disposition = sender(payload);
        if (disposition == GroupFanOutPayloadDisposition::Written) {
            ++result.writtenCount;
            ++result.deliveredCount;
            if (options.onDelivered) {
                options.onDelivered(payload);
            }
        } else if (disposition == GroupFanOutPayloadDisposition::Queued) {
            ++result.queuedCount;
        } else {
            ++result.failedCount;
            qInfo().noquote()
                << QStringLiteral("[%1] sendPayload failed for").arg(options.logPrefix)
                << payload.targetId.left(8)
                << ", keeping pending";
        }
    }
    return result;
}

}  // namespace

GroupFanOutDeliveryResult deliverGroupFanOutPayloads(
    const std::vector<GroupFanOutPayload>& payloads,
    QObject* context,
    GroupFanOutPayloadSender sender,
    GroupFanOutDeliveryOptions options)
{
    if (payloads.empty() || !sender) {
        return {};
    }

    const int batchSize = qMax(1, options.batchSize);
    if (!context || payloads.size() <= static_cast<size_t>(batchSize)) {
        return deliverRange(payloads, 0, payloads.size(), sender, options);
    }

    auto sharedPayloads =
        std::make_shared<std::vector<GroupFanOutPayload>>(payloads);
    QPointer<QObject> contextGuard(context);
    const GroupFanOutDeliveryResult firstBatchResult =
        deliverRange(*sharedPayloads,
                     0,
                     qMin(static_cast<size_t>(batchSize), sharedPayloads->size()),
                     sender,
                     options);
    if (static_cast<size_t>(batchSize) >= sharedPayloads->size()) {
        return firstBatchResult;
    }

    auto sendBatch = std::make_shared<std::function<void(size_t)>>();
    *sendBatch =
        [sharedPayloads,
         contextGuard,
         sender = std::move(sender),
         options = std::move(options),
        batchSize,
        sendBatch](size_t offset) mutable {
            if (!contextGuard) {
                sendBatch.reset();
                return;
            }

            const size_t end =
                qMin(offset + static_cast<size_t>(batchSize),
                     sharedPayloads->size());
            deliverRange(*sharedPayloads, offset, end, sender, options);

            if (end < sharedPayloads->size()) {
                QTimer::singleShot(0, contextGuard, [sendBatch, end]() {
                    (*sendBatch)(end);
                });
            } else {
                sendBatch.reset();
            }
    };
    QTimer::singleShot(0, contextGuard, [sendBatch, batchSize]() {
        (*sendBatch)(static_cast<size_t>(batchSize));
    });
    return firstBatchResult;
}

GroupFanOutDeliveryResult deliverGroupFanOutPayloadsWithStatus(
    const std::vector<GroupFanOutPayload>& payloads,
    QObject* context,
    GroupFanOutPayloadStatusSender sender,
    GroupFanOutDeliveryOptions options)
{
    if (payloads.empty() || !sender) {
        return {};
    }

    const int batchSize = qMax(1, options.batchSize);
    if (!context || payloads.size() <= static_cast<size_t>(batchSize)) {
        return deliverStatusRange(payloads, 0, payloads.size(), sender, options);
    }

    auto sharedPayloads =
        std::make_shared<std::vector<GroupFanOutPayload>>(payloads);
    QPointer<QObject> contextGuard(context);
    const GroupFanOutDeliveryResult firstBatchResult =
        deliverStatusRange(*sharedPayloads,
                           0,
                           qMin(static_cast<size_t>(batchSize), sharedPayloads->size()),
                           sender,
                           options);
    if (static_cast<size_t>(batchSize) >= sharedPayloads->size()) {
        return firstBatchResult;
    }

    auto sendBatch = std::make_shared<std::function<void(size_t)>>();
    *sendBatch =
        [sharedPayloads,
         contextGuard,
         sender = std::move(sender),
         options = std::move(options),
         batchSize,
         sendBatch](size_t offset) mutable {
            if (!contextGuard) {
                sendBatch.reset();
                return;
            }

            const size_t end =
                qMin(offset + static_cast<size_t>(batchSize), sharedPayloads->size());
            deliverStatusRange(*sharedPayloads, offset, end, sender, options);
            if (end < sharedPayloads->size()) {
                QTimer::singleShot(0, contextGuard, [sendBatch, end]() {
                    (*sendBatch)(end);
                });
            } else {
                sendBatch.reset();
            }
        };
    QTimer::singleShot(0, contextGuard, [sendBatch, batchSize]() {
        (*sendBatch)(static_cast<size_t>(batchSize));
    });
    return firstBatchResult;
}
