#include "ui/ConversationListModel.h"

#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QString>
#include <QTextDocument>
#include "store/ChatDataStore.h"
#include "ui/PinyinHelper.h"

namespace {
QString stripHtml(const QString& html) {
    if (!html.contains(QLatin1Char('<'))) {
        return html;
    }
    QTextDocument doc;
    doc.setHtml(html);
    return doc.toPlainText();
}

bool looksLikeImageSuffix(const QString& name) {
    const QString suffix = QFileInfo(name).suffix().toLower();
    return suffix == QLatin1String("png") || suffix == QLatin1String("jpg")
        || suffix == QLatin1String("jpeg") || suffix == QLatin1String("gif")
        || suffix == QLatin1String("bmp") || suffix == QLatin1String("webp")
        || suffix == QLatin1String("svg") || suffix == QLatin1String("tiff")
        || suffix == QLatin1String("ico");
}

QString simplifyFilePreview(const QString& text) {
    // 兼容旧数据：[File] xxx 或 [图片] xxx → 简化为 [文件] 或 [图片]
    if (text.startsWith(QStringLiteral("[\u56FE\u7247]"), Qt::CaseInsensitive)) {
        return QStringLiteral("[\u56FE\u7247]");
    }
    if (text.startsWith(QStringLiteral("[File]"), Qt::CaseInsensitive)) {
        // 检查文件名是否像图片
        const QString fileName = text.mid(6).trimmed();
        if (!fileName.isEmpty() && looksLikeImageSuffix(fileName)) {
            return QStringLiteral("[\u56FE\u7247]");
        }
        return QStringLiteral("[\u6587\u4EF6]");
    }
    return text;
}

QString titleText(const ConversationSummary& item) {
    const QString title = QString::fromStdWString(item.title).trimmed();
    return title.isEmpty() ? QStringLiteral("\u672A\u547D\u540D\u4F1A\u8BDD") : title;
}

QString previewText(const ConversationSummary& item) {
    const QString raw = QString::fromStdWString(item.lastMessagePreview).trimmed();
    const QString preview = simplifyFilePreview(stripHtml(raw).trimmed());
    return preview.isEmpty() ? QStringLiteral("\u6682\u65E0\u6700\u65B0\u6D88\u606F") : preview;
}

bool hasMeaningfulPreview(const ConversationSummary& item) {
    const QString raw = QString::fromStdWString(item.lastMessagePreview).trimmed();
    const QString preview = stripHtml(raw).trimmed();
    return !preview.isEmpty();
}

QString timeLabel(const ConversationSummary& item) {
    if (item.lastMessageAtMs <= 0 || !hasMeaningfulPreview(item)) {
        return {};
    }

    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(item.lastMessageAtMs);
    if (!timestamp.isValid()) {
        return {};
    }

    if (timestamp.date() == QDate::currentDate()) {
        return timestamp.toString(QStringLiteral("HH:mm"));
    }
    return timestamp.toString(QStringLiteral("MM-dd"));
}

bool isGroupConversationSnapshot(const ConversationSummary& item) {
    const QString id = QString::fromStdWString(item.conversationId);
    return !id.contains(QLatin1Char('|'));
}

bool passesFilterSnapshot(const ConversationSummary& item,
                          ConversationListModel::Filter filter,
                          const QString& searchText,
                          const QSet<QString>& unreadConversationIds) {
    const QString id = QString::fromStdWString(item.conversationId);
    const bool hasUnread = unreadConversationIds.contains(id) || item.isManuallyUnread;
    const QString needle = searchText.trimmed();
    if (!needle.isEmpty()) {
        const QString title = titleText(item);
        const QString preview = previewText(item);
        const QString time = timeLabel(item);
        if (!title.contains(needle, Qt::CaseInsensitive)
            && !preview.contains(needle, Qt::CaseInsensitive)
            && !time.contains(needle, Qt::CaseInsensitive)
            && !id.contains(needle, Qt::CaseInsensitive)
            && !PinyinHelper::matchesPinyin(title, needle)) {
            return false;
        }
    }

    switch (filter) {
    case ConversationListModel::Filter::All:
        return !item.isDone;
    case ConversationListModel::Filter::Unread:
        return !item.isDone && hasUnread;
    case ConversationListModel::Filter::Starred:
        return item.isStarred;
    case ConversationListModel::Filter::AtMe:
        return !item.isDone && item.hasMentionMe;
    case ConversationListModel::Filter::Tagged:
        return !item.isDone;
    case ConversationListModel::Filter::Direct:
        return !item.isDone && !isGroupConversationSnapshot(item);
    case ConversationListModel::Filter::Group:
        return isGroupConversationSnapshot(item);
    case ConversationListModel::Filter::Done:
        return item.isDone;
    }
    return !item.isDone;
}

int filteredCountSnapshot(const QVector<ConversationSummary>& items,
                         ConversationListModel::Filter filter,
                         const QString& searchText,
                         const QSet<QString>& unreadConversationIds) {
    int count = 0;
    for (const auto& item : items) {
        if (passesFilterSnapshot(item, filter, searchText, unreadConversationIds)) {
            ++count;
        }
    }
    return count;
}
} // namespace

ConversationListModel::ConversationListModel(QObject* parent)
    : QAbstractListModel(parent) {}

// Static helper: group conversations have UUID format (no pipe '|' separator)
bool ConversationListModel::isGroupConversation(const ConversationSummary& item) {
    const QString id = QString::fromStdWString(item.conversationId);
    return !id.contains(QLatin1Char('|'));
}

bool ConversationListModel::passesFilter(const ConversationSummary& item) const {
    const QString id = QString::fromStdWString(item.conversationId);
    const bool hasUnread = m_unreadConversationIds.contains(id) || item.isManuallyUnread;
    const QString needle = m_searchText.trimmed();
    if (!needle.isEmpty()) {
        const QString title = titleText(item);
        const QString preview = previewText(item);
        const QString time = timeLabel(item);
        if (!title.contains(needle, Qt::CaseInsensitive)
            && !preview.contains(needle, Qt::CaseInsensitive)
            && !time.contains(needle, Qt::CaseInsensitive)
            && !id.contains(needle, Qt::CaseInsensitive)
            && !PinyinHelper::matchesPinyin(title, needle)) {
            return false;
        }
    }

    switch (m_filter) {
    case Filter::All:
        return !item.isDone;
    case Filter::Unread:
        return !item.isDone && hasUnread;
    case Filter::Starred:
        return item.isStarred;
    case Filter::AtMe:
        return !item.isDone && item.hasMentionMe;
    case Filter::Tagged:
        return !item.isDone; // placeholder
    case Filter::Direct:
        return !item.isDone && !isGroupConversation(item);
    case Filter::Group:
        // 群聊工作区需要持续显示群本身；在消息工作区“关闭会话”后，
        // 该群只应从消息列表收起，不应从专门的群聊列表里消失。
        return isGroupConversation(item);
    case Filter::Done:
        return item.isDone;
    }
    return !item.isDone;
}

int ConversationListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_filteredIndices.size();
}

QVariant ConversationListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_filteredIndices.size()) {
        return {};
    }

    const ConversationSummary* found = &m_items[m_filteredIndices[index.row()]];

    const auto& item = *found;
    const QString conversationId = QString::fromStdWString(item.conversationId);
    const bool hasUnread = m_unreadConversationIds.contains(conversationId) || item.isManuallyUnread;

    if (role == Qt::DisplayRole) {
        const QString title = titleText(item);
        const QString preview = previewText(item);
        const QString time = timeLabel(item);
        const QString displayTitle = hasUnread ? QStringLiteral("\u2022 %1").arg(title) : title;
        if (time.isEmpty()) {
            return QStringLiteral("%1\n%2").arg(displayTitle, preview);
        }
        return QStringLiteral("%1    %2\n%3").arg(displayTitle, time, preview);
    }
    if (role == Qt::ToolTipRole) {
        const QString title = titleText(item);
        const QString preview = previewText(item);
        if (preview.isEmpty()) {
            return title;
        }
        return QStringLiteral("%1\n%2").arg(title, preview);
    }
    if (role == ConversationIdRole) {
        return conversationId;
    }
    if (role == TitleRole) {
        return titleText(item);
    }
    if (role == PreviewRole) {
        return previewText(item);
    }
    if (role == TimeLabelRole) {
        return timeLabel(item);
    }
    if (role == HasUnreadRole) {
        return hasUnread;
    }
    if (role == IsPinnedRole) {
        return item.isPinned;
    }
    if (role == IsStarredRole) {
        return item.isStarred;
    }
    if (role == IsMutedRole) {
        return item.isMuted;
    }
    if (role == IsDoneRole) {
        return item.isDone;
    }
    if (role == HasMentionMeRole) {
        return item.hasMentionMe;
    }
    if (role == AvatarPathRole) {
        return m_avatarPaths.value(conversationId);
    }
    if (role == DraftTextRole) {
        return m_draftTexts.value(conversationId);
    }
    if (role == IsOnlineRole) {
        return m_onlinePeerIds.contains(conversationId);
    }
    return {};
}

QHash<int, QByteArray> ConversationListModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles.insert(ConversationIdRole, "conversationId");
    roles.insert(TitleRole, "title");
    roles.insert(PreviewRole, "preview");
    roles.insert(TimeLabelRole, "timeLabel");
    roles.insert(HasUnreadRole, "hasUnread");
    roles.insert(IsPinnedRole, "isPinned");
    roles.insert(IsStarredRole, "isStarred");
    roles.insert(IsMutedRole, "isMuted");
    roles.insert(IsDoneRole, "isDone");
    roles.insert(HasMentionMeRole, "hasMentionMe");
    roles.insert(AvatarPathRole, "avatarPath");
    roles.insert(DraftTextRole, "draftText");
    roles.insert(IsOnlineRole, "isOnline");
    return roles;
}

void ConversationListModel::setItems(QVector<ConversationSummary> items) {
    if (items.size() == m_items.size()) {
        bool same = true;
        for (int i = 0, n = items.size(); i < n; ++i) {
            const auto& a = items[i];
            const auto& b = m_items[i];
            if (a.conversationId != b.conversationId
                || a.title != b.title
                || a.lastMessagePreview != b.lastMessagePreview
                || a.lastMessageAtMs != b.lastMessageAtMs
                || a.isPinned != b.isPinned
                || a.isStarred != b.isStarred
                || a.isMuted != b.isMuted
                || a.isDone != b.isDone
                || a.isManuallyUnread != b.isManuallyUnread
                || a.hasMentionMe != b.hasMentionMe) {
                same = false;
                break;
            }
        }
        if (same) return;
    }
    // 当会话 ID 集合与顺序不变时（仅内容更新），用 dataChanged 代替 resetModel，
    // 保留 QListView 的滚动位置和选中状态。
    if (items.size() == m_items.size()) {
        bool sameStructure = true;
        for (int i = 0, n = items.size(); i < n; ++i) {
            if (items[i].conversationId != m_items[i].conversationId) {
                sameStructure = false;
                break;
            }
        }
        const int oldVisibleCount =
            filteredCountSnapshot(m_items, m_filter, m_searchText, m_unreadConversationIds);
        const int newVisibleCount =
            filteredCountSnapshot(items, m_filter, m_searchText, m_unreadConversationIds);
        if (sameStructure && oldVisibleCount == newVisibleCount) {
            m_items = std::move(items);
            rebuildFilteredIndices();
            if (newVisibleCount > 0) {
                emit dataChanged(index(0), index(newVisibleCount - 1));
            }
            return;
        }
    }
    beginResetModel();
    m_items = std::move(items);
    rebuildFilteredIndices();
    endResetModel();
}

void ConversationListModel::setUnreadConversationIds(QSet<QString> conversationIds) {
    if (conversationIds == m_unreadConversationIds) return;
    beginResetModel();
    m_unreadConversationIds = std::move(conversationIds);
    rebuildFilteredIndices();
    endResetModel();
}

void ConversationListModel::setItemsAndUnread(QVector<ConversationSummary> items,
                                              QSet<QString> unreadIds) {
    // 数据完全相同时跳过 model reset，避免 QListView 闪烁
    if (items.size() == m_items.size() && unreadIds == m_unreadConversationIds) {
        bool same = true;
        for (int i = 0, n = items.size(); i < n; ++i) {
            const auto& a = items[i];
            const auto& b = m_items[i];
            if (a.conversationId != b.conversationId
                || a.title != b.title
                || a.lastMessagePreview != b.lastMessagePreview
                || a.lastMessageAtMs != b.lastMessageAtMs
                || a.isPinned != b.isPinned
                || a.isStarred != b.isStarred
                || a.isMuted != b.isMuted
                || a.isDone != b.isDone
                || a.isManuallyUnread != b.isManuallyUnread
                || a.hasMentionMe != b.hasMentionMe) {
                same = false;
                break;
            }
        }
        if (same) return;
    }
    // 当会话 ID 集合与顺序不变时，用 dataChanged 代替 resetModel
    if (items.size() == m_items.size()) {
        bool sameStructure = true;
        for (int i = 0, n = items.size(); i < n; ++i) {
            if (items[i].conversationId != m_items[i].conversationId) {
                sameStructure = false;
                break;
            }
        }
        const int oldVisibleCount =
            filteredCountSnapshot(m_items, m_filter, m_searchText, m_unreadConversationIds);
        const int newVisibleCount =
            filteredCountSnapshot(items, m_filter, m_searchText, unreadIds);
        if (sameStructure && oldVisibleCount == newVisibleCount) {
            m_items = std::move(items);
            m_unreadConversationIds = std::move(unreadIds);
            rebuildFilteredIndices();
            if (newVisibleCount > 0) {
                emit dataChanged(index(0), index(newVisibleCount - 1));
            }
            return;
        }
    }
    beginResetModel();
    m_items = std::move(items);
    m_unreadConversationIds = std::move(unreadIds);
    rebuildFilteredIndices();
    endResetModel();
}

void ConversationListModel::setFilter(int filterIndex) {
    const auto f = static_cast<Filter>(filterIndex);
    if (f == m_filter) {
        return;
    }
    beginResetModel();
    m_filter = f;
    rebuildFilteredIndices();
    endResetModel();
}

void ConversationListModel::setSearchText(const QString& text)
{
    const QString normalized = text.trimmed();
    if (normalized == m_searchText) {
        return;
    }
    beginResetModel();
    m_searchText = normalized;
    rebuildFilteredIndices();
    endResetModel();
}

void ConversationListModel::setAvatarPaths(const QHash<QString, QString>& avatarPaths)
{
    m_avatarPaths = avatarPaths;
    if (!m_items.isEmpty()) {
        emit dataChanged(index(0), index(rowCount() - 1), {AvatarPathRole});
    }
}

void ConversationListModel::setDraftTexts(const QHash<QString, QString>& drafts)
{
    if (drafts == m_draftTexts) return;
    m_draftTexts = drafts;
    if (!m_items.isEmpty()) {
        emit dataChanged(index(0), index(rowCount() - 1), {DraftTextRole});
    }
}

void ConversationListModel::setOnlinePeerIds(const QSet<QString>& onlineIds)
{
    if (onlineIds == m_onlinePeerIds) return;
    m_onlinePeerIds = onlineIds;
    if (!m_items.isEmpty()) {
        emit dataChanged(index(0), index(rowCount() - 1), {IsOnlineRole});
    }
}

int ConversationListModel::totalUnreadCount() const
{
    int count = 0;
    for (const auto& item : m_items) {
        const QString id = QString::fromStdWString(item.conversationId);
        if (m_unreadConversationIds.contains(id) || item.isManuallyUnread) {
            ++count;
        }
    }
    return count;
}

int ConversationListModel::totalGroupUnreadCount() const
{
    int count = 0;
    for (const auto& item : m_items) {
        const QString id = QString::fromStdWString(item.conversationId);
        if (!(m_unreadConversationIds.contains(id) || item.isManuallyUnread)) {
            continue;
        }
        if (isGroupConversation(item)) {
            ++count;
        }
    }
    return count;
}

void ConversationListModel::bindToStore(ChatDataStore* store) {
    if (m_store) disconnect(m_store, nullptr, this, nullptr);
    m_store = store;
    if (m_store) {
        connect(m_store, &ChatDataStore::conversationListChanged,
                this, &ConversationListModel::onConversationListChanged);
        connect(m_store, &ChatDataStore::conversationUpserted,
                this, &ConversationListModel::onConversationUpserted);
        connect(m_store, &ChatDataStore::unreadSetChanged,
                this, &ConversationListModel::onUnreadSetChanged);
    }
}

void ConversationListModel::onConversationListChanged() {
    if (!m_store) return;
    setItemsAndUnread(m_store->allConversations(), m_store->unreadConversationIds());
}

void ConversationListModel::onConversationUpserted(const QString& conversationId) {
    if (!m_store) return;
    Q_UNUSED(conversationId)
    setItemsAndUnread(m_store->allConversations(), m_store->unreadConversationIds());
}

void ConversationListModel::onUnreadSetChanged() {
    if (!m_store) return;
    const auto newUnread = m_store->unreadConversationIds();
    if (newUnread == m_unreadConversationIds) return;

    // Unread filter 等依赖 unread 状态过滤，需要完整 reset
    if (m_filter == Filter::Unread) {
        beginResetModel();
        m_unreadConversationIds = newUnread;
        rebuildFilteredIndices();
        endResetModel();
    } else {
        m_unreadConversationIds = newUnread;
        if (!m_filteredIndices.isEmpty()) {
            emit dataChanged(index(0), index(rowCount() - 1));
        }
    }
}

void ConversationListModel::rebuildFilteredIndices()
{
    m_filteredIndices.clear();
    m_filteredIndices.reserve(m_items.size());
    for (int i = 0, n = m_items.size(); i < n; ++i) {
        if (passesFilter(m_items[i])) {
            m_filteredIndices.append(i);
        }
    }
}
