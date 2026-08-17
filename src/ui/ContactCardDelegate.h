#pragma once

#include <QStyledItemDelegate>

class ContactCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ContactCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;
};
