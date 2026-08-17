#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "domain/ReminderItem.h"

class ReminderListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        ReminderIdRole = Qt::UserRole + 1,
        TitleRole,
        PreviewRole,
        DueTimeRole,
        StateRole,
        TargetTypeRole
    };

    explicit ReminderListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<ReminderItem> items);
    ReminderItem itemAt(int row) const;

private:
    QVector<ReminderItem> m_items;
};
