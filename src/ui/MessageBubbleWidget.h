// MessageBubbleWidget.h — 消息气泡 Widget（替代 MessageBubbleDelegate QPainter 绘制）
#pragma once

#include <QFrame>
#include <QHash>

#include <memory>

class ElaText;
class QHBoxLayout;
class QLabel;
class QModelIndex;
class QMovie;
class QPushButton;
class QVBoxLayout;

class MessageBubbleWidget : public QFrame {
    Q_OBJECT
    Q_PROPERTY(bool outgoing READ isOutgoing)
    Q_PROPERTY(bool selected READ isSelected WRITE setSelected)
public:
    explicit MessageBubbleWidget(QWidget* parent = nullptr);

    void populateFromIndex(const QModelIndex& index);
    void updateFromIndex(const QModelIndex& index, const QList<int>& roles);
    void setAvailableWidth(int width);

    // 回收池支持
    void resetForRecycling();
    bool isRecyclable() const { return m_layoutBuilt; }

    void setSelected(bool selected);
    bool isSelected() const { return m_selected; }
    bool isOutgoing() const { return m_outgoing; }
    QString messageId() const { return m_messageId; }

    void applyThemeStyleSheet();

    // Lightweight partial updates (no full rebuild)
    void updateTransferProgress(int percent, const QString& statusText);
    void updateDeliveryState(int deliveryState, int groupReadCount = 0, int activeMemberCount = 0);
    bool isFileCard() const;

    QString selectedText() const;
    bool hasSelection() const;
    void clearSelection();

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
    void contextMenuRequested(const QString& messageId, const QPoint& globalPos);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildLayout();
    void clearContent();
    void arrangeMessageAlignment();
    bool m_layoutBuilt = false;

    void populateSystemMessage(const QModelIndex& index);
    void populateCallRecord(const QModelIndex& index);
    void populateRecalledMessage(const QModelIndex& index);
    void populateStickerMessage(const QModelIndex& index);
    void populateGroupFileCard(const QModelIndex& index);
    void populateResourceReference(const QModelIndex& index);
    void populateNormalMessage(const QModelIndex& index);

    void setupAvatar(const QString& avatarPath, const QString& senderName, bool outgoing);
    void setupHeader(const QString& sender, const QString& timeLabel, bool outgoing);
    void setupDeliveryIndicator(int deliveryState, bool outgoing,
                                 int groupReadCount, int groupActiveMemberCount);
    void setupReplyQuote(const QString& replyToSenderName, const QString& replyToBody);
    void setupFileCard(const QModelIndex& index, bool outgoing);
    void setupImagePreview(const QString& localFilePath, bool outgoing, bool pureImageBubble);
    void setupTransferCard(const QModelIndex& index, bool outgoing);
    void setupActionChips(const QModelIndex& index, bool outgoing);
    void onThumbnailReady(const QString& cacheKey, const QPixmap& pixmap);

    static QColor avatarFallbackColor(const QString& senderName);

    // 数据
    QString m_messageId;
    QString m_senderId;
    QString m_transferTaskId;
    bool m_outgoing = false;
    bool m_selected = false;
    int m_availableWidth = 0;

    // 顶层布局
    QVBoxLayout* m_mainLayout = nullptr;

    // 日期分隔
    QLabel* m_dateSeparator = nullptr;

    // 系统/通话记录居中消息
    QLabel* m_centeredLabel = nullptr;

    // 正常消息区域
    QWidget* m_messageArea = nullptr;
    QHBoxLayout* m_messageLayout = nullptr;

    // 左侧头像
    QLabel* m_avatarLabel = nullptr;
    // 右侧头像（发送方）
    QLabel* m_avatarLabelRight = nullptr;

    // 消息体
    QWidget* m_bodyWidget = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;

    // 头部 (sender + time)
    QLabel* m_headerLabel = nullptr;

    // 气泡
    QFrame* m_bubbleFrame = nullptr;
    QVBoxLayout* m_bubbleLayout = nullptr;

    // 引用回复
    QFrame* m_quoteFrame = nullptr;
    QLabel* m_quoteSenderLabel = nullptr;
    QLabel* m_quoteBodyLabel = nullptr;

    // 消息正文
    QLabel* m_bodyLabel = nullptr;

    // 贴纸
    QLabel* m_stickerLabel = nullptr;
    std::shared_ptr<QMovie> m_stickerMovie;

    // 文件卡片
    QFrame* m_fileCardFrame = nullptr;

    // 图片预览
    QLabel* m_imagePreviewLabel = nullptr;

    // 传输进度
    QLabel* m_transferStatusLabel = nullptr;

    // 操作按钮
    QWidget* m_actionChipsWidget = nullptr;

    // 缩略图异步缓存 key
    QString m_currentThumbnailKey;

    // 送达指示
    QLabel* m_deliveryLabel = nullptr;

    // 撤回
    QLabel* m_recalledLabel = nullptr;
};
