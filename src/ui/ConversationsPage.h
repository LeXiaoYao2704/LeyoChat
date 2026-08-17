// ConversationsPage.h — 承载完整工作区（sideStack + contentStack + workspaceSplitter）
#pragma once

#include <QByteArray>
#include <QHash>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSet>
#include <QVector>
#include <QWidget>

#include "ui/ComposerRecoveryContext.h"
#include <optional>
#include <vector>

#include "architecture/RuntimeArchitectureSnapshot.h"
#include "call/CallSession.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "ui/GroupMemberListEntry.h"
#include "ui/ProfileCardPopup.h"

class QAction;
class QJsonObject;
class ChatComposerWidget;
class ChatHeaderWidget;
class ContactListModel;
class ContextPanel;
class ConversationCardDelegate;
class ConversationItemWidget;
class ConversationListModel;
class QEvent;
class QListView;
class GroupInfoPanel;
class QAbstractItemModel;
class ElaFrame;
class QGraphicsOpacityEffect;
class QHBoxLayout;
class QImage;
class QKeyEvent;
class ElaText;
class ElaLineEdit;
class ElaToolButton;
class ElaPushButton;
class QNetworkAccessManager;
class ElaListView;
class ElaListWidget;
class MessageBubbleDelegate;
class MessageBubbleWidget;
class MessageListModel;
class ProfileCardPopup;
class QPropertyAnimation;
class QScrollBar;
class ElaTextEdit;
class ElaListWidget;
class ElaStackedWidget;
class ElaSplitter;
class QTimer;
class TransferListModel;
class MainWindow;
#ifdef LEYOCHAT_HAS_WEBENGINE
class QWebEngineView;
#endif
struct RuntimeArchitecturePresentation;

struct PinnedCardInfo {
    QString messageId;
    QString pinnedBody;
    QString authorName;
    QString pinnerName;
};

class ConversationsPage : public QWidget {
    Q_OBJECT
public:
    explicit ConversationsPage(MainWindow* mainWindow, QWidget* parent = nullptr);

    void setConversationModel(ConversationListModel* model);
    void setContactModel(ContactListModel* model);
    void setMessageModel(MessageListModel* model);
    void setTransferModel(QAbstractItemModel* model);
    void setListenPort(quint16 port);

    void setChatHeader(const QString& title, const QString& status);
    void setChatHeaderDirect(const QString& name,
                             const QString& status,
                             const QString& signature,
                             const QString& avatarImagePath = QString());
    void setChatHeaderGroup(const QString& groupId, const QString& groupName, int memberCount);

    void setStatusMessage(const QString& message, int timeoutMs = 0);
    void showChatToast(const QString& message, int timeoutMs = 3000);

    void setRuntimeArchitectureSummary(int serviceCount,
                                       int workspaceBindingCount,
                                       int groupBindingCount,
                                       int resourceCount,
                                       bool bound,
                                       const QString& activeServiceName);
    void setRuntimeArchitectureSnapshot(const RuntimeArchitectureSnapshot& snapshot);

    void setSelectedConversationId(const QString& conversationId);
    void setSelectedTransferId(const QString& taskId);

    void focusChatInput();

    bool isShowingWelcomePage() const;
    bool isShowingChatPage() const;
    bool isGroupPanelVisible() const;
    bool isGroupWorkspaceActive() const;
    void showDirectConversation(const QString& conversationId, const QString& title);
    void showGroupConversation(const QString& conversationId, const QString& title);
    void clearCurrentConversationView();
    void clearComposerDraft(const QString& outgoingConversationId = QString());
    void restoreComposerDraft(const QString& conversationId);
    ComposerRecoveryContext activeComposerRecoveryContext() const;
    QString activeComposerContextId() const;
    void stageRecoveredComposerContext(const QString& conversationId,
                                       const ComposerRecoveryContext& context);
    void syncDraftsToModel();
    bool importScreenshotPreview(const QImage& image);
    void triggerScreenshot(bool forceHideWindow = false);
    int pendingAttachmentCount() const;
    void submitCurrentComposer();

    GroupInfoPanel* groupInfoPanel() const { return m_groupInfoPanel; }
    ChatComposerWidget* chatComposerWidget() const { return m_chatComposerWidget; }
    ChatHeaderWidget* chatHeaderWidget() const { return m_chatHeaderWidget; }

    void showProfileCard(const ProfileCardPopup::ProfileInfo& info, const QPoint& globalPos);
    void showConversationAvatarPopup(const QModelIndex& index);

    void setGroupInfoPanel(const QString& announcement,
                           const GroupMemberListEntries& members,
                           bool currentUserCanManageMembers);
    void setGroupMembers(const GroupMemberListEntries& members,
                         bool currentUserCanManageMembers = false);
    void setPinnedMessageCards(const std::vector<PinnedCardInfo>& cards);
    void clearPinnedMessageCards();
    void setCurrentUserIsGroupOwner(bool isOwner);

    void playWelcomeReveal();
    void applyRuntimeArchitecturePresentation(const RuntimeArchitecturePresentation& presentation);
    void refreshTheme();

    void setGroupWorkspaceMode(bool groupMode) { m_groupWorkspaceMode = groupMode; }
    bool groupWorkspaceMode() const { return m_groupWorkspaceMode; }

    void syncConversationSidebarMode();

    void setLocalDisplayName(const QString& name) { m_localDisplayName = name; }
    QString localDisplayName() const { return m_localDisplayName; }

#ifdef LEYOCHAT_HAS_WEBENGINE
#endif

signals:
    void sendRequested(const QString& htmlBody);
    void stickerSendRequested(const QString& packId, const QString& stickerId,
                               const QByteArray& gifData);
    void fileSendRequested(const QString& filePath);
    void nudgeRequested();
    void devOpsInsertRequested();
    void connectRequested(const QString& host, quint16 port);
    void createGroupRequested();
    void createGroupWithPeerRequested(const QString& peerId);
    void conversationSelected(const QString& conversationId);
    void contactSelected(const QString& clientId);
    void avatarProfileRequested(const QString& clientId, const QPoint& globalPos);
    void conversationAvatarHovered(const QString& conversationId, const QPoint& globalPos);
    void retryPendingRequested();
    void retryMessageRequested(const QString& messageId);
    void recallMessageRequested(const QString& messageId);
    void editSaveRequested(const QString& messageId, const QString& newHtmlBody);
    void reactionRequested(const QString& messageId, const QString& emoji);
    void retryTransferRequested(const QString& taskId);
    void deleteTransferRequested(const QString& taskId);
    void clearPendingTransfersRequested();
    void clearCompletedTransfersRequested();
    void clearFailedTransfersRequested();
    void openMessageFileRequested(const QString& messageId);
    void revealMessageFileRequested(const QString& messageId);
    void readReceiptDetailRequested(const QString& messageId);
    void pinMessageRequested(const QString& messageId, const QString& bodyPreview, const QString& authorName);
    void unpinMessageRequested(const QString& messageId);
    void replyToMessageRequested(const QString& messageId, const QString& senderId,
                                 const QString& senderName, const QString& bodyPreview);
    void forwardMessageRequested(const QString& body, bool isFile, const QString& localFilePath,
                                 const QString& attachmentName, const QString& senderName);
    void messageReminderRequested(const QString& messageId,
                                  const QString& conversationId,
                                  const QString& titleSnapshot,
                                  const QString& previewSnapshot);
    void mergedForwardRequested(const QString& mergedHtml);
    void mergedForwardPackageRequested(const QJsonObject& package);
    void openTransferFileRequested(const QString& taskId);
    void revealTransferFileRequested(const QString& taskId);
    void cancelTransferRequested(const QString& taskId);
    void cancelSameNameTransfersRequested(const QString& fileName);
    void voiceCallRequested(const QString& peerId);
    void conversationFilterChanged(int filterIndex);
    void conversationPinToggled(const QString& conversationId, bool pinned);
    void conversationStarToggled(const QString& conversationId, bool starred);
    void conversationMuteToggled(const QString& conversationId, bool muted);
    void conversationMarkUnread(const QString& conversationId, bool unread);
    void conversationMarkDone(const QString& conversationId);
    void groupAnnouncementRequested(const QString& groupId);
    void groupAnnouncementReminderRequested(const QString& groupId,
                                            const QString& groupName,
                                            const QString& announcement);
    void groupAddMemberRequested(const QString& groupId);
    void groupChatHistoryRequested(const QString& groupId);
    void groupSettingsRequested(const QString& groupId);
    void groupMemberDirectChatRequested(const QString& clientId);
    void groupMemberAdminRequested(const QString& groupId, const QString& clientId);
    void groupMemberRemoveRequested(const QString& groupId, const QString& clientId);
    void groupMemberMuteRequested(const QString& groupId, const QString& clientId);
    void chatHistoryRequested(const QString& conversationId);
    void closeCurrentConversationRequested();
    void screenshotRequested();
    void groupNavSelected();
    void messageUrlOpenRequested(const QString& url);
    void olderMessagesRequested(const QString& conversationId, const QString& beforeMessageId);
    void composerRecoveryContextChanged();
    void composerRecoveryContextCommitted();
    void fileServiceDownloadRequested(const QString& messageId);
    void fileServiceVersionHistoryRequested(const QString& messageId);
    void groupFileServiceSaveRequested(const GroupFileServiceConfig& config);
    void groupFileManagerRequested(const QString& groupId, const GroupFileServiceConfig& config);
    void groupFileDownloadRequested(const QString& messageId);
    void groupFileOpenRequested(const QString& messageId);
    void viewportReachedBottom();

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void showGroupInfoPanelAnimated();
    void hideGroupInfoPanelAnimated();
    void toggleGroupInfoPanel();
    void prepareGroupInfoPanelDetail();
    void syncGroupPanelToggleButton();
    void positionGroupPanelToggleButton();
    void updateMessageStageContext(bool groupMode, const QString& title, const QString& detail);
    void syncMessageStageEmptyState();
    void syncComposerDraftState();
    void syncConversationWorkspaceStatus();
    void syncWelcomeContactMetric();
    void syncTransferWorkspaceStatus();
    void syncGroupRuntimeArchitectureStatus();
    void syncNudgeAvailability();
    void syncSendModePresentation();
    void showConversationFilterPanel();
    void hideConversationFilterPanel();
    void positionConversationFilterPanel();
    bool isMessageViewportNearBottom() const;
    void scheduleMessageViewportToBottom();
    QString messageCopyTextForIndex(const QModelIndex& index) const;
    bool copyMessageAtIndexToClipboard(const QModelIndex& index);
    bool handleMentionPopupKeyPress(QKeyEvent* keyEvent);
    QString currentMentionQuery() const;
    QStringList filteredMentionCandidates(const QString& query) const;
    bool replaceCurrentMentionToken(const QString& mentionText);
    void refreshMentionPopup();
    void syncGroupSharedFileCount();
    void refreshConversationListWidgets();
    void insertConversationListRow(int row);
    void removeConversationListRows(int first, int last);
    void updateConversationListRow(int row);
    void scheduleConversationListRebuild();
    void onConversationItemClicked(const QString& conversationId);
    void onConversationItemContextMenu(const QString& conversationId, const QPoint& globalPos);
    void refreshMessageListWidgets();
    void insertMessageListRow(int row);
    void updateMessageListRow(int row);
    void removeMessageListRows(int first, int last);
    void syncMessageListAfterLayoutChanged();
    QString topVisibleMessageId() const;
    void restoreTopVisibleMessage(const QString& messageId);
    void processPendingInsertBatch();
    void refreshVisibleMessageBubbleThemes();
    void ensureViewportWidgets();
    void recycleOffscreenWidgets();
    MessageBubbleWidget* acquireBubbleWidget();
    void releaseBubbleWidget(MessageBubbleWidget* widget);
    ConversationItemWidget* acquireConversationWidget();
    void releaseConversationWidget(ConversationItemWidget* widget);
    void onMessageBubbleContextMenu(const QString& messageId, const QPoint& globalPos);
    void enterMessageMultiSelectMode();
    void exitMessageMultiSelectMode();
    void toggleMessageMultiSelect(const QString& messageId);
    void updateMultiSelectBar();

    MainWindow* m_mainWindow = nullptr;

    ElaStackedWidget* m_sideStack = nullptr;
    ElaStackedWidget* m_contentStack = nullptr;
    ElaSplitter* m_workspaceSplitter = nullptr;
    ElaSplitter* m_chatWorkspaceSplitter = nullptr;
    QListView* m_conversationList = nullptr;
    ConversationCardDelegate* m_conversationCardDelegate = nullptr;
    ConversationListModel* m_conversationModel = nullptr;
    ContactListModel* m_contactModel = nullptr;
    ElaListView* m_transferList = nullptr;
    QListView* m_messageList = nullptr;
    MessageBubbleDelegate* m_messageBubbleDelegate = nullptr;
    MessageListModel* m_messageModel = nullptr;
    ElaFrame* m_pinnedCardsContainer = nullptr;
    QHBoxLayout* m_pinnedCardsLayout = nullptr;
    QStringList m_currentPinnedIds;
    bool m_currentUserIsGroupOwner = false;
    ElaText* m_conversationEmptyLabel = nullptr;
    QPointer<ProfileCardPopup> m_profileCard;
    QTimer* m_avatarHoverTimer = nullptr;
    QPersistentModelIndex m_avatarHoverIndex;
    ElaText* m_transferEmptyLabel = nullptr;
    ElaText* m_conversationsModeChip = nullptr;
    ElaText* m_conversationsStatusChip = nullptr;
    ElaText* m_transferStatusChip = nullptr;
    ElaText* m_conversationWorkspaceTitle = nullptr;
    ElaTextEdit* m_inputEdit = nullptr;
    ElaLineEdit* m_hostEdit = nullptr;
    ElaLineEdit* m_portEdit = nullptr;
    ChatComposerWidget* m_chatComposerWidget = nullptr;
    ElaToolButton* m_sendFileButton = nullptr;
    ElaPushButton* m_sendButton = nullptr;
    ElaToolButton* m_transferFilterAllBtn = nullptr;
    ElaToolButton* m_transferFilterOutgoingBtn = nullptr;
    ElaToolButton* m_transferFilterIncomingBtn = nullptr;
    ElaToolButton* m_transferFilterActiveBtn = nullptr;
    ElaToolButton* m_transferFilterFailedBtn = nullptr;
    ElaToolButton* m_transferFilterCompletedBtn = nullptr;
    ElaPushButton* m_sendModeBtn = nullptr;
    ElaToolButton* m_screenshotBtn = nullptr;
    ElaToolButton* m_directHistoryBtn = nullptr;
    ElaPushButton* m_connectButton = nullptr;
    ElaToolButton* m_conversationFilterToggleBtn = nullptr;
    bool m_groupWorkspaceMode = false;

    ChatHeaderWidget* m_chatHeaderWidget = nullptr;
    ElaFrame* m_messageStageFrame = nullptr;
    ElaFrame* m_messageStageTopBand = nullptr;
    ElaFrame* m_messageStageEmptyCard = nullptr;
    ElaFrame* m_conversationFilterPanel = nullptr;
    ElaText* m_messageStageModeChip = nullptr;
    ElaText* m_messageStageContextChip = nullptr;
    ElaText* m_messageStageHintLabel = nullptr;
    ElaText* m_messageStageEmptyTitle = nullptr;
    ElaText* m_messageStageEmptyBody = nullptr;
    ElaText* m_chatToastLabel = nullptr;
    QTimer* m_chatToastTimer = nullptr;
    ElaText* m_welcomeMessagesMetricValue = nullptr;
    ElaText* m_welcomeContactsMetricValue = nullptr;
    ElaText* m_welcomeTransfersMetricValue = nullptr;
    ElaText* m_welcomeMessagesSignal = nullptr;
    ElaText* m_welcomeContactsSignal = nullptr;
    ElaText* m_welcomeTransfersSignal = nullptr;
    ElaText* m_welcomeRuntimeSummary = nullptr;
    ElaText* m_welcomeRuntimeDetail = nullptr;
    ElaText* m_welcomeRuntimeChromeStatus = nullptr;
#ifdef LEYOCHAT_HAS_WEBENGINE
#endif
    RuntimeArchitectureSnapshot m_runtimeArchitectureSnapshot;
    bool m_hasRuntimeArchitectureSnapshot = false;
    QString m_selectedConversationId;
    ElaToolButton* m_groupAnnouncementBtn = nullptr;
    ElaToolButton* m_groupAddMemberBtn = nullptr;
    ElaToolButton* m_groupHistoryBtn = nullptr;
    ElaToolButton* m_groupSettingsBtn = nullptr;
    ElaToolButton* m_groupPanelToggleBtn = nullptr;
    QString m_currentGroupId;
    QString m_currentGroupName;
    QString m_localDisplayName;

    ContextPanel* m_contextPanel = nullptr;
    GroupInfoPanel* m_groupInfoPanel = nullptr;
    int m_groupPanelExpandedWidth = 0;
    bool m_groupPanelVisibleState = false;
    QPointer<QPropertyAnimation> m_groupPanelAnimation;
    QNetworkAccessManager* m_groupFileNam = nullptr;
    QHash<QString, int> m_groupSharedFileCountCache;

    bool m_messageMultiSelectMode = false;
    QSet<QString> m_multiSelectedMessageIds;
    ElaFrame* m_multiSelectBar = nullptr;
    ElaPushButton* m_multiSelectForwardBtn = nullptr;

    QPointer<ElaListWidget> m_mentionPopup;
    QStringList m_currentGroupMembers;
    GroupMemberListEntries m_currentGroupMemberEntries;
    bool m_currentUserCanManageGroupMembers = false;

    QTimer* m_screenshotPollTimer = nullptr;
    QByteArray m_lastClipboardImageFingerprint;
    QStringList m_pendingAttachmentPaths;
    QMap<QString, QString> m_screenshotPreviewToPath;
    QHash<QString, QString> m_composerDrafts;
    QHash<QString, QString> m_composerDraftHtml;             // conversationId → HTML content (for image drafts)
    QHash<QString, QStringList> m_draftAttachmentPaths;       // conversationId → pending attachment paths
    QHash<QString, QMap<QString, QString>> m_draftScreenshotMap; // conversationId → screenshot preview→path map
    QHash<QString, ComposerRecoveryContext> m_recoveredComposerContexts;
    bool m_restoreAfterScreenshot = false;
    int m_screenshotPollAttempts = 0;
    bool m_followLatestMessages = true;
    bool m_programmaticMessageScroll = false;
    int m_messageListFirstModelRow = 0;
    QString m_lastOlderMessagesRequestKey;

    struct PendingMessageInsert {
        int nextRow = 0;
        int lastRow = -1;
        int batchSize = 20;
        QString anchorMessageId;
        int anchorScrollValue = 0;
        int anchorScrollMaximum = 0;
    };
    std::optional<PendingMessageInsert> m_pendingInsert;
    QString m_activeComposerContextId;

    // Widget 回收池
    QVector<MessageBubbleWidget*> m_bubbleWidgetPool;
    QVector<ConversationItemWidget*> m_convWidgetPool;
    static constexpr int kMaxBubblePoolSize = 20;
    static constexpr int kMaxConvPoolSize = 20;

    // Viewport-only 实例化
    QTimer* m_viewportSyncTimer = nullptr;

    // 会话列表 debounce
    QTimer* m_convListRebuildTimer = nullptr;

    // 消息滚动到底部防抖
    QTimer* m_scrollToBottomTimer = nullptr;

    QString m_attachmentDraftContextId;
    QString m_lastSubmitContextId;
    QString m_lastSubmitFingerprint;
    qint64 m_lastSubmitAtMs = 0;

    ElaFrame* m_welcomeCard = nullptr;
    ElaFrame* m_welcomePreviewCard = nullptr;
    QGraphicsOpacityEffect* m_welcomeCardOpacity = nullptr;
    QGraphicsOpacityEffect* m_welcomePreviewOpacity = nullptr;
    QPropertyAnimation* m_welcomeRevealAnimation = nullptr;
    QPropertyAnimation* m_welcomePreviewAnimation = nullptr;

};
