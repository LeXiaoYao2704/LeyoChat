#include "ui/ContactListModel.h"

#include <QDateTime>
#include <QSettings>
#include <QString>
#include <algorithm>
#include "app/AppSettings.h"
#include "domain/PeerPresenceEvaluator.h"
#include "store/ChatDataStore.h"
#include "ui/PinyinHelper.h"

namespace {
QString displayNameFor(const PeerEndpoint& endpoint) {
    const QString displayName = QString::fromStdString(endpoint.displayName).trimmed();
    if (!displayName.isEmpty()) {
        return displayName;
    }
    return QString::fromStdString(endpoint.clientId);
}

PeerPresenceStatus effectivePresenceFor(const PeerEndpoint& endpoint)
{
    return PeerPresenceEvaluator::effectivePresence(
        endpoint,
        QDateTime::currentMSecsSinceEpoch());
}

QString statusTextFor(const PeerEndpoint& endpoint) {
    switch (effectivePresenceFor(endpoint)) {
    case PeerPresenceStatus::Offline:
        return QStringLiteral("\u79bb\u7ebf");
    case PeerPresenceStatus::Away:
        return QStringLiteral("\u79bb\u5f00");
    case PeerPresenceStatus::Online:
        break;
    }
    return QStringLiteral("\u5728\u7EBF");
}

QString displayText(const PeerEndpoint& endpoint) {
    const QString displayName = displayNameFor(endpoint);
    const QString host = QString::fromStdString(endpoint.host);
    return QStringLiteral("%1 \u00B7 %2 (%3:%4)")
        .arg(displayName, statusTextFor(endpoint), host, QString::number(endpoint.port));
}

bool matchesSearch(const PeerEndpoint& endpoint, const QString& needle, const QString& alias = QString())
{
    if (needle.isEmpty()) {
        return true;
    }
    const QString clientId = QString::fromStdString(endpoint.clientId);
    const QString displayName = displayNameFor(endpoint);
    const QString host = QString::fromStdString(endpoint.host);
    const QString status = statusTextFor(endpoint);

    if (clientId.contains(needle, Qt::CaseInsensitive)
        || displayName.contains(needle, Qt::CaseInsensitive)
        || host.contains(needle, Qt::CaseInsensitive)
        || status.contains(needle, Qt::CaseInsensitive)
        || PinyinHelper::matchesPinyin(displayName, needle)) {
        return true;
    }
    // 备注名搜索
    if (!alias.isEmpty()) {
        return alias.contains(needle, Qt::CaseInsensitive)
            || PinyinHelper::matchesPinyin(alias, needle);
    }
    return false;
}

bool matchesPresenceFilter(const PeerEndpoint& endpoint, ContactListModel::PresenceFilter filter)
{
    if (filter == ContactListModel::PresenceFilter::All) {
        return true;
    }
    const PeerPresenceStatus presence = effectivePresenceFor(endpoint);
    if (filter == ContactListModel::PresenceFilter::Online) {
        return presence == PeerPresenceStatus::Online || presence == PeerPresenceStatus::Away;
    }
    return presence == PeerPresenceStatus::Offline;
}

// 获取显示名称的拼音排序键（首字母大写，用于字母分组）
QChar sortLetterFor(const QString& displayName)
{
    if (displayName.isEmpty()) return QChar('#');
    const QString initials = PinyinHelper::toPinyinInitials(displayName);
    if (initials.isEmpty()) return QChar('#');
    const QChar first = initials.at(0).toUpper();
    if (first >= 'A' && first <= 'Z') return first;
    return QChar('#');
}

} // namespace

ContactListModel::ContactListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    loadFavorites();
    loadAliases();
}

int ContactListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ContactListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto& item = m_items.at(index.row());

    if (item.isSectionHeader) {
        if (role == IsSectionHeaderRole) return true;
        if (role == SectionRole) return item.section;
        if (role == Qt::DisplayRole) return item.section;
        return {};
    }

    const auto& peer = item.peer;
    if (role == Qt::DisplayRole) return displayText(peer);
    if (role == ClientIdRole) return QString::fromStdString(peer.clientId);
    if (role == DisplayNameRole) {
        // 备注名优先
        const QString cid = QString::fromStdString(peer.clientId);
        auto ait = m_aliases.constFind(cid);
        if (ait != m_aliases.constEnd() && !ait.value().isEmpty()) {
            return ait.value();
        }
        return displayNameFor(peer);
    }
    if (role == StatusTextRole) return statusTextFor(peer);
    if (role == HostRole) return QString::fromStdString(peer.host);
    if (role == PortRole) return static_cast<int>(peer.port);
    if (role == PresenceRole) return static_cast<int>(effectivePresenceFor(peer));
    if (role == SectionRole) return item.section;
    if (role == IsSectionHeaderRole) return false;
    if (role == IsFavoriteRole) return m_favorites.contains(QString::fromStdString(peer.clientId));
    return {};
}

QHash<int, QByteArray> ContactListModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles.insert(ClientIdRole, "clientId");
    roles.insert(DisplayNameRole, "displayName");
    roles.insert(StatusTextRole, "statusText");
    roles.insert(HostRole, "host");
    roles.insert(PortRole, "port");
    roles.insert(PresenceRole, "presence");
    roles.insert(SectionRole, "section");
    roles.insert(IsSectionHeaderRole, "isSectionHeader");
    roles.insert(IsFavoriteRole, "isFavorite");
    return roles;
}

void ContactListModel::rebuildDisplayList()
{
    // 1. 筛选
    QVector<PeerEndpoint> filtered;
    filtered.reserve(m_allItems.size());
    for (const auto& item : m_allItems) {
        const QString alias = m_aliases.value(QString::fromStdString(item.clientId));
        if (matchesSearch(item, m_searchText, alias)
            && matchesPresenceFilter(item, m_presenceFilter)) {
            filtered.push_back(item);
        }
    }

    // 2. 计算每个 peer 的排序键
    struct SortItem {
        PeerEndpoint peer;
        int presenceOrder;   // 0=online, 1=away, 2=offline
        bool isFavorite;
        QChar letter;
        QString pinyinKey;
    };

    QVector<SortItem> sortItems;
    sortItems.reserve(filtered.size());
    for (const auto& p : filtered) {
        SortItem si;
        si.peer = p;
        si.presenceOrder = static_cast<int>(effectivePresenceFor(p));
        si.isFavorite = m_favorites.contains(QString::fromStdString(p.clientId));
        // 备注名优先用于排序
        const QString cid = QString::fromStdString(p.clientId);
        auto ait = m_aliases.constFind(cid);
        const QString name = (ait != m_aliases.constEnd() && !ait.value().isEmpty())
                                 ? ait.value()
                                 : displayNameFor(p);
        si.letter = sortLetterFor(name);
        si.pinyinKey = PinyinHelper::toPinyinFull(name).toLower();
        sortItems.push_back(std::move(si));
    }

    // 3. 排序：收藏 > 在线/离开 > 离线，组内按拼音
    std::sort(sortItems.begin(), sortItems.end(),
              [](const SortItem& a, const SortItem& b) {
                  // 收藏优先
                  if (a.isFavorite != b.isFavorite) return a.isFavorite > b.isFavorite;
                  // 在线状态分组
                  if (a.presenceOrder != b.presenceOrder) return a.presenceOrder < b.presenceOrder;
                  // 同组内按字母
                  if (a.letter != b.letter) return a.letter < b.letter;
                  // 同字母按拼音全拼
                  if (a.pinyinKey != b.pinyinKey) return a.pinyinKey < b.pinyinKey;
                  return a.peer.clientId < b.peer.clientId;
              });

    // 4. 构建带分组头的显示列表
    QVector<DisplayItem> newItems;
    newItems.reserve(sortItems.size() + 30); // 估算分组头数量

    const bool showSectionHeaders = m_searchText.isEmpty();
    QString lastSection;
    for (const auto& si : sortItems) {
        // 确定分组名
        QString section;
        if (si.isFavorite) {
            section = QStringLiteral("\u2605 \u6536\u85CF"); // "★ 收藏"
        } else if (si.presenceOrder == 2) {
            section = QStringLiteral("\u79BB\u7EBF"); // "离线"
        } else {
            // 在线/离开的用字母分组
            section = QString(si.letter);
        }

        // 插入分组头
        if (showSectionHeaders && section != lastSection) {
            DisplayItem header;
            header.section = section;
            header.isSectionHeader = true;
            newItems.push_back(std::move(header));
            lastSection = section;
        }

        DisplayItem di;
        di.peer = si.peer;
        di.section = section;
        di.isSectionHeader = false;
        newItems.push_back(std::move(di));
    }

    beginResetModel();
    m_items = std::move(newItems);
    endResetModel();
}

void ContactListModel::setItems(QVector<PeerEndpoint> items) {
    m_allItems = std::move(items);
    rebuildDisplayList();
}

void ContactListModel::setSearchText(const QString& text)
{
    const QString normalized = text.trimmed();
    if (normalized == m_searchText) {
        return;
    }
    m_searchText = normalized;
    rebuildDisplayList();
}

void ContactListModel::setPresenceFilter(PresenceFilter filter)
{
    if (filter == m_presenceFilter) {
        return;
    }
    m_presenceFilter = filter;
    rebuildDisplayList();
}

void ContactListModel::bindToStore(ChatDataStore* store) {
    if (m_store) disconnect(m_store, nullptr, this, nullptr);
    m_store = store;
}

void ContactListModel::onContactListChanged() {
    if (!m_store) return;
    auto items = m_store->allContacts();
    std::sort(items.begin(), items.end(),
              [](const PeerEndpoint& a, const PeerEndpoint& b) {
                  if (a.displayName != b.displayName)
                      return a.displayName < b.displayName;
                  return a.clientId < b.clientId;
              });
    setItems(items);
}

void ContactListModel::toggleFavorite(const QString& clientId)
{
    if (m_favorites.contains(clientId)) {
        m_favorites.remove(clientId);
    } else {
        m_favorites.insert(clientId);
    }
    saveFavorites();
    rebuildDisplayList();
}

bool ContactListModel::isFavorite(const QString& clientId) const
{
    return m_favorites.contains(clientId);
}

void ContactListModel::loadFavorites()
{
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    const QStringList list = cfg.value(QStringLiteral("contacts/favorites")).toStringList();
    m_favorites = QSet<QString>(list.begin(), list.end());
}

void ContactListModel::saveFavorites()
{
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    cfg.setValue(QStringLiteral("contacts/favorites"), QStringList(m_favorites.begin(), m_favorites.end()));
}

QStringList ContactListModel::sectionLetters() const
{
    QStringList letters;
    QSet<QString> seen;
    for (const auto& item : m_items) {
        if (item.isSectionHeader && !seen.contains(item.section)) {
            letters.append(item.section);
            seen.insert(item.section);
        }
    }
    return letters;
}

int ContactListModel::rowForSection(const QString& letter) const
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].isSectionHeader && m_items[i].section == letter) {
            return i;
        }
    }
    return -1;
}

void ContactListModel::setAlias(const QString& clientId, const QString& alias)
{
    if (alias.isEmpty()) {
        m_aliases.remove(clientId);
    } else {
        m_aliases[clientId] = alias;
    }
    saveAliases();
    rebuildDisplayList();
}

QString ContactListModel::aliasFor(const QString& clientId) const
{
    return m_aliases.value(clientId);
}

void ContactListModel::loadAliases()
{
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    cfg.beginGroup(QStringLiteral("contacts/aliases"));
    const QStringList keys = cfg.childKeys();
    for (const auto& key : keys) {
        m_aliases[key] = cfg.value(key).toString();
    }
    cfg.endGroup();
}

void ContactListModel::saveAliases()
{
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    cfg.beginGroup(QStringLiteral("contacts/aliases"));
    cfg.remove(QString()); // 清空旧数据
    for (auto it = m_aliases.constBegin(); it != m_aliases.constEnd(); ++it) {
        cfg.setValue(it.key(), it.value());
    }
    cfg.endGroup();
}
