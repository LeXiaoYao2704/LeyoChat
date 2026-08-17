#include "app/ConversationSummaryHelpers.h"

#include "app/PeerPresentationHelpers.h"
#include "services/ChatService.h"
#include "services/DirectConversationAddressing.h"

bool tryLoadConversationSummary(const ConversationRepository& repository,
                                const QString& conversationId,
                                ConversationSummary* outSummary)
{
    if (!outSummary || conversationId.trimmed().isEmpty()) {
        return false;
    }

    const std::wstring wantedId = conversationId.trimmed().toStdWString();
    const auto summaries = repository.loadConversationSummaries();
    for (const auto& summary : summaries) {
        if (summary.conversationId == wantedId) {
            *outSummary = summary;
            return true;
        }
    }

    return false;
}

void upsertConversationSummaryPreservingLatest(ConversationRepository& repository,
                                               const ConversationSummary& incoming)
{
    ConversationSummary existing;
    if (!tryLoadConversationSummary(repository,
                                    QString::fromStdWString(incoming.conversationId),
                                    &existing)) {
        repository.upsertConversation(incoming);
        return;
    }

    ConversationSummary merged = existing;
    if (!incoming.title.empty()) {
        merged.title = incoming.title;
    }

    const bool incomingHasPreview =
        !QString::fromStdWString(incoming.lastMessagePreview).trimmed().isEmpty();
    if (incomingHasPreview
        && (existing.lastMessagePreview.empty()
            || incoming.lastMessageAtMs >= existing.lastMessageAtMs)) {
        merged.lastMessagePreview = incoming.lastMessagePreview;
        merged.lastMessageAtMs = incoming.lastMessageAtMs;
    }

    repository.upsertConversation(merged);
}

ConversationSummary decorateConversationSummary(const ConversationSummary& summary,
                                               const QString& localClientId,
                                               const PeerDirectoryService& peerDirectoryService) {
    ConversationSummary decorated = summary;
    const QString conversationId = QString::fromStdWString(summary.conversationId);
    const QString otherParticipant =
        DirectConversationAddressing::otherParticipant(localClientId, conversationId);
    if (otherParticipant.isEmpty()) {
        return decorated;
    }
    const auto peer = peerDirectoryService.findPeerByClientId(toUtf8(otherParticipant));
    if (!peer.has_value()) {
        return decorated;
    }

    const QString name = displayNameForPeer(*peer);
    if (name.isEmpty()) {
        return decorated;
    }
    decorated.title = name.toStdWString();
    return decorated;
}

QVector<ConversationSummary> decorateConversationVector(const std::vector<ConversationSummary>& summaries,
                                                        const QString& localClientId,
                                                        const PeerDirectoryService& peerDirectoryService) {
    QVector<ConversationSummary> items;
    items.reserve(static_cast<qsizetype>(summaries.size()));
    for (const auto& summary : summaries) {
        items.push_back(decorateConversationSummary(summary, localClientId, peerDirectoryService));
    }
    return items;
}

QSet<QString> collectUnreadConversationIds(const std::vector<ConversationSummary>& summaries,
                                           const QString& localClientId,
                                           ConversationRepository& conversationRepository) {
    QSet<QString> unreadConversationIds;
    for (const auto& summary : summaries) {
        const QString conversationId = QString::fromStdWString(summary.conversationId);
        // 鎵嬪姩鏍囪鏈浼樺厛
        if (summary.isManuallyUnread) {
            unreadConversationIds.insert(conversationId);
            continue;
        }
        const auto messages = ChatService::loadMessages(&conversationRepository, conversationId);
        for (const auto& message : messages) {
            if (QString::fromStdWString(message.senderId) != localClientId
                && message.deliveryState == MessageDeliveryState::Received) {
                unreadConversationIds.insert(conversationId);
                break;
            }
        }
    }
    return unreadConversationIds;
}
