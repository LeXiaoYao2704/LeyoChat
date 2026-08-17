#pragma once
#include "ElaScrollPage.h"
#include <QVector>

#include "domain/ReminderItem.h"
#include "domain/SystemNotificationItem.h"

class ElaFrame;
class ElaListWidget;
class ElaPushButton;
class ElaText;
class QListWidgetItem;
class QWidget;
#ifdef LEYOCHAT_HAS_WEBENGINE
class QWebEngineView;
#endif

class NotificationsPage : public ElaScrollPage {
    Q_OBJECT
public:
    explicit NotificationsPage(QWidget* parent = nullptr);

    void setNotificationItems(const QVector<SystemNotificationItem>& items);
    void appendNotificationItem(const SystemNotificationItem& item);
    void setActiveReminders(const QVector<ReminderItem>& reminders);
    void markAllNotificationsRead();
    void setStatusMessage(const QString& message, int timeoutMs = 0);
    void refreshTheme();

signals:
    void notificationMarkedReadRequested(const QString& notificationId);
    void notificationArchivedRequested(const QString& notificationId);
    void notificationsMarkAllReadRequested();
    void reminderDoneRequested(const QString& reminderId);
    void reminderSnoozeRequested(const QString& reminderId, int minutes);
    void messageUrlOpenRequested(const QString& url);
    void unreadCountChanged(int count);

private:
    void refreshNotificationList();
    void refreshActiveReminderList();
    QWidget* createNotificationItemCard(const SystemNotificationItem& item);
    QWidget* createActiveReminderCard(const ReminderItem& item);
    void selectNotificationById(const QString& notificationId);
    bool markNotificationReadLocally(const QString& notificationId);
    bool archiveNotificationLocally(const QString& notificationId);
    void updateNotificationDetailPane();
    void syncNotificationWorkspaceStatus();
#ifdef LEYOCHAT_HAS_WEBENGINE
    void ensureNotificationBodyView();
#endif

    ElaListWidget* m_notificationList = nullptr;
    ElaPushButton* m_notificationTestButton = nullptr;
    ElaText* m_notificationModeChip = nullptr;
    ElaText* m_notificationStatusChip = nullptr;
    ElaText* m_notificationEmptyLabel = nullptr;
    ElaFrame* m_activeReminderSection = nullptr;
    ElaText* m_activeReminderStatus = nullptr;
    ElaListWidget* m_activeReminderList = nullptr;

    ElaText* m_notificationDetailSource = nullptr;
    ElaText* m_notificationDetailTitle = nullptr;
    ElaText* m_notificationDetailSummary = nullptr;
    ElaText* m_notificationDetailDetail = nullptr;
    ElaText* m_notificationDetailTimestamp = nullptr;
    ElaPushButton* m_notificationMarkReadButton = nullptr;
    ElaPushButton* m_notificationArchiveButton = nullptr;
    ElaPushButton* m_notificationOpenButton = nullptr;
    ElaPushButton* m_notificationCopyLinkButton = nullptr;
    ElaFrame* m_notificationDetailCard = nullptr;
#ifdef LEYOCHAT_HAS_WEBENGINE
    QWebEngineView* m_notificationBodyView = nullptr;
#endif

    QVector<SystemNotificationItem> m_notificationItems;
    QVector<ReminderItem> m_activeReminders;
    QString m_selectedNotificationId;
};
