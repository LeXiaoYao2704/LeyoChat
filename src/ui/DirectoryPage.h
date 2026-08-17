#pragma once
#include "ElaScrollPage.h"
#include <QHash>
#include <QVector>

class ElaListView;
class ElaTreeView;
class ElaText;
class ElaLineEdit;
class ElaFrame;
class QLabel;
class QListWidget;
class QStackedWidget;
class QStandardItemModel;
class ElaPushButton;
class AlphabetIndexBar;
class ContactListModel;

struct GroupSummary {
    QString groupId;
    QString groupName;
    QString ownerDisplayName;
    int memberCount = 0;
    int unreadCount = 0;
    qint64 lastActiveMs = 0;
};

struct OrgContactEntry {
    QString clientId;
    QString displayName;
    QString jobTitle;
    bool isOnline = false;
};

class DirectoryPage : public ElaScrollPage {
    Q_OBJECT
public:
    explicit DirectoryPage(QWidget* parent = nullptr);

    void setContactModel(ContactListModel* model);
    void setSelectedContactId(const QString& clientId);
    void syncContactWorkspaceStatus();
    ElaListView* contactList() const { return m_contactList; }

    void setGroups(const QVector<GroupSummary>& groups);
    void setOrgData(const QHash<QString, QVector<OrgContactEntry>>& departments);
    void switchToOrgTab();
    void refreshTheme();

signals:
    void contactSelected(const QString& clientId);
    void contactProfileRequested(const QString& clientId);
    void contactReminderRequested(const QString& contactId,
                                  const QString& displayName,
                                  const QString& previewSnapshot);
    void avatarProfileRequested(const QString& clientId, const QPoint& globalPos);
    void contactDeleteRequested(const QString& clientId);
    void openConversationRequested(const QString& clientId);
    void groupConversationRequested(const QString& groupId);
    void createGroupRequested();
    void addContactRequested();

private:
    void rebuildGroupList();
    void updateTabStyle();
    void updatePresenceFilterStyle();
    void updateDirectoryStats();

    // Hero panel
    QLabel* m_directoryTitleLabel = nullptr;
    QLabel* m_heroSummaryLabel = nullptr;
    QLabel* m_heroRuntimeLabel = nullptr;
    QLabel* m_contactTotalValueLabel = nullptr;
    QLabel* m_contactOnlineValueLabel = nullptr;
    QLabel* m_groupTotalValueLabel = nullptr;
    QLabel* m_groupUnreadValueLabel = nullptr;
    QLabel* m_myGroupValueLabel = nullptr;
    QLabel* m_departmentValueLabel = nullptr;

    // Search
    ElaLineEdit* m_searchEdit = nullptr;
    ElaFrame* m_presenceFilterTabs = nullptr;
    ElaPushButton* m_allFilterBtn = nullptr;
    ElaPushButton* m_onlineFilterBtn = nullptr;
    ElaPushButton* m_offlineFilterBtn = nullptr;
    int m_presenceFilterIndex = 0;

    // Tab switching
    ElaFrame* m_directorySegmentedTabs = nullptr;
    ElaPushButton* m_contactsTabBtn = nullptr;
    ElaPushButton* m_groupsTabBtn = nullptr;
    ElaPushButton* m_orgTabBtn = nullptr;
    QStackedWidget* m_tabStack = nullptr;
    int m_activeTabIndex = 0;

    // Contacts view
    ElaListView* m_contactList = nullptr;
    AlphabetIndexBar* m_alphabetIndexBar = nullptr;
    ElaText* m_contactEmptyLabel = nullptr;
    ElaText* m_contactsModeChip = nullptr;
    ElaText* m_contactsStatusChip = nullptr;

    // Groups view
    QListWidget* m_groupListWidget = nullptr;
    QLabel* m_groupEmptyLabel = nullptr;
    QVector<GroupSummary> m_groups;

    // Org structure view
    ElaTreeView* m_orgTreeWidget = nullptr;
    QStandardItemModel* m_orgTreeModel = nullptr;
    ElaText* m_orgEmptyLabel = nullptr;
    int m_departmentCount = 0;

    qint64 m_contactOnlineHoldUntilMs = 0;
    int m_lastStableOnlineCount = 0;
};
