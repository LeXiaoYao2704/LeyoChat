#pragma once

#include <QAbstractListModel>
#include <QVector>

#include "domain/FileTransferTask.h"

enum class TransferListFilter {
    All,
    OutgoingOnly,
    IncomingOnly,
    ActiveOnly,
    FailedOnly,
    CompletedOnly
};

struct TransferListItem {
    QString taskId;
    QString titleText;
    QString statusText;
    QString detailText;
    QString peerLabel;
    QString localFilePath;
    FileTransferDirection direction = FileTransferDirection::Outgoing;
    FileTransferState state = FileTransferState::PendingOffer;
    bool openable = false;
    bool revealable = false;
    bool retryable = false;
};

class TransferListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TaskIdRole = Qt::UserRole + 1,
        StatusTextRole,
        DetailTextRole,
        PeerLabelRole,
        FileBadgeRole,
        DirectionRole,
        StateRole,
        LocalFilePathRole,
        OpenableRole,
        RevealableRole,
        RetryableRole
    };

    explicit TransferListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QVector<TransferListItem> items);
    void setFilter(TransferListFilter filter);
    TransferListFilter filter() const;

private:
    void rebuildVisibleItems();
    static bool matchesFilter(const TransferListItem& item, TransferListFilter filter);

    QVector<TransferListItem> m_sourceItems;
    QVector<TransferListItem> m_items;
    TransferListFilter m_filter = TransferListFilter::All;
};
