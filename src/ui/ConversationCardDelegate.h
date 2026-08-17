// ConversationCardDelegate.h — 会话列表项 QPainter delegate（替代 ConversationItemWidget）
#pragma once

#include <QStyledItemDelegate>

class ConversationCardDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ConversationCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void setSelectedConversationId(const QString& id);
    QString selectedConversationId() const { return m_selectedConvId; }

protected:
    void initStyleOption(QStyleOptionViewItem* option,
                         const QModelIndex& index) const override;
    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

signals:
    void clicked(const QString& conversationId);
    void contextMenuRequested(const QString& conversationId, const QPoint& globalPos);
    void avatarHovered(const QString& conversationId, const QPoint& globalPos);
    void avatarHoverLeft();

private:
    QRect computeAvatarRect(const QRect& cardRect, bool selected) const;

    QString m_selectedConvId;
    mutable QModelIndex m_hoveredIndex;
};
