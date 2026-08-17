#pragma once

#include <QByteArray>
#include <QHash>
#include <ElaWindow.h>
#include <QPointer>
#include <QPixmap>
#include <QSet>
#include <QVector>
#include <vector>

#include "architecture/RuntimeArchitectureSnapshot.h"
#include "call/CallSession.h"
#include "domain/ReminderItem.h"
#include "domain/SystemNotificationItem.h"
#include "integrations/KnowledgeServiceSettings.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "ui/GroupMemberListEntry.h"
#include "ui/ProfileCardPopup.h"
#include "ui/ConversationsPage.h"
#include "ui/DirectoryPage.h"
#include "ui/GlobalSearchPanel.h"

#include "ui/CallWindow.h"

class QAction;
class ChatComposerWidget;
class ChatHeaderWidget;
class ContactListModel;
class ConversationListModel;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QEvent;
class GroupInfoPanel;
class QAbstractItemModel;
class QCloseEvent;
class QImage;
class QKeyEvent;
class QModelIndex;
class QLineEdit;
class QTimer;
class MessageListModel;
class SettingsPage;
class UpdateBar;
class NotificationsPage;
class KnowledgePage;
class GlobalSearchHistory;
#ifdef LEYOCHAT_HAS_WEBENGINE
class QWebEngineView;
#endif

class MainWindow : public ElaWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    static QString conversationDoneActionTextForTesting(bool isGroupConversation, bool isDone);
    int primaryPageCountForTesting() const;
    bool hasPrimaryPageForTesting(const QString& key) const;
    QPixmap avatarPixmapForTesting() const { return m_currentAvatarPixmap; }

    // ── 转发到 ConversationsPage 的公共 API ──
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
    void showCallWindow(bool outgoing, const QString& peerName, const QString& avatarPath = QString());
    void updateCallWindowState(CallSession::State state);
    void closeCallWindow();
    CallWindow* callWindow() const { return m_callWindow.data(); }
    UpdateBar* updateBar() const { return m_updateBar; }
    void repositionUpdateBar();
    void setCallWindowMuted(bool muted);
    void setStatusMessage(const QString& message, int timeoutMs = 0);
    void showChatToast(const QString& message, int timeoutMs = 3000);
    void setRuntimeArchitectureSummary(int serviceCount,
                                       int workspaceBindingCount,
                                       int groupBindingCount,
                                       int resourceCount,
                                       bool bound,
                                       const QString& activeServiceName);
    void setRuntimeArchitectureSnapshot(const RuntimeArchitectureSnapshot& snapshot);
    void setNotificationItems(const QVector<SystemNotificationItem>& items);
    void appendNotificationItem(const SystemNotificationItem& item);
    void setActiveReminders(const QVector<ReminderItem>& reminders);
    void setSelectedConversationId(const QString& conversationId);
    void setSelectedContactId(const QString& clientId);
    void setSelectedTransferId(const QString& taskId);
    void focusChatInput();
    void setAvatarText(const QString& letter);
    void setAvatarImagePath(const QString& imagePath);
    bool isShowingWelcomePage() const;
    bool isShowingChatPage() const;
    bool isShowingNotificationPage() const;
    void showMessagesPage();
    QString recoveryPageId() const;
    bool navigateToRecoveryPage(const QString& stablePageId);
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
    void setNavUnreadCount(int count);
    void setNavGroupUnreadCount(int count);
    void setNavNotificationCount(int count);
    void setLocalDisplayName(const QString& name);
    QString localDisplayName() const { return m_localDisplayName; }
    GroupInfoPanel* groupInfoPanel() const;
    ChatComposerWidget* chatComposerWidget() const;
    ChatHeaderWidget* chatHeaderWidget() const;
    SettingsPage* settingsPage() const { return m_settingsPage; }
    ConversationsPage* conversationsPage() const { return m_conversationsPage; }

    void setGlobalSearchResults(const QVector<GlobalSearchPanel::ContactResult>& contacts,
                              const QVector<GlobalSearchPanel::GroupResult>& groups,
                              const QVector<GlobalSearchPanel::MessageResult>& messages,
                              const QVector<GlobalSearchPanel::FileResult>& files,
                              const QVector<GlobalSearchPanel::DepartmentResult>& departments);
    GlobalSearchHistory* globalSearchHistory() const { return m_globalSearchHistory; }
    void showProfileCard(const ProfileCardPopup::ProfileInfo& info, const QPoint& globalPos);

    void setGroupInfoPanel(const QString& announcement,
                           const GroupMemberListEntries& members,
                           bool currentUserCanManageMembers);
    void setGroupMembers(const GroupMemberListEntries& members,
                         bool currentUserCanManageMembers = false);
    void setPinnedMessageCards(const std::vector<PinnedCardInfo>& cards);
    void clearPinnedMessageCards();
    void setCurrentUserIsGroupOwner(bool isOwner);
    void reloadAppearanceSettings();
    void setAiKnowledgeServices(const QVector<KnowledgeServiceConfig>& configs,
                                const QString& preferredServiceId = QString());
    void setDirectoryGroups(const QVector<GroupSummary>& groups);
    void setDirectoryOrgData(const QHash<QString, QVector<OrgContactEntry>>& departments);

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
    void addContactRequested();
    void conversationSelected(const QString& conversationId);
    void composerRecoveryContextChanged();
    void composerRecoveryContextCommitted();
    void contactSelected(const QString& clientId);
    void contactProfileRequested(const QString& clientId);
    void contactReminderRequested(const QString& contactId,
                                  const QString& displayName,
                                  const QString& previewSnapshot);
    void avatarProfileRequested(const QString& clientId, const QPoint& globalPos);
    void conversationAvatarHovered(const QString& conversationId, const QPoint& globalPos);
    void contactDeleteRequested(const QString& clientId);
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
    void callCancelRequested();
    void callAnswered(const QString& callId);
    void callRejected(const QString& callId);
    void callHungUp();
    void callMuteToggled(bool muted);
    void screenShareToggled();
    void remoteControlToggled();
    void avatarClicked();
    void globalSearchRequested(const QString& keyword, int tab);
    void searchResultJumpRequested(const QString& conversationId, const QString& messageId);
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
    void windowMinimizedToTrayRequested();
    void windowInteractiveStateChanged();
    void messageUrlOpenRequested(const QString& url);
    void olderMessagesRequested(const QString& conversationId, const QString& beforeMessageId);

    void fileServiceDownloadRequested(const QString& messageId);
    void fileServiceVersionHistoryRequested(const QString& messageId);
    void notificationMarkedReadRequested(const QString& notificationId);
    void notificationArchivedRequested(const QString& notificationId);
    void notificationsMarkAllReadRequested();
    void reminderDoneRequested(const QString& reminderId);
    void reminderSnoozeRequested(const QString& reminderId, int minutes);
    void groupFileServiceSaveRequested(const GroupFileServiceConfig& config);
    void groupFileManagerRequested(const QString& groupId, const GroupFileServiceConfig& config);

    void groupFileDownloadRequested(const QString& messageId);
    void groupFileOpenRequested(const QString& messageId);
    void viewportReachedBottom();
    void dataExportRequested();
    void dataImportRequested();

private:
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
#ifdef Q_OS_WIN
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
    void refreshTheme();

    // 信号转发辅助：将 ConversationsPage 信号连接到 MainWindow 信号
    void connectConversationsPageSignals();
    QWidget* buildGlobalSearchWidget();
    void showGlobalSearchPanel();
    void hideGlobalSearchPanel();
    void applyGlobalSearchStyle();

    QString m_navMsgKey;
    QString m_navNotifKey;
    QString m_navContactKey;
    QString m_navAiKey;
    QString m_navSettingsKey;
    QHash<QString, QString> m_primaryPageKeys;
    SettingsPage* m_settingsPage = nullptr;

    ConversationsPage* m_conversationsPage = nullptr;
    DirectoryPage* m_directoryPage = nullptr;
    NotificationsPage* m_notificationsPage = nullptr;
    KnowledgePage* m_knowledgePage = nullptr;
    ContactListModel* m_contactModel = nullptr;
    ConversationListModel* m_conversationModel = nullptr;
    QVector<GroupSummary> m_directoryGroups;
    QPointer<QWidget> m_globalSearchHost;
    QPointer<QLineEdit> m_globalSearchEdit;
    GlobalSearchPanel* m_globalSearchPanel = nullptr;
    GlobalSearchHistory* m_globalSearchHistory = nullptr;

    QString m_conversationsPageKey;
    QString m_directoryPageKey;
    QString m_notificationsPageKey;
    QString m_knowledgePageKey;

    QPointer<CallWindow> m_callWindow;
    UpdateBar* m_updateBar = nullptr;
    QString m_localDisplayName;
    QPixmap m_currentAvatarPixmap;
    bool m_dropTargetRegistered = false;

    void forceRegisterOleDropTarget();
};
