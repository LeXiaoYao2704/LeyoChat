// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增20行/修改0行/删除0行; 总行数111行
// @AI-LastModified: 2026-04-16 14:21:39

#pragma once

#include <vector>

#include <QString>
#include <QStringList>

#include "architecture/ResourceReferenceMessage.h"
#include "domain/Group.h"
#include "domain/GroupEvent.h"
#include "domain/MessageEnvelope.h"
#include "domain/MessageMutation.h"
#include "domain/ResourceRefPayload.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "integrations/SharedFileResourceContracts.h"

class ConversationRepository;
class FileTransferService;
class GroupRepository;

class GroupService {
public:
    GroupService(GroupRepository* groupRepository, ConversationRepository* conversationRepository);

    bool createGroup(const QString& ownerClientId,
                     const QString& groupName,
                     const QStringList& memberClientIds,
                     Group* outGroup);
    bool addMembers(const QString& operatorClientId,
                    const QString& groupId,
                    const QStringList& memberClientIds,
                    GroupEvent* outEvent);
    bool leaveGroup(const QString& memberClientId,
                    const QString& groupId,
                    GroupEvent* outEvent);
    bool removeMember(const QString& operatorClientId,
                      const QString& groupId,
                      const QString& memberClientId,
                      GroupEvent* outEvent);
    bool disbandGroup(const QString& operatorClientId,
                      const QString& groupId,
                      GroupEvent* outEvent);
    bool setAdmin(const QString& operatorClientId,
                  const QString& groupId,
                  const QString& targetMemberId,
                  GroupEvent* outEvent);
    bool unsetAdmin(const QString& operatorClientId,
                    const QString& groupId,
                    const QString& targetMemberId,
                    GroupEvent* outEvent);
    QStringList activeMemberIds(const QString& groupId) const;
    std::vector<QString> activeRecipients(const QString& localClientId, const QString& groupId) const;
    bool shouldApplyIncomingMeta(const QString& groupId,
                                 int incomingVersion,
                                 qint64 incomingUpdatedAtMs) const;

    // fan-out
    bool isGroupConversation(const QString& conversationId) const;
    std::vector<MessageEnvelope> buildGroupTextFanOut(const QString& localClientId,
                                                      const QString& groupId,
                                                      const QString& text) const;
    std::vector<MessageEnvelope> buildGroupNudgeFanOut(const QString& localClientId,
                                                       const QString& groupId) const;
    std::vector<MessageEnvelope> buildGroupInlineAttachmentFanOut(const QString& localClientId,
                                                                  const QString& groupId,
                                                                  const QString& filePath) const;
    std::vector<MessageEnvelope> buildGroupResourceReferenceFanOut(
        const QString& localClientId,
        const QString& groupId,
        const ResourceReferenceMessagePayload& payload) const;
    std::vector<MessageEnvelope> buildGroupResourceReferenceFanOut(
        const QString& localClientId,
        const QString& groupId,
        const ResourceRefPayload& payload) const;
    std::vector<MessageEnvelope> buildGroupSharedFileReferenceFanOut(
        const QString& localClientId,
        const QString& groupId,
        const SharedFileResource& resource) const;
    // 构建文件服务配置同步消息的 fan-out envelopes（不存入 DB，接收方静默保存配置）
    std::vector<MessageEnvelope> buildGroupFileServiceConfigFanOut(
        const QString& localClientId,
        const QString& groupId,
        const GroupFileServiceConfig& config) const;
    std::vector<MessageEnvelope> buildGroupCreateMetaFanOut(const QString& localClientId,
                                                             const Group& group,
                                                             const QStringList& allMemberIds) const;
    std::vector<MessageEnvelope> buildGroupMetaFanOut(const QString& localClientId,
                                                      const Group& group,
                                                      const QStringList& snapshotMemberIds,
                                                      const QStringList& recipientIds,
                                                      const QString& eventType,
                                                      const QString& affectedMemberId = QString()) const;
    std::vector<MessageEnvelope> buildGroupMutationFanOut(
        const QString& localClientId,
        const QString& groupId,
        const MessageMutation& mutation) const;
    std::vector<MessageEnvelope> buildGroupPinFanOut(
        const QString& localClientId,
        const QString& groupId,
        const QString& messageId,
        const QString& pinnedBody,
        const QString& authorName,
        const QString& pinnerName,
        bool unpin) const;
    std::vector<QString> createOutgoingGroupFileTasks(const QString& localClientId,
                                                      const QString& groupId,
                                                      const QString& filePath,
                                                      FileTransferService* fileTransferService) const;
    // P2P Offer-only: 广播文件卡片元数据，不自动推送文件
    std::vector<MessageEnvelope> buildGroupFileOfferFanOut(
        const QString& localClientId,
        const QString& groupId,
        const QString& filePath,
        const QString& uploaderName) const;

private:
    GroupRepository* m_groupRepository = nullptr;
    ConversationRepository* m_conversationRepository = nullptr;
};
