#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QVector>

#include "domain/ConversationSummary.h"

class ChatDataStore;

class ConversationListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        ConversationIdRole = Qt::UserRole + 1,
        TitleRole,
        PreviewRole,
        TimeLabelRole,
        HasUnreadRole,
        IsPinnedRole,
        IsStarredRole,
        IsMutedRole,
        IsDoneRole,
        HasMentionMeRole,
        AvatarPathRole,
        DraftTextRole,
        IsOnlineRole
    };

    // Matches filter tab index order in MainWindow (0-based)
    enum class Filter {
        All      = 0,
        Unread   = 1,
        Starred  = 2,
        AtMe     = 3,
        Tagged   = 4,
        Direct   = 5,
        Group    = 6,
        Done     = 7
    };

    explicit ConversationListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<ConversationSummary> items);
    void setUnreadConversationIds(QSet<QString> conversationIds);
    void setItemsAndUnread(QVector<ConversationSummary> items, QSet<QString> unreadIds);
    void setFilter(int filterIndex);
    void setSearchText(const QString& text);
    void setAvatarPaths(const QHash<QString, QString>& avatarPaths);
    void setDraftTexts(const QHash<QString, QString>& drafts);
    void setOnlinePeerIds(const QSet<QString>& onlineIds);
    int totalUnreadCount() const;
    int totalGroupUnreadCount() const;
    void bindToStore(ChatDataStore* store);

private slots:
    void onConversationListChanged();
    void onConversationUpserted(const QString& conversationId);
    void onUnreadSetChanged();

private:
    bool passesFilter(const ConversationSummary& item) const;
    static bool isGroupConversation(const ConversationSummary& item);
    void rebuildFilteredIndices();

    QSet<QString> m_unreadConversationIds;
    QVector<ConversationSummary> m_items;
    Filter m_filter = Filter::All;
    QString m_searchText;
    ChatDataStore* m_store = nullptr;
    QHash<QString, QString> m_avatarPaths; // conversationId → avatar file path
    QHash<QString, QString> m_draftTexts;  // conversationId → draft text
    QSet<QString> m_onlinePeerIds;         // conversationId of online peers
    QVector<int> m_filteredIndices;        // 缓存过滤结果: filtered row → m_items index
};
