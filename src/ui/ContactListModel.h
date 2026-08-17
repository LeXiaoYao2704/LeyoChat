#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QVector>

#include "domain/PeerEndpoint.h"

class ChatDataStore;

class ContactListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum class PresenceFilter {
        All = 0,
        Online,
        Offline
    };

    enum Roles {
        ClientIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        StatusTextRole,
        HostRole,
        PortRole,
        PresenceRole,       // int: 0=Online, 1=Away, 2=Offline
        SectionRole,        // QString: 分组标题（如 "★ 收藏", "在线", "A", "B" …, "离线"）
        IsSectionHeaderRole,// bool: 该行是分组头
        IsFavoriteRole      // bool: 是否收藏
    };

    explicit ContactListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<PeerEndpoint> items);
    void setSearchText(const QString& text);
    void setPresenceFilter(PresenceFilter filter);
    PresenceFilter presenceFilter() const { return m_presenceFilter; }
    void bindToStore(ChatDataStore* store);

    // 收藏管理
    void toggleFavorite(const QString& clientId);
    bool isFavorite(const QString& clientId) const;
    QStringList sectionLetters() const;
    int rowForSection(const QString& letter) const;

    // 备注名管理
    void setAlias(const QString& clientId, const QString& alias);
    QString aliasFor(const QString& clientId) const;

private slots:
    void onContactListChanged();

private:
    struct DisplayItem {
        PeerEndpoint peer;
        QString section;
        bool isSectionHeader = false;
    };

    void rebuildDisplayList();
    void loadFavorites();
    void saveFavorites();
    void loadAliases();
    void saveAliases();

    QVector<PeerEndpoint> m_allItems;
    QVector<DisplayItem> m_items;
    QString m_searchText;
    PresenceFilter m_presenceFilter = PresenceFilter::All;
    ChatDataStore* m_store = nullptr;
    QSet<QString> m_favorites;
    QHash<QString, QString> m_aliases; // clientId → 备注名
};
