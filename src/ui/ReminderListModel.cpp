#include "ui/ReminderListModel.h"

#include <algorithm>

ReminderListModel::ReminderListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int ReminderListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_items.size();
}

QVariant ReminderListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const ReminderItem& item = m_items.at(index.row());
    switch (role) {
    case ReminderIdRole:
        return item.reminderId;
    case TitleRole:
        return item.titleSnapshot;
    case PreviewRole:
        return item.previewSnapshot;
    case DueTimeRole:
        return item.dueAtMs;
    case StateRole:
        return item.state;
    case TargetTypeRole:
        return item.targetType;
    default:
        return {};
    }
}

QHash<int, QByteArray> ReminderListModel::roleNames() const
{
    return {
        {ReminderIdRole, QByteArrayLiteral("reminderId")},
        {TitleRole, QByteArrayLiteral("title")},
        {PreviewRole, QByteArrayLiteral("preview")},
        {DueTimeRole, QByteArrayLiteral("dueTime")},
        {StateRole, QByteArrayLiteral("state")},
        {TargetTypeRole, QByteArrayLiteral("targetType")},
    };
}

void ReminderListModel::setItems(QVector<ReminderItem> items)
{
    std::sort(items.begin(), items.end(), [](const ReminderItem& lhs, const ReminderItem& rhs) {
        if (lhs.dueAtMs == rhs.dueAtMs) {
            return lhs.reminderId < rhs.reminderId;
        }
        return lhs.dueAtMs < rhs.dueAtMs;
    });

    beginResetModel();
    m_items = std::move(items);
    endResetModel();
}

ReminderItem ReminderListModel::itemAt(int row) const
{
    if (row < 0 || row >= m_items.size()) {
        return {};
    }
    return m_items.at(row);
}
