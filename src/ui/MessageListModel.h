#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <vector>

#include "domain/ChatMessage.h"
#include "domain/FileTransferTask.h"

class ChatDataStore;

class MessageListModel : public QAbstractListModel {
    Q_OBJECT

public:
    struct TransferVisualState {
        QString taskId;
        FileTransferState state = FileTransferState::PendingOffer;
        qint64 bytesCompleted = 0;
        qint64 fileSize = 0;
        qint64 speedBytesPerSec = 0;
        bool cancelable = false;

        bool operator==(const TransferVisualState& o) const {
            return taskId == o.taskId && state == o.state
                && bytesCompleted == o.bytesCompleted && fileSize == o.fileSize
                && speedBytesPerSec == o.speedBytesPerSec
                && cancelable == o.cancelable;
        }
        bool operator!=(const TransferVisualState& o) const { return !(*this == o); }
    };

    enum Roles {
        MessageIdRole = Qt::UserRole + 1,
        DeliveryStateRole,
        OutgoingRole,
        FileMessageRole,
        AttachmentNameRole,
        LocalFilePathRole,
        BodyRole,
        MessageTypeRole,
        PayloadJsonRole,
        ResourceReferenceRole,
        SenderNameRole,
        SenderAvatarPathRole,
        TimeLabelRole,
        TransferTaskIdRole,
        TransferStateRole,
        TransferBytesCompletedRole,
        TransferFileSizeRole,
        TransferCancelableRole,
        TransferSpeedRole,
        GroupReadCountRole,
        GroupActiveMemberCountRole,
        RecalledRole,
        EditedRole,
        EditedAtRole,
        LastEditorIdRole,
        CreatedAtRole,
        ShowDateSeparatorRole,
        DateLabelRole,
        ReplyToMessageIdRole,
        ReplyToSenderNameRole,
        ReplyToBodyRole,
        SenderIdRole,
        FileCardJsonRole,
        ForwardPackageRole,
        ReactionsJsonRole
    };

    explicit MessageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setDisplayContext(const QString& localClientId,
                           const QString& peerDisplayName,
                           const QString& peerClientId = QString());
    QString localClientId() const { return m_localClientId; }
    QString displayNameForClientId(const QString& clientId) const;
    bool hasMoreMessagesBefore() const;
    QString firstMessageId() const;
    void setAvatarContext(const QString& localAvatarPath, const QString& peerAvatarPath);
    void setGroupMemberNames(const QHash<QString, QString>& names);
    void setGroupMemberAvatars(const QHash<QString, QString>& avatars);
    void setGroupActiveMemberCount(int count);
    void setTransferStates(const QHash<QString, TransferVisualState>& states);
    void setItems(std::vector<ChatMessage> items);
    const std::vector<ChatMessage>& items() const { return m_items; }
    QHash<QString, QString> senderDisplayNameMap() const;
    int findRowByMessageId(const QString& messageId) const;

    void bindToStore(ChatDataStore* store);
    void switchToConversation(const QString& conversationId);

    // 批量上下文更新：抑制中间 dataChanged，避免切换时无效重绘
    void beginBulkContextUpdate();
    void endBulkContextUpdate();

private slots:
    void onMessageAppended(const QString& conversationId, int newIndex);
    void onMessagesPrepended(const QString& conversationId, int count);
    void onMessageUpdated(const QString& conversationId, const QString& messageId);
    void onMessagesReset(const QString& conversationId);

private:
    bool isOutgoingMessage(const ChatMessage& item) const;

    QString m_localClientId;
    QString m_peerDisplayName;
    QString m_peerClientId;
    QString m_localAvatarPath;
    QString m_peerAvatarPath;
    QHash<QString, QString> m_groupMemberNames;
    QHash<QString, QString> m_groupMemberAvatars;
    QHash<QString, TransferVisualState> m_transferStates;
    int m_groupActiveMemberCount = 0;
    bool m_contextDirty = false;
    bool m_bulkContextUpdate = false;
    std::vector<ChatMessage> m_items;
    ChatDataStore* m_store = nullptr;
    QString m_activeConversationId;
};
