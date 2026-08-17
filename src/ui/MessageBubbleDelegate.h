#pragma once

#include <QHash>
#include <QPersistentModelIndex>
#include <QStyledItemDelegate>

#include <functional>
#include <memory>

class QMovie;
class QAbstractItemView;
class QHelpEvent;

class MessageBubbleDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    struct FileCardActionGeometry {
        bool hasActionChips = false;
        QRect openFileChipRect;
        QRect openFolderChipRect;
        bool hasPreviewChip = false;
        QRect previewChipRect;
        bool hasTransferCancelChip = false;
        QRect transferCancelChipRect;
        bool hasResourceActionChips = false;
        QRect resourceDownloadChipRect;
        QRect resourceOpenChipRect;
    };

    explicit MessageBubbleDelegate(QObject* parent = nullptr);

    static QColor fileCardBackgroundColorForTesting(bool outgoing, bool isImageFile);
    static QColor fileCardTitleColorForTesting(bool outgoing, bool isImageFile);
    static QString deliveryIndicatorTextForTesting(int state, bool outgoing,
                                                   int groupReadCount, int groupActiveMemberCount);
    static FileCardActionGeometry fileCardActionGeometryForTesting(
        const QStyleOptionViewItem& option,
        const QModelIndex& index);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    bool editorEvent(QEvent* event,
                     QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

    bool helpEvent(QHelpEvent* event,
                   QAbstractItemView* view,
                   const QStyleOptionViewItem& option,
                   const QModelIndex& index) override;

    QString selectedText() const;
    bool hasSelection() const;
    void clearSelection();
    bool isPointInPureImageBubble(const QModelIndex& index, const QPoint& viewportPos) const;
    void setLocalClientId(const QString& id) { m_localClientId = id; }
    void setNameResolver(std::function<QString(const QString&)> resolver) { m_nameResolver = std::move(resolver); }

signals:
    void messageFileOpenRequested(const QString& messageId);
    void messageFileRevealRequested(const QString& messageId);
    void messageTransferCancelRequested(const QString& taskId);
    void messageFileDownloadRequested(const QString& messageId);
    void messageFilePreviewRequested(const QString& messageId);
    void messageFileVersionHistoryRequested(const QString& messageId);
    void readReceiptDetailRequested(const QString& messageId);
    void linkClicked(const QUrl& url);
    void avatarClicked(const QString& senderId, const QPoint& globalPos);
    void forwardCardClicked(const QString& messageId);
    void replyQuoteClicked(const QString& replyToMessageId);
    void reactionToggled(const QString& messageId, const QString& emoji);

private:
    struct TextSelectionState {
        QPersistentModelIndex index;
        int anchor = -1;
        int cursor = -1;
        bool dragging = false;
        QString text;
    };
    mutable QHash<QPersistentModelIndex, QRect> m_deliveryTextRects;
    mutable QHash<QPersistentModelIndex, QRect> m_avatarRects;
    mutable QHash<QPersistentModelIndex, QRect> m_pureImageBubbleRects;
    mutable QHash<QPersistentModelIndex, QRect> m_forwardCardRects;
    mutable QHash<QPersistentModelIndex, QRect> m_replyQuoteRects;
    mutable QHash<QPersistentModelIndex, QVector<QPair<QRect, QString>>> m_reactionPillRects;
    mutable QHash<QPersistentModelIndex, std::shared_ptr<QMovie>> m_stickerMovies;
    QString m_localClientId;
    std::function<QString(const QString&)> m_nameResolver;

    static int bubbleMaxWidth(int viewWidth);
    mutable TextSelectionState m_selectionState;
};
