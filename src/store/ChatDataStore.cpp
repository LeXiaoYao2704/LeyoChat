// src/store/ChatDataStore.cpp
#include "store/ChatDataStore.h"

#include <algorithm>
#include <iterator>
#include <QDebug>

#include "ui/MainWindow.h"  // for PinnedCardInfo

const std::vector<ChatMessage> ChatDataStore::s_emptyMessages;

namespace {
bool sameMessageForReset(const ChatMessage& lhs, const ChatMessage& rhs) {
    return lhs.messageId == rhs.messageId
        && lhs.body == rhs.body
        && lhs.deliveryState == rhs.deliveryState
        && lhs.createdAtMs == rhs.createdAtMs
        && lhs.isRecalled == rhs.isRecalled
        && lhs.recalledAtMs == rhs.recalledAtMs
        && lhs.editedAtMs == rhs.editedAtMs
        && lhs.lastEditorId == rhs.lastEditorId
        && lhs.groupReadCount == rhs.groupReadCount
        && lhs.localFilePath == rhs.localFilePath
        && lhs.fileCardJson == rhs.fileCardJson
        && lhs.reactionsJson == rhs.reactionsJson;
}
}

ChatDataStore::ChatDataStore(QObject* parent)
    : QObject(parent)
{
}

// ──── 会话 ────

QVector<ConversationSummary> ChatDataStore::allConversations() const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    if (m_sortDirty) {
        const_cast<ChatDataStore*>(this)->rebuildSortedConversations();
    }
    return m_sortedConversations;
}

std::optional<ConversationSummary> ChatDataStore::conversation(const QString& id) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_conversations.find(id);
    if (it == m_conversations.end()) return std::nullopt;
    return *it;
}

void ChatDataStore::upsertConversation(const ConversationSummary& summary) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    const QString id = QString::fromStdWString(summary.conversationId);
    const bool isNew = !m_conversations.contains(id);
    if (!isNew) {
        // 仅当新消息时间戳 >= 已有记录时才更新预览和时间戳，防止乱序消息覆盖
        auto& existing = m_conversations[id];
        existing.title = summary.title;
        if (summary.lastMessageAtMs >= existing.lastMessageAtMs) {
            existing.lastMessagePreview = summary.lastMessagePreview;
            existing.lastMessageAtMs    = summary.lastMessageAtMs;
        }
        // 保留 isPinned/isStarred/isMuted 等用户设置，仅重置 isDone
        existing.isDone = false;
        if (summary.hasMentionMe) existing.hasMentionMe = true;
    } else {
        m_conversations[id] = summary;
    }
    m_sortDirty = true;
    emit conversationUpserted(id);
    if (isNew) {
        emit conversationListChanged();
    }
}

void ChatDataStore::removeConversation(const QString& id) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    if (m_conversations.remove(id)) {
        m_sortDirty = true;
        emit conversationRemoved(id);
        emit conversationListChanged();
    }
}

void ChatDataStore::setConversationFlag(const QString& id, ConversationFlag flag, bool value) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_conversations.find(id);
    if (it == m_conversations.end()) return;
    auto& c = *it;
    switch (flag) {
        case ConversationFlag::Pinned:         c.isPinned = value; break;
        case ConversationFlag::Starred:        c.isStarred = value; break;
        case ConversationFlag::Muted:          c.isMuted = value; break;
        case ConversationFlag::Done:           c.isDone = value; break;
        case ConversationFlag::ManuallyUnread: c.isManuallyUnread = value; break;
        case ConversationFlag::HasMentionMe:   c.hasMentionMe = value; break;
    }
    m_sortDirty = true;  // pinned 影响排序
    emit conversationUpserted(id);
}

void ChatDataStore::rebuildSortedConversations() {
    m_sortedConversations.clear();
    m_sortedConversations.reserve(m_conversations.size());
    for (auto it = m_conversations.cbegin(); it != m_conversations.cend(); ++it) {
        m_sortedConversations.append(it.value());
    }
    std::stable_sort(m_sortedConversations.begin(), m_sortedConversations.end(),
        [](const ConversationSummary& a, const ConversationSummary& b) {
            if (a.isPinned != b.isPinned) return a.isPinned;
            return a.lastMessageAtMs > b.lastMessageAtMs;
        });
    m_sortDirty = false;
}

// ──── 消息 ────

const std::vector<ChatMessage>& ChatDataStore::messages(const QString& conversationId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_messages.find(conversationId);
    if (it == m_messages.end()) return s_emptyMessages;
    return *it;
}

bool ChatDataStore::hasMessages(const QString& conversationId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    return m_messages.contains(conversationId);
}

bool ChatDataStore::hasMoreMessagesBefore(const QString& conversationId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    return m_hasMoreMessagesBefore.value(conversationId, false);
}

QString ChatDataStore::firstMessageId(const QString& conversationId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    const auto it = m_messages.find(conversationId);
    if (it == m_messages.end() || it->empty()) return {};
    return QString::fromStdWString(it->front().messageId);
}

void ChatDataStore::appendMessage(const QString& conversationId, const ChatMessage& msg) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto& vec = m_messages[conversationId];
    const int newIndex = static_cast<int>(vec.size());
    vec.push_back(msg);
    touchMessagesLru(conversationId);
    emit messageAppended(conversationId, newIndex);
}

void ChatDataStore::updateMessage(const QString& conversationId, const ChatMessage& msg) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_messages.find(conversationId);
    if (it == m_messages.end()) return;
    const QString messageId = QString::fromStdWString(msg.messageId);
    for (auto& existing : *it) {
        if (existing.messageId == msg.messageId) {
            existing = msg;
            emit messageUpdated(conversationId, messageId);
            return;
        }
    }
}

void ChatDataStore::updateDeliveryState(const QString& conversationId,
                                         const QString& messageId,
                                         MessageDeliveryState state) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_messages.find(conversationId);
    if (it == m_messages.end()) return;
    const std::wstring wid = messageId.toStdWString();
    for (auto& msg : *it) {
        if (msg.messageId == wid) {
            if (msg.deliveryState == state) return;  // 无变化
            msg.deliveryState = state;
            emit messageUpdated(conversationId, messageId);
            return;
        }
    }
}

void ChatDataStore::setMessages(const QString& conversationId,
                                std::vector<ChatMessage> msgs,
                                bool hasMoreBefore) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_messages.find(conversationId);
    if (it != m_messages.end()) {
        const auto& current = *it;

        // 若 store 里已有比新数据更多的消息（说明通过 prependMessages 加载了历史），
        // 则合并：保留历史前缀 + 用新数据更新尾部，避免历史消息被 setMessages 覆盖。
        if (current.size() > msgs.size() && !msgs.empty()) {
            const std::wstring& firstNewId = msgs.front().messageId;
            // 在当前 store 里找新数据首条消息的位置
            for (size_t i = 0; i < current.size(); ++i) {
                if (current[i].messageId == firstNewId) {
                    // i 之前的是历史记录，i 之后用 msgs 替换（含送达状态更新）
                    std::vector<ChatMessage> merged;
                    merged.reserve(i + msgs.size());
                    merged.insert(merged.end(), current.begin(), current.begin() + i);
                    merged.insert(merged.end(), msgs.begin(), msgs.end());
                    qDebug() << "[BUG3-FIX] setMessages: merging history prefix" << i << "+ recent" << msgs.size() << "= total" << merged.size();
                    m_messages[conversationId] = std::move(merged);
                    // hasMoreBefore 沿用历史标记（已知有更多历史）
                    touchMessagesLru(conversationId);
                    emit messagesReset(conversationId);
                    return;
                }
            }
            // firstNewId 不在 current 里（极罕见：会话被清空再加载），走正常替换路径
        }

        // 尺寸相同时检查是否有变化，无变化直接跳过
        if (current.size() == msgs.size()) {
            bool same = true;
            for (size_t i = 0, n = msgs.size(); i < n; ++i) {
                if (!sameMessageForReset(current[i], msgs[i])) {
                    same = false;
                    break;
                }
            }
            if (same) {
                m_hasMoreMessagesBefore[conversationId] = hasMoreBefore;
                touchMessagesLru(conversationId);
                return;
            }
        }
    }
    m_messages[conversationId] = std::move(msgs);
    m_hasMoreMessagesBefore[conversationId] = hasMoreBefore;
    touchMessagesLru(conversationId);
    emit messagesReset(conversationId);
}

void ChatDataStore::prependMessages(const QString& conversationId,
                                    std::vector<ChatMessage> msgs,
                                    bool hasMoreBefore) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    m_hasMoreMessagesBefore[conversationId] = hasMoreBefore;
    if (msgs.empty()) {
        touchMessagesLru(conversationId);
        return;
    }

    auto& vec = m_messages[conversationId];
    QSet<QString> existingIds;
    existingIds.reserve(static_cast<int>(vec.size()));
    for (const auto& msg : vec) {
        existingIds.insert(QString::fromStdWString(msg.messageId));
    }

    msgs.erase(std::remove_if(msgs.begin(), msgs.end(), [&](const ChatMessage& msg) {
                   return existingIds.contains(QString::fromStdWString(msg.messageId));
               }),
               msgs.end());
    if (msgs.empty()) {
        touchMessagesLru(conversationId);
        return;
    }

    const int inserted = static_cast<int>(msgs.size());
    vec.insert(vec.begin(),
               std::make_move_iterator(msgs.begin()),
               std::make_move_iterator(msgs.end()));
    touchMessagesLru(conversationId);
    emit messagesPrepended(conversationId, inserted);
}

void ChatDataStore::recallMessage(const QString& conversationId, const QString& messageId,
                                   qint64 recalledAtMs) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_messages.find(conversationId);
    if (it == m_messages.end()) return;
    const std::wstring wid = messageId.toStdWString();
    for (auto& msg : *it) {
        if (msg.messageId == wid) {
            msg.isRecalled = true;
            msg.recalledAtMs = recalledAtMs;
            emit messageUpdated(conversationId, messageId);
            return;
        }
    }
}

void ChatDataStore::editMessage(const QString& conversationId, const QString& messageId,
                                 const std::wstring& newBody, qint64 editedAtMs,
                                 const std::wstring& editorId) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_messages.find(conversationId);
    if (it == m_messages.end()) return;
    const std::wstring wid = messageId.toStdWString();
    for (auto& msg : *it) {
        if (msg.messageId == wid) {
            msg.body = newBody;
            msg.editedAtMs = editedAtMs;
            msg.lastEditorId = editorId;
            emit messageUpdated(conversationId, messageId);
            return;
        }
    }
}

void ChatDataStore::incrementGroupReadCount(const QString& conversationId, const QString& messageId) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_messages.find(conversationId);
    if (it == m_messages.end()) return;
    const std::wstring wid = messageId.toStdWString();
    for (auto& msg : *it) {
        if (msg.messageId == wid) {
            ++msg.groupReadCount;
            emit messageUpdated(conversationId, messageId);
            return;
        }
    }
}

// ──── 未读 ────

QSet<QString> ChatDataStore::unreadConversationIds() const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    return m_unreadConversationIds;
}

void ChatDataStore::setUnreadConversationIds(QSet<QString> ids) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    if (m_unreadConversationIds == ids) return;
    m_unreadConversationIds = std::move(ids);
    emit unreadSetChanged();
}

void ChatDataStore::markConversationRead(const QString& id) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    if (m_unreadConversationIds.remove(id)) {
        emit unreadSetChanged();
    }
}

void ChatDataStore::markConversationUnread(const QString& id) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    if (m_unreadConversationIds.insert(id) != m_unreadConversationIds.end()) {
        emit unreadSetChanged();
    }
}

// ──── 群组 ────

std::optional<Group> ChatDataStore::group(const QString& groupId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_groups.find(groupId);
    if (it == m_groups.end()) return std::nullopt;
    return *it;
}

void ChatDataStore::upsertGroup(const Group& group) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    const QString id = QString::fromStdWString(group.groupId);
    m_groups[id] = group;
    invalidateGroupMemberEntriesCache(id);
    emit groupUpdated(id);
}

std::vector<GroupMember> ChatDataStore::groupMembers(const QString& groupId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_groupMembers.find(groupId);
    if (it == m_groupMembers.end()) return {};
    return *it;
}

void ChatDataStore::setGroupMembers(const QString& groupId, std::vector<GroupMember> members) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    m_groupMembers[groupId] = std::move(members);
    invalidateGroupMemberEntriesCache(groupId);
    emit groupMembersChanged(groupId);
}

GroupMemberListEntries ChatDataStore::groupMemberEntries(const QString& groupId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto cacheIt = m_groupMemberEntriesCache.find(groupId);
    if (cacheIt != m_groupMemberEntriesCache.end()) {
        return *cacheIt;
    }
    auto membersIt = m_groupMembers.find(groupId);
    if (membersIt == m_groupMembers.end()) return {};
    const auto& members = *membersIt;
    auto groupIt = m_groups.find(groupId);
    const QString ownerClientId = (groupIt != m_groups.end())
        ? QString::fromStdWString(groupIt->ownerClientId) : QString();

    GroupMemberListEntries entries;
    entries.reserve(static_cast<qsizetype>(members.size()));
    for (const auto& m : members) {
        if (!m.isActive) continue;
        const QString memberId = QString::fromStdWString(m.memberClientId);
        QString displayName;
        if (memberId == m_localClientId && m_displayNameResolver) {
            displayName = m_displayNameResolver(memberId);
        }
        if (displayName.isEmpty() && m_displayNameResolver) {
            displayName = m_displayNameResolver(memberId);
        }
        if (displayName.isEmpty() && !m.memberDisplayNameSnapshot.empty()) {
            displayName = QString::fromStdWString(m.memberDisplayNameSnapshot);
        }
        if (displayName.isEmpty()) {
            displayName = (memberId == m_localClientId)
                ? QStringLiteral("我") : QStringLiteral("未知成员");
        }
        const QString avatarPath = m_avatarPathResolver ? m_avatarPathResolver(memberId) : QString();
        const bool isOnline = m_onlineChecker ? m_onlineChecker(memberId) : (memberId == m_localClientId);
        entries.push_back(GroupMemberListEntry{
            memberId, displayName,
            memberId == ownerClientId,
            m.role == L"admin",
            memberId == m_localClientId,
            isOnline,
            avatarPath
        });
    }
    std::stable_sort(entries.begin(), entries.end(),
        [](const GroupMemberListEntry& lhs, const GroupMemberListEntry& rhs) {
            if (lhs.isOwner != rhs.isOwner) return lhs.isOwner;
            if (lhs.isAdmin != rhs.isAdmin) return lhs.isAdmin;
            if (lhs.isSelf != rhs.isSelf) return lhs.isSelf;
            if (lhs.isOnline != rhs.isOnline) return lhs.isOnline;
            return lhs.displayName.localeAwareCompare(rhs.displayName) < 0;
        });
    m_groupMemberEntriesCache[groupId] = entries;
    return entries;
}

// ──── 联系人 ────

QVector<PeerEndpoint> ChatDataStore::allContacts() const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    QVector<PeerEndpoint> result;
    result.reserve(m_contacts.size());
    for (auto it = m_contacts.cbegin(); it != m_contacts.cend(); ++it) {
        result.append(it.value());
    }
    return result;
}

std::optional<PeerEndpoint> ChatDataStore::contact(const QString& clientId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_contacts.find(clientId);
    if (it == m_contacts.end()) return std::nullopt;
    return *it;
}

void ChatDataStore::upsertContact(const PeerEndpoint& peer) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    const QString id = QString::fromStdString(peer.clientId);
    m_contacts[id] = peer;
    // 失效包含该联系人的所有群缓存
    for (auto it = m_groupMembers.cbegin(); it != m_groupMembers.cend(); ++it) {
        for (const auto& m : it.value()) {
            if (QString::fromStdWString(m.memberClientId) == id) {
                invalidateGroupMemberEntriesCache(it.key());
                break;
            }
        }
    }
    emit contactUpserted(id);
}

void ChatDataStore::removeContact(const QString& clientId) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    if (m_contacts.remove(clientId)) {
        emit contactRemoved(clientId);
        emit contactListChanged();
    }
}

void ChatDataStore::setContacts(QVector<PeerEndpoint> peers) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    m_contacts.clear();
    for (const auto& p : peers) {
        m_contacts[QString::fromStdString(p.clientId)] = p;
    }
    m_groupMemberEntriesCache.clear();
    emit contactListChanged();
}

// ──── 置顶消息 ────

std::vector<PinnedCardInfo> ChatDataStore::pinnedMessages(const QString& conversationId) const {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    auto it = m_pinnedMessages.find(conversationId);
    if (it == m_pinnedMessages.end()) return {};
    return *it;
}

void ChatDataStore::setPinnedMessages(const QString& conversationId, std::vector<PinnedCardInfo> pins) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    m_pinnedMessages[conversationId] = std::move(pins);
    emit pinnedMessagesChanged(conversationId);
}

// ──── 批量初始化 ────

void ChatDataStore::bulkLoadConversations(QVector<ConversationSummary> items, QSet<QString> unreadIds) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    m_conversations.clear();
    for (const auto& c : items) {
        m_conversations[QString::fromStdWString(c.conversationId)] = c;
    }
    m_unreadConversationIds = std::move(unreadIds);
    m_sortDirty = true;
    emit conversationListChanged();
    emit unreadSetChanged();
}

void ChatDataStore::bulkLoadGroups(QHash<QString, Group> groups,
                                    QHash<QString, std::vector<GroupMember>> members) {
    LEYOCHAT_ASSERT_MAIN_THREAD();
    m_groups = std::move(groups);
    m_groupMembers = std::move(members);
    m_groupMemberEntriesCache.clear();
    for (auto it = m_groups.cbegin(); it != m_groups.cend(); ++it) {
        emit groupUpdated(it.key());
    }
}

// ──── 配置 ────

void ChatDataStore::setLocalClientId(const QString& id) { m_localClientId = id; }
void ChatDataStore::setDisplayNameResolver(std::function<QString(const QString&)> resolver) { m_displayNameResolver = std::move(resolver); }
void ChatDataStore::setAvatarPathResolver(std::function<QString(const QString&)> resolver) { m_avatarPathResolver = std::move(resolver); }
void ChatDataStore::setOnlineChecker(std::function<bool(const QString&)> checker) { m_onlineChecker = std::move(checker); }

// ──── LRU ────

void ChatDataStore::touchMessagesLru(const QString& conversationId) {
    m_messagesLruOrder.removeAll(conversationId);
    m_messagesLruOrder.prepend(conversationId);
    evictMessagesLruIfNeeded();
}

void ChatDataStore::evictMessagesLruIfNeeded() {
    while (m_messagesLruOrder.size() > m_messagesLruCapacity) {
        const QString evicted = m_messagesLruOrder.takeLast();
        m_messages.remove(evicted);
        m_hasMoreMessagesBefore.remove(evicted);
    }
}

void ChatDataStore::invalidateGroupMemberEntriesCache(const QString& groupId) {
    m_groupMemberEntriesCache.remove(groupId);
}
