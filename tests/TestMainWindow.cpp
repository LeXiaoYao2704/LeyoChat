#include <QtTest/QTest>
#include <QAbstractButton>
#include <QAction>
#include <QCoreApplication>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QGraphicsEffect>
#include <QImage>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QScreen>
#include <QSplitter>
#include <QStackedWidget>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QInputMethodEvent>
#include <QTextEdit>

#include <ElaToolButton.h>

#include "app/ReminderActionRouting.h"
#include "architecture/RuntimeArchitectureSnapshot.h"
#include "domain/ChatMessage.h"
#include "domain/ResourceRefPayload.h"
#include "services/ResourceRefRouter.h"
#include "ui/ContactListModel.h"
#include "ui/AppStyle.h"
#include "ui/ChatComposerWidget.h"
#include "ui/ChatHeaderWidget.h"
#include "ui/CloseToTrayDialog.h"
#include "ui/ConnectIpDialog.h"
#include "ui/ConversationCardDelegate.h"
#include "ui/ConversationListModel.h"
#include "ui/ConversationsPage.h"
#include "ui/DirectoryPage.h"
#include "ui/GroupInfoPanel.h"
#include "ui/MainWindow.h"
#include "ui/MessageListModel.h"
#include "ui/NotificationsPage.h"
#include "ui/TransferListModel.h"

namespace {
GroupMemberListEntries makeGroupMembers(std::initializer_list<GroupMemberListEntry> members)
{
    GroupMemberListEntries entries;
    entries.reserve(static_cast<qsizetype>(members.size()));
    for (const GroupMemberListEntry& member : members) {
        entries.push_back(member);
    }
    return entries;
}

QTextEdit* composerEditor(MainWindow& window)
{
    return window.findChild<QTextEdit*>(QStringLiteral("composerMessageEditor"));
}

PeerEndpoint makePeerEndpoint(const QString& clientId,
                              const QString& displayName,
                              const QString& host,
                              quint16 port,
                              PeerPresenceStatus presence)
{
    PeerEndpoint endpoint;
    endpoint.clientId = clientId.toStdString();
    endpoint.displayName = displayName.toStdString();
    endpoint.host = host.toStdString();
    endpoint.port = port;
    endpoint.isConnected = presence != PeerPresenceStatus::Offline;
    endpoint.presence = presence;
    endpoint.lastPresenceAtMs = QDateTime::currentMSecsSinceEpoch();
    return endpoint;
}

QString widgetTextTree(QWidget* widget)
{
    if (!widget) {
        return {};
    }

    QStringList parts;
    for (auto* label : widget->findChildren<QLabel*>()) {
        if (label && !label->text().trimmed().isEmpty()) {
            parts.push_back(label->text());
        }
    }
    return parts.join(QStringLiteral("\n"));
}

QString notificationRowText(QListWidget* list, int row)
{
    if (!list || row < 0 || row >= list->count()) {
        return {};
    }

    if (QListWidgetItem* item = list->item(row)) {
        if (QWidget* widget = list->itemWidget(item)) {
            return widgetTextTree(widget);
        }
        return item->text();
    }
    return {};
}

class WidgetEventCounter final : public QObject {
public:
    int updateRequestCount = 0;
    int paintCount = 0;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        Q_UNUSED(watched);
        if (!event) {
            return false;
        }
        if (event->type() == QEvent::UpdateRequest) {
            ++updateRequestCount;
        } else if (event->type() == QEvent::Paint) {
            ++paintCount;
        }
        return false;
    }
};
}

class TestMainWindow : public QObject {
    Q_OBJECT

private slots:
    void recoveryNavigationUsesStablePageIds()
    {
        MainWindow window;

        QVERIFY(window.navigateToRecoveryPage(QStringLiteral("directory")));
        QCOMPARE(window.recoveryPageId(), QStringLiteral("directory"));
        QVERIFY(window.navigateToRecoveryPage(QStringLiteral("messages")));
        QCOMPARE(window.recoveryPageId(), QStringLiteral("messages"));
        QVERIFY(!window.navigateToRecoveryPage(QStringLiteral("unknown")));
    }

    void stagedRecoveryComposerDoesNotSend()
    {
        MainWindow window;
        ComposerRecoveryContext context;
        context.composerHtml = QStringLiteral("<p>recovered draft</p>");
        context.replyMessageId = QStringLiteral("reply-1");
        context.replySenderId = QStringLiteral("sender-1");
        context.replySenderName = QStringLiteral("Alice");
        context.replyBody = QStringLiteral("reply body");
        context.editingMessageId = QStringLiteral("edit-1");
        context.editingBody = QStringLiteral("<p>recovered edit</p>");

        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);
        QSignalSpy fileSpy(&window, &MainWindow::fileSendRequested);
        QSignalSpy editSaveSpy(&window, &MainWindow::editSaveRequested);
        window.stageRecoveredComposerContext(QStringLiteral("conversation-1"), context);
        window.restoreComposerDraft(QStringLiteral("conversation-1"));

        QCOMPARE(sendSpy.count(), 0);
        QCOMPARE(fileSpy.count(), 0);
        QCOMPARE(editSaveSpy.count(), 0);
        QCOMPARE(window.activeComposerContextId(), QStringLiteral("conversation-1"));
        QCOMPARE(window.activeComposerRecoveryContext().replyMessageId,
                 QStringLiteral("reply-1"));
        QCOMPARE(window.activeComposerRecoveryContext().editingMessageId,
                 QStringLiteral("edit-1"));
        QVERIFY(composerEditor(window)->toHtml().contains(QStringLiteral("recovered edit")));
    }

    void appStyle_defaultsUnknownThemeToLight()
    {
        QCOMPARE(AppStyle::themeModeFromString(QString()), AppStyle::ThemeMode::Light);
        QCOMPARE(AppStyle::themeModeFromString(QStringLiteral("unknown-value")), AppStyle::ThemeMode::Light);
        QCOMPARE(AppStyle::themeModeFromString(QStringLiteral("system")), AppStyle::ThemeMode::FollowSystem);
    }

    void appStyle_usesPropertySelectorForNavigationButtons()
    {
        const QString stylesheet = AppStyle::stylesheet();
        QVERIFY(stylesheet.contains(QStringLiteral("QPushButton[navRole]")));
        QVERIFY(stylesheet.contains(QStringLiteral("QPushButton[navRole]:checked")));
    }

    void contactsPageExposesSearchEntry()
    {
        MainWindow window;

        auto* contactSearchEdit = window.findChild<QLineEdit*>(QStringLiteral("directorySearchEdit"));
        QVERIFY(contactSearchEdit != nullptr);
        QVERIFY(contactSearchEdit->placeholderText().contains(QStringLiteral("\u641c\u7d22\u8054\u7cfb\u4eba")));
    }

    void directoryContactContextMenuEmitsTomorrowFollowUpSnapshot()
    {
        DirectoryPage page;
        ContactListModel model(&page);
        model.setItems({
            makePeerEndpoint(QStringLiteral("client-1"),
                             QStringLiteral("Alice"),
                             QStringLiteral("192.0.2.3"),
                             9527,
                             PeerPresenceStatus::Online),
        });
        page.setContactModel(&model);

        QSignalSpy reminderSpy(&page, &DirectoryPage::contactReminderRequested);
        auto* contactList = page.findChild<QListView*>(QStringLiteral("directoryContactList"));
        QVERIFY(contactList != nullptr);

        page.resize(760, 640);
        page.show();
        QVERIFY(QTest::qWaitForWindowExposed(&page));
        QTest::qWait(20);

        QModelIndex contactIndex;
        for (int row = 0; row < model.rowCount(); ++row) {
            const QModelIndex index = model.index(row, 0);
            if (!index.data(ContactListModel::IsSectionHeaderRole).toBool()) {
                contactIndex = index;
                break;
            }
        }
        QVERIFY(contactIndex.isValid());

        const QRect rowRect = contactList->visualRect(contactIndex);
        QVERIFY(rowRect.isValid());

        QTimer::singleShot(50, []() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                for (QWidget* widget : QApplication::topLevelWidgets()) {
                    menu = qobject_cast<QMenu*>(widget);
                    if (menu) {
                        break;
                    }
                }
            }
            if (menu) {
                const QList<QAction*> actions = menu->actions();
                if (actions.size() >= 3) {
                    QAction* followUpAction = actions.at(2);
                    menu->setActiveAction(followUpAction);
                    QTest::keyClick(menu, Qt::Key_Return);
                }
            }
        });

        QMetaObject::invokeMethod(contactList,
                                  "customContextMenuRequested",
                                  Qt::DirectConnection,
                                  Q_ARG(QPoint, rowRect.center()));

        QCOMPARE(reminderSpy.count(), 1);
        const QList<QVariant> args = reminderSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("client-1"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("Alice"));
        QCOMPARE(args.at(2).toString(), QStringLiteral("在线"));
    }

    void mainWindowForwardsDirectoryContactReminderSnapshot()
    {
        MainWindow window;
        auto* directoryPage = window.findChild<DirectoryPage*>();
        QVERIFY(directoryPage != nullptr);

        QSignalSpy reminderSpy(&window, &MainWindow::contactReminderRequested);
        QMetaObject::invokeMethod(directoryPage,
                                  "contactReminderRequested",
                                  Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("client-1")),
                                  Q_ARG(QString, QStringLiteral("Alice")),
                                  Q_ARG(QString, QStringLiteral("Online")));

        QCOMPARE(reminderSpy.count(), 1);
        const QList<QVariant> args = reminderSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("client-1"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("Alice"));
        QCOMPARE(args.at(2).toString(), QStringLiteral("Online"));
    }

    void defaultWindowSize_keepsNotificationWorkspaceReadable()
    {
        MainWindow window;

        QSize expectedInitialSize(1520, 900);
        QSize expectedMinimumSize(1360, 860);
        if (const QScreen* screen = QGuiApplication::primaryScreen()) {
            const QSize available = screen->availableGeometry().size() - QSize(1, 1);
            expectedInitialSize.setWidth(qMin(expectedInitialSize.width(), available.width()));
            expectedInitialSize.setHeight(qMin(expectedInitialSize.height(), available.height()));
            expectedMinimumSize.setWidth(qMin(expectedMinimumSize.width(), qMax(960, available.width())));
            expectedMinimumSize.setHeight(qMin(expectedMinimumSize.height(), qMax(640, available.height())));
        }

        QCOMPARE(window.size(), expectedInitialSize.expandedTo(expectedMinimumSize));
        QCOMPARE(window.minimumSize(), expectedMinimumSize);
    }

    void workspacePanels_keepCompactVerticalPolicies()
    {
        MainWindow window;

        auto* directoryToolbar =
            window.findChild<QWidget*>(QStringLiteral("directoryToolbar"));
        auto* transferFilterBand =
            window.findChild<QFrame*>(QStringLiteral("transferFilterBand"));

        QVERIFY(directoryToolbar != nullptr);
        QVERIFY(transferFilterBand != nullptr);
        QVERIFY(directoryToolbar->sizePolicy().verticalPolicy() != QSizePolicy::Expanding);
        QVERIFY(transferFilterBand->sizePolicy().verticalPolicy() != QSizePolicy::Expanding);
    }

    void directoryAndTransferPanels_exposeToolsWithoutStandaloneTitles()
    {
        MainWindow window;

        QVERIFY(window.findChild<QFrame*>(QStringLiteral("directoryHeroPanel")) != nullptr);
        QVERIFY(window.findChild<QWidget*>(QStringLiteral("directoryToolbar")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("directoryPresenceFilterTabs")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("directorySegmentedTabs")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("transferHeaderCard")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("transferFilterBand")) != nullptr);

        const auto sideHeaderTitles = window.findChildren<QLabel*>(QStringLiteral("sideHeaderTitle"));
        QStringList texts;
        for (QLabel* label : sideHeaderTitles) {
            texts.push_back(label->text());
        }

        QVERIFY(!texts.contains(QStringLiteral("\u8054\u7cfb\u4eba")));
        QVERIFY(!texts.contains(QStringLiteral("浼犺緭")));
    }

    void groupWorkspace_switchesConversationSidebarToGroupSemantics()
    {
        MainWindow window;

        auto* conversationsPage = window.findChild<ConversationsPage*>(QStringLiteral("chatPageRoot"));
        auto* modeChip = window.findChild<QLabel*>(QStringLiteral("conversationsModeChip"));
        QAbstractButton* filterToggleBtn = nullptr;
        for (QAbstractButton* button : window.findChildren<QAbstractButton*>(QStringLiteral("sideIconBtn"))) {
            if (button && button->toolTip().contains(QStringLiteral("\u7b5b\u9009"))) {
                filterToggleBtn = button;
                break;
            }
        }

        QVERIFY(conversationsPage != nullptr);
        QVERIFY(modeChip != nullptr);
        QVERIFY(filterToggleBtn != nullptr);

        QVERIFY(modeChip->text().contains(QStringLiteral("\u6d88\u606f")));
        QVERIFY(!filterToggleBtn->isHidden());

        conversationsPage->setGroupWorkspaceMode(true);
        conversationsPage->syncConversationSidebarMode();

        QVERIFY(modeChip->text().contains(QStringLiteral("\u7fa4\u804a")));
        QVERIFY(filterToggleBtn->isHidden());
    }

    void notificationsWorkspace_hasDedicatedNavEntry()
    {
        MainWindow window;

        auto* notificationsPage = window.findChild<NotificationsPage*>();
        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* notificationModeChip =
            window.findChild<QLabel*>(QStringLiteral("notificationModeChip"));
        auto* notificationStatusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));

        QVERIFY(notificationsPage != nullptr);
        QVERIFY(notificationList != nullptr);
        QVERIFY(notificationModeChip != nullptr);
        QVERIFY(notificationStatusChip != nullptr);
        QVERIFY(notificationModeChip->text().contains(QStringLiteral("\u901a\u77e5")));
    }

    void notificationsWorkspace_listsNotificationsFromAllSources()
    {
        MainWindow window;

        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("\u6784\u5efa\u5931\u8d25"), QStringLiteral("Build #18"), QStringLiteral("failed"),
             QStringLiteral("\u6253\u5f00\u539f\u59cb\u9875\u9762"), QStringLiteral("https://dev.azure.com/example"), QString(), 1, true},
            {QStringLiteral("n2"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("\u90ae\u4ef6\u63d0\u9192"), QStringLiteral("\u6765\u81ea\u5f20\u4e09"), QStringLiteral("inbox"),
             QStringLiteral("\u6253\u5f00\u90ae\u4ef6"), QStringLiteral("https://outlook.office.com/mail"), QString(), 2, true},
            {QStringLiteral("n3"), QStringLiteral("system"), QStringLiteral("\u7cfb\u7edf"),
             QStringLiteral("\u7248\u672c\u66f4\u65b0"), QStringLiteral("\u5df2\u5b89\u88c5\u6210\u529f"), QStringLiteral("local"),
             QString(), QString(), QString(), 3, false},
        });

        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* modeChip =
            window.findChild<QLabel*>(QStringLiteral("notificationModeChip"));
        auto* statusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));

        QVERIFY(notificationList != nullptr);
        QVERIFY(modeChip != nullptr);
        QVERIFY(statusChip != nullptr);
        QCOMPARE(notificationList->count(), 3);
        QCOMPARE(modeChip->text(), QStringLiteral("通知 3"));
        QCOMPARE(statusChip->text(), QStringLiteral("未读 2"));
        QVERIFY(notificationRowText(notificationList, 0).contains(QStringLiteral("构建失败")));
        QVERIFY(notificationRowText(notificationList, 1).contains(QStringLiteral("邮件提醒")));
        QVERIFY(notificationRowText(notificationList, 2).contains(QStringLiteral("版本更新")));
    }

    void notificationsWorkspace_statusChipsReflectTotalsAndUnread()
    {
        MainWindow window;

        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("构建失败"), QStringLiteral("Build #18"), QStringLiteral("failed"),
             QStringLiteral("打开原始页面"), QStringLiteral("https://dev.azure.com/example"), QString(), 1, true},
            {QStringLiteral("n2"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("邮件提醒"), QStringLiteral("来自张三"), QStringLiteral("inbox"),
             QStringLiteral("打开邮件"), QStringLiteral("https://outlook.office.com/mail"), QString(), 2, true},
            {QStringLiteral("n3"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("会议提醒"), QStringLiteral("15:00 项目例会"), QStringLiteral("calendar"),
             QStringLiteral("打开日程"), QStringLiteral("https://outlook.office.com/calendar"), QString(), 3, false},
        });

        auto* statusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));
        auto* modeChip =
            window.findChild<QLabel*>(QStringLiteral("notificationModeChip"));
        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        QSignalSpy readSpy(&window, &MainWindow::notificationMarkedReadRequested);

        QVERIFY(statusChip != nullptr);
        QVERIFY(modeChip != nullptr);
        QVERIFY(notificationList != nullptr);
        QCOMPARE(modeChip->text(), QStringLiteral("通知 3"));
        QCOMPARE(statusChip->text(), QStringLiteral("未读 2"));

        notificationList->setCurrentRow(0);
        QCOMPARE(readSpy.count(), 1);
        QCOMPARE(statusChip->text(), QStringLiteral("未读 1"));

        notificationList->setCurrentRow(1);
        QCOMPARE(readSpy.count(), 2);
        QVERIFY(statusChip->isHidden());
        QCOMPARE(modeChip->text(), QStringLiteral("通知 3"));
    }

    void uses_echo_style_shell_pages()
    {
        MainWindow window;

        QCOMPARE(window.primaryPageCountForTesting(), 6);
        QVERIFY(window.hasPrimaryPageForTesting(QStringLiteral("messages")));
        QVERIFY(window.hasPrimaryPageForTesting(QStringLiteral("directory")));
        QVERIFY(window.hasPrimaryPageForTesting(QStringLiteral("notifications")));
        QVERIFY(window.hasPrimaryPageForTesting(QStringLiteral("knowledge")));
        QVERIFY(window.hasPrimaryPageForTesting(QStringLiteral("workbench")));
        QVERIFY(window.hasPrimaryPageForTesting(QStringLiteral("settings")));
    }

    void auxiliary_pages_use_current_page_scaffolds()
    {
        MainWindow window;

        auto* transfersScaffold =
            window.findChild<QWidget*>(QStringLiteral("secondaryPageScaffold_transfers"));
        auto* settingsScaffold =
            window.findChild<QWidget*>(QStringLiteral("secondaryPageScaffold_settings"));
        auto* directoryPage =
            window.findChild<DirectoryPage*>();
        auto* directoryRoot =
            window.findChild<QWidget*>(QStringLiteral("directoryPageRoot"));

        QVERIFY(transfersScaffold != nullptr);
        QVERIFY(settingsScaffold != nullptr);
        QVERIFY(directoryPage != nullptr);
        QVERIFY(directoryRoot != nullptr);

        QCOMPARE(transfersScaffold->property("pageScaffoldRole").toString(),
                 QStringLiteral("secondary"));
        QCOMPARE(settingsScaffold->property("pageScaffoldRole").toString(),
                 QStringLiteral("secondary"));
    }

    void close_to_tray_dialog_uses_stable_dialog_object_name()
    {
        MainWindow window;
        CloseToTrayDialog dialog(&window);

        QCOMPARE(dialog.objectName(),
                 QStringLiteral("closeToTrayDialog"));
    }

    void connect_ip_dialog_uses_shared_input_dialog_sections()
    {
        MainWindow window;
        ConnectIpDialog dialog(&window);

        QCOMPARE(dialog.objectName(),
                 QStringLiteral("secondaryPageScaffold_connectIpDialog"));
        QCOMPARE(dialog.property("pageScaffoldRole").toString(),
                 QStringLiteral("secondary"));
        QCOMPARE(dialog.property("pageScaffoldKey").toString(),
                 QStringLiteral("connectIpDialog"));
        QVERIFY(dialog.findChild<QWidget*>(QStringLiteral("connectIpDialogInputSection")) != nullptr);
        QVERIFY(dialog.findChild<QWidget*>(QStringLiteral("connectIpDialogActionSection")) != nullptr);
    }

    void message_page_has_sidebar_widget()
    {
        MainWindow window;

        QVERIFY(window.findChild<QWidget*>(QStringLiteral("conversationSidebarWidget")) != nullptr);
    }

    void message_page_has_chat_workspace_widget()
    {
        MainWindow window;

        QVERIFY(window.findChild<QWidget*>(QStringLiteral("chatWorkspaceWidget")) != nullptr);
    }

    void notificationsWorkspace_selectedItemPopulatesDetailActions()
    {
        MainWindow window;

        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("\u90ae\u4ef6\u63d0\u9192"),
             QStringLiteral("\u6765\u81ea\u5f20\u4e09"),
             QStringLiteral("\u6536\u4ef6\u7bb1\u5df2\u5230\u8fbe"),
             QStringLiteral("\u6253\u5f00\u90ae\u4ef6"),
             QStringLiteral("https://outlook.office.com/mail"), QString(), 2, true},
        });
        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* sourceLabel =
            window.findChild<QLabel*>(QStringLiteral("notificationDetailSource"));
        auto* titleLabel =
            window.findChild<QLabel*>(QStringLiteral("notificationDetailTitle"));
        auto* openButton =
            window.findChild<QPushButton*>(QStringLiteral("notificationOpenButton"));

        QVERIFY(notificationList != nullptr);
        QVERIFY(sourceLabel != nullptr);
        QVERIFY(titleLabel != nullptr);
        QVERIFY(openButton != nullptr);

        notificationList->setCurrentRow(0);

        QCOMPARE(sourceLabel->text(), QStringLiteral("Outlook"));
        QCOMPARE(titleLabel->text(), QStringLiteral("邮件提醒"));
        QVERIFY(!openButton->isHidden());
        QCOMPARE(openButton->text(), QStringLiteral("打开邮件"));
    }

    void notificationsWorkspace_unreadCountSignalTracksLocalReadState()
    {
        MainWindow window;
        auto* notificationsPage = window.findChild<NotificationsPage*>();
        QVERIFY(notificationsPage != nullptr);
        QSignalSpy unreadSpy(notificationsPage, &NotificationsPage::unreadCountChanged);

        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("构建失败"), QStringLiteral("Build #18"), QStringLiteral("failed"),
             QStringLiteral("打开原始页面"), QStringLiteral("https://dev.azure.com/example"), QString(), 1, true},
            {QStringLiteral("n2"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("邮件提醒"), QStringLiteral("来自张三"), QStringLiteral("inbox"),
             QStringLiteral("打开邮件"), QStringLiteral("https://outlook.office.com/mail"), QString(), 2, true},
            {QStringLiteral("n3"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("会议提醒"), QStringLiteral("15:00 项目例会"), QStringLiteral("calendar"),
             QStringLiteral("打开日程"), QStringLiteral("https://outlook.office.com/calendar"), QString(), 3, false},
        });

        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* statusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));

        QVERIFY(notificationList != nullptr);
        QVERIFY(statusChip != nullptr);
        QVERIFY(unreadSpy.count() >= 1);
        QCOMPARE(unreadSpy.last().at(0).toInt(), 2);
        QCOMPARE(statusChip->text(), QStringLiteral("未读 2"));

        notificationList->setCurrentRow(0);
        QVERIFY(unreadSpy.count() >= 2);
        QCOMPARE(unreadSpy.last().at(0).toInt(), 1);
        QCOMPARE(statusChip->text(), QStringLiteral("未读 1"));
    }

    void notificationsWorkspace_emitsReadAndArchiveActions()
    {
        MainWindow window;
        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("PR 更新"),
             QStringLiteral("需要你查看"),
             QStringLiteral("等待处理"),
             QStringLiteral("打开原始页面"),
             QStringLiteral("https://dev.azure.com/example"), QString(), 2, true},
        });

        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* markReadButton =
            window.findChild<QPushButton*>(QStringLiteral("notificationMarkReadButton"));
        auto* archiveButton =
            window.findChild<QPushButton*>(QStringLiteral("notificationArchiveButton"));

        QVERIFY(notificationList != nullptr);
        QVERIFY(markReadButton != nullptr);
        QVERIFY(archiveButton != nullptr);

        QSignalSpy readSpy(&window, &MainWindow::notificationMarkedReadRequested);
        QSignalSpy archiveSpy(&window, &MainWindow::notificationArchivedRequested);

        notificationList->setCurrentRow(0);

        QVERIFY(markReadButton->isHidden());
        QVERIFY(!archiveButton->isHidden());
        QCOMPARE(readSpy.count(), 1);
        QCOMPARE(readSpy.takeFirst().at(0).toString(), QStringLiteral("n1"));

        archiveButton->click();
        QCOMPARE(archiveSpy.count(), 1);
        QCOMPARE(archiveSpy.takeFirst().at(0).toString(), QStringLiteral("n1"));
    }

    void notificationsWorkspace_forwardsActiveReminderActions()
    {
        MainWindow window;
        ReminderItem reminder;
        reminder.reminderId = QStringLiteral("reminder/with space");
        reminder.targetType = QStringLiteral("message");
        reminder.titleSnapshot = QStringLiteral("Reply to Alice");
        reminder.previewSnapshot = QStringLiteral("Confirm tomorrow");
        reminder.dueAtMs = 333;
        window.setActiveReminders({reminder});

        auto* reminderList = window.findChild<QListWidget*>(QStringLiteral("activeReminderList"));
        auto* openButton = window.findChild<QPushButton*>(QStringLiteral("activeReminderOpenButton"));
        auto* doneButton = window.findChild<QPushButton*>(QStringLiteral("activeReminderDoneButton"));
        auto* snoozeButton = window.findChild<QPushButton*>(QStringLiteral("activeReminderSnoozeButton"));
        QVERIFY(reminderList != nullptr);
        QVERIFY(openButton != nullptr);
        QVERIFY(doneButton != nullptr);
        QVERIFY(snoozeButton != nullptr);
        QCOMPARE(reminderList->count(), 1);

        QSignalSpy openSpy(&window, &MainWindow::messageUrlOpenRequested);
        QSignalSpy doneSpy(&window, &MainWindow::reminderDoneRequested);
        QSignalSpy snoozeSpy(&window, &MainWindow::reminderSnoozeRequested);

        openButton->click();
        doneButton->click();
        snoozeButton->click();

        QCOMPARE(openSpy.count(), 1);
        QCOMPARE(openSpy.takeFirst().at(0).toString(),
                 reminderActionUrl(QStringLiteral("open"), reminder.reminderId));
        QCOMPARE(doneSpy.count(), 1);
        QCOMPARE(doneSpy.takeFirst().at(0).toString(), reminder.reminderId);
        QCOMPARE(snoozeSpy.count(), 1);
        const QList<QVariant> snoozeArgs = snoozeSpy.takeFirst();
        QCOMPARE(snoozeArgs.at(0).toString(), reminder.reminderId);
        QCOMPARE(snoozeArgs.at(1).toInt(), 30);
    }

    void notificationsWorkspace_markAllReadUpdatesStatusChip()
    {
        MainWindow window;
        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("PR 更新"),
             QStringLiteral("需要你查看"),
             QStringLiteral("等待处理"),
             QStringLiteral("打开原始页面"),
             QStringLiteral("https://dev.azure.com/example"), QString(), 2, true},
            {QStringLiteral("n2"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("邮件提醒"),
             QStringLiteral("来自张三"),
             QStringLiteral("收件箱已到达"),
             QStringLiteral("打开邮件"),
             QStringLiteral("https://outlook.office.com/mail"), QString(), 3, true},
        });

        auto* notificationsPage = window.findChild<NotificationsPage*>();
        auto* statusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));
        QSignalSpy markAllSpy(&window, &MainWindow::notificationsMarkAllReadRequested);

        QVERIFY(notificationsPage != nullptr);
        QVERIFY(statusChip != nullptr);
        QCOMPARE(statusChip->text(), QStringLiteral("未读 2"));

        notificationsPage->markAllNotificationsRead();

        QCOMPARE(markAllSpy.count(), 1);
        QVERIFY(statusChip->isHidden());
    }

    void notificationsWorkspace_markAllReadClearsUnreadStateInList()
    {
        MainWindow window;
        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("PR 更新"),
             QStringLiteral("需要你查看"),
             QStringLiteral("等待处理"),
             QStringLiteral("打开原始页面"),
             QStringLiteral("https://dev.azure.com/example"), QString(), 2, true},
            {QStringLiteral("n2"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("邮件提醒"),
             QStringLiteral("来自张三"),
             QStringLiteral("收件箱已到达"),
             QStringLiteral("打开邮件"),
             QStringLiteral("https://outlook.office.com/mail"), QString(), 3, true},
        });

        auto* notificationsPage = window.findChild<NotificationsPage*>();
        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* statusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));

        QVERIFY(notificationsPage != nullptr);
        QVERIFY(notificationList != nullptr);
        QVERIFY(statusChip != nullptr);
        QCOMPARE(statusChip->text(), QStringLiteral("未读 2"));

        notificationsPage->markAllNotificationsRead();

        QVERIFY(statusChip->isHidden());
        QCOMPARE(notificationList->count(), 2);
        QVERIFY(!notificationRowText(notificationList, 0).contains(QStringLiteral("●")));
        QVERIFY(!notificationRowText(notificationList, 1).contains(QStringLiteral("●")));
    }

    void notificationsWorkspace_selectingItemMarksItReadLocally()
    {
        MainWindow window;
        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("PR 更新"),
             QStringLiteral("需要你查看"),
             QStringLiteral("等待处理"),
             QStringLiteral("打开原始页面"),
             QStringLiteral("https://dev.azure.com/example"), QString(), 1, true},
            {QStringLiteral("n2"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("邮件提醒"),
             QStringLiteral("来自张三"),
             QStringLiteral("收件箱已到达"),
             QStringLiteral("打开邮件"),
             QStringLiteral("https://outlook.office.com/mail"), QString(), 2, false},
        });

        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* statusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));
        QSignalSpy readSpy(&window, &MainWindow::notificationMarkedReadRequested);

        QVERIFY(notificationList != nullptr);
        QVERIFY(statusChip != nullptr);
        QCOMPARE(statusChip->text(), QStringLiteral("未读 1"));

        notificationList->setCurrentRow(0);

        QCOMPARE(readSpy.count(), 1);
        QVERIFY(statusChip->isHidden());
        QVERIFY(!notificationRowText(notificationList, 0).contains(QStringLiteral("●")));
    }

    void notificationsWorkspace_archiveRemovesItemLocally()
    {
        MainWindow window;
        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("PR 更新"),
             QStringLiteral("需要你查看"),
             QStringLiteral("等待处理"),
             QStringLiteral("打开原始页面"),
             QStringLiteral("https://dev.azure.com/example"), QString(), 1, false},
        });

        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* archiveButton =
            window.findChild<QPushButton*>(QStringLiteral("notificationArchiveButton"));
        auto* emptyLabel =
            window.findChild<QLabel*>(QStringLiteral("notificationEmptyLabel"));
        QSignalSpy archiveSpy(&window, &MainWindow::notificationArchivedRequested);

        QVERIFY(notificationList != nullptr);
        QVERIFY(archiveButton != nullptr);
        QVERIFY(emptyLabel != nullptr);

        notificationList->setCurrentRow(0);
        archiveButton->click();

        QCOMPARE(archiveSpy.count(), 1);
        QCOMPARE(notificationList->count(), 0);
        QVERIFY(!emptyLabel->isHidden());
    }

    void notificationsWorkspace_openActionEmitsUrlAndMarksRead()
    {
        MainWindow window;
        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("azure-devops"), QStringLiteral("Azure DevOps"),
             QStringLiteral("PR 更新"),
             QStringLiteral("需要你查看"),
             QStringLiteral("等待处理"),
             QStringLiteral("打开原始页面"),
             QStringLiteral("https://dev.azure.com/example/_git/repo/pullrequest/12"), QString(), 1, true},
        });

        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* openButton =
            window.findChild<QPushButton*>(QStringLiteral("notificationOpenButton"));
        auto* statusChip =
            window.findChild<QLabel*>(QStringLiteral("notificationStatusChip"));
        QSignalSpy openSpy(&window, &MainWindow::messageUrlOpenRequested);
        QSignalSpy readSpy(&window, &MainWindow::notificationMarkedReadRequested);

        QVERIFY(notificationList != nullptr);
        QVERIFY(openButton != nullptr);
        QVERIFY(statusChip != nullptr);

        notificationList->setCurrentRow(0);
        openButton->click();

        QCOMPARE(openSpy.count(), 1);
        QCOMPARE(openSpy.takeFirst().at(0).toString(),
                 QStringLiteral("https://dev.azure.com/example/_git/repo/pullrequest/12"));
        QVERIFY(readSpy.count() >= 1);
        QVERIFY(statusChip->isHidden());
    }

    void notificationsWorkspace_copyLinkCopiesSelectedNotificationUrl()
    {
        MainWindow window;
        window.setNotificationItems({
            {QStringLiteral("n1"), QStringLiteral("outlook"), QStringLiteral("Outlook"),
             QStringLiteral("邮件提醒"),
             QStringLiteral("来自张三"),
             QStringLiteral("收件箱已到达"),
             QStringLiteral("打开邮件"),
             QStringLiteral("https://outlook.office.com/mail/inbox/id/42"), QString(), 1, false},
        });

        auto* notificationList =
            window.findChild<QListWidget*>(QStringLiteral("notificationList"));
        auto* copyButton =
            window.findChild<QPushButton*>(QStringLiteral("notificationCopyLinkButton"));

        QVERIFY(notificationList != nullptr);
        QVERIFY(copyButton != nullptr);
        QVERIFY(QGuiApplication::clipboard() != nullptr);

        notificationList->setCurrentRow(0);
        copyButton->click();

        QCOMPARE(QGuiApplication::clipboard()->text(),
                 QStringLiteral("https://outlook.office.com/mail/inbox/id/42"));
    }

    void windowBranding_usesLeyoChatDisplayName()
    {
        MainWindow window;

        QCOMPARE(window.windowTitle(), QStringLiteral("LeyoChat"));
    }

    void switchesBetweenWelcomeDirectAndGroupStates()
    {
        MainWindow window;

        window.setChatHeader(QString(), QString());
        QVERIFY(window.isShowingWelcomePage());
        QVERIFY(!window.isShowingChatPage());
        QVERIFY(!window.isGroupPanelVisible());

        window.setChatHeaderDirect(QStringLiteral("\u5F20\u4E09"),
                                   QStringLiteral("\u5728\u7EBF"),
                                   QStringLiteral("\u81EA\u52A8\u5316\u5DE5\u7A0B\u5E08"));
        QVERIFY(!window.isShowingWelcomePage());
        QVERIFY(window.isShowingChatPage());
        QVERIFY(!window.isGroupPanelVisible());

        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
            {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
            {QStringLiteral("wangwu"), QStringLiteral("\u738B\u4E94"), false, false, false},
        }));
        window.setChatHeaderGroup(QStringLiteral("group:project"),
                                  QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                                  3);
        QVERIFY(window.isShowingChatPage());
        QVERIFY(!window.isGroupPanelVisible());

        window.setChatHeader(QString(), QString());
        QVERIFY(window.isShowingWelcomePage());
    }

    void helperConversationTransitions_driveShellState()
    {
        MainWindow window;

        QVERIFY(window.isShowingWelcomePage());
        QVERIFY(!window.isShowingChatPage());
        QVERIFY(!window.isGroupPanelVisible());

        window.showDirectConversation(QStringLiteral("direct:zhangsan"),
                                      QStringLiteral("\u5F20\u4E09"));
        QVERIFY(!window.isShowingWelcomePage());
        QVERIFY(window.isShowingChatPage());
        QVERIFY(!window.isGroupPanelVisible());

        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QVERIFY(window.isShowingChatPage());
        QVERIFY(!window.isGroupPanelVisible());
    }

    void renamingGroupHeader_preservesGroupPanelMembers()
    {
        MainWindow window;

        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
            {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u65E7\u7FA4\u540D"));
        window.setGroupInfoPanel(QStringLiteral("\u6682\u65E0\u516C\u544A"),
                                 makeGroupMembers({
                                     {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                     {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                                 }),
                                 true);

        auto* panel = window.findChild<GroupInfoPanel*>();
        QVERIFY(panel != nullptr);
        QCOMPARE(panel->groupTitleText(), QStringLiteral("\u65E7\u7FA4\u540D"));
        QCOMPARE(panel->memberCount(), 2);

        window.setChatHeaderGroup(QStringLiteral("group:project"),
                                  QStringLiteral("\u65B0\u7FA4\u540D"),
                                  2);
        window.setGroupInfoPanel(QStringLiteral("\u6682\u65E0\u516C\u544A"),
                                 makeGroupMembers({
                                     {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                     {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                                 }),
                                 true);

        QCOMPARE(panel->groupTitleText(), QStringLiteral("\u65B0\u7FA4\u540D"));
        QCOMPARE(panel->memberCount(), 2);
    }

    void groupInfoRefresh_usesCurrentGroupHeaderTitleInsteadOfPanelFallback()
    {
        MainWindow window;

        window.setGroupInfoPanel(QStringLiteral("\u6682\u65E0\u516C\u544A"),
                                 makeGroupMembers({
                                     {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                     {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                                 }),
                                 true);

        auto* panel = window.findChild<GroupInfoPanel*>();
        QVERIFY(panel != nullptr);
        QVERIFY(panel->groupTitleText().contains(QStringLiteral("\u672A\u547D\u540D")));

        window.setChatHeaderGroup(QStringLiteral("group:hmi"),
                                  QStringLiteral("HMI\u6C9F\u901A\u7FA4"),
                                  2);
        window.setGroupInfoPanel(QStringLiteral("\u6682\u65E0\u516C\u544A"),
                                 makeGroupMembers({
                                     {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
                                     {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                                 }),
                                 true);

        QCOMPARE(panel->groupTitleText(), QStringLiteral("HMI\u6C9F\u901A\u7FA4"));
    }

    void groupAnnouncementReminderButtonForwardsContextSnapshot()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, true},
            {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        window.setGroupInfoPanel(QStringLiteral("\u672C\u5468\u4E94\u4E0B\u5348\u63D0\u4EA4\u8BC4\u5BA1\u6750\u6599"),
                                 makeGroupMembers({
                                     {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, true},
                                     {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
                                 }),
                                 true);

        auto* panel = window.findChild<GroupInfoPanel*>();
        QVERIFY(panel != nullptr);
        auto* remindButton =
            panel->findChild<QPushButton*>(QStringLiteral("groupAnnouncementReminderButton"));
        QVERIFY(remindButton != nullptr);

        QSignalSpy reminderSpy(&window, &MainWindow::groupAnnouncementReminderRequested);
        QVERIFY(reminderSpy.isValid());

        remindButton->click();

        QCOMPARE(reminderSpy.count(), 1);
        const QList<QVariant> args = reminderSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("group:project"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QCOMPARE(args.at(2).toString(),
                 QStringLiteral("\u672C\u5468\u4E94\u4E0B\u5348\u63D0\u4EA4\u8BC4\u5BA1\u6750\u6599"));
    }

    void groupConversation_keepsRemovedFileServiceSettingsHidden()
    {
        MainWindow window;
        auto* panel = window.findChild<GroupInfoPanel*>();
        QVERIFY(panel != nullptr);

        panel->showFileServiceSettingsView();
        QVERIFY(!panel->isShowingFileServiceSettingsView());

        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, true},
            {QStringLiteral("lisi"), QStringLiteral("\u674E\u56DB"), false, false, false},
        }));
        window.setChatHeaderGroup(QStringLiteral("group:project"),
                                  QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
                                  2);

        QVERIFY(!panel->isShowingFileServiceSettingsView());
    }

    void welcomePage_exposesHeroPreviewAndTieredActions()
    {
        MainWindow window;

        QVERIFY(window.isShowingWelcomePage());
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("welcomeHeroShell")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("welcomeAtmospherePanel")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("welcomePreviewCard")) != nullptr);
        QVERIFY(window.findChild<QPushButton*>(QStringLiteral("welcomePrimaryAction")) != nullptr);
        QVERIFY(window.findChild<QPushButton*>(QStringLiteral("welcomeSecondaryAction")) != nullptr);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("welcomeMessagesSignal")) == nullptr);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("welcomeContactsSignal")) == nullptr);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("welcomeTransfersSignal")) == nullptr);
        auto* welcomeCard = window.findChild<QFrame*>(QStringLiteral("welcomeCard"));
        QVERIFY(welcomeCard != nullptr);
        QVERIFY(welcomeCard->graphicsEffect() == nullptr);
    }

    void welcomePage_exposesAtmosphereVisualStage()
    {
        MainWindow window;

        auto* heroChrome = window.findChild<QFrame*>(QStringLiteral("welcomeHeroChrome"));
        auto* atmospherePanel =
            window.findChild<QFrame*>(QStringLiteral("welcomeAtmospherePanel"));
        QVERIFY(heroChrome != nullptr);
        QVERIFY(atmospherePanel != nullptr);
        QVERIFY(window.findChildren<QFrame*>(QStringLiteral("welcomeHeroChromeDot")).size() >= 3);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("welcomeHeroChromeMode")) != nullptr);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("welcomeHeroChromeStatus")) != nullptr);
        QVERIFY(atmospherePanel->width() >= 280);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("welcomeAtmosphereSpotlight")) != nullptr);
        QVERIFY(window.findChildren<QFrame*>(QStringLiteral("welcomeAtmosphereLane")).size() >= 3);
        QVERIFY(window.findChildren<QFrame*>(QStringLiteral("welcomeAtmosphereDockCard")).size() >= 2);
        QVERIFY(window.findChildren<QFrame*>(QStringLiteral("welcomeAtmosphereNode")).size() >= 4);
    }

    void welcomePreviewMetrics_reflectWorkspaceCounts()
    {
        MainWindow window;

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({ConversationSummary{L"group:project",
                                                         L"\u9879\u76EE\u8BA8\u8BBA\u7EC4",
                                                         L"demo",
                                                         1,
                                                         true,
                                                         false,
                                                         false,
                                                         false,
                                                         false},
                                     ConversationSummary{L"direct:lisi",
                                                         L"\u674E\u56DB",
                                                         L"demo",
                                                         2,
                                                         false,
                                                         false,
                                                         false,
                                                         false,
                                                         false}});
        conversationModel->setUnreadConversationIds(QSet<QString>{QStringLiteral("group:project")});
        window.setConversationModel(conversationModel);

        auto* contactModel = new ContactListModel(&window);
        contactModel->setItems({
            makePeerEndpoint(QStringLiteral("zhangsan"), QStringLiteral("张三"),
                             QStringLiteral("192.0.2.3"), 9527, PeerPresenceStatus::Online),
            makePeerEndpoint(QStringLiteral("lisi"), QStringLiteral("李四"),
                             QStringLiteral("192.0.2.8"), 9527, PeerPresenceStatus::Offline),
            makePeerEndpoint(QStringLiteral("wangwu"), QStringLiteral("王五"),
                             QStringLiteral("192.0.2.12"), 9527, PeerPresenceStatus::Online),
        });
        window.setContactModel(contactModel);

        auto* transferModel = new TransferListModel(&window);
        transferModel->setItems({TransferListItem{QStringLiteral("task-001"),
                                                  QStringLiteral("demo.pdf"),
                                                  QStringLiteral("\u4F20\u8F93\u4E2D"),
                                                  QStringLiteral("72%"),
                                                  QStringLiteral("\u674E\u56DB"),
                                                  QStringLiteral("C:/demo.pdf"),
                                                  FileTransferDirection::Outgoing,
                                                  FileTransferState::Transferring,
                                                  false,
                                                  true,
                                                  false},
                                 TransferListItem{QStringLiteral("task-002"),
                                                  QStringLiteral("notes.docx"),
                                                  QStringLiteral("\u5DF2\u5B8C\u6210"),
                                                  QStringLiteral("100%"),
                                                  QStringLiteral("\u5F20\u4E09"),
                                                  QStringLiteral("C:/notes.docx"),
                                                  FileTransferDirection::Incoming,
                                                  FileTransferState::Completed,
                                                  true,
                                                  true,
                                                  false}});
        window.setTransferModel(transferModel);

        auto* messagesMetric =
            window.findChild<QLabel*>(QStringLiteral("welcomeMessagesMetricValue"));
        auto* contactsMetric =
            window.findChild<QLabel*>(QStringLiteral("welcomeContactsMetricValue"));
        auto* transfersMetric =
            window.findChild<QLabel*>(QStringLiteral("welcomeTransfersMetricValue"));

        QVERIFY(messagesMetric != nullptr);
        QVERIFY(contactsMetric != nullptr);
        QVERIFY(transfersMetric != nullptr);

        QCOMPARE(messagesMetric->text(), QStringLiteral("1"));
        QCOMPARE(contactsMetric->text(), QStringLiteral("2"));
        QCOMPARE(transfersMetric->text(), QStringLiteral("2"));

    }

    void runtimeArchitectureSummary_updatesWelcomeSnapshotLabels()
    {
        MainWindow window;

        auto* runtimeChip = window.findChild<QLabel*>(QStringLiteral("welcomeHeroChromeStatus"));
        auto* runtimeSummary = window.findChild<QLabel*>(QStringLiteral("welcomeRuntimeSummary"));
        auto* runtimeDetail = window.findChild<QLabel*>(QStringLiteral("welcomeRuntimeDetail"));
        QVERIFY(runtimeChip != nullptr);
        QVERIFY(runtimeSummary != nullptr);
        QVERIFY(runtimeDetail != nullptr);

        window.setRuntimeArchitectureSummary(2,
                                             1,
                                             3,
                                             6,
                                             true,
                                             QStringLiteral("LeyoChat Service"));

        QCOMPARE(runtimeChip->text(), QStringLiteral("2 \u4e2a\u534f\u4f5c\u670d\u52a1"));
        QCOMPARE(runtimeSummary->text(), QStringLiteral("\u5df2\u53d1\u73b0 2 \u4e2a\u534f\u4f5c\u670d\u52a1\uff0c\u5f53\u524d\u5df2\u7ed1\u5b9a"));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("LeyoChat Service")));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("\u53ef\u89c1\u8d44\u6e90 6 \u4e2a")));
    }

    void runtimeArchitectureSnapshot_updatesWelcomeSummaryWithSelectedResource()
    {
        MainWindow window;

        auto* runtimeSummary = window.findChild<QLabel*>(QStringLiteral("welcomeRuntimeSummary"));
        auto* runtimeDetail = window.findChild<QLabel*>(QStringLiteral("welcomeRuntimeDetail"));
        QVERIFY(runtimeSummary != nullptr);
        QVERIFY(runtimeDetail != nullptr);

        RuntimeArchitectureSnapshot snapshot;
        snapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
            QStringLiteral("svc-001"),
            QStringLiteral("LeyoChat Service"),
            QStringLiteral("Demo Org"),
            QStringLiteral("lan"),
            0,
            {}
        });
        snapshot.selection.serviceId = QStringLiteral("svc-001");
        snapshot.selection.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selection.bound = true;
        snapshot.selection.selectedResource = ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("res-001"),
            QStringLiteral("shared_doc"),
            QStringLiteral("Spec Board"),
            QStringLiteral("v3"),
            QStringLiteral("Architecture board"),
            ResourceOrigin::Service
        };

        window.setRuntimeArchitectureSnapshot(snapshot);

        QVERIFY(runtimeSummary->text().contains(QStringLiteral("\u5df2\u53d1\u73b0")));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("LeyoChat Service")));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("Spec Board")));
    }

    void runtimeArchitectureSnapshot_updatesCurrentGroupPanelWithSpecificBinding()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));

        RuntimeArchitectureSnapshot snapshot;
        snapshot.selection.serviceId = QStringLiteral("svc-001");
        snapshot.selection.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selection.bound = true;
        snapshot.groupBindings.push_back(GroupServiceBindingSnapshot{
            QStringLiteral("group:project"),
            QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
            ServiceBinding{QStringLiteral("svc-001"), true, false, false},
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QString(),
                0,
                false,
                {}
            },
            ServiceDiscoverySnapshot{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                0,
                {}
            },
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_group"),
                QStringLiteral("Design Group"),
                QStringLiteral("v1"),
                QStringLiteral("Group binding"),
                ResourceOrigin::Service
            },
            true
        });
        snapshot.visibleResources.push_back(ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("res-001"),
            QStringLiteral("shared_file"),
            QStringLiteral("Spec"),
            QStringLiteral("v1"),
            QStringLiteral("Spec"),
            ResourceOrigin::Service
        });
        snapshot.visibleResources.push_back(ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("res-002"),
            QStringLiteral("shared_file"),
            QStringLiteral("Runbook"),
            QStringLiteral("v2"),
            QStringLiteral("Runbook"),
            ResourceOrigin::Service
        });

        window.setRuntimeArchitectureSnapshot(snapshot);

        auto* header = window.chatHeaderWidget();
        QVERIFY(header != nullptr);
        auto* groupInfoButton = header->groupInfoButton();
        QVERIFY(groupInfoButton != nullptr);
        QTest::mouseClick(groupInfoButton, Qt::LeftButton);
        QTRY_VERIFY(window.isGroupPanelVisible());

        auto* panel = window.findChild<GroupInfoPanel*>();
        QVERIFY(panel != nullptr);
        auto* runtimeChip = panel->findChild<QLabel*>(QStringLiteral("groupRuntimeChip"));
        auto* runtimeDetail = panel->findChild<QLabel*>(QStringLiteral("groupRuntimeDetail"));
        QVERIFY(runtimeChip != nullptr);
        QVERIFY(runtimeDetail != nullptr);
        QCOMPARE(runtimeChip->text(), QStringLiteral("\u5df2\u7ed1\u5b9a\u7fa4\u670d\u52a1"));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("LeyoChat Service")));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("group:project")));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("Design Group")));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("服务优先")));
        QVERIFY(runtimeDetail->text().contains(QStringLiteral("\u5171\u4eab\u6587\u4ef6 2 \u4e2a")));
    }

    void runtimeArchitectureSnapshot_updatesGroupHeaderWithSpecificBinding()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhangsan"), QStringLiteral("\u5F20\u4E09"), true, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));

        RuntimeArchitectureSnapshot snapshot;
        snapshot.selection.serviceId = QStringLiteral("svc-001");
        snapshot.selection.serviceName = QStringLiteral("LeyoChat Service");
        snapshot.selection.bound = true;
        snapshot.groupBindings.push_back(GroupServiceBindingSnapshot{
            QStringLiteral("group:project"),
            QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"),
            ServiceBinding{QStringLiteral("svc-001"), true, false, false},
            ServiceRegistryEntry{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                QString(),
                0,
                false,
                {}
            },
            ServiceDiscoverySnapshot{
                QStringLiteral("svc-001"),
                QStringLiteral("LeyoChat Service"),
                QStringLiteral("Demo Org"),
                QStringLiteral("lan"),
                0,
                {}
            },
            ResourceReference{
                QStringLiteral("svc-001"),
                QStringLiteral("ws-001"),
                QStringLiteral("res-001"),
                QStringLiteral("shared_group"),
                QStringLiteral("Design Group"),
                QStringLiteral("v1"),
                QStringLiteral("group workspace"),
                ResourceOrigin::Service
            },
            true
        });
        snapshot.visibleResources.push_back(ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("res-001"),
            QStringLiteral("shared_file"),
            QStringLiteral("Spec"),
            QStringLiteral("v1"),
            QStringLiteral("Spec"),
            ResourceOrigin::Service
        });
        snapshot.visibleResources.push_back(ResourceReference{
            QStringLiteral("svc-001"),
            QStringLiteral("ws-001"),
            QStringLiteral("res-002"),
            QStringLiteral("shared_file"),
            QStringLiteral("Runbook"),
            QStringLiteral("v2"),
            QStringLiteral("Runbook"),
            ResourceOrigin::Service
        });

        window.setRuntimeArchitectureSnapshot(snapshot);

        auto* headerStatus = window.findChild<QLabel*>(QStringLiteral("headerStatusValue"));
        auto* headerHint = window.findChild<QLabel*>(QStringLiteral("headerConsoleHint"));
        QVERIFY(headerStatus != nullptr);
        QVERIFY(headerHint != nullptr);
        QCOMPARE(headerStatus->text(), QStringLiteral("\u5DF2\u7ED1\u5B9A\u7FA4\u670D\u52A1"));
        QVERIFY(headerHint->text().contains(QStringLiteral("LeyoChat Service")));
        QVERIFY(headerHint->text().contains(QStringLiteral("group:project")));
        QVERIFY(headerHint->text().contains(QStringLiteral("Design Group")));
        QVERIFY(headerHint->text().contains(QStringLiteral("\u670d\u52a1\u4f18\u5148")));
        QVERIFY(headerHint->text().contains(QStringLiteral("\u5171\u4eab\u6587\u4ef6 2 \u4e2a")));
    }

    void searchFiltersConversationAndContactListsByName()
    {
        MainWindow window;

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({
            ConversationSummary{L"direct:zhangsan", L"\u5F20\u4E09", L"\u4F60\u597D", 1, false, false, false, false, false},
            ConversationSummary{L"group:ops", L"\u8FD0\u7EF4\u7FA4", L"\u7CFB\u7EDF\u64AD\u62A5", 2, false, false, false, false, false}
        });
        auto* contactModel = new ContactListModel(&window);
        contactModel->setItems({
            makePeerEndpoint(QStringLiteral("zhangsan"), QStringLiteral("zhangsan"),
                             QStringLiteral("192.0.2.20"), 45454, PeerPresenceStatus::Online),
            makePeerEndpoint(QStringLiteral("lisi"), QStringLiteral("lisi"),
                             QStringLiteral("192.0.2.21"), 45454, PeerPresenceStatus::Online),
        });
        window.setConversationModel(conversationModel);
        window.setContactModel(contactModel);

        conversationModel->setSearchText(QStringLiteral("\u8FD0\u7EF4"));
        contactModel->setSearchText(QStringLiteral("lisi"));

        QCOMPARE(conversationModel->rowCount(), 1);
        QCOMPARE(contactModel->rowCount(), 1);
        QCOMPARE(conversationModel->index(0, 0).data(ConversationListModel::TitleRole).toString(),
                 QStringLiteral("\u8FD0\u7EF4\u7FA4"));
        QCOMPARE(contactModel->index(0, 0).data(ContactListModel::DisplayNameRole).toString(),
                 QStringLiteral("lisi"));
    }

    void groupWorkspaceStillShowsArchivedGroupConversation()
    {
        MainWindow window;

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({
            ConversationSummary{L"group:ops",
                                L"\u8FD0\u7EF4\u7FA4",
                                L"\u6700\u65B0\u7FA4\u6D88\u606F",
                                2,
                                false,
                                false,
                                false,
                                true,
                                false},
            ConversationSummary{L"direct:zhangsan|self",
                                L"\u5F20\u4E09",
                                L"\u4F60\u597D",
                                1,
                                false,
                                false,
                                false,
                                false,
                                false}
        });
        window.setConversationModel(conversationModel);

        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::All));
        QCOMPARE(conversationModel->rowCount(), 1);
        QCOMPARE(conversationModel->index(0, 0).data(ConversationListModel::TitleRole).toString(),
                 QStringLiteral("\u5F20\u4E09"));

        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::Group));
        QCOMPARE(conversationModel->rowCount(), 1);
        QCOMPARE(conversationModel->index(0, 0).data(ConversationListModel::TitleRole).toString(),
                 QStringLiteral("\u8FD0\u7EF4\u7FA4"));
    }

    void nudgeAction_emitsDedicatedSignal()
    {
        MainWindow window;

        QSignalSpy nudgeSpy(&window, &MainWindow::nudgeRequested);
        QVERIFY(nudgeSpy.isValid());

        auto* button = window.findChild<QAbstractButton*>(QStringLiteral("composerNudgeButton"));
        QVERIFY(button != nullptr);
        QTest::mouseClick(button, Qt::LeftButton);

        QCOMPARE(nudgeSpy.count(), 1);
    }

    void groupNudge_isDisabledForRegularMembers()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("owner-001"), QStringLiteral("\u5F20\u4E09"), true, false, false},
            {QStringLiteral("self-001"), QStringLiteral("\u6211"), false, false, true},
        }));
        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));

        auto* button = window.findChild<QAbstractButton*>(QStringLiteral("composerNudgeButton"));
        QVERIFY(button != nullptr);
        QVERIFY(!button->isEnabled());
    }

    void groupNudge_staysEnabledForOwnerOrAdmin()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("self-001"), QStringLiteral("\u6211"), false, true, true},
            {QStringLiteral("member-001"), QStringLiteral("\u674E\u56DB"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));

        auto* button = window.findChild<QAbstractButton*>(QStringLiteral("composerNudgeButton"));
        QVERIFY(button != nullptr);
        QVERIFY(button->isEnabled());
    }

    void welcomePage_preservesTwoColumnHeroRhythm()
    {
        MainWindow window;
        window.resize(1366, 860);
        window.show();
        QTest::qWait(30);

        auto* welcomeCard = window.findChild<QFrame*>(QStringLiteral("welcomeCard"));
        auto* welcomePreviewCard = window.findChild<QFrame*>(QStringLiteral("welcomePreviewCard"));

        QVERIFY(welcomeCard != nullptr);
        QVERIFY(welcomePreviewCard != nullptr);
        QVERIFY(welcomeCard->width() >= 460);
        QVERIFY(welcomePreviewCard->width() >= 180);
    }

    void welcomeHero_staysVisuallyCenteredInContentArea()
    {
        MainWindow window;
        window.resize(1366, 860);
        window.show();
        QTest::qWait(30);

        auto* welcomePage = window.findChild<QWidget*>(QStringLiteral("welcomePage"));
        auto* welcomeShell = window.findChild<QFrame*>(QStringLiteral("welcomeHeroShell"));
        auto* welcomeCard = window.findChild<QFrame*>(QStringLiteral("welcomeCard"));

        QVERIFY(welcomePage != nullptr);
        QVERIFY(welcomeShell != nullptr);
        QVERIFY(welcomeCard != nullptr);
        QVERIFY(welcomeShell->height() >= 520);

        const QPoint welcomeCenter = welcomePage->rect().center();
        const QPoint shellCenter = welcomeShell->geometry().center();

        QVERIFY(qAbs(shellCenter.x() - welcomeCenter.x()) < 120);
        QVERIFY(qAbs(shellCenter.y() - welcomeCenter.y()) < 140);
    }

    void welcomeHero_keepsPrimaryCardInsideShellBounds()
    {
        MainWindow window;
        window.resize(1366, 860);
        window.show();
        QTest::qWait(30);

        auto* welcomeShell = window.findChild<QFrame*>(QStringLiteral("welcomeHeroShell"));
        auto* welcomeStage = window.findChild<QFrame*>(QStringLiteral("welcomeHeroStage"));
        auto* atmospherePanel =
            window.findChild<QFrame*>(QStringLiteral("welcomeAtmospherePanel"));
        auto* welcomeCard = window.findChild<QFrame*>(QStringLiteral("welcomeCard"));

        QVERIFY(welcomeShell != nullptr);
        QVERIFY(welcomeStage != nullptr);
        QVERIFY(atmospherePanel != nullptr);
        QVERIFY(welcomeCard != nullptr);

        const QRect atmosphereRect = QRect(welcomeStage->mapFromGlobal(atmospherePanel->mapToGlobal(QPoint(0, 0))),
                                           atmospherePanel->size());
        const QRect cardRect = QRect(welcomeStage->mapFromGlobal(welcomeCard->mapToGlobal(QPoint(0, 0))),
                                     welcomeCard->size());

        QVERIFY(cardRect.left() >= atmosphereRect.right() - 4);
        const int desktopHeroMinimumWidth =
            atmosphereRect.width() + 24 + welcomeCard->minimumWidth();
        if (welcomeStage->width() >= desktopHeroMinimumWidth) {
            QVERIFY(cardRect.right() <= welcomeStage->rect().right());
        }
    }

    void groupPanel_revealsThroughAnimatedWidthContract()
    {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* panel = window.findChild<GroupInfoPanel*>();
        QVERIFY(panel != nullptr);
        QCOMPARE(panel->maximumWidth(), 0);

        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QVERIFY(!window.isGroupPanelVisible());
        QCOMPARE(panel->maximumWidth(), 0);

        auto* header = window.chatHeaderWidget();
        QVERIFY(header != nullptr);
        auto* groupInfoButton = header->groupInfoButton();
        QVERIFY(groupInfoButton != nullptr);
        QTest::mouseClick(groupInfoButton, Qt::LeftButton);
        QTRY_VERIFY(panel->maximumWidth() > 0);
        QVERIFY(window.isGroupPanelVisible());

        window.showDirectConversation(QStringLiteral("direct:zhangsan"),
                                      QStringLiteral("\u5F20\u4E09"));
        QVERIFY(!window.isGroupPanelVisible());
        QTRY_COMPARE(panel->maximumWidth(), 0);
    }

    void chatWorkspace_exposesStageShellAndContextualEmptyState()
    {
        MainWindow window;

        window.showDirectConversation(QStringLiteral("direct:zhangsan"),
                                      QStringLiteral("\u5F20\u4E09"));

        auto* stageFrame = window.findChild<QFrame*>(QStringLiteral("messageStageFrame"));
        QVERIFY(stageFrame != nullptr);

        auto* topBand = window.findChild<QFrame*>(QStringLiteral("messageStageTopBand"));
        QVERIFY(topBand != nullptr);

        auto* modeChip = window.findChild<QLabel*>(QStringLiteral("messageStageModeChip"));
        QVERIFY(modeChip != nullptr);
        QCOMPARE(modeChip->text(), QStringLiteral("\u76F4\u8FDE\u4F1A\u8BDD"));

        auto* emptyCard = window.findChild<QFrame*>(QStringLiteral("messageStageEmptyCard"));
        QVERIFY(emptyCard != nullptr);
        QVERIFY(!emptyCard->isHidden());

        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        QCOMPARE(modeChip->text(), QStringLiteral("\u7FA4\u534F\u4F5C\u7A7A\u95F4"));
    }

    void sidePanels_exposeWorkspaceCardsAndFilterBand()
    {
        MainWindow window;

        QVERIFY(window.findChild<QFrame*>(QStringLiteral("conversationsHeaderCard")) != nullptr);
        QVERIFY(window.findChild<QLabel*>(QStringLiteral("conversationsModeChip")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("filterPanel")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("directoryHeroPanel")) != nullptr);
        QVERIFY(window.findChild<QWidget*>(QStringLiteral("directoryToolbar")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("directorySegmentedTabs")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("transferHeaderCard")) != nullptr);
        QVERIFY(window.findChild<QFrame*>(QStringLiteral("transferFilterBand")) != nullptr);
    }

    void sideWorkspaceHeaders_exposeModeAndStatusChips()
    {
        MainWindow window;

        auto* conversationsModeChip =
            window.findChild<QLabel*>(QStringLiteral("conversationsModeChip"));
        auto* conversationsStatusChip =
            window.findChild<QLabel*>(QStringLiteral("conversationsStatusChip"));
        auto* contactsModeChip = window.findChild<QLabel*>(QStringLiteral("contactsModeChip"));
        auto* contactsStatusChip = window.findChild<QLabel*>(QStringLiteral("contactsStatusChip"));
        auto* transferModeChip = window.findChild<QLabel*>(QStringLiteral("transferModeChip"));
        auto* transferStatusChip = window.findChild<QLabel*>(QStringLiteral("transferStatusChip"));

        QVERIFY(conversationsModeChip != nullptr);
        QVERIFY(conversationsStatusChip != nullptr);
        QVERIFY(contactsModeChip != nullptr);
        QVERIFY(contactsStatusChip != nullptr);
        QVERIFY(transferModeChip != nullptr);
        QVERIFY(transferStatusChip != nullptr);

        QCOMPARE(conversationsModeChip->text(), QStringLiteral("\u6D88\u606F 0"));
        QCOMPARE(contactsModeChip->text(), QStringLiteral("\u8054\u7CFB\u4EBA\u76EE\u5F55"));
        QCOMPARE(transferModeChip->text(), QStringLiteral("\u4F20\u8F93\u770B\u677F"));
    }

    void sideWorkspaceStatusChips_reflectLoadedData()
    {
        MainWindow window;

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({ConversationSummary{L"group:project",
                                                         L"\u9879\u76EE\u8BA8\u8BBA\u7EC4",
                                                         L"demo",
                                                         1,
                                                         true,
                                                         false,
                                                         false,
                                                         false,
                                                         false},
                                     ConversationSummary{L"direct:lisi",
                                                         L"\u674E\u56DB",
                                                         L"demo",
                                                         2,
                                                         false,
                                                         false,
                                                         false,
                                                         false,
                                                         false}});
        conversationModel->setUnreadConversationIds(QSet<QString>{QStringLiteral("group:project")});
        window.setConversationModel(conversationModel);

        auto* contactModel = new ContactListModel(&window);
        contactModel->setItems({
            makePeerEndpoint(QStringLiteral("zhangsan"), QStringLiteral("张三"),
                             QStringLiteral("192.0.2.3"), 9527, PeerPresenceStatus::Online),
            makePeerEndpoint(QStringLiteral("lisi"), QStringLiteral("李四"),
                             QStringLiteral("192.0.2.8"), 9527, PeerPresenceStatus::Offline),
            makePeerEndpoint(QStringLiteral("wangwu"), QStringLiteral("王五"),
                             QStringLiteral("192.0.2.12"), 9527, PeerPresenceStatus::Online),
        });
        window.setContactModel(contactModel);

        auto* transferModel = new TransferListModel(&window);
        transferModel->setItems({TransferListItem{QStringLiteral("task-001"),
                                                  QStringLiteral("demo.pdf"),
                                                  QStringLiteral("\u4F20\u8F93\u4E2D"),
                                                  QStringLiteral("72%"),
                                                  QStringLiteral("\u674E\u56DB"),
                                                  QStringLiteral("C:/demo.pdf"),
                                                  FileTransferDirection::Outgoing,
                                                  FileTransferState::Transferring,
                                                  false,
                                                  true,
                                                  false},
                                 TransferListItem{QStringLiteral("task-002"),
                                                  QStringLiteral("notes.docx"),
                                                  QStringLiteral("\u5DF2\u5B8C\u6210"),
                                                  QStringLiteral("100%"),
                                                  QStringLiteral("\u5F20\u4E09"),
                                                  QStringLiteral("C:/notes.docx"),
                                                  FileTransferDirection::Incoming,
                                                  FileTransferState::Completed,
                                                  true,
                                                  true,
                                                  false}});
        window.setTransferModel(transferModel);

        QCOMPARE(window.findChild<QLabel*>(QStringLiteral("conversationsStatusChip"))->text(),
                 QStringLiteral("\u672A\u8BFB 1"));
        QCOMPARE(window.findChild<QLabel*>(QStringLiteral("contactsStatusChip"))->text(),
                 QStringLiteral("2/3 \u5728\u7EBF"));
        QCOMPARE(window.findChild<QLabel*>(QStringLiteral("transferStatusChip"))->text(),
                 QStringLiteral("2 \u9879\u4EFB\u52A1"));
    }

    void conversationWorkspaceStatus_clearsUnreadStatusWhenUnreadStateClears()
    {
        MainWindow window;

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({ConversationSummary{L"group:project",
                                                         L"\u9879\u76EE\u8BA8\u8BBA\u7EC4",
                                                         L"demo",
                                                         1,
                                                         true,
                                                         false,
                                                         false,
                                                         false,
                                                         false},
                                     ConversationSummary{L"direct:lisi",
                                                         L"\u674E\u56DB",
                                                         L"demo",
                                                         2,
                                                         false,
                                                         false,
                                                         false,
                                                         false,
                                                         false}});
        conversationModel->setUnreadConversationIds(
            QSet<QString>{QStringLiteral("group:project")});
        window.setConversationModel(conversationModel);

        auto* statusChip = window.findChild<QLabel*>(QStringLiteral("conversationsStatusChip"));

        QVERIFY(statusChip != nullptr);
        QVERIFY(statusChip->isHidden());

        conversationModel->setUnreadConversationIds({});
        QCoreApplication::processEvents();

        QVERIFY(statusChip->isHidden());
    }

    void conversationWorkspaceStatus_keepsUnreadFilterInSync()
    {
        MainWindow window;
        window.resize(1520, 900);
        window.show();
        QTest::qWait(50);

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({ConversationSummary{L"group:project",
                                                         L"\u9879\u76EE\u8BA8\u8BBA\u7EC4",
                                                         L"demo",
                                                         1,
                                                         true,
                                                         false,
                                                         false,
                                                         false,
                                                         false}});
        window.setConversationModel(conversationModel);

        conversationModel->setUnreadConversationIds(
            QSet<QString>{QStringLiteral("group:project")});
        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::Unread));
        QCoreApplication::processEvents();

        QCOMPARE(conversationModel->rowCount(), 1);

        conversationModel->setUnreadConversationIds({});
        QCoreApplication::processEvents();

        QCOMPARE(conversationModel->rowCount(), 0);
    }

    void conversationWorkspaceStatus_keepsUnreadFilterInSyncInGroupWorkspace()
    {
        MainWindow window;
        window.resize(1520, 900);
        window.show();
        QTest::qWait(50);

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({ConversationSummary{L"group:project",
                                                         L"项目讨论组",
                                                         L"demo",
                                                         1,
                                                         true,
                                                         false,
                                                         false,
                                                         false,
                                                         false}});
        window.setConversationModel(conversationModel);

        auto* conversationsPage = window.findChild<ConversationsPage*>(QStringLiteral("chatPageRoot"));
        QVERIFY(conversationsPage != nullptr);
        conversationsPage->setGroupWorkspaceMode(true);
        conversationsPage->syncConversationSidebarMode();
        QCoreApplication::processEvents();

        conversationModel->setUnreadConversationIds(
            QSet<QString>{QStringLiteral("group:project")});
        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::Unread));
        QCoreApplication::processEvents();

        QCOMPARE(conversationModel->rowCount(), 1);
    }

    void conversationWorkspaceStatus_keepsDirectUnreadAvailableWhenGroupFilterIsActive()
    {
        MainWindow window;
        window.resize(1520, 900);
        window.show();
        QTest::qWait(50);

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({ConversationSummary{L"group:project",
                                                         L"项目讨论组",
                                                         L"群消息",
                                                         1,
                                                         false,
                                                         false,
                                                         false,
                                                         false,
                                                         false},
                                     ConversationSummary{L"zhangsan|self",
                                                         L"张三",
                                                         L"直聊消息",
                                                         2,
                                                         false,
                                                         false,
                                                         false,
                                                         false,
                                                         false}});
        conversationModel->setUnreadConversationIds(
            QSet<QString>{QStringLiteral("zhangsan|self")});
        window.setConversationModel(conversationModel);

        auto* conversationsPage = window.findChild<ConversationsPage*>(QStringLiteral("chatPageRoot"));
        QVERIFY(conversationsPage != nullptr);

        conversationsPage->setGroupWorkspaceMode(true);
        conversationsPage->syncConversationSidebarMode();
        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::Group));
        QCoreApplication::processEvents();

        QCOMPARE(conversationModel->rowCount(), 1);

        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::Unread));
        QCoreApplication::processEvents();

        QCOMPARE(conversationModel->rowCount(), 1);
        QCOMPARE(conversationModel->index(0).data(ConversationListModel::ConversationIdRole).toString(),
                 QStringLiteral("zhangsan|self"));
    }

    void conversationWorkspaceStatus_keepsUnreadDataWhenFilteredListIsEmpty()
    {
        MainWindow window;
        window.resize(1520, 900);
        window.show();
        QTest::qWait(50);

        auto* conversationModel = new ConversationListModel(&window);
        conversationModel->setItems({ConversationSummary{L"zhangsan|self",
                                                         L"张三",
                                                         L"直聊消息",
                                                         2,
                                                         false,
                                                         false,
                                                         false,
                                                         false,
                                                         false}});
        conversationModel->setUnreadConversationIds(
            QSet<QString>{QStringLiteral("zhangsan|self")});
        window.setConversationModel(conversationModel);

        auto* conversationsPage = window.findChild<ConversationsPage*>(QStringLiteral("chatPageRoot"));
        auto* statusChip = window.findChild<QLabel*>(QStringLiteral("conversationsStatusChip"));
        QVERIFY(conversationsPage != nullptr);
        QVERIFY(statusChip != nullptr);

        conversationsPage->setGroupWorkspaceMode(true);
        conversationsPage->syncConversationSidebarMode();
        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::Group));
        QCoreApplication::processEvents();

        QCOMPARE(conversationModel->rowCount(), 0);
        QCOMPARE(statusChip->text(), QStringLiteral("未读 1"));

        conversationModel->setFilter(static_cast<int>(ConversationListModel::Filter::Unread));
        QCoreApplication::processEvents();

        QCOMPARE(conversationModel->rowCount(), 1);
        QCOMPARE(conversationModel->index(0).data(ConversationListModel::ConversationIdRole).toString(),
                 QStringLiteral("zhangsan|self"));
    }

    void denseLayout_usesResizableSplittersAndCollapsedStageBand()
    {
        MainWindow window;
        window.resize(1366, 860);
        window.showDirectConversation(QStringLiteral("direct:zhangsan"),
                                      QStringLiteral("\u5F20\u4E09"));
        window.show();
        QTest::qWait(30);

        auto* chatSplitter =
            window.findChild<QSplitter*>(QStringLiteral("chatWorkspaceSplitter"));
        auto* topBand = window.findChild<QFrame*>(QStringLiteral("messageStageTopBand"));

        QVERIFY(chatSplitter != nullptr);
        QVERIFY(topBand != nullptr);
        QCOMPARE(chatSplitter->count(), 2);
        QVERIFY(topBand->isHidden());
        QCOMPARE(chatSplitter->handleWidth(), 0);

        window.showGroupConversation(QStringLiteral("group:project"),
                                     QStringLiteral("\u9879\u76EE\u8BA8\u8BBA\u7EC4"));
        auto* header = window.chatHeaderWidget();
        QVERIFY(header != nullptr);
        auto* groupInfoButton = header->groupInfoButton();
        QVERIFY(groupInfoButton != nullptr);
        QTest::mouseClick(groupInfoButton, Qt::LeftButton);
        QTRY_VERIFY(window.isGroupPanelVisible());
        QVERIFY(chatSplitter->handleWidth() >= 2);
    }

    void canSelectTransferRowsByTaskId()
    {
        MainWindow window;

        auto* model = new TransferListModel(&window);
        model->setItems({TransferListItem{QStringLiteral("task-001"),
                                          QStringLiteral("demo.pdf"),
                                          QStringLiteral("\u4F20\u8F93\u4E2D"),
                                          QStringLiteral("72%"),
                                          QStringLiteral("\u674E\u56DB"),
                                          QStringLiteral("C:/demo.pdf"),
                                          FileTransferDirection::Outgoing,
                                          FileTransferState::Transferring,
                                          false,
                                          true,
                                          false}});
        window.setTransferModel(model);
        window.setSelectedTransferId(QStringLiteral("task-001"));

        QListView* transferList = nullptr;
        for (QListView* view : window.findChildren<QListView*>()) {
            if (view->model() == model) {
                transferList = view;
                break;
            }
        }
        QVERIFY(transferList != nullptr);

        QVERIFY(transferList->currentIndex().isValid());
        QCOMPARE(transferList->currentIndex().data(TransferListModel::TaskIdRole).toString(),
                 QStringLiteral("task-001"));
    }

    void conversationSelection_survivesConversationModelReset()
    {
        MainWindow window;

        auto* model = new ConversationListModel(&window);
        model->setItemsAndUnread({ConversationSummary{L"direct:zhangsan",
                                                      L"张三",
                                                      L"你好",
                                                      1,
                                                      false,
                                                      false,
                                                      false,
                                                      false,
                                                      false},
                                ConversationSummary{L"direct:lisi",
                                                    L"李四",
                                                    L"收到",
                                                    2,
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    false}},
                               QSet<QString>{});
        window.setConversationModel(model);
        window.setSelectedConversationId(QStringLiteral("direct:lisi"));

        QListView* conversationList = nullptr;
        for (QListView* view : window.findChildren<QListView*>()) {
            if (view->model() == model) {
                conversationList = view;
                break;
            }
        }
        QVERIFY(conversationList != nullptr);
        auto* delegate = qobject_cast<ConversationCardDelegate*>(conversationList->itemDelegate());
        QVERIFY(delegate != nullptr);
        QCOMPARE(delegate->selectedConversationId(), QStringLiteral("direct:lisi"));

        model->setItemsAndUnread({ConversationSummary{L"direct:zhangsan",
                                                      L"张三",
                                                      L"你好",
                                                      1,
                                                      false,
                                                      false,
                                                      false,
                                                      false,
                                                      false},
                                ConversationSummary{L"direct:lisi",
                                                    L"李四",
                                                    L"收到，请看最新进展",
                                                    3,
                                                    false,
                                                    false,
                                                    false,
                                                    false,
                                                    false}},
                               QSet<QString>{QStringLiteral("direct:lisi")});
        QCoreApplication::processEvents();

        QCOMPARE(delegate->selectedConversationId(), QStringLiteral("direct:lisi"));
    }

    void importsScreenshotPreviewAndQueuesAttachment()
    {
        MainWindow window;
        QImage image(64, 32, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor("#2B5CE6"));

        QVERIFY(window.importScreenshotPreview(image));
        QCOMPARE(window.pendingAttachmentCount(), 1);

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        QVERIFY(editor->toHtml().contains("img", Qt::CaseInsensitive));

        auto* metaChip = window.findChild<QLabel*>(QStringLiteral("composerMetaChip"));
        QVERIFY(metaChip != nullptr);
        QCOMPARE(metaChip->text(), QStringLiteral("1 \u4E2A\u9644\u4EF6\u5F85\u53D1"));
    }

    void submittingScreenshotEmitsPendingFileAttachment()
    {
        MainWindow window;
        QImage image(80, 40, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor("#6AA5FF"));
        QVERIFY(window.importScreenshotPreview(image));

        QSignalSpy fileSpy(&window, &MainWindow::fileSendRequested);
        QSignalSpy messageSpy(&window, &MainWindow::sendRequested);

        window.submitCurrentComposer();

        QCOMPARE(fileSpy.count(), 1);
        QCOMPARE(messageSpy.count(), 0);
        QCOMPARE(window.pendingAttachmentCount(), 0);

        auto* metaChip = window.findChild<QLabel*>(QStringLiteral("composerMetaChip"));
        QVERIFY(metaChip != nullptr);
        QCOMPARE(metaChip->text(), QStringLiteral("\u622A\u56FE / \u6587\u4EF6 / \u5F85\u53D1"));
    }

    void submittingScreenshotWithTextStripsInlinePreviewMarkup()
    {
        MainWindow window;
        window.showDirectConversation(QStringLiteral("local|peer-a"),
                                      QStringLiteral("\u5F20\u4E09"));

        QImage image(90, 46, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor("#3D7EFF"));
        QVERIFY(window.importScreenshotPreview(image));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->moveCursor(QTextCursor::End);
        editor->insertPlainText(QStringLiteral("\u6DF7\u53D1\u6587\u672C"));

        QSignalSpy fileSpy(&window, &MainWindow::fileSendRequested);
        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);
        window.submitCurrentComposer();

        QCOMPARE(fileSpy.count(), 1);
        QCOMPARE(sendSpy.count(), 1);

        const QString payload = sendSpy.takeFirst().at(0).toString();
        QVERIFY(payload.contains(QStringLiteral("\u6DF7\u53D1\u6587\u672C")));
        QVERIFY(!payload.contains(QStringLiteral("<img"), Qt::CaseInsensitive));
        QVERIFY(!payload.contains(QStringLiteral("leyochat-screenshot://")));
    }

    void sendingRichLinkMessage_stripsRiskyInlineColorStyles()
    {
        MainWindow window;
        window.showDirectConversation(QStringLiteral("local|peer-a"),
                                      QStringLiteral("\u5F20\u4E09"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setHtml(QStringLiteral(
            "<p><a href=\"https://example.com/repo\" "
            "style=\"color:#2B5CE6;background-color:#F6F8FB;\">椤圭洰鍦板潃</a></p>"));

        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);
        window.submitCurrentComposer();

        QCOMPARE(sendSpy.count(), 1);
        const QString payload = sendSpy.takeFirst().at(0).toString();
        QVERIFY(payload.contains(QStringLiteral("https://example.com/repo")));
        QVERIFY(!payload.contains(QStringLiteral("#2B5CE6"), Qt::CaseInsensitive));
        QVERIFY(!payload.contains(QStringLiteral("#F6F8FB"), Qt::CaseInsensitive));
        QVERIFY(!payload.contains(QStringLiteral("color=\"#2B5CE6\""), Qt::CaseInsensitive));
    }

    void screenshotDraftDoesNotLeakAcrossConversationSwitch()
    {
        MainWindow window;
        window.showDirectConversation(QStringLiteral("local|peer-a"),
                                      QStringLiteral("\u5F20\u4E09"));

        QImage image(72, 32, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor("#4F88FF"));
        QVERIFY(window.importScreenshotPreview(image));
        QCOMPARE(window.pendingAttachmentCount(), 1);

        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u9879\u76EE\u7FA4"));
        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setPlainText(QStringLiteral("11"));

        QSignalSpy fileSpy(&window, &MainWindow::fileSendRequested);
        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);
        window.submitCurrentComposer();

        QCOMPARE(fileSpy.count(), 0);
        QCOMPARE(sendSpy.count(), 1);
    }

    void rapidDuplicateSubmit_isIgnoredWithinShortWindow()
    {
        MainWindow window;
        window.showDirectConversation(QStringLiteral("local|peer-a"),
                                      QStringLiteral("\u5F20\u4E09"));
        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);

        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);

        editor->setPlainText(QStringLiteral("11"));
        window.submitCurrentComposer();
        editor->setPlainText(QStringLiteral("11"));
        window.submitCurrentComposer();

        QCOMPARE(sendSpy.count(), 1);
    }

    void ctrlEnterSendModeHonorsConfiguredShortcut()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("LeyoChat"));
        QCoreApplication::setApplicationName(QStringLiteral("LeyoChat"));
        QSettings settings(QStringLiteral("LeyoChat"), QStringLiteral("LeyoChat"));
        settings.setValue(QStringLiteral("sendMode"), QStringLiteral("ctrl+enter"));
        settings.sync();

        MainWindow window;
        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setPlainText(QStringLiteral("\u5FEB\u6377\u53D1\u9001\u6D4B\u8BD5"));
        editor->setFocus();

        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);
        QSignalSpy fileSpy(&window, &MainWindow::fileSendRequested);

        QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier);

        QCOMPARE(sendSpy.count(), 1);
        QCOMPARE(fileSpy.count(), 0);
        QVERIFY(sendSpy.takeFirst().at(0).toString().contains(QStringLiteral("\u5FEB\u6377\u53D1\u9001\u6D4B\u8BD5")));

        settings.remove(QStringLiteral("sendMode"));
        settings.sync();
    }

    void enterSendModeHonorsConfiguredShortcutAndPersistsUi()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("LeyoChat"));
        QCoreApplication::setApplicationName(QStringLiteral("LeyoChat"));
        QSettings settings(QStringLiteral("LeyoChat"), QStringLiteral("LeyoChat"));
        settings.setValue(QStringLiteral("sendMode"), QStringLiteral("enter"));
        settings.sync();

        MainWindow window;
        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setPlainText(QStringLiteral("\u56DE\u8F66\u53D1\u9001\u6D4B\u8BD5"));
        editor->setFocus();

        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);
        QTest::keyClick(editor, Qt::Key_Return);

        QCOMPARE(sendSpy.count(), 1);
        QVERIFY(sendSpy.takeFirst().at(0).toString().contains(QStringLiteral("\u56DE\u8F66\u53D1\u9001\u6D4B\u8BD5")));

        settings.remove(QStringLiteral("sendMode"));
        settings.sync();
    }

    void defaultSendModeUsesEnterAndShowsReadableModeButton()
    {
        QCoreApplication::setOrganizationName(QStringLiteral("LeyoChat"));
        QCoreApplication::setApplicationName(QStringLiteral("LeyoChat"));
        QSettings settings(QStringLiteral("LeyoChat"), QStringLiteral("LeyoChat"));
        settings.remove(QStringLiteral("sendMode"));
        settings.sync();

        MainWindow window;
        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setPlainText(QStringLiteral("默认回车发送"));
        editor->setFocus();

        auto* sendModeButton =
            window.findChild<QPushButton*>(QStringLiteral("composerSendModeButton"));
        QVERIFY(sendModeButton != nullptr);
        QVERIFY(sendModeButton->text().contains(QStringLiteral("Enter")));
        QVERIFY(sendModeButton->width() >= 64);

        QSignalSpy sendSpy(&window, &MainWindow::sendRequested);
        QTest::keyClick(editor, Qt::Key_Return);

        QCOMPARE(sendSpy.count(), 1);
        QVERIFY(sendSpy.takeFirst().at(0).toString().contains(QStringLiteral("默认回车发送")));
    }

    void groupMentionSelection_doesNotDuplicateAtPrefix()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhang"), QStringLiteral("\u5f20\u5c0f\u4e50"), false, false, false},
            {QStringLiteral("li"), QStringLiteral("\u674e\u5c0f\u4e50"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u5927\u4e50\u5c0f\u4e50\u7fa4"));
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QTest::keyClicks(editor, "@");
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());
        QVERIFY(popup->currentItem() != nullptr);
        QCOMPARE(popup->currentItem()->text(), QStringLiteral("\u5f20\u5c0f\u4e50"));

        QTest::keyClick(editor, Qt::Key_Return);
        QCoreApplication::processEvents();

        QCOMPARE(editor->toPlainText(), QStringLiteral("@\u5f20\u5c0f\u4e50 "));

        QTextCursor mentionCursor =
            editor->document()->find(QStringLiteral("@\u5f20\u5c0f\u4e50"));
        QVERIFY(mentionCursor.hasSelection());
        const QTextCharFormat mentionFormat = mentionCursor.charFormat();
        QCOMPARE(mentionFormat.foreground().color(), QColor(AppStyle::accent()));
        QVERIFY(mentionFormat.fontWeight() >= QFont::DemiBold);
    }

    void groupMentionSelection_supportsFullwidthAtPrefix()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhang"), QStringLiteral("\u5f20\u5c0f\u4e50"), false, false, false},
            {QStringLiteral("li"), QStringLiteral("\u674e\u5c0f\u4e50"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u5927\u4e50\u5c0f\u4e50\u7fa4"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QKeyEvent atPress(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("\uff20"));
        QCoreApplication::sendEvent(editor, &atPress);
        QKeyEvent atRelease(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("\uff20"));
        QCoreApplication::sendEvent(editor, &atRelease);
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());
        QVERIFY(popup->currentItem() != nullptr);
        QCOMPARE(popup->currentItem()->text(), QStringLiteral("\u5f20\u5c0f\u4e50"));

        QTest::keyClick(editor, Qt::Key_Return);
        QCoreApplication::processEvents();

        QCOMPARE(editor->toPlainText(), QStringLiteral("@\u5f20\u5c0f\u4e50 "));
    }

    void hidesDevOpsActionFromComposer()
    {
        MainWindow window;

        auto* button = window.findChild<QPushButton*>(QStringLiteral("composerDevOpsButton"));
        QVERIFY(button == nullptr);
    }

    void clickingResourceReferenceMessageEmitsOpenUrlSignal()
    {
        MainWindow window;
        MessageListModel model;

        QSignalSpy openUrlSpy(&window, &MainWindow::messageUrlOpenRequested);

        ResourceRefPayload payload;
        payload.serviceId = QStringLiteral("ado-local");
        payload.workspaceId = QStringLiteral("default");
        payload.origin = QStringLiteral("service");
        payload.kind = QStringLiteral("devops_work_item");
        payload.resourceId = QStringLiteral("workitem-123");
        payload.title = QStringLiteral("淇鐧诲綍闂");
        payload.subtitle = QStringLiteral("Bug / LeyoChat");
        payload.status = QStringLiteral("Active");
        payload.actions.push_back(ResourceRefAction{
            QStringLiteral("open"),
            QStringLiteral("\u6253\u5f00\u5de5\u4f5c\u9879"),
            QStringLiteral("https://dev.azure.com/example/LeyoChat/_workitems/edit/123"),
            true,
        });

        ChatMessage message;
        message.messageId = L"msg-resource-ref";
        message.senderId = L"peer-1";
        message.body = L"[DevOps 宸ヤ綔椤筣 淇鐧诲綍闂";
        message.messageType = L"resource_ref";
        message.payloadJson =
            QString::fromUtf8(ResourceRefRouter::serializePayload(payload)).toStdWString();
        message.createdAtMs = QDateTime::currentMSecsSinceEpoch();
        message.deliveryState = MessageDeliveryState::Received;

        model.setDisplayContext(QStringLiteral("local-user"), QStringLiteral("寮犱笁"));
        model.setItems({message});
        window.setMessageModel(&model);
        window.showDirectConversation(QStringLiteral("direct:peer-1"), QStringLiteral("寮犱笁"));
        window.show();
        QCoreApplication::processEvents();

        auto* messageList =
            window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);

        const QModelIndex index = model.index(0, 0);
        QVERIFY(index.isValid());
        const QRect rect = messageList->visualRect(index);
        QVERIFY(rect.isValid());

        QTest::mouseClick(messageList->viewport(),
                          Qt::LeftButton,
                          Qt::NoModifier,
                          rect.center());

        QCOMPARE(openUrlSpy.count(), 1);
        QCOMPARE(openUrlSpy.takeFirst().at(0).toString(),
                 QStringLiteral("https://dev.azure.com/example/LeyoChat/_workitems/edit/123"));
    }

    void canClearCurrentConversationBackToWelcome()
    {
        MainWindow window;
        window.showDirectConversation(QStringLiteral("local|peer-a"),
                                      QStringLiteral("寮犱笁"));

        QVERIFY(window.isShowingChatPage());
        window.clearCurrentConversationView();
        QVERIFY(window.isShowingWelcomePage());
        QVERIFY(!window.isShowingChatPage());
        QVERIFY(!window.isGroupPanelVisible());
    }

    void clickingWindowCloseMinimizesToTrayWhenTrayIsAvailable()
    {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            QSKIP("System tray is not available in this test environment.");
        }

        MainWindow window;
        QSignalSpy traySpy(&window, &MainWindow::windowMinimizedToTrayRequested);
        window.show();
        QTest::qWait(40);

        QVERIFY(window.isVisible());
        window.close();
        QCoreApplication::processEvents();

        QCOMPARE(traySpy.count(), 1);
        QVERIFY(!window.isVisible());
    }

    void minimizingWindowDoesNotRouteToTray()
    {
        MainWindow window;
        QSignalSpy traySpy(&window, &MainWindow::windowMinimizedToTrayRequested);
        window.show();
        QTest::qWait(40);

        window.showMinimized();
        QCoreApplication::processEvents();
        QTest::qWait(40);

        QCOMPARE(traySpy.count(), 0);
    }

    void avatarButtonCanRenderUploadedAvatar()
    {
        MainWindow window;
        QImage image(48, 48, QImage::Format_ARGB32_Premultiplied);
        image.fill(QColor("#2B5CE6"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString avatarPath = dir.filePath(QStringLiteral("avatar.png"));
        QVERIFY(image.save(avatarPath));

        window.setAvatarImagePath(avatarPath);

        const QPixmap avatarPixmap = window.avatarPixmapForTesting();
        QVERIFY(!avatarPixmap.isNull());
        QCOMPARE(avatarPixmap.size(), QSize(48, 48));
    }

    void sideWorkspaceStatusChips_keepUsefulFullText()
    {
        MainWindow window;

        auto* contactsStatusChip = window.findChild<QLabel*>(QStringLiteral("contactsStatusChip"));
        auto* contactsModeChip = window.findChild<QLabel*>(QStringLiteral("contactsModeChip"));
        auto* conversationsStatusChip =
            window.findChild<QLabel*>(QStringLiteral("conversationsStatusChip"));
        QVERIFY(contactsStatusChip != nullptr);
        QVERIFY(contactsModeChip != nullptr);
        QVERIFY(conversationsStatusChip != nullptr);

        QVERIFY(contactsStatusChip->minimumWidth() >= 72);
        QVERIFY(contactsModeChip->minimumWidth() >= 84);
        QVERIFY(conversationsStatusChip->minimumWidth() >= 72);
    }

    void groupConversationDoneLabels_dependOnWorkspaceContext()
    {
        QCOMPARE(MainWindow::conversationDoneActionTextForTesting(true, false),
                 QStringLiteral("\u9000\u51fa\u7fa4\u804a"));
        QCOMPARE(MainWindow::conversationDoneActionTextForTesting(true, true),
                 QStringLiteral("\u6062\u590d\u7fa4\u804a"));
    }

    void welcomeMetricCards_keepLabelsReadable()
    {
        MainWindow window;

        const auto metricCards =
            window.findChildren<QFrame*>(QStringLiteral("welcomeMetricCard"));
        QVERIFY(metricCards.size() >= 3);
        for (QFrame* card : metricCards) {
            QVERIFY(card != nullptr);
            QVERIFY(card->minimumWidth() >= 120);
        }

        const auto labels = window.findChildren<QLabel*>(QStringLiteral("welcomePreviewLabel"));
        QVERIFY(labels.size() >= 3);
        for (QLabel* label : labels) {
            QVERIFY(label->wordWrap());
        }
    }

    void messageViewportAutoScrollsToLatestWhenFollowingBottom()
    {
        MainWindow window;
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("local"), QStringLiteral("\u5F20\u4E09"));
        window.setMessageModel(&model);
        window.showDirectConversation(QStringLiteral("local|peer"),
                                      QStringLiteral("\u5F20\u4E09"));
        window.resize(900, 720);
        window.show();
        QTest::qWait(60);

        auto* messageList = window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);

        std::vector<ChatMessage> items;
        for (int i = 0; i < 18; ++i) {
            ChatMessage message;
            message.messageId = QStringLiteral("msg-%1").arg(i).toStdWString();
            message.conversationId = QStringLiteral("local|peer").toStdWString();
            message.senderId = (i % 2 == 0 ? QStringLiteral("local") : QStringLiteral("peer")).toStdWString();
            message.body = QStringLiteral("\u6D88\u606F %1 \uFF1A%2")
                               .arg(i)
                               .arg(QString(72, QLatin1Char('A')))
                               .toStdWString();
            message.createdAtMs = 1000 + i;
            message.deliveryState = MessageDeliveryState::Received;
            items.push_back(message);
        }
        model.setItems(items);
        QTest::qWait(80);

        auto* scrollBar = messageList->verticalScrollBar();
        QVERIFY(scrollBar != nullptr);
        QTRY_VERIFY(scrollBar->maximum() > 0);
        QTRY_VERIFY(scrollBar->value() >= scrollBar->maximum() - 4);

        ChatMessage extraMessage;
        extraMessage.messageId = QStringLiteral("msg-last").toStdWString();
        extraMessage.conversationId = QStringLiteral("local|peer").toStdWString();
        extraMessage.senderId = QStringLiteral("peer").toStdWString();
        extraMessage.body = QStringLiteral("\u6700\u65B0\u4E00\u6761\u81EA\u52A8\u8FFD\u8E2A\u6D88\u606F")
                                .toStdWString();
        extraMessage.createdAtMs = 9999;
        extraMessage.deliveryState = MessageDeliveryState::Received;
        items.push_back(extraMessage);
        model.setItems(items);
        QTest::qWait(80);

        QTRY_VERIFY(scrollBar->value() >= scrollBar->maximum() - 4);
    }

    void messageViewportUsesGentlerWheelScrollStep()
    {
        MainWindow window;
        auto* messageList =
            window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);
        QVERIFY(messageList->verticalScrollBar() != nullptr);
        QCOMPARE(messageList->verticalScrollBar()->singleStep(), 36);
    }

    void selectedMessageCanBeCopiedWithShortcut()
    {
        MainWindow window;
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("local"), QStringLiteral("\u5f20\u5c0f\u4e50"));

        ChatMessage message;
        message.messageId = QStringLiteral("msg-copy-1").toStdWString();
        message.conversationId = QStringLiteral("local|peer").toStdWString();
        message.senderId = QStringLiteral("peer").toStdWString();
        message.body = QStringLiteral("\u8fd9\u662f\u4e00\u6761\u53ef\u590d\u5236\u7684\u6d88\u606f").toStdWString();
        message.createdAtMs = 1234;
        message.deliveryState = MessageDeliveryState::Received;
        model.setItems({message});

        window.setMessageModel(&model);
        window.showDirectConversation(QStringLiteral("local|peer"),
                                      QStringLiteral("\u5f20\u5c0f\u4e50"));
        window.show();
        QTest::qWait(40);
        QTest::qWait(40);

        auto* messageList =
            window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);

        const QModelIndex firstIndex = model.index(0, 0);
        QVERIFY(firstIndex.isValid());
        messageList->setCurrentIndex(firstIndex);
        messageList->selectionModel()->select(firstIndex,
                                              QItemSelectionModel::ClearAndSelect
                                                  | QItemSelectionModel::Rows);
        messageList->setFocus();

        QApplication::clipboard()->clear();
        QTest::keySequence(messageList, QKeySequence::Copy);

        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("\u8fd9\u662f\u4e00\u6761\u53ef\u590d\u5236\u7684\u6d88\u606f"));
    }

    void selectedMessageSupportsPartialCopyViaDragSelection()
    {
        MainWindow window;
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("local"), QStringLiteral("\u5f20\u5c0f\u4e50"));

        const QString bodyText = QStringLiteral("\u8fd9\u662f\u4e00\u6761\u53ef\u4ee5\u5c40\u90e8\u590d\u5236\u7684\u6d88\u606f\u5185\u5bb9\uff0c\u7528\u6765\u9a8c\u8bc1\u62d6\u62fd\u9009\u62e9\u3002");

        ChatMessage message;
        message.messageId = QStringLiteral("msg-copy-partial-1").toStdWString();
        message.conversationId = QStringLiteral("local|peer").toStdWString();
        message.senderId = QStringLiteral("peer").toStdWString();
        message.body = bodyText.toStdWString();
        message.createdAtMs = 2234;
        message.deliveryState = MessageDeliveryState::Received;
        model.setItems({message});

        window.setMessageModel(&model);
        window.showDirectConversation(QStringLiteral("local|peer"),
                                      QStringLiteral("\u5f20\u5c0f\u4e50"));
        window.show();
        QTest::qWait(60);
        window.show();
        QTest::qWait(60);

        auto* messageList =
            window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);

        const QModelIndex firstIndex = model.index(0, 0);
        QVERIFY(firstIndex.isValid());
        const QRect rowRect = messageList->visualRect(firstIndex);
        QVERIFY(rowRect.isValid());

        const QPoint dragStart = rowRect.topLeft() + QPoint(96, 46);
        const QPoint dragEnd = rowRect.topLeft() + QPoint(224, 46);

        QApplication::clipboard()->clear();
        QTest::mousePress(messageList->viewport(), Qt::LeftButton, Qt::NoModifier, dragStart);
        QTest::mouseMove(messageList->viewport(), dragEnd, 20);
        QTest::mouseRelease(messageList->viewport(), Qt::LeftButton, Qt::NoModifier, dragEnd);
        QCoreApplication::processEvents();

        messageList->setFocus();
        QTest::keySequence(messageList, QKeySequence::Copy);

        const QString copiedText = QApplication::clipboard()->text();
        QVERIFY(!copiedText.isEmpty());
        QVERIFY(bodyText.contains(copiedText));
        QVERIFY(copiedText != bodyText);
    }

    void groupMentionSelection_supportsMentioningEveryone()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhang"), QStringLiteral("\u5f20\u5c0f\u4e50"), false, false, false},
            {QStringLiteral("li"), QStringLiteral("\u674e\u5c0f\u4e50"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u5927\u4e50\u5c0f\u4e50\u7fa4"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QTest::keyClicks(editor, "@");
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());
        QVERIFY(popup->count() >= 3);
        QCOMPARE(popup->item(popup->count() - 1)->text(), QStringLiteral("\u6240\u6709\u4EBA"));

        popup->setCurrentRow(popup->count() - 1);
        QTest::keyClick(editor, Qt::Key_Return);
        QCoreApplication::processEvents();

        QCOMPARE(editor->toPlainText(), QStringLiteral("@\u6240\u6709\u4EBA "));
    }

    void groupMentionPopupFiltersWhileTyping()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhang"), QStringLiteral("\u5f20\u5c0f\u4e50"), false, false, false},
            {QStringLiteral("zhou"), QStringLiteral("鍛ㄩ"), false, false, false},
            {QStringLiteral("li"), QStringLiteral("\u674e\u5c0f\u4e50"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u6d4b\u8bd5\u7fa4"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QKeyEvent atPress(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("@"));
        QCoreApplication::sendEvent(editor, &atPress);
        QKeyEvent atRelease(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("@"));
        QCoreApplication::sendEvent(editor, &atRelease);
        QKeyEvent zhangPress(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("\u5f20"));
        QCoreApplication::sendEvent(editor, &zhangPress);
        QKeyEvent zhangRelease(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("\u5f20"));
        QCoreApplication::sendEvent(editor, &zhangRelease);
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());
        QCOMPARE(popup->count(), 1);
        QCOMPARE(popup->item(0)->text(), QStringLiteral("\u5f20\u5c0f\u4e50"));
    }

    void groupMentionPopupStaysOpenWhenBackspacingQuery()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhang"), QStringLiteral("\u5f20\u5c0f\u4e50"), false, false, false},
            {QStringLiteral("zhou"), QStringLiteral("鍛ㄩ"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u6d4b\u8bd5\u7fa4"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QKeyEvent atPress(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("@"));
        QCoreApplication::sendEvent(editor, &atPress);
        QKeyEvent atRelease(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("@"));
        QCoreApplication::sendEvent(editor, &atRelease);
        QKeyEvent zhangPress(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("\u5f20"));
        QCoreApplication::sendEvent(editor, &zhangPress);
        QKeyEvent zhangRelease(QEvent::KeyRelease, Qt::Key_unknown, Qt::NoModifier, QStringLiteral("\u5f20"));
        QCoreApplication::sendEvent(editor, &zhangRelease);
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());
        QCOMPARE(popup->count(), 1);

        QTest::keyClick(editor, Qt::Key_Backspace);
        QCoreApplication::processEvents();

        QVERIFY(popup->isVisible());
        QVERIFY(popup->count() >= 2);
        QCOMPARE(editor->toPlainText(), QStringLiteral("@"));
    }

    void groupMentionPopupAppearsForFullwidthAtCommittedByInputMethod()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhang"), QStringLiteral("\u5f20\u5c0f\u4e50"), false, false, false},
            {QStringLiteral("li"), QStringLiteral("\u674e\u5c0f\u4e50"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u6d4b\u8bd5\u7fa4"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QInputMethodEvent imeEvent;
        imeEvent.setCommitString(QString(QChar(0xFF20)));
        QCoreApplication::sendEvent(editor, &imeEvent);
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY2(popup->isVisible(), "Fullwidth @ committed by IME should open mention popup");
        QVERIFY(popup->currentItem() != nullptr);
        QCOMPARE(popup->currentItem()->text(), QStringLiteral("\u5f20\u5c0f\u4e50"));
    }

    void groupMentionPopupSupportsEnterSelectionAndEscapeDismiss()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("zhang"), QStringLiteral("\u5f20\u5c0f\u4e50"), false, false, false},
            {QStringLiteral("li"), QStringLiteral("\u674e\u5c0f\u4e50"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("\u6d4b\u8bd5\u7fa4"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QTest::keyClicks(editor, "@");
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());

        QTest::keyClick(editor, Qt::Key_Down);
        QCoreApplication::processEvents();
        QVERIFY(popup->currentItem() != nullptr);
        QCOMPARE(popup->currentItem()->text(), QStringLiteral("\u674e\u5c0f\u4e50"));

        QTest::keyClick(editor, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(editor->toPlainText(), QStringLiteral("@\u674e\u5c0f\u4e50 "));
        QVERIFY(!popup->isVisible());

        QTest::keyClicks(editor, "@");
        QCoreApplication::processEvents();
        QVERIFY(popup->isVisible());
        QTest::keyClick(editor, Qt::Key_Escape);
        QCoreApplication::processEvents();
        QVERIFY(!popup->isVisible());
    }

    void groupMentionPopupHandlesEnterAndEscapeWhenPopupReceivesKeys()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("alpha"), QStringLiteral("Alpha"), false, false, false},
            {QStringLiteral("beta"), QStringLiteral("Beta"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("demo"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QInputMethodEvent imeEvent;
        imeEvent.setCommitString(QStringLiteral("\uff20"));
        QCoreApplication::sendEvent(editor, &imeEvent);
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());

        QTest::keyClick(popup, Qt::Key_Down);
        QCoreApplication::processEvents();
        QVERIFY(popup->currentItem() != nullptr);
        QCOMPARE(popup->currentItem()->text(), QStringLiteral("Beta"));

        QTest::keyClick(popup, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(editor->toPlainText(), QStringLiteral("@Beta "));
        QVERIFY(!popup->isVisible());

        QInputMethodEvent nextImeEvent;
        nextImeEvent.setCommitString(QString(QChar(0xFF20)));
        QCoreApplication::sendEvent(editor, &nextImeEvent);
        QCoreApplication::processEvents();
        QVERIFY(popup->isVisible());

        QTest::keyClick(popup, Qt::Key_Escape);
        QCoreApplication::processEvents();
        QVERIFY(!popup->isVisible());
    }

    void groupMentionInsertedNameCanBeDeletedWithBackspace()
    {
        MainWindow window;
        window.setGroupMembers(makeGroupMembers({
            {QStringLiteral("alpha"), QStringLiteral("Alpha"), false, false, false},
            {QStringLiteral("beta"), QStringLiteral("Beta"), false, false, false},
        }));
        window.showGroupConversation(QStringLiteral("group:demo"),
                                     QStringLiteral("demo"));

        auto* editor = composerEditor(window);
        QVERIFY(editor != nullptr);
        editor->setFocus();

        QTest::keyClicks(editor, "@");
        QCoreApplication::processEvents();

        auto* popup = window.findChild<QListWidget*>(QStringLiteral("mentionPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(popup->isVisible());

        QTest::keyClick(editor, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(editor->toPlainText(), QStringLiteral("@Alpha "));

        QTest::keyClick(editor, Qt::Key_Backspace);
        QCoreApplication::processEvents();
        QVERIFY(popup->isVisible());
        QCOMPARE(editor->toPlainText(), QStringLiteral("@Alpha"));

        QTest::keyClick(popup, Qt::Key_Backspace);
        QCoreApplication::processEvents();
        QCOMPARE(editor->toPlainText(), QStringLiteral("@Alph"));

        QTest::keyClick(popup, Qt::Key_Backspace);
        QCoreApplication::processEvents();
        QCOMPARE(editor->toPlainText(), QStringLiteral("@Alp"));

        for (int i = 0; i < 3; ++i) {
            QTest::keyClick(editor, Qt::Key_Backspace);
            QCoreApplication::processEvents();
        }
        QCOMPARE(editor->toPlainText(), QStringLiteral("@"));
        QVERIFY(popup->isVisible());

        QTest::keyClick(editor, Qt::Key_Backspace);
        QCoreApplication::processEvents();
        QCOMPARE(editor->toPlainText(), QString());
        QVERIFY(!popup->isVisible());
    }

    void copyingRichTextMessageStripsHtmlMarkup()
    {
        MainWindow window;
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("local"), QStringLiteral("Ebel"));

        ChatMessage message;
        message.messageId = QStringLiteral("msg-copy-rich-1").toStdWString();
        message.conversationId = QStringLiteral("local|peer").toStdWString();
        message.senderId = QStringLiteral("peer").toStdWString();
        message.body =
            QStringLiteral("<p><b>bold laugh</b> <span style=\"color:#2B5CE6;\">how are you?</span></p>")
                .toStdWString();
        message.createdAtMs = 5678;
        message.deliveryState = MessageDeliveryState::Received;
        model.setItems({message});

        window.setMessageModel(&model);
        window.showDirectConversation(QStringLiteral("local|peer"),
                                      QStringLiteral("Ebel"));
        window.resize(900, 640);
        window.show();
        QTest::qWait(40);

        auto* messageList =
            window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);

        const QModelIndex firstIndex = model.index(0, 0);
        QVERIFY(firstIndex.isValid());
        messageList->setCurrentIndex(firstIndex);
        messageList->selectionModel()->select(firstIndex,
                                              QItemSelectionModel::ClearAndSelect
                                                  | QItemSelectionModel::Rows);
        messageList->setFocus();

        QApplication::clipboard()->clear();
        QTest::keySequence(messageList, QKeySequence::Copy);

        QCOMPARE(QApplication::clipboard()->text(),
                 QStringLiteral("bold laugh how are you?"));
    }

    void messageContextMenuEmitsReminderSnapshot()
    {
        MainWindow window;
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("local"),
                                QStringLiteral("Alice"),
                                QStringLiteral("peer"));

        ChatMessage message;
        message.messageId = QStringLiteral("msg-remind-1").toStdWString();
        message.conversationId = QStringLiteral("local|peer").toStdWString();
        message.senderId = QStringLiteral("peer").toStdWString();
        message.body =
            QStringLiteral("<p><b>Please reply</b> <span style=\"color:#2B5CE6;\">tomorrow</span></p>")
                .toStdWString();
        message.createdAtMs = 5679;
        message.deliveryState = MessageDeliveryState::Received;
        model.setItems({message});

        window.setMessageModel(&model);
        window.showDirectConversation(QStringLiteral("local|peer"),
                                      QStringLiteral("Alice"));
        window.resize(900, 640);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(40);

        auto* messageList =
            window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);

        const QModelIndex firstIndex = model.index(0, 0);
        QVERIFY(firstIndex.isValid());
        const QRect rowRect = messageList->visualRect(firstIndex);
        QVERIFY(rowRect.isValid());

        QSignalSpy reminderSpy(&window, &MainWindow::messageReminderRequested);
        QVERIFY(reminderSpy.isValid());

        QTimer::singleShot(50, []() {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            if (!menu) {
                for (QWidget* widget : QApplication::topLevelWidgets()) {
                    menu = qobject_cast<QMenu*>(widget);
                    if (menu) {
                        break;
                    }
                }
            }
            if (!menu) {
                return;
            }

            for (QAction* action : menu->actions()) {
                if (action && action->text().contains(QStringLiteral("\u63d0\u9192"))) {
                    menu->setActiveAction(action);
                    QTest::keyClick(menu, Qt::Key_Return);
                    return;
                }
            }
            menu->close();
        });
        QTimer::singleShot(1000, []() {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (auto* menu = qobject_cast<QMenu*>(widget)) {
                    menu->close();
                }
            }
        });

        const bool invoked = QMetaObject::invokeMethod(
            messageList,
            "customContextMenuRequested",
            Qt::DirectConnection,
            Q_ARG(QPoint, rowRect.center()));
        QVERIFY(invoked);

        QCOMPARE(reminderSpy.count(), 1);
        const QList<QVariant> args = reminderSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("msg-remind-1"));
        QCOMPARE(args.at(1).toString(), QStringLiteral("local|peer"));
        QCOMPARE(args.at(2).toString(), QStringLiteral("Alice"));
        QCOMPARE(args.at(3).toString(), QStringLiteral("Please reply tomorrow"));
    }

    void doubleClickingUrlMessage_emitsOpenUrlSignal()
    {
        MainWindow window;
        MessageListModel model;
        model.setDisplayContext(QStringLiteral("local"), QStringLiteral("Ebel"));

        ChatMessage message;
        message.messageId = QStringLiteral("msg-url-open-1").toStdWString();
        message.conversationId = QStringLiteral("local|peer").toStdWString();
        message.senderId = QStringLiteral("peer").toStdWString();
        message.body = QStringLiteral("璇锋墦寮€ https://example.com/path?a=1").toStdWString();
        message.createdAtMs = 6789;
        message.deliveryState = MessageDeliveryState::Received;
        model.setItems({message});

        window.setMessageModel(&model);
        window.showDirectConversation(QStringLiteral("local|peer"),
                                      QStringLiteral("Ebel"));
        window.resize(900, 640);
        window.show();
        QTest::qWait(40);

        auto* messageList =
            window.findChild<QListView*>(QStringLiteral("messageListView"));
        QVERIFY(messageList != nullptr);

        QSignalSpy urlSpy(&window, &MainWindow::messageUrlOpenRequested);
        const QModelIndex index = model.index(0, 0);
        QVERIFY(index.isValid());
        const bool invoked = QMetaObject::invokeMethod(
            messageList,
            "doubleClicked",
            Qt::DirectConnection,
            Q_ARG(QModelIndex, index));
        QVERIFY(invoked);
        QCoreApplication::processEvents();

        QCOMPARE(urlSpy.count(), 1);
        const QString openedUrl = urlSpy.takeFirst().at(0).toString();
        QVERIFY(openedUrl.startsWith(QStringLiteral("https://example.com/path")));
    }

    void denseLayout_prioritizesMessageViewportOverComposerHeight()
    {
        MainWindow window;
        window.resize(1366, 860);
        window.showDirectConversation(QStringLiteral("direct:zhangsan"),
                                      QStringLiteral("\u5F20\u4E09"));
        window.show();
        QTest::qWait(30);

        auto* stageFrame = window.findChild<QFrame*>(QStringLiteral("messageStageFrame"));
        auto* composer = window.findChild<ChatComposerWidget*>();
        QVERIFY(stageFrame != nullptr);
        QVERIFY(composer != nullptr);
        QVERIFY(stageFrame->height() > composer->height() * 2);
        QVERIFY(composer->height() < 210);
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    TestMainWindow tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestMainWindow.moc"
