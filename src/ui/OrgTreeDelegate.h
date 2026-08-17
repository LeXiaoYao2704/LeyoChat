#pragma once

#include <QStyledItemDelegate>

class OrgTreeDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    // 自定义数据角色
    enum DataRole {
        ClientIdRole = Qt::UserRole + 1,
        DisplayNameRole = Qt::UserRole + 2,
        JobTitleRole = Qt::UserRole + 3,
        IsOnlineRole = Qt::UserRole + 4,
    };

    explicit OrgTreeDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};
