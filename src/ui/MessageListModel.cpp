#include "ui/MessageListModel.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QString>

#include "store/ChatDataStore.h"
#include "ui/MessageDeliveryPresentation.h"

namespace {
QString deliveryStateLabel(MessageDeliveryState state) {
    return messageDeliveryStateText(state);
}

QString displaySender(const ChatMessage& item,
                      bool outgoing,
                      const QString& peerClientId,
                      const QString& peerDisplayName,
                      const QHash<QString, QString>& groupMemberNames) {
    const QString senderId = QString::fromStdWString(item.senderId);
    if (outgoing) {
        return QStringLiteral("我");
    }
    if (!groupMemberNames.isEmpty()) {
        const auto it = groupMemberNames.constFind(senderId);
        if (it != groupMemberNames.constEnd()) {
            return it.value();
        }
        return senderId;
    }
    if (!peerClientId.trimmed().isEmpty() && senderId == peerClientId) {
        return peerDisplayName.trimmed().isEmpty() ? senderId : peerDisplayName.trimmed();
    }
    if (!peerDisplayName.trimmed().isEmpty()) {
        return peerDisplayName.trimmed();
    }

    return senderId;
}

QString displayTime(const ChatMessage& item) {
    const QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(item.createdAtMs);
    if (!timestamp.isValid()) {
        return QStringLiteral("--:--");
    }
    const QString time = timestamp.toString(QStringLiteral("HH:mm"));
    const QDate date = timestamp.date();
    const QDate today = QDate::currentDate();
    if (date == today) {
        return time;
    }
    if (date == today.addDays(-1)) {
        return QStringLiteral("\u6628\u5929 %1").arg(time);
    }
    if (date.year() == today.year()) {
        return QStringLiteral("%1\u6708%2\u65E5 %3").arg(date.month()).arg(date.day()).arg(time);
    }
    return QStringLiteral("%1\u5E74%2\u6708%3\u65E5 %4").arg(date.year()).arg(date.month()).arg(date.day()).arg(time);
}

QString resolvedLocalFilePath(const ChatMessage& item, bool outgoing)
{
    const QString directPath = QString::fromStdWString(item.localFilePath).trimmed();
    if (!directPath.isEmpty()) {
        return directPath;
    }

    const QString fileCardJson = QString::fromStdWString(item.fileCardJson).trimmed();
    if (fileCardJson.isEmpty()) {
        return {};
    }

    const QJsonObject cardObj = QJsonDocument::fromJson(fileCardJson.toUtf8()).object();
    const QString localPath = cardObj.value(QStringLiteral("local_path")).toString().trimmed();
    if (!localPath.isEmpty()) {
        return localPath;
    }

    if (outgoing) {
        return cardObj.value(QStringLiteral("sender_file_path")).toString().trimmed();
    }

    return {};
}
}

MessageListModel::MessageListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int MessageListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

bool MessageListModel::isOutgoingMessage(const ChatMessage& item) const {
    const QString senderId = QString::fromStdWString(item.senderId).trimmed();
    return !senderId.isEmpty()
        && !m_localClientId.isEmpty()
        && senderId == m_localClientId;
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return {};
    }

    const auto& item = m_items.at(static_cast<std::size_t>(index.row()));
    const QString messageId = QString::fromStdWString(item.messageId);
    const bool outgoing = isOutgoingMessage(item);
    const QString senderId = QString::fromStdWString(item.senderId).trimmed();
    const auto transferIt = m_transferStates.constFind(messageId);
    const bool hasTransferState = transferIt != m_transferStates.constEnd();

    if (role == MessageIdRole) {
        return messageId;
    }
    if (role == Qt::ToolTipRole) {
        return {};
    }
    if (role == DeliveryStateRole) {
        return static_cast<int>(item.deliveryState);
    }
    if (role == OutgoingRole) {
        return outgoing;
    }
    if (role == FileMessageRole) {
        return !QString::fromStdWString(item.attachmentName).trimmed().isEmpty()
               || !QString::fromStdWString(item.localFilePath).trimmed().isEmpty();
    }
    if (role == AttachmentNameRole) {
        return QString::fromStdWString(item.attachmentName);
    }
    if (role == LocalFilePathRole) {
        return resolvedLocalFilePath(item, outgoing);
    }
    if (role == BodyRole) {
        return QString::fromStdWString(item.body);
    }
    if (role == MessageTypeRole) {
        return QString::fromStdWString(item.messageType);
    }
    if (role == PayloadJsonRole) {
        return QString::fromStdWString(item.payloadJson);
    }
    if (role == ResourceReferenceRole) {
        return QString::fromStdWString(item.messageType).trimmed() == QStringLiteral("resource_ref");
    }
    if (role == ForwardPackageRole) {
        return QString::fromStdWString(item.messageType).trimmed() == QStringLiteral("forward_package");
    }
    if (role == ReactionsJsonRole) {
        return QString::fromStdWString(item.reactionsJson);
    }
    if (role == SenderNameRole) {
        return displaySender(item,
                             outgoing,
                             m_peerClientId,
                             m_peerDisplayName,
                             m_groupMemberNames);
    }
    if (role == SenderAvatarPathRole) {
        if (outgoing) {
            return m_localAvatarPath;
        }
        if (!m_groupMemberAvatars.isEmpty()) {
            return m_groupMemberAvatars.value(senderId);
        }
        if (!m_peerClientId.trimmed().isEmpty() && senderId == m_peerClientId) {
            return m_peerAvatarPath;
        }
        return QString();
    }
    if (role == TimeLabelRole) {
        return displayTime(item);
    }
    if (role == TransferTaskIdRole) {
        return hasTransferState ? transferIt->taskId : QString();
    }
    if (role == TransferStateRole) {
        return hasTransferState ? static_cast<int>(transferIt->state) : QVariant();
    }
    if (role == TransferBytesCompletedRole) {
        return hasTransferState ? transferIt->bytesCompleted : QVariant();
    }
    if (role == TransferFileSizeRole) {
        return hasTransferState ? transferIt->fileSize : QVariant();
    }
    if (role == TransferCancelableRole) {
        return hasTransferState ? transferIt->cancelable : false;
    }
    if (role == TransferSpeedRole) {
        return hasTransferState ? transferIt->speedBytesPerSec : 0LL;
    }
    if (role == GroupReadCountRole) {
        return item.groupReadCount;
    }
    if (role == GroupActiveMemberCountRole) {
        return m_groupActiveMemberCount;
    }
    if (role == RecalledRole) {
        return item.isRecalled;
    }
    if (role == EditedRole) {
        return item.editedAtMs > 0;
    }
    if (role == EditedAtRole) {
        return item.editedAtMs;
    }
    if (role == LastEditorIdRole) {
        return QString::fromStdWString(item.lastEditorId);
    }
    if (role == CreatedAtRole) {
        return item.createdAtMs;
    }
    if (role == ShowDateSeparatorRole) {
        const int row = index.row();
        if (row == 0) {
            return true;
        }
        const QDate currentDate = QDateTime::fromMSecsSinceEpoch(item.createdAtMs).date();
        const QDate prevDate = QDateTime::fromMSecsSinceEpoch(m_items[static_cast<std::size_t>(row - 1)].createdAtMs).date();
        return currentDate != prevDate;
    }
    if (role == DateLabelRole) {
        const QDate date = QDateTime::fromMSecsSinceEpoch(item.createdAtMs).date();
        const QDate today = QDate::currentDate();
        if (date == today) {
            return QStringLiteral("\u4ECA\u5929");
        }
        if (date == today.addDays(-1)) {
            return QStringLiteral("\u6628\u5929");
        }
        if (date.year() == today.year()) {
            return QStringLiteral("%1\u6708%2\u65E5").arg(date.month()).arg(date.day());
        }
        return QStringLiteral("%1\u5E74%2\u6708%3\u65E5").arg(date.year()).arg(date.month()).arg(date.day());
    }
    if (role == ReplyToMessageIdRole) {
        return QString::fromStdWString(item.replyToMessageId);
    }
    if (role == ReplyToSenderNameRole) {
        const QString replyToSenderId = QString::fromStdWString(item.replyToSenderId);
        if (replyToSenderId.isEmpty()) {
            return QString();
        }
        if (!m_groupMemberNames.isEmpty()) {
            return m_groupMemberNames.value(replyToSenderId, replyToSenderId);
        }
        if (!m_peerClientId.trimmed().isEmpty() && replyToSenderId == m_peerClientId) {
            return m_peerDisplayName.trimmed().isEmpty() ? replyToSenderId : m_peerDisplayName.trimmed();
        }
        if (replyToSenderId == m_localClientId) {
            return QStringLiteral("\u6211");
        }
        return replyToSenderId;
    }
    if (role == ReplyToBodyRole) {
        return QString::fromStdWString(item.replyToBody);
    }
    if (role == SenderIdRole) {
        return QString::fromStdWString(item.senderId);
    }
    if (role == FileCardJsonRole) {
        return QString::fromStdWString(item.fileCardJson);
    }
    if (role != Qt::DisplayRole) {
        return {};
    }

    const QString sender = displaySender(item,
                                         outgoing,
                                         m_peerClientId,
                                         m_peerDisplayName,
                                         m_groupMemberNames);
    const QString body = QString::fromStdWString(item.body);
    if (outgoing) {
        return QStringLiteral("[%1] %2: %3 (%4)")
            .arg(displayTime(item), sender, body, deliveryStateLabel(item.deliveryState));
    }

    return QStringLiteral("[%1] %2: %3").arg(displayTime(item), sender, body);
}

QHash<int, QByteArray> MessageListModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles.insert(MessageIdRole, "messageId");
    roles.insert(DeliveryStateRole, "deliveryState");
    roles.insert(OutgoingRole, "outgoing");
    roles.insert(FileMessageRole, "fileMessage");
    roles.insert(AttachmentNameRole, "attachmentName");
    roles.insert(LocalFilePathRole, "localFilePath");
    roles.insert(BodyRole, "body");
    roles.insert(MessageTypeRole, "messageType");
    roles.insert(PayloadJsonRole, "payloadJson");
    roles.insert(ResourceReferenceRole, "resourceReference");
    roles.insert(SenderNameRole, "senderName");
    roles.insert(SenderAvatarPathRole, "senderAvatarPath");
    roles.insert(TimeLabelRole, "timeLabel");
    roles.insert(TransferTaskIdRole, "transferTaskId");
    roles.insert(TransferStateRole, "transferState");
    roles.insert(TransferBytesCompletedRole, "transferBytesCompleted");
    roles.insert(TransferFileSizeRole, "transferFileSize");
    roles.insert(TransferCancelableRole, "transferCancelable");
    roles.insert(TransferSpeedRole, "transferSpeed");
    roles.insert(GroupReadCountRole, "groupReadCount");
    roles.insert(GroupActiveMemberCountRole, "groupActiveMemberCount");
    roles.insert(RecalledRole,       "recalled");
    roles.insert(EditedRole,         "edited");
    roles.insert(EditedAtRole,       "editedAt");
    roles.insert(LastEditorIdRole,   "lastEditorId");
    roles.insert(CreatedAtRole,      "createdAt");
    roles.insert(ShowDateSeparatorRole, "showDateSeparator");
    roles.insert(DateLabelRole,        "dateLabel");
    roles.insert(ReplyToMessageIdRole,   "replyToMessageId");
    roles.insert(ReplyToSenderNameRole,  "replyToSenderName");
    roles.insert(ReplyToBodyRole,        "replyToBody");
    roles.insert(SenderIdRole,             "senderId");
    roles.insert(FileCardJsonRole,          "fileCardJson");
    roles.insert(ForwardPackageRole,        "forwardPackage");
    return roles;
}

void MessageListModel::setDisplayContext(const QString& localClientId,
                                         const QString& peerDisplayName,
                                         const QString& peerClientId) {
    const QString nextLocalClientId = localClientId.trimmed();
    const QString nextPeerDisplayName = peerDisplayName.trimmed();
    const QString nextPeerClientId = peerClientId.trimmed();
    if (m_localClientId == nextLocalClientId
        && m_peerDisplayName == nextPeerDisplayName
        && m_peerClientId == nextPeerClientId) {
        return;
    }

    m_localClientId = nextLocalClientId;
    m_peerDisplayName = nextPeerDisplayName;
    m_peerClientId = nextPeerClientId;
    m_groupMemberNames.clear();
    m_groupMemberAvatars.clear();
    m_contextDirty = true;
}

void MessageListModel::setAvatarContext(const QString& localAvatarPath, const QString& peerAvatarPath) {
    m_localAvatarPath = localAvatarPath.trimmed();
    m_peerAvatarPath = peerAvatarPath.trimmed();
}

void MessageListModel::setGroupMemberNames(const QHash<QString, QString>& names) {
    if (m_groupMemberNames == names) return;
    m_groupMemberNames = names;
    if (!m_bulkContextUpdate && !m_items.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1),
                         {SenderNameRole, ReplyToSenderNameRole});
    }
}

QString MessageListModel::displayNameForClientId(const QString& clientId) const {
    if (clientId == m_localClientId) return QStringLiteral("\u6211");
    if (!m_groupMemberNames.isEmpty()) {
        const auto it = m_groupMemberNames.constFind(clientId);
        if (it != m_groupMemberNames.constEnd()) return it.value();
    }
    if (!m_peerClientId.isEmpty() && clientId == m_peerClientId)
        return m_peerDisplayName.isEmpty() ? clientId : m_peerDisplayName;
    return clientId;
}

void MessageListModel::setGroupMemberAvatars(const QHash<QString, QString>& avatars) {
    if (m_groupMemberAvatars == avatars) return;
    m_groupMemberAvatars = avatars;
    if (!m_bulkContextUpdate && !m_items.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1),
                         {SenderAvatarPathRole});
    }
}

void MessageListModel::setGroupActiveMemberCount(int count) {
    if (m_groupActiveMemberCount == count) {
        return;
    }
    m_groupActiveMemberCount = count;
    if (!m_bulkContextUpdate && !m_items.empty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1),
                         {GroupActiveMemberCountRole});
    }
}

void MessageListModel::setTransferStates(const QHash<QString, TransferVisualState>& states) {
    if (m_transferStates == states) return;
    // 检测是否有传输任务刚完成（需要触发 sizeHint 重新计算，否则图片气泡高度不更新导致重叠）
    bool hasNewCompletion = false;
    for (auto it = states.constBegin(); it != states.constEnd(); ++it) {
        if (it.value().state == FileTransferState::Completed) {
            const auto prev = m_transferStates.constFind(it.key());
            if (prev == m_transferStates.constEnd()
                || prev.value().state != FileTransferState::Completed) {
                hasNewCompletion = true;
                break;
            }
        }
    }
    m_transferStates = states;
    if (!m_bulkContextUpdate && !m_items.empty()) {
        QVector<int> roles = {TransferStateRole, TransferBytesCompletedRole,
                              TransferFileSizeRole, TransferCancelableRole, TransferSpeedRole};
        if (hasNewCompletion) {
            roles.append(Qt::SizeHintRole);
        }
        emit dataChanged(index(0), index(static_cast<int>(m_items.size()) - 1), roles);
    }
}

void MessageListModel::setItems(std::vector<ChatMessage> items) {
    // 显示上下文变化时强制重刷，防止消息归属缓存失效
    const bool forceReset = m_contextDirty;
    m_contextDirty = false;

    // 数据不变时跳过 model reset，避免消息列表闪烁
    if (!forceReset && items.size() == m_items.size() && !items.empty()) {
        bool same = true;
        for (size_t i = 0, n = items.size(); i < n; ++i) {
            const auto& a = items[i];
            const auto& b = m_items[i];
            if (a.messageId != b.messageId
                || a.body != b.body
                || a.deliveryState != b.deliveryState
                || a.createdAtMs != b.createdAtMs
                || a.isRecalled != b.isRecalled
                || a.editedAtMs != b.editedAtMs
                || a.groupReadCount != b.groupReadCount
                || a.localFilePath != b.localFilePath
                || a.fileCardJson != b.fileCardJson) {
                same = false;
                break;
            }
        }
        if (same) return;
    }
    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

QHash<QString, QString> MessageListModel::senderDisplayNameMap() const
{
    QHash<QString, QString> names;
    for (const ChatMessage& item : m_items) {
        const QString senderId = QString::fromStdWString(item.senderId).trimmed();
        if (senderId.isEmpty()) {
            continue;
        }
        const QString displayName = displaySender(item,
                                                  isOutgoingMessage(item),
                                                  m_peerClientId,
                                                  m_peerDisplayName,
                                                  m_groupMemberNames).trimmed();
        if (!displayName.isEmpty()) {
            names.insert(senderId, displayName);
        }
    }
    return names;
}

int MessageListModel::findRowByMessageId(const QString& messageId) const {
    if (messageId.isEmpty()) return -1;
    const std::wstring wid = messageId.toStdWString();
    for (size_t i = 0, n = m_items.size(); i < n; ++i) {
        if (m_items[i].messageId == wid) return static_cast<int>(i);
    }
    return -1;
}

bool MessageListModel::hasMoreMessagesBefore() const {
    return m_store && m_store->hasMoreMessagesBefore(m_activeConversationId);
}

QString MessageListModel::firstMessageId() const {
    if (!m_items.empty()) {
        return QString::fromStdWString(m_items.front().messageId);
    }
    return m_store ? m_store->firstMessageId(m_activeConversationId) : QString();
}

void MessageListModel::bindToStore(ChatDataStore* store) {
    if (m_store) {
        disconnect(m_store, nullptr, this, nullptr);
    }
    m_store = store;
    if (m_store) {
        connect(m_store, &ChatDataStore::messageAppended,
                this, &MessageListModel::onMessageAppended);
        connect(m_store, &ChatDataStore::messagesPrepended,
                this, &MessageListModel::onMessagesPrepended);
        connect(m_store, &ChatDataStore::messageUpdated,
                this, &MessageListModel::onMessageUpdated);
        connect(m_store, &ChatDataStore::messagesReset,
                this, &MessageListModel::onMessagesReset);
    }
}

void MessageListModel::beginBulkContextUpdate() {
    m_bulkContextUpdate = true;
}

void MessageListModel::endBulkContextUpdate() {
    m_bulkContextUpdate = false;
    // 注意：如果紧接着调用 switchToConversation (beginResetModel/endResetModel),
    // 这里的 dataChanged 对旧数据是多余的，但不会造成问题（reset 会覆盖）。
    // 如果不接 switch（例如仅更新上下文），则需要通知刷新。
}

void MessageListModel::switchToConversation(const QString& conversationId) {
    if (!m_store) {
        m_activeConversationId = conversationId;
        beginResetModel();
        m_items.clear();
        endResetModel();
        return;
    }
    const auto& nextItems = m_store->messages(conversationId);
    // 同一会话且数据未变时跳过 reset，避免图片闪烁
    if (conversationId == m_activeConversationId && !m_contextDirty
        && nextItems.size() == m_items.size()) {
        bool same = true;
        for (size_t i = 0, n = nextItems.size(); i < n; ++i) {
            if (nextItems[i].messageId != m_items[i].messageId
                || nextItems[i].body != m_items[i].body
                || nextItems[i].deliveryState != m_items[i].deliveryState) {
                same = false;
                break;
            }
        }
        if (same) return;
    }
    m_activeConversationId = conversationId;
    beginResetModel();
    m_items = nextItems;
    m_contextDirty = false;
    endResetModel();
}

void MessageListModel::onMessageAppended(const QString& conversationId, int newIndex) {
    if (conversationId != m_activeConversationId || !m_store) return;
    const auto& storeMessages = m_store->messages(conversationId);
    if (newIndex < 0 || newIndex >= static_cast<int>(storeMessages.size())) return;
    beginInsertRows(QModelIndex(), newIndex, newIndex);
    m_items.insert(m_items.begin() + newIndex, storeMessages[newIndex]);
    endInsertRows();
}

void MessageListModel::onMessagesPrepended(const QString& conversationId, int count) {
    if (conversationId != m_activeConversationId || !m_store || count <= 0) return;
    const auto& storeMessages = m_store->messages(conversationId);
    if (count > static_cast<int>(storeMessages.size())) return;
    beginInsertRows(QModelIndex(), 0, count - 1);
    m_items.insert(m_items.begin(), storeMessages.begin(), storeMessages.begin() + count);
    endInsertRows();
}

void MessageListModel::onMessageUpdated(const QString& conversationId, const QString& messageId) {
    if (conversationId != m_activeConversationId || !m_store) return;
    const std::wstring wid = messageId.toStdWString();
    for (int i = 0, n = static_cast<int>(m_items.size()); i < n; ++i) {
        if (m_items[i].messageId == wid) {
            const auto& storeMessages = m_store->messages(conversationId);
            if (i < static_cast<int>(storeMessages.size()) && storeMessages[i].messageId == wid) {
                const ChatMessage previous = m_items[i];
                const ChatMessage& updated = storeMessages[i];
                const bool sizeAffectingChange =
                    previous.localFilePath != updated.localFilePath
                    || previous.attachmentName != updated.attachmentName
                    || previous.fileCardJson != updated.fileCardJson
                    || previous.messageType != updated.messageType;
                m_items[i] = updated;
                emit dataChanged(index(i), index(i));
                if (sizeAffectingChange) {
                    emit dataChanged(index(i), index(i), {Qt::SizeHintRole});
                }
                return;
            }
            emit dataChanged(index(i), index(i));
            return;
        }
    }
}

void MessageListModel::onMessagesReset(const QString& conversationId) {
    if (conversationId != m_activeConversationId || !m_store) return;
    const auto& nextItems = m_store->messages(conversationId);
    // 数据未变时跳过 reset，避免图片闪烁
    if (nextItems.size() == m_items.size() && !nextItems.empty()) {
        bool same = true;
        for (size_t i = 0, n = nextItems.size(); i < n; ++i) {
            if (nextItems[i].messageId != m_items[i].messageId
                || nextItems[i].body != m_items[i].body
                || nextItems[i].deliveryState != m_items[i].deliveryState
                || nextItems[i].localFilePath != m_items[i].localFilePath
                || nextItems[i].isRecalled != m_items[i].isRecalled
                || nextItems[i].editedAtMs != m_items[i].editedAtMs
                || nextItems[i].reactionsJson != m_items[i].reactionsJson) {
                same = false;
                break;
            }
        }
        if (same) return;
    }
    beginResetModel();
    m_items = nextItems;
    endResetModel();
}
