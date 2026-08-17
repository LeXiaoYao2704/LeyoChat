#pragma once

#include <QPoint>
#include <QWidget>

#include "ui/GroupMemberListEntry.h"
#include "integrations/RemoteFileServiceSettings.h"

class ElaCheckBox;
class ElaLineEdit;
class ElaPushButton;
class ElaSpinBox;
class ElaText;
class QFrame;
class ElaFrame;
class QListWidget;
class ElaListWidget;
class QPushButton;
class QStackedWidget;
class ElaStackedWidget;

class GroupInfoPanel : public QWidget {
    Q_OBJECT

public:
    explicit GroupInfoPanel(QWidget* parent = nullptr);
    void refreshTheme();

    void setGroupSummary(const QString& groupName,
                         const QString& announcement,
                         const GroupMemberListEntries& members,
                         bool currentUserCanManageMembers);
    void setHybridRuntimeSummary(const QString& badge, const QString& detail);

    void setGroupId(const QString& groupId);
    void setGroupFileServiceConfig(const GroupFileServiceConfig& config, bool canEdit);
    void updateSyncStatus(const QString& text);
    void showDetailView();
    void showFileServiceSettingsView();
    bool isShowingFileServiceSettingsView() const;

    QString groupTitleText() const;
    QString announcementText() const;
    int memberCount() const;
    QString memberDisplayName(const QString& clientId) const;

signals:
    void memberActivated(const QString& clientId);
    void memberAvatarHovered(const QString& clientId, const QPoint& globalPos);
    void memberAvatarHoverLeft();
    void memberContextMenuRequested(const QString& clientId, const QPoint& globalPos);
    void fileServiceConfigSaveRequested(const GroupFileServiceConfig& config);
    void sharedFilesClicked();
    void announcementEditRequested();
    void groupAnnouncementReminderRequested(const QString& groupId,
                                            const QString& groupName,
                                            const QString& announcement);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ElaText* m_modeChipLabel = nullptr;
    ElaText* m_consoleChipLabel = nullptr;
    ElaText* m_topHintLabel = nullptr;
    ElaText* m_groupAvatarLabel = nullptr;
    ElaText* m_groupTitleLabel = nullptr;
    ElaText* m_groupSubtitleLabel = nullptr;
    ElaText* m_memberCountLabel = nullptr;
    ElaText* m_announcementChipLabel = nullptr;
    ElaText* m_memberChipLabel = nullptr;
    ElaText* m_memberSectionLabel = nullptr;
    ElaText* m_announcementTextLabel = nullptr;
    ElaPushButton* m_announcementReminderButton = nullptr;
    QPushButton* m_announcementEditButton = nullptr;
    ElaText* m_runtimeChipLabel = nullptr;
    ElaText* m_runtimeDetailLabel = nullptr;
    ElaText* m_sharedFilesArrow = nullptr;
    ElaFrame* m_sharedFilesCard = nullptr;
    bool m_fileServiceAvailable = false;
    QFrame* m_membersCard = nullptr;
    ElaListWidget* m_memberListWidget = nullptr;
    GroupMemberListEntries m_memberEntries;
    bool m_currentUserCanManageMembers = false;

    // Tab switching
    QWidget*        m_detailPage             = nullptr;
    QWidget*        m_settingsPage           = nullptr;
    ElaStackedWidget* m_stackedWidget          = nullptr;
    ElaPushButton*  m_settingsBackButton     = nullptr;

    // Group settings tab — file service form
    QString         m_currentGroupId;
    ElaFrame*         m_fileServiceCard        = nullptr;
    ElaCheckBox*    m_fsEnabledCheck         = nullptr;
    ElaLineEdit*    m_fsBaseUrlEdit          = nullptr;
    ElaLineEdit*    m_fsBearerTokenEdit      = nullptr;
    ElaLineEdit*    m_fsWorkspaceIdEdit      = nullptr;
    ElaPushButton*  m_fsTestBtn              = nullptr;
    ElaPushButton*  m_fsSaveBtn              = nullptr;
    ElaText*        m_fsSyncStatusLabel      = nullptr;
    ElaText*        m_fsReadonlyNotice       = nullptr;
    ElaSpinBox*     m_fsTtlDaysSpin          = nullptr;
    ElaSpinBox*     m_fsQuotaMbSpin          = nullptr;
    GroupFileServiceConfig m_lastFileServiceConfig;
    bool            m_lastFileServiceEditable = false;

    void switchToTab(int index);
    QWidget* buildDetailPage();
    QWidget* buildSettingsPage();
};
