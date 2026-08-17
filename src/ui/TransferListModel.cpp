// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增138行/修改0行/删除0行; 总行数138行
// @AI-LastModified: 2026-04-15 13:11:09

#include "ui/TransferListModel.h"

#include <QFileInfo>

TransferListModel::TransferListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int TransferListModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant TransferListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const auto& item = m_items.at(index.row());
    if (role == Qt::DisplayRole) {
        return item.titleText;
    }
    if (role == Qt::ToolTipRole) {
        const QString localPath = item.localFilePath.trimmed().isEmpty()
                                      ? QStringLiteral("\u672A\u843D\u5730")
                                      : item.localFilePath.trimmed();
        return QStringLiteral("%1\n%2\n%3")
            .arg(item.titleText,
                 item.statusText,
                 QStringLiteral("%1\n%2").arg(item.detailText, localPath));
    }
    if (role == TaskIdRole) {
        return item.taskId;
    }
    if (role == StatusTextRole) {
        return item.statusText;
    }
    if (role == DetailTextRole) {
        return item.detailText;
    }
    if (role == PeerLabelRole) {
        return item.peerLabel;
    }
    if (role == FileBadgeRole) {
        const QString suffix = QFileInfo(item.titleText).suffix().trimmed().toUpper();
        return suffix.isEmpty() ? QStringLiteral("FILE") : suffix.left(4);
    }
    if (role == DirectionRole) {
        return static_cast<int>(item.direction);
    }
    if (role == StateRole) {
        return static_cast<int>(item.state);
    }
    if (role == LocalFilePathRole) {
        return item.localFilePath;
    }
    if (role == OpenableRole) {
        return item.openable;
    }
    if (role == RevealableRole) {
        return item.revealable;
    }
    if (role == RetryableRole) {
        return item.retryable;
    }
    return {};
}

QHash<int, QByteArray> TransferListModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles.insert(TaskIdRole, "taskId");
    roles.insert(StatusTextRole, "statusText");
    roles.insert(DetailTextRole, "detailText");
    roles.insert(PeerLabelRole, "peerLabel");
    roles.insert(FileBadgeRole, "fileBadge");
    roles.insert(DirectionRole, "direction");
    roles.insert(StateRole, "state");
    roles.insert(LocalFilePathRole, "localFilePath");
    roles.insert(OpenableRole, "openable");
    roles.insert(RevealableRole, "revealable");
    roles.insert(RetryableRole, "retryable");
    return roles;
}

void TransferListModel::setItems(QVector<TransferListItem> items) {
    m_sourceItems = std::move(items);
    rebuildVisibleItems();
}

void TransferListModel::setFilter(TransferListFilter filter) {
    if (m_filter == filter) {
        return;
    }
    m_filter = filter;
    rebuildVisibleItems();
}

TransferListFilter TransferListModel::filter() const {
    return m_filter;
}

void TransferListModel::rebuildVisibleItems() {
    QVector<TransferListItem> newItems;
    newItems.reserve(m_sourceItems.size());
    for (const auto& item : m_sourceItems) {
        if (matchesFilter(item, m_filter)) {
            newItems.push_back(item);
        }
    }
    // 数据不变时跳过 model reset，避免传输列表闪烁
    if (newItems.size() == m_items.size()) {
        bool same = true;
        for (int i = 0, n = newItems.size(); i < n; ++i) {
            const auto& a = newItems[i];
            const auto& b = m_items[i];
            if (a.taskId != b.taskId
                || a.state != b.state
                || a.statusText != b.statusText
                || a.detailText != b.detailText) {
                same = false;
                break;
            }
        }
        if (same) return;
    }
    beginResetModel();
    m_items = std::move(newItems);
    endResetModel();
}

bool TransferListModel::matchesFilter(const TransferListItem& item, TransferListFilter filter) {
    switch (filter) {
    case TransferListFilter::All:
        return true;
    case TransferListFilter::OutgoingOnly:
        return item.direction == FileTransferDirection::Outgoing;
    case TransferListFilter::IncomingOnly:
        return item.direction == FileTransferDirection::Incoming;
    case TransferListFilter::ActiveOnly:
        return item.state != FileTransferState::Completed
            && item.state != FileTransferState::Failed
            && item.state != FileTransferState::Canceled;
    case TransferListFilter::FailedOnly:
        return item.state == FileTransferState::Failed
            || item.state == FileTransferState::Interrupted
            || item.state == FileTransferState::Canceled;
    case TransferListFilter::CompletedOnly:
        return item.state == FileTransferState::Completed;
    }

    return true;
}
