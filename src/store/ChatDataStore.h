// src/store/ChatDataStore.h
#pragma once

#include <optional>
#include <vector>

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QThread>
#include <QVector>

// Debug-mode assertion: ChatDataStore must only be accessed from the main thread.
#define LEYOCHAT_ASSERT_MAIN_THREAD() \
    Q_ASSERT_X(QThread::currentThread() == this->thread(), \
               Q_FUNC_INFO, "ChatDataStore must be accessed from the main thread only")

#include "domain/ChatMessage.h"
#include "domain/ConversationSummary.h"
#include "domain/Group.h"
#include "domain/GroupMember.h"
#include "domain/PeerEndpoint.h"
#include "storage/ConversationRepository.h"  // for ConversationFlag enum
#include "ui/GroupMemberListEntry.h"
#include "ui/MainWindow.h"  // for PinnedCardInfo

class ChatDataStore : public QObject {
    Q_OBJECT
public:
    explicit ChatDataStore(QObject* parent = nullptr);

    // ──── 会话 ────
    QVector<ConversationSummary> allConversations() const;
    std::optional<ConversationSummary> conversation(const QString& id) const;
    void upsertConversation(const ConversationSummary& summary);
    void removeConversation(const QString& id);
    void setConversationFlag(const QString& id, ConversationFlag flag, bool value);

    // ──── 消息 ────
    const std::vector<ChatMessage>& messages(const QString& conversationId) const;
    bool hasMessages(const QString& conversationId) const;
    bool hasMoreMessagesBefore(const QString& conversationId) const;
    QString firstMessageId(const QString& conversationId) const;
    void appendMessage(const QString& conversationId, const ChatMessage& msg);
    void updateMessage(const QString& conversationId, const ChatMessage& msg);
    void updateDeliveryState(const QString& conversationId,
                             const QString& messageId,
                             MessageDeliveryState state);
    void setMessages(const QString& conversationId,
                     std::vector<ChatMessage> msgs,
                     bool hasMoreBefore = false);
    void prependMessages(const QString& conversationId,
                         std::vector<ChatMessage> msgs,
                         bool hasMoreBefore);
    void recallMessage(const QString& conversationId, const QString& messageId,
                       qint64 recalledAtMs);
    void editMessage(const QString& conversationId, const QString& messageId,
                     const std::wstring& newBody, qint64 editedAtMs,
                     const std::wstring& editorId);
    void incrementGroupReadCount(const QString& conversationId, const QString& messageId);

    // ──── 未读 ────
    QSet<QString> unreadConversationIds() const;
    void setUnreadConversationIds(QSet<QString> ids);
    void markConversationRead(const QString& id);
    void markConversationUnread(const QString& id);

    // ──── 群组 ────
    std::optional<Group> group(const QString& groupId) const;
    void upsertGroup(const Group& group);
    std::vector<GroupMember> groupMembers(const QString& groupId) const;
    void setGroupMembers(const QString& groupId, std::vector<GroupMember> members);
    GroupMemberListEntries groupMemberEntries(const QString& groupId) const;

    // ──── 联系人 / Peer ────
    QVector<PeerEndpoint> allContacts() const;
    std::optional<PeerEndpoint> contact(const QString& clientId) const;
    void upsertContact(const PeerEndpoint& peer);
    void removeContact(const QString& clientId);
    void setContacts(QVector<PeerEndpoint> peers);

    // ──── 置顶消息 ────
    std::vector<PinnedCardInfo> pinnedMessages(const QString& conversationId) const;
    void setPinnedMessages(const QString& conversationId, std::vector<PinnedCardInfo> pins);

    // ──── 批量初始化 ────
    void bulkLoadConversations(QVector<ConversationSummary> items, QSet<QString> unreadIds);
    void bulkLoadGroups(QHash<QString, Group> groups,
                        QHash<QString, std::vector<GroupMember>> members);

    // ──── GroupMemberListEntry 构建依赖 ────
    void setLocalClientId(const QString& id);
    void setDisplayNameResolver(std::function<QString(const QString& clientId)> resolver);
    void setAvatarPathResolver(std::function<QString(const QString& clientId)> resolver);
    void setOnlineChecker(std::function<bool(const QString& clientId)> checker);

signals:
    // 会话
    void conversationUpserted(const QString& conversationId);
    void conversationRemoved(const QString& conversationId);
    void conversationListChanged();

    // 消息
    void messageAppended(const QString& conversationId, int newIndex);
    void messagesPrepended(const QString& conversationId, int count);
    void messageUpdated(const QString& conversationId, const QString& messageId);
    void messagesReset(const QString& conversationId);

    // 未读
    void unreadSetChanged();

    // 群组
    void groupUpdated(const QString& groupId);
    void groupMembersChanged(const QString& groupId);

    // 联系人
    void contactUpserted(const QString& clientId);
    void contactRemoved(const QString& clientId);
    void contactListChanged();

    // 置顶
    void pinnedMessagesChanged(const QString& conversationId);

private:
    QHash<QString, ConversationSummary> m_conversations;
    QVector<ConversationSummary> m_sortedConversations;  // 排序缓存
    bool m_sortDirty = true;

    QHash<QString, std::vector<ChatMessage>> m_messages;
    QHash<QString, bool> m_hasMoreMessagesBefore;
    QList<QString> m_messagesLruOrder;
    int m_messagesLruCapacity = 30;

    QHash<QString, Group> m_groups;
    QHash<QString, std::vector<GroupMember>> m_groupMembers;
    mutable QHash<QString, GroupMemberListEntries> m_groupMemberEntriesCache;

    QHash<QString, PeerEndpoint> m_contacts;
    QHash<QString, std::vector<PinnedCardInfo>> m_pinnedMessages;
    QSet<QString> m_unreadConversationIds;

    QString m_localClientId;
    std::function<QString(const QString&)> m_displayNameResolver;
    std::function<QString(const QString&)> m_avatarPathResolver;
    std::function<bool(const QString&)> m_onlineChecker;

    static const std::vector<ChatMessage> s_emptyMessages;

    void touchMessagesLru(const QString& conversationId);
    void evictMessagesLruIfNeeded();
    void invalidateGroupMemberEntriesCache(const QString& groupId);
    void rebuildSortedConversations();
};
