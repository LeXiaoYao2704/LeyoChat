#include "services/GroupService.h"

#include "domain/ConversationSummary.h"
#include "domain/GroupMember.h"
#include "integrations/SharedFileResourceContracts.h"
#include "services/FileTransferService.h"
#include "storage/ConversationRepository.h"
#include "storage/GroupRepository.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

namespace {
constexpr qint64 kMaxInlineGroupAttachmentBytes = 2 * 1024 * 1024;

QString trimmedOrEmpty(const QString& value) {
    return value.trimmed();
}

std::wstring toWide(const QString& value) {
    return value.toStdWString();
}

QHash<QString, QString> knownPeerDisplayNames(ConversationRepository* conversationRepository) {
    QHash<QString, QString> names;
    if (!conversationRepository) {
        return names;
    }

    for (const PeerEndpoint& peer : conversationRepository->loadKnownPeers()) {
        const QString clientId = QString::fromStdString(peer.clientId).trimmed();
        const QString displayName = QString::fromStdString(peer.displayName).trimmed();
        if (!clientId.isEmpty() && !displayName.isEmpty()) {
            names.insert(clientId, displayName);
        }
    }
    return names;
}

QString resolveMemberDisplayName(const QString& memberId,
                                 const QHash<QString, QString>& knownPeerNames,
                                 const std::vector<ConversationSummary>& summaries) {
    const auto knownPeerIt = knownPeerNames.constFind(memberId);
    if (knownPeerIt != knownPeerNames.constEnd() && !knownPeerIt.value().trimmed().isEmpty()) {
        return knownPeerIt.value().trimmed();
    }

    for (const auto& summary : summaries) {
        const QString title = QString::fromStdWString(summary.title).trimmed();
        if (title.isEmpty()) {
            continue;
        }

        const QString conversationId = QString::fromStdWString(summary.conversationId);
        const QStringList participants = conversationId.split('|', Qt::KeepEmptyParts);
        if (participants.size() == 2 && participants.contains(memberId)) {
            return title;
        }
    }

    return {};
}

QStringList normalizedUniqueMemberIds(const QStringList& rawMemberIds) {
    QStringList memberIds = rawMemberIds;
    for (QString& memberId : memberIds) {
        memberId = memberId.trimmed();
    }
    memberIds.removeAll(QString());
    memberIds.removeDuplicates();
    return memberIds;
}

bool isActiveMember(const std::vector<GroupMember>& members, const QString& memberClientId) {
    for (const auto& member : members) {
        if (member.isActive && QString::fromStdWString(member.memberClientId) == memberClientId) {
            return true;
        }
    }

    return false;
}

bool persistGroupMutation(GroupRepository* groupRepository,
                          const Group& group,
                          const std::vector<GroupMember>& members,
                          const GroupEvent& event) {
    return groupRepository && groupRepository->upsertGroup(group) && groupRepository->replaceMembers(group.groupId, members)
           && groupRepository->appendEvent(event);
}

bool isInlineImageCandidate(const QString& filePath) {
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile() || info.size() <= 0 || info.size() > kMaxInlineGroupAttachmentBytes) {
        return false;
    }

    const QString suffix = info.suffix().trimmed().toLower();
    return suffix == QStringLiteral("png")
        || suffix == QStringLiteral("jpg")
        || suffix == QStringLiteral("jpeg")
        || suffix == QStringLiteral("bmp")
        || suffix == QStringLiteral("gif")
        || suffix == QStringLiteral("webp");
}
}

GroupService::GroupService(GroupRepository* groupRepository, ConversationRepository* conversationRepository)
    : m_groupRepository(groupRepository),
      m_conversationRepository(conversationRepository) {}

bool GroupService::createGroup(const QString& ownerClientId,
                               const QString& groupName,
                               const QStringList& memberClientIds,
                               Group* outGroup) {
    const QString normalizedOwnerId = trimmedOrEmpty(ownerClientId);
    const QString normalizedGroupName = trimmedOrEmpty(groupName);
    if (!m_groupRepository || !m_conversationRepository || normalizedOwnerId.isEmpty()
        || normalizedGroupName.isEmpty() || !outGroup) {
        return false;
    }

    const QString groupId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Group group{
        groupId.toStdWString(),
        normalizedGroupName.toStdWString(),
        normalizedOwnerId.toStdWString(),
        std::wstring(),
        1,
        nowMs,
        nowMs,
        true
    };

    QStringList allMembers = memberClientIds;
    if (!allMembers.contains(normalizedOwnerId)) {
        allMembers.push_front(normalizedOwnerId);
    }

    for (QString& memberId : allMembers) {
        memberId = memberId.trimmed();
    }

    allMembers.removeAll(QString());
    allMembers.removeDuplicates();

    const QHash<QString, QString> knownPeerNames = knownPeerDisplayNames(m_conversationRepository);
    const auto conversationSummaries = m_conversationRepository->loadConversationSummaries();

    std::vector<GroupMember> members;
    members.reserve(static_cast<std::size_t>(allMembers.size()));
    for (const QString& memberId : allMembers) {
        members.push_back(GroupMember{
            group.groupId,
            memberId.toStdWString(),
            resolveMemberDisplayName(memberId, knownPeerNames, conversationSummaries).toStdWString(),
            nowMs,
            true,
            memberId == normalizedOwnerId ? std::wstring(L"owner") : std::wstring(L"member")
        });
    }

    if (!m_groupRepository->upsertGroup(group) || !m_groupRepository->replaceMembers(group.groupId, members)) {
        return false;
    }

    if (!m_conversationRepository->upsertConversationWithType(ConversationSummary{
            group.groupId,
            group.groupName,
            std::wstring(),
            nowMs
        },
        QStringLiteral("group"))) {
        return false;
    }

    *outGroup = group;
    return true;
}

bool GroupService::addMembers(const QString& operatorClientId,
                              const QString& groupId,
                              const QStringList& memberClientIds,
                              GroupEvent* outEvent) {
    if (!m_groupRepository || trimmedOrEmpty(operatorClientId).isEmpty() || trimmedOrEmpty(groupId).isEmpty()
        || memberClientIds.isEmpty() || !outEvent) {
        return false;
    }

    const auto storedGroup = m_groupRepository->findGroupById(toWide(groupId));
    if (!storedGroup.has_value() || !storedGroup->isActive) {
        return false;
    }

    std::vector<GroupMember> members = m_groupRepository->loadMembers(storedGroup->groupId);
    if (!isActiveMember(members, trimmedOrEmpty(operatorClientId))) {
        return false;
    }

    bool changed = false;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QStringList normalizedMemberIds = memberClientIds;
    for (QString& memberId : normalizedMemberIds) {
        memberId = memberId.trimmed();
    }
    normalizedMemberIds.removeAll(QString());
    normalizedMemberIds.removeDuplicates();

    const QHash<QString, QString> knownPeerNames = knownPeerDisplayNames(m_conversationRepository);
    const auto conversationSummaries = m_conversationRepository->loadConversationSummaries();

    for (const QString& memberId : normalizedMemberIds) {
        bool found = false;
        for (auto& member : members) {
            if (QString::fromStdWString(member.memberClientId) != memberId) {
                continue;
            }

            found = true;
            if (!member.isActive) {
                member.isActive = true;
                member.joinedAtMs = nowMs;
                changed = true;
            }
            break;
        }

        if (!found) {
            members.push_back(GroupMember{
                storedGroup->groupId,
                memberId.toStdWString(),
                resolveMemberDisplayName(memberId, knownPeerNames, conversationSummaries).toStdWString(),
                nowMs,
                true,
                std::wstring(L"member")
            });
            changed = true;
        }
    }

    if (!changed) {
        return false;
    }

    Group updatedGroup = *storedGroup;
    updatedGroup.version += 1;
    updatedGroup.updatedAtMs = nowMs;

    GroupEvent event{
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString(),
        updatedGroup.groupId,
        std::wstring(L"add_members"),
        trimmedOrEmpty(operatorClientId).toStdWString(),
        updatedGroup.version,
        normalizedMemberIds.join(QStringLiteral(",")).toStdWString(),
        nowMs
    };

    if (!persistGroupMutation(m_groupRepository, updatedGroup, members, event)) {
        return false;
    }

    *outEvent = event;
    return true;
}

bool GroupService::leaveGroup(const QString& memberClientId,
                              const QString& groupId,
                              GroupEvent* outEvent) {
    if (!m_groupRepository || trimmedOrEmpty(memberClientId).isEmpty()
        || trimmedOrEmpty(groupId).isEmpty() || !outEvent) {
        return false;
    }

    const auto storedGroup = m_groupRepository->findGroupById(toWide(groupId));
    if (!storedGroup.has_value() || !storedGroup->isActive) {
        return false;
    }

    const QString normalizedMemberId = trimmedOrEmpty(memberClientId);
    if (QString::fromStdWString(storedGroup->ownerClientId) == normalizedMemberId) {
        return false;
    }

    std::vector<GroupMember> members = m_groupRepository->loadMembers(storedGroup->groupId);
    if (!isActiveMember(members, normalizedMemberId)) {
        return false;
    }

    bool changed = false;
    for (auto& member : members) {
        if (QString::fromStdWString(member.memberClientId) == normalizedMemberId && member.isActive) {
            member.isActive = false;
            changed = true;
            break;
        }
    }

    if (!changed) {
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Group updatedGroup = *storedGroup;
    updatedGroup.version += 1;
    updatedGroup.updatedAtMs = nowMs;

    GroupEvent event{
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString(),
        updatedGroup.groupId,
        std::wstring(L"remove_member"),
        normalizedMemberId.toStdWString(),
        updatedGroup.version,
        normalizedMemberId.toStdWString(),
        nowMs
    };

    if (!persistGroupMutation(m_groupRepository, updatedGroup, members, event)) {
        return false;
    }

    *outEvent = event;
    return true;
}

bool GroupService::removeMember(const QString& operatorClientId,
                                const QString& groupId,
                                const QString& memberClientId,
                                GroupEvent* outEvent) {
    if (!m_groupRepository || trimmedOrEmpty(operatorClientId).isEmpty() || trimmedOrEmpty(groupId).isEmpty()
        || trimmedOrEmpty(memberClientId).isEmpty() || !outEvent) {
        return false;
    }

    const auto storedGroup = m_groupRepository->findGroupById(toWide(groupId));
    if (!storedGroup.has_value() || !storedGroup->isActive) {
        return false;
    }

    const QString normalizedOperatorId = trimmedOrEmpty(operatorClientId);
    const QString normalizedMemberId = trimmedOrEmpty(memberClientId);
    if (normalizedMemberId == normalizedOperatorId) {
        return false;
    }

    const QString ownerQStr = QString::fromStdWString(storedGroup->ownerClientId);
    std::vector<GroupMember> members = m_groupRepository->loadMembers(storedGroup->groupId);
    if (!isActiveMember(members, normalizedOperatorId)) {
        return false;
    }

    // 权限检查：群主可以移除任何人；管理员只能移除普通成员
    const bool operatorIsOwner = (ownerQStr == normalizedOperatorId);
    bool operatorIsAdmin = false;
    bool targetIsAdminOrOwner = false;
    for (const auto& m : members) {
        const QString mid = QString::fromStdWString(m.memberClientId);
        if (mid == normalizedOperatorId && m.role == L"admin") {
            operatorIsAdmin = true;
        }
        if (mid == normalizedMemberId && (m.role == L"admin" || m.role == L"owner")) {
            targetIsAdminOrOwner = true;
        }
    }
    if (!operatorIsOwner && !operatorIsAdmin) {
        return false; // 普通成员不能移除别人
    }
    if (operatorIsAdmin && targetIsAdminOrOwner) {
        return false; // 管理员不能移除群主或其他管理员
    }

    bool changed = false;
    for (auto& member : members) {
        if (QString::fromStdWString(member.memberClientId) == normalizedMemberId && member.isActive) {
            member.isActive = false;
            changed = true;
            break;
        }
    }

    if (!changed) {
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Group updatedGroup = *storedGroup;
    updatedGroup.version += 1;
    updatedGroup.updatedAtMs = nowMs;

    GroupEvent event{
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString(),
        updatedGroup.groupId,
        std::wstring(L"remove_member"),
        normalizedOperatorId.toStdWString(),
        updatedGroup.version,
        normalizedMemberId.toStdWString(),
        nowMs
    };

    if (!persistGroupMutation(m_groupRepository, updatedGroup, members, event)) {
        return false;
    }

    *outEvent = event;
    return true;
}

bool GroupService::disbandGroup(const QString& operatorClientId,
                                const QString& groupId,
                                GroupEvent* outEvent) {
    if (!m_groupRepository || trimmedOrEmpty(operatorClientId).isEmpty()
        || trimmedOrEmpty(groupId).isEmpty() || !outEvent) {
        return false;
    }

    const auto storedGroup = m_groupRepository->findGroupById(toWide(groupId));
    if (!storedGroup.has_value() || !storedGroup->isActive) {
        return false;
    }

    const QString normalizedOperatorId = trimmedOrEmpty(operatorClientId);
    if (QString::fromStdWString(storedGroup->ownerClientId) != normalizedOperatorId) {
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Group updatedGroup = *storedGroup;
    updatedGroup.isActive = false;
    updatedGroup.version += 1;
    updatedGroup.updatedAtMs = nowMs;

    const auto existingMembers = m_groupRepository->loadMembers(updatedGroup.groupId);
    GroupEvent event{
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString(),
        updatedGroup.groupId,
        std::wstring(L"disband_group"),
        normalizedOperatorId.toStdWString(),
        updatedGroup.version,
        QStringLiteral("disbanded").toStdWString(),
        nowMs
    };

    if (!persistGroupMutation(m_groupRepository, updatedGroup, existingMembers, event)) {
        return false;
    }

    *outEvent = event;
    return true;
}

bool GroupService::setAdmin(const QString& operatorClientId,
                            const QString& groupId,
                            const QString& targetMemberId,
                            GroupEvent* outEvent) {
    if (!m_groupRepository || trimmedOrEmpty(operatorClientId).isEmpty()
        || trimmedOrEmpty(groupId).isEmpty() || trimmedOrEmpty(targetMemberId).isEmpty()
        || !outEvent) {
        return false;
    }

    const auto storedGroup = m_groupRepository->findGroupById(toWide(groupId));
    if (!storedGroup.has_value() || !storedGroup->isActive) {
        return false;
    }

    const QString normalizedOperatorId = trimmedOrEmpty(operatorClientId);
    const QString normalizedTargetId = trimmedOrEmpty(targetMemberId);

    // 仅群主可以设置管理员
    if (QString::fromStdWString(storedGroup->ownerClientId) != normalizedOperatorId) {
        return false;
    }

    // 不能设自己或已是owner
    if (normalizedTargetId == normalizedOperatorId) {
        return false;
    }

    std::vector<GroupMember> members = m_groupRepository->loadMembers(storedGroup->groupId);
    if (!isActiveMember(members, normalizedTargetId)) {
        return false;
    }

    // 检查目标是否已是管理员
    for (const auto& m : members) {
        if (QString::fromStdWString(m.memberClientId) == normalizedTargetId) {
            if (m.role == L"admin" || m.role == L"owner") {
                return false; // 已是管理员或群主
            }
            break;
        }
    }

    // 检查管理员数量上限（最多3人）
    const int adminCount = m_groupRepository->countMembersByRole(storedGroup->groupId, L"admin");
    if (adminCount >= 3) {
        return false;
    }

    // 更新 role
    for (auto& m : members) {
        if (QString::fromStdWString(m.memberClientId) == normalizedTargetId) {
            m.role = L"admin";
            break;
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Group updatedGroup = *storedGroup;
    updatedGroup.version += 1;
    updatedGroup.updatedAtMs = nowMs;

    GroupEvent event{
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString(),
        updatedGroup.groupId,
        std::wstring(L"set_admin"),
        normalizedOperatorId.toStdWString(),
        updatedGroup.version,
        normalizedTargetId.toStdWString(),
        nowMs
    };

    if (!persistGroupMutation(m_groupRepository, updatedGroup, members, event)) {
        return false;
    }

    *outEvent = event;
    return true;
}

bool GroupService::unsetAdmin(const QString& operatorClientId,
                              const QString& groupId,
                              const QString& targetMemberId,
                              GroupEvent* outEvent) {
    if (!m_groupRepository || trimmedOrEmpty(operatorClientId).isEmpty()
        || trimmedOrEmpty(groupId).isEmpty() || trimmedOrEmpty(targetMemberId).isEmpty()
        || !outEvent) {
        return false;
    }

    const auto storedGroup = m_groupRepository->findGroupById(toWide(groupId));
    if (!storedGroup.has_value() || !storedGroup->isActive) {
        return false;
    }

    const QString normalizedOperatorId = trimmedOrEmpty(operatorClientId);
    const QString normalizedTargetId = trimmedOrEmpty(targetMemberId);

    // 仅群主可以取消管理员
    if (QString::fromStdWString(storedGroup->ownerClientId) != normalizedOperatorId) {
        return false;
    }

    std::vector<GroupMember> members = m_groupRepository->loadMembers(storedGroup->groupId);

    // 检查目标是否确实是管理员
    bool targetIsAdmin = false;
    for (auto& m : members) {
        if (QString::fromStdWString(m.memberClientId) == normalizedTargetId) {
            if (m.role == L"admin") {
                targetIsAdmin = true;
                m.role = L"member";
            }
            break;
        }
    }
    if (!targetIsAdmin) {
        return false;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    Group updatedGroup = *storedGroup;
    updatedGroup.version += 1;
    updatedGroup.updatedAtMs = nowMs;

    GroupEvent event{
        QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString(),
        updatedGroup.groupId,
        std::wstring(L"unset_admin"),
        normalizedOperatorId.toStdWString(),
        updatedGroup.version,
        normalizedTargetId.toStdWString(),
        nowMs
    };

    if (!persistGroupMutation(m_groupRepository, updatedGroup, members, event)) {
        return false;
    }

    *outEvent = event;
    return true;
}

QStringList GroupService::activeMemberIds(const QString& groupId) const {
    QStringList memberIds;
    if (!m_groupRepository || trimmedOrEmpty(groupId).isEmpty()) {
        return memberIds;
    }

    const auto members = m_groupRepository->loadMembers(groupId.toStdWString());
    memberIds.reserve(static_cast<qsizetype>(members.size()));
    for (const auto& member : members) {
        if (!member.isActive) {
            continue;
        }
        memberIds.append(QString::fromStdWString(member.memberClientId));
    }

    return normalizedUniqueMemberIds(memberIds);
}

std::vector<QString> GroupService::activeRecipients(const QString& localClientId, const QString& groupId) const {
    std::vector<QString> recipients;
    const QStringList memberIds = activeMemberIds(groupId);
    recipients.reserve(static_cast<std::size_t>(memberIds.size()));
    for (const QString& memberId : memberIds) {
        if (memberId == localClientId) {
            continue;
        }
        recipients.push_back(memberId);
    }

    return recipients;
}

bool GroupService::shouldApplyIncomingMeta(const QString& groupId,
                                           int incomingVersion,
                                           qint64 incomingUpdatedAtMs) const {
    if (!m_groupRepository || trimmedOrEmpty(groupId).isEmpty()) {
        return true;
    }

    const auto existingGroup = m_groupRepository->findGroupById(groupId.toStdWString());
    if (!existingGroup.has_value()) {
        return true;
    }

    if (incomingVersion > 0 && existingGroup->version > 0) {
        if (incomingVersion < existingGroup->version) {
            return false;
        }
        if (incomingVersion > existingGroup->version) {
            return true;
        }
    }

    if (incomingUpdatedAtMs > 0 && existingGroup->updatedAtMs > 0) {
        if (incomingUpdatedAtMs < existingGroup->updatedAtMs) {
            return false;
        }
        if (incomingUpdatedAtMs > existingGroup->updatedAtMs) {
            return true;
        }
    }

    // 版本号和时间戳完全相同时，视为已处理过的重复元数据
    return false;
}

bool GroupService::isGroupConversation(const QString& conversationId) const {
    if (!m_groupRepository || conversationId.isEmpty()) {
        return false;
    }
    return m_groupRepository->findGroupById(conversationId.toStdWString()).has_value();
}

std::vector<MessageEnvelope> GroupService::buildGroupTextFanOut(const QString& localClientId,
                                                                 const QString& groupId,
                                                                 const QString& text) const {
    std::vector<MessageEnvelope> envelopes;
    const auto recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) {
        return envelopes;
    }

    const std::string messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const std::string senderIdStr = localClientId.toStdString();
    const std::string groupIdStr  = groupId.toStdString();
    // Build body JSON with proper escaping via QJsonDocument
    QJsonObject bodyObj;
    bodyObj.insert(QStringLiteral("group_id"), groupId);
    bodyObj.insert(QStringLiteral("message_kind"), QStringLiteral("text"));
    bodyObj.insert(QStringLiteral("content_type"), QStringLiteral("html"));
    bodyObj.insert(QStringLiteral("text"), text);
    const QByteArray bodyJson = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);
    const std::string bodyStr(bodyJson.constData(), static_cast<std::size_t>(bodyJson.size()));

    envelopes.reserve(recipients.size());
    for (const QString& recipient : recipients) {
        MessageEnvelope envelope;
        envelope.messageId      = messageId;
        envelope.type           = MessageType::GroupMessage;
        envelope.senderId       = senderIdStr;
        envelope.targetId       = recipient.toStdString();
        envelope.conversationId = groupIdStr;
        envelope.body           = bodyStr;
        envelope.createdAtMs    = nowMs;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupNudgeFanOut(const QString& localClientId,
                                                                 const QString& groupId) const {
    std::vector<MessageEnvelope> envelopes;
    const auto recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) {
        return envelopes;
    }

    const std::string messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const std::string senderIdStr = localClientId.toStdString();
    const std::string groupIdStr = groupId.toStdString();
    QJsonObject bodyObj;
    bodyObj.insert(QStringLiteral("group_id"), groupId);
    bodyObj.insert(QStringLiteral("message_kind"), QStringLiteral("nudge"));
    bodyObj.insert(QStringLiteral("content_type"), QStringLiteral("nudge"));
    bodyObj.insert(QStringLiteral("text"), QStringLiteral("[窗口抖动提醒]"));
    const QByteArray bodyJson = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);
    const std::string bodyStr(bodyJson.constData(), static_cast<std::size_t>(bodyJson.size()));

    envelopes.reserve(recipients.size());
    for (const QString& recipient : recipients) {
        MessageEnvelope envelope;
        envelope.messageId = messageId;
        envelope.type = MessageType::GroupMessage;
        envelope.senderId = senderIdStr;
        envelope.targetId = recipient.toStdString();
        envelope.conversationId = groupIdStr;
        envelope.body = bodyStr;
        envelope.contentType = "nudge";
        envelope.createdAtMs = nowMs;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupInlineAttachmentFanOut(const QString& localClientId,
                                                                            const QString& groupId,
                                                                            const QString& filePath) const {
    std::vector<MessageEnvelope> envelopes;
    if (trimmedOrEmpty(filePath).isEmpty() || !isInlineImageCandidate(filePath)) {
        return envelopes;
    }

    const auto recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) {
        return envelopes;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return envelopes;
    }

    const QByteArray rawBytes = file.readAll();
    file.close();
    if (rawBytes.isEmpty()) {
        return envelopes;
    }

    const QString fileName = QFileInfo(filePath).fileName().trimmed();
    const QString previewText = fileName.isEmpty()
        ? QStringLiteral("[图片]")
        : QStringLiteral("[图片] %1").arg(fileName);

    QJsonObject bodyObj;
    bodyObj.insert(QStringLiteral("group_id"), groupId);
    bodyObj.insert(QStringLiteral("message_kind"), QStringLiteral("attachment"));
    bodyObj.insert(QStringLiteral("attachment_kind"), QStringLiteral("image"));
    bodyObj.insert(QStringLiteral("attachment_name"), fileName);
    bodyObj.insert(QStringLiteral("text"), previewText);
    bodyObj.insert(QStringLiteral("base64"),
                   QString::fromLatin1(rawBytes.toBase64(QByteArray::Base64Encoding)));

    const QByteArray bodyJson = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);
    const std::string bodyStr(bodyJson.constData(), static_cast<std::size_t>(bodyJson.size()));
    const std::string messageId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const std::string senderIdStr = localClientId.toStdString();
    const std::string groupIdStr = groupId.toStdString();

    envelopes.reserve(recipients.size());
    for (const QString& recipient : recipients) {
        MessageEnvelope envelope;
        envelope.messageId = messageId;
        envelope.type = MessageType::GroupMessage;
        envelope.senderId = senderIdStr;
        envelope.targetId = recipient.toStdString();
        envelope.conversationId = groupIdStr;
        envelope.body = bodyStr;
        envelope.contentType = "inline_attachment";
        envelope.attachmentName = fileName.toStdString();
        envelope.createdAtMs = nowMs;
        envelopes.push_back(std::move(envelope));
    }

    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupResourceReferenceFanOut(
    const QString& localClientId,
    const QString& groupId,
    const ResourceReferenceMessagePayload& payload) const
{
    return buildGroupResourceReferenceFanOut(localClientId,
                                             groupId,
                                             ResourceRefPayload{
                                                 payload.resource.serviceId,
                                                 payload.resource.workspaceId,
                                                 payload.resource.origin == ResourceOrigin::Service
                                                     ? QStringLiteral("service")
                                                     : QStringLiteral("local"),
                                                 payload.resource.resourceKind,
                                                 payload.resource.resourceId,
                                                 payload.resource.title,
                                                 payload.summary,
                                                 QString(),
                                                 {},
                                                 payload.resource.version,
                                                 0
                                             });
}

std::vector<MessageEnvelope> GroupService::buildGroupResourceReferenceFanOut(
    const QString& localClientId,
    const QString& groupId,
    const ResourceRefPayload& payload) const
{
    std::vector<MessageEnvelope> envelopes;
    const auto recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) {
        return envelopes;
    }

    const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    envelopes.reserve(recipients.size());
    for (const QString& recipient : recipients) {
        envelopes.push_back(buildResourceReferenceEnvelope(messageId,
                                                           localClientId,
                                                           recipient,
                                                           groupId,
                                                           payload,
                                                           nowMs));
    }
    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupSharedFileReferenceFanOut(
    const QString& localClientId,
    const QString& groupId,
    const SharedFileResource& resource) const
{
    return buildGroupResourceReferenceFanOut(localClientId,
                                             groupId,
                                             SharedFileResourceContracts::makePayload(resource));
}

std::vector<MessageEnvelope> GroupService::buildGroupCreateMetaFanOut(
    const QString& localClientId,
    const Group& group,
    const QStringList& allMemberIds) const
{
    return buildGroupMetaFanOut(localClientId,
                                group,
                                normalizedUniqueMemberIds(allMemberIds),
                                normalizedUniqueMemberIds(allMemberIds),
                                QStringLiteral("create"));

    std::vector<MessageEnvelope> envelopes;
    const QHash<QString, QString> peerNames = knownPeerDisplayNames(m_conversationRepository);
    const std::vector<ConversationSummary> summaries =
        m_conversationRepository ? m_conversationRepository->loadConversationSummaries()
                                 : std::vector<ConversationSummary>{};

    QJsonObject payload;
    payload.insert(QStringLiteral("event_type"), QStringLiteral("create"));
    payload.insert(QStringLiteral("group_id"), QString::fromStdWString(group.groupId));
    payload.insert(QStringLiteral("group_name"), QString::fromStdWString(group.groupName));
    payload.insert(QStringLiteral("owner_client_id"), QString::fromStdWString(group.ownerClientId));
    payload.insert(QStringLiteral("created_at_ms"), static_cast<double>(group.createdAtMs));
    QJsonArray membersArray;
    for (const QString& id : allMemberIds) {
        QJsonObject memberObject;
        memberObject.insert(QStringLiteral("client_id"), id);
        const QString displayName = resolveMemberDisplayName(id, peerNames, summaries);
        if (!displayName.isEmpty()) {
            memberObject.insert(QStringLiteral("display_name"), displayName);
        }
        membersArray.append(memberObject);
    }
    payload.insert(QStringLiteral("members"), membersArray);
    const QByteArray bodyJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const std::string bodyStr(bodyJson.constData(), static_cast<std::size_t>(bodyJson.size()));

    const std::string groupIdStr = QString::fromStdWString(group.groupId).toStdString();
    const std::string senderStr  = localClientId.toStdString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (const QString& memberId : allMemberIds) {
        if (memberId == localClientId) {
            continue; // 不发给自己
        }
        MessageEnvelope env;
        env.messageId      = QStringLiteral("meta-create-%1-%2")
                                 .arg(QString::fromStdWString(group.groupId), memberId)
                                 .toStdString();
        env.type           = MessageType::GroupMeta;
        env.senderId       = senderStr;
        env.targetId       = memberId.toStdString();
        env.conversationId = groupIdStr;
        env.body           = bodyStr;
        env.createdAtMs    = nowMs;
        envelopes.push_back(std::move(env));
    }
    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupMetaFanOut(
    const QString& localClientId,
    const Group& group,
    const QStringList& snapshotMemberIds,
    const QStringList& recipientIds,
    const QString& eventType,
    const QString& affectedMemberId) const
{
    std::vector<MessageEnvelope> envelopes;
    const QString normalizedEventType = eventType.trimmed();
    if (normalizedEventType.isEmpty()) {
        return envelopes;
    }

    const QHash<QString, QString> peerNames = knownPeerDisplayNames(m_conversationRepository);
    const std::vector<ConversationSummary> summaries =
        m_conversationRepository ? m_conversationRepository->loadConversationSummaries()
                                 : std::vector<ConversationSummary>{};

    QJsonObject payload;
    payload.insert(QStringLiteral("event_type"), normalizedEventType);
    payload.insert(QStringLiteral("group_id"), QString::fromStdWString(group.groupId));
    payload.insert(QStringLiteral("group_name"), QString::fromStdWString(group.groupName));
    payload.insert(QStringLiteral("owner_client_id"), QString::fromStdWString(group.ownerClientId));
    payload.insert(QStringLiteral("version"), group.version);
    payload.insert(QStringLiteral("created_at_ms"), static_cast<double>(group.createdAtMs));
    payload.insert(QStringLiteral("updated_at_ms"), static_cast<double>(group.updatedAtMs));
    payload.insert(QStringLiteral("announcement"), QString::fromStdWString(group.announcement));
    if (!affectedMemberId.trimmed().isEmpty()) {
        payload.insert(QStringLiteral("affected_member_id"), affectedMemberId.trimmed());
    }

    QJsonArray membersArray;
    // 查询角色信息以便在 meta 消息中携带
    const auto storedMembers = m_groupRepository->loadMembers(group.groupId);
    QHash<QString, QString> memberRoles;
    for (const auto& sm : storedMembers) {
        memberRoles.insert(QString::fromStdWString(sm.memberClientId),
                           QString::fromStdWString(sm.role));
    }
    for (const QString& rawId : normalizedUniqueMemberIds(snapshotMemberIds)) {
        const QString id = rawId.trimmed();
        if (id.isEmpty()) {
            continue;
        }
        QJsonObject memberObject;
        memberObject.insert(QStringLiteral("client_id"), id);
        const QString displayName = resolveMemberDisplayName(id, peerNames, summaries);
        if (!displayName.isEmpty()) {
            memberObject.insert(QStringLiteral("display_name"), displayName);
        }
        const QString role = memberRoles.value(id, QStringLiteral("member"));
        if (!role.isEmpty()) {
            memberObject.insert(QStringLiteral("role"), role);
        }
        membersArray.append(memberObject);
    }
    payload.insert(QStringLiteral("members"), membersArray);

    // 携带当前群的置顶消息快照，以便新成员加入时能立即看到置顶
    if (m_conversationRepository) {
        const auto pinnedMsgs = m_conversationRepository->loadPinnedMessages(
            QString::fromStdWString(group.groupId));
        QJsonArray pinnedArray;
        for (const auto& p : pinnedMsgs) {
            QJsonObject pin;
            pin.insert(QStringLiteral("message_id"), p.messageId);
            pin.insert(QStringLiteral("pinned_body"), p.pinnedBody);
            pin.insert(QStringLiteral("author_name"), p.authorName);
            pin.insert(QStringLiteral("pinner_name"), p.pinnerName);
            pin.insert(QStringLiteral("pinned_at_ms"), static_cast<double>(p.pinnedAtMs));
            pinnedArray.append(pin);
        }
        if (!pinnedArray.isEmpty()) {
            payload.insert(QStringLiteral("pinned_messages"), pinnedArray);
        }
    }

    const QByteArray bodyJson = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const std::string bodyStr(bodyJson.constData(), static_cast<std::size_t>(bodyJson.size()));

    const std::string groupIdStr = QString::fromStdWString(group.groupId).toStdString();
    const std::string senderStr = localClientId.toStdString();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    QStringList normalizedRecipients = recipientIds;
    for (QString& memberId : normalizedRecipients) {
        memberId = memberId.trimmed();
    }
    normalizedRecipients.removeAll(QString());
    normalizedRecipients.removeDuplicates();

    for (const QString& memberId : normalizedRecipients) {
        if (memberId == localClientId) {
            continue;
        }

        MessageEnvelope env;
        env.messageId = QStringLiteral("meta-%1-%2-%3")
                            .arg(normalizedEventType,
                                 QString::fromStdWString(group.groupId),
                                 memberId)
                            .toStdString();
        env.type = MessageType::GroupMeta;
        env.senderId = senderStr;
        env.targetId = memberId.toStdString();
        env.conversationId = groupIdStr;
        env.body = bodyStr;
        env.createdAtMs = nowMs;
        envelopes.push_back(std::move(env));
    }

    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupMutationFanOut(
    const QString& localClientId,
    const QString& groupId,
    const MessageMutation& mutation) const
{
    std::vector<MessageEnvelope> envelopes;
    const auto recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) {
        return envelopes;
    }

    // Use mutation.mutationMessageId if set; otherwise generate a UUID
    const std::string mutationMsgId = mutation.mutationMessageId.isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString()
        : mutation.mutationMessageId.toStdString();

    const std::string senderIdStr = localClientId.toStdString();
    const std::string groupIdStr = groupId.toStdString();

    // Build payloadJson once
    std::string payload;
    std::string subtype;
    if (mutation.kind == MessageMutationKind::Recall) {
        payload = buildRecallPayloadJson(mutation.targetMessageId, mutation.mutatedAtMs);
        subtype = "recall";
    } else {
        payload = buildEditPayloadJson(mutation.targetMessageId, mutation.newBody,
                                       mutation.newContentType, mutation.mutatedAtMs);
        subtype = "edit";
    }

    envelopes.reserve(recipients.size());
    for (const QString& recipient : recipients) {
        MessageEnvelope envelope;
        envelope.messageId      = mutationMsgId;
        envelope.type           = MessageType::MessageMutation;
        envelope.senderId       = senderIdStr;
        envelope.targetId       = recipient.toStdString();
        envelope.conversationId = groupIdStr;
        envelope.messageSubtype = subtype;
        envelope.payloadJson    = payload;
        envelope.createdAtMs    = mutation.mutatedAtMs;
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupPinFanOut(
    const QString& localClientId,
    const QString& groupId,
    const QString& messageId,
    const QString& pinnedBody,
    const QString& authorName,
    const QString& pinnerName,
    bool unpin) const
{
    std::vector<MessageEnvelope> envelopes;
    const auto recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) {
        return envelopes;
    }
    const std::string pinMsgId = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
    const std::string senderIdStr = localClientId.toStdString();
    const std::string groupIdStr = groupId.toStdString();

    // JSON payload
    QJsonObject obj;
    obj.insert(QStringLiteral("group_id"), groupId);
    obj.insert(QStringLiteral("message_id"), messageId);
    obj.insert(QStringLiteral("pinned_body"), pinnedBody);
    obj.insert(QStringLiteral("author_name"), authorName);
    obj.insert(QStringLiteral("pinner_name"), pinnerName);
    obj.insert(QStringLiteral("action"), unpin ? QStringLiteral("unpin") : QStringLiteral("pin"));
    const std::string payload = QJsonDocument(obj).toJson(QJsonDocument::Compact).toStdString();

    envelopes.reserve(recipients.size());
    for (const QString& recipient : recipients) {
        MessageEnvelope envelope;
        envelope.messageId      = pinMsgId;
        envelope.type           = MessageType::PinMessage;
        envelope.senderId       = senderIdStr;
        envelope.targetId       = recipient.toStdString();
        envelope.conversationId = groupIdStr;
        envelope.payloadJson    = payload;
        envelope.createdAtMs    = QDateTime::currentMSecsSinceEpoch();
        envelopes.push_back(std::move(envelope));
    }
    return envelopes;
}

std::vector<QString> GroupService::createOutgoingGroupFileTasks(const QString& localClientId,
                                                                  const QString& groupId,
                                                                  const QString& filePath,
                                                                  FileTransferService* fileTransferService) const {
    std::vector<QString> taskIds;
    if (!fileTransferService || groupId.isEmpty() || filePath.isEmpty()) {
        return taskIds;
    }

    const auto recipients = activeRecipients(localClientId, groupId);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    taskIds.reserve(recipients.size());
    for (const QString& recipient : recipients) {
        FileTransferTask task;
        if (!fileTransferService->createOutgoingTask(groupId, recipient, groupId, filePath, nowMs, &task)) {
            continue;
        }
        taskIds.push_back(QString::fromStdWString(task.taskId));
    }
    return taskIds;
}

std::vector<MessageEnvelope> GroupService::buildGroupFileServiceConfigFanOut(
    const QString& localClientId,
    const QString& groupId,
    const GroupFileServiceConfig& config) const
{
    const std::vector<QString> recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) return {};

    const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    QJsonObject configObj;
    configObj[QStringLiteral("enabled")]      = config.enabled;
    configObj[QStringLiteral("base_url")]     = config.baseUrl;
    // DESIGN DECISION (2026-04-12): bearer_token is intentionally broadcast to all group members.
    // This software runs in a closed intranet with trusted users only.
    // Cross-group isolation is enforced by workspace-scoped tokens (see Fix 3 in the P1 design doc):
    // each group must be configured with a token whose allowedWorkspaces covers only that group's
    // workspaceId. A wildcard (*) token must never be shared across multiple groups.
    configObj[QStringLiteral("bearer_token")] = config.bearerToken;
    configObj[QStringLiteral("workspace_id")] = config.workspaceId;

    QJsonObject bodyObj;
    bodyObj[QStringLiteral("group_id")]            = groupId;
    bodyObj[QStringLiteral("message_kind")]        = QStringLiteral("group_file_service_config");
    bodyObj[QStringLiteral("file_service_config")] = configObj;

    const QByteArray bodyBytes = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    const std::string messageIdStr  = messageId.toStdString();
    const std::string senderIdStr   = localClientId.toStdString();
    const std::string groupIdStr    = groupId.toStdString();
    const std::string bodyStr(bodyBytes.constData(),
                              static_cast<std::size_t>(bodyBytes.size()));

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(recipients.size());
    for (const QString& recipientId : recipients) {
        MessageEnvelope env;
        env.messageId      = messageIdStr;
        env.senderId       = senderIdStr;
        env.targetId       = recipientId.toStdString();   // varies per recipient, stays in loop
        env.conversationId = groupIdStr;
        env.type           = MessageType::GroupMessage;
        env.contentType    = "group_config";
        env.messageSubtype = "group_file_service_config";
        env.body           = bodyStr;
        env.createdAtMs    = nowMs;
        envelopes.push_back(std::move(env));
    }
    return envelopes;
}

std::vector<MessageEnvelope> GroupService::buildGroupFileOfferFanOut(
    const QString& localClientId,
    const QString& groupId,
    const QString& filePath,
    const QString& uploaderName) const
{
    const auto recipients = activeRecipients(localClientId, groupId);
    if (recipients.empty()) return {};

    const QFileInfo fi(filePath);
    const QString messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    QJsonObject fileCard;
    fileCard[QStringLiteral("channel")]          = QStringLiteral("p2p");
    fileCard[QStringLiteral("file_name")]        = fi.fileName();
    fileCard[QStringLiteral("file_size")]        = fi.size();
    fileCard[QStringLiteral("sender_id")]        = localClientId;
    fileCard[QStringLiteral("sender_file_path")] = filePath;
    fileCard[QStringLiteral("uploader_name")]    = uploaderName;
    fileCard[QStringLiteral("download_state")]   = QStringLiteral("none");

    const QByteArray cardBytes = QJsonDocument(fileCard).toJson(QJsonDocument::Compact);
    const std::string cardStr(cardBytes.constData(), static_cast<std::size_t>(cardBytes.size()));
    const QByteArray bodyBytes = QStringLiteral("[群文件] %1").arg(fi.fileName()).toUtf8();
    const std::string bodyStr(bodyBytes.constData(), static_cast<std::size_t>(bodyBytes.size()));
    const std::string messageIdStr = messageId.toStdString();
    const std::string senderIdStr  = localClientId.toStdString();
    const std::string groupIdStr   = groupId.toStdString();

    std::vector<MessageEnvelope> envelopes;
    envelopes.reserve(recipients.size());
    for (const QString& recipientId : recipients) {
        MessageEnvelope env;
        env.messageId      = messageIdStr;
        env.senderId       = senderIdStr;
        env.targetId       = recipientId.toStdString();
        env.conversationId = groupIdStr;
        env.type           = MessageType::GroupMessage;
        env.messageSubtype = "group_file_card";
        env.body           = bodyStr;
        env.payloadJson    = cardStr;
        env.createdAtMs    = nowMs;
        envelopes.push_back(std::move(env));
    }
    return envelopes;
}
