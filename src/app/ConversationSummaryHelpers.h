#pragma once

#include <vector>

#include <QSet>
#include <QString>
#include <QVector>

#include "domain/ConversationSummary.h"
#include "services/PeerDirectoryService.h"
#include "storage/ConversationRepository.h"

bool tryLoadConversationSummary(const ConversationRepository& repository,
                                const QString& conversationId,
                                ConversationSummary* outSummary);
void upsertConversationSummaryPreservingLatest(ConversationRepository& repository,
                                               const ConversationSummary& incoming);
ConversationSummary decorateConversationSummary(const ConversationSummary& summary,
                                               const QString& localClientId,
                                               const PeerDirectoryService& peerDirectoryService);
QVector<ConversationSummary> decorateConversationVector(const std::vector<ConversationSummary>& summaries,
                                                        const QString& localClientId,
                                                        const PeerDirectoryService& peerDirectoryService);
QSet<QString> collectUnreadConversationIds(const std::vector<ConversationSummary>& summaries,
                                           const QString& localClientId,
                                           ConversationRepository& conversationRepository);
