// MainWindow.cpp — 精简后的 MainWindow，业务逻辑已迁移到 ConversationsPage
// 只保留：页面注册、信号转发、窗口管理、通话管理、外观管理

#include "ui/MainWindow.h"

#include "app/AppSettings.h"
#include "ui/AppStyle.h"
#include "ui/ClientAppearance.h"
#include "ui/ClientPreferences.h"
#include "ui/CloseToTrayDialog.h"
#include "ui/SettingsPage.h"
#include "ui/ConversationsPage.h"
#include "ui/DirectoryPage.h"
#include "ui/NotificationsPage.h"
#include "ui/KnowledgePage.h"
#include "ui/CallWindow.h"
#include "ui/UpdateBar.h"
#include "ui/GroupInfoPanel.h"
#include "ui/ChatComposerWidget.h"
#include "ui/ChatHeaderWidget.h"
#include "ui/AiKnowledgePanel.h"
#include "ui/ContactListModel.h"
#include "ui/ConversationListModel.h"
#include "ui/GlobalSearchPanel.h"
#include "ui/GlobalSearchHistory.h"

#include <ElaDef.h>
#include <ElaStackedWidget.h>
#include <QAbstractItemView>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPixmap>
#include <QPixmapCache>
#include <QPointer>
#include <QFileInfo>
#include <QResizeEvent>
#include <QScreen>
#include <QSettings>
#include <QShowEvent>
#include <QStandardPaths>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <ole2.h>
#endif

namespace {

constexpr int kGlobalSearchWidth = 300;
constexpr int kGlobalSearchHeight = 25;

QString rgbaString(const QColor& color)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(QString::number(color.alphaF(), 'f', 3));
}

QWidget* buildPageScaffold(QWidget* page, const QString& key)
{
    if (!page) return nullptr;
    page->setObjectName(QStringLiteral("secondaryPageScaffold_%1").arg(key));
    page->setProperty("pageScaffoldRole", QStringLiteral("secondary"));
    page->setProperty("pageScaffoldKey", key);
    return page;
}

bool uiRestoreTraceEnabled()
{
    return qApp && qApp->property("leyochat.uiRestoreTraceEnabled").toBool();
}

int uiRestoreTraceSequence()
{
    return qApp ? qApp->property("leyochat.uiRestoreTraceSequence").toInt() : 0;
}

QString mainWindowStateSummary(const QWidget* widget)
{
    if (!widget) return QStringLiteral("window=null");
    const Qt::WindowStates state = widget->windowState();
    QStringList flags;
    if (state.testFlag(Qt::WindowMinimized)) flags.push_back(QStringLiteral("minimized"));
    if (state.testFlag(Qt::WindowMaximized)) flags.push_back(QStringLiteral("maximized"));
    if (state.testFlag(Qt::WindowFullScreen)) flags.push_back(QStringLiteral("fullscreen"));
    if (flags.isEmpty()) flags.push_back(QStringLiteral("normal"));
    return QStringLiteral("visible=%1 active=%2 hidden=%3 state=%4")
        .arg(widget->isVisible() ? QStringLiteral("true") : QStringLiteral("false"),
             widget->isActiveWindow() ? QStringLiteral("true") : QStringLiteral("false"),
             widget->isHidden() ? QStringLiteral("true") : QStringLiteral("false"),
             flags.join(QLatin1Char('|')));
}

// 保留 conversationDoneActionText 用于 testing API
QString conversationDoneActionText(bool isGroupConversation, bool isDone, bool groupWorkspaceActive)
{
    if (isGroupConversation && groupWorkspaceActive) {
        return isDone ? QStringLiteral("\u6062\u590D\u7FA4\u804A") : QStringLiteral("\u9000\u51FA\u7FA4\u804A");
    }
    return isDone ? QStringLiteral("\u6062\u590D\u4F1A\u8BDD") : QStringLiteral("\u5173\u95ED\u4F1A\u8BDD");
}

void persistThemeMode(AppStyle::ThemeMode mode)
{
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    cfg.setValue(QStringLiteral("appearance/themeMode"), AppStyle::themeModeToString(mode));
    cfg.sync();
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
// Testing helpers
// ═══════════════════════════════════════════════════════════════════════

QString MainWindow::conversationDoneActionTextForTesting(bool isGroupConversation, bool isDone)
{
    return conversationDoneActionText(isGroupConversation, isDone, true);
}

int MainWindow::primaryPageCountForTesting() const
{
    return m_primaryPageKeys.size();
}

bool MainWindow::hasPrimaryPageForTesting(const QString& key) const
{
    return m_primaryPageKeys.contains(key.trimmed().toLower());
}

// ═══════════════════════════════════════════════════════════════════════
// 构造函数 — 页面注册 + 导航 + 信号转发
// ═══════════════════════════════════════════════════════════════════════

QWidget* MainWindow::buildGlobalSearchWidget()
{
    m_globalSearchHost = new QWidget(this);
    m_globalSearchHost->setObjectName(QStringLiteral("GlobalSearchHost"));
    m_globalSearchHost->setFixedSize(kGlobalSearchWidth, kGlobalSearchHeight);
    m_globalSearchHost->setAttribute(Qt::WA_StyledBackground, false);

    auto* layout = new QHBoxLayout(m_globalSearchHost);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_globalSearchEdit = new QLineEdit(m_globalSearchHost);
    m_globalSearchEdit->setObjectName(QStringLiteral("GlobalDirectorySearchEdit"));
    m_globalSearchEdit->setPlaceholderText(QStringLiteral("\u641C\u7D22\u8054\u7CFB\u4EBA\u3001\u7FA4\u7EC4\u3001\u804A\u5929\u8BB0\u5F55\u2026"));
    m_globalSearchEdit->setClearButtonEnabled(true);
    m_globalSearchEdit->setFixedSize(kGlobalSearchWidth, kGlobalSearchHeight);
    m_globalSearchEdit->setToolTip(QStringLiteral("\u641C\u7D22\u8054\u7CFB\u4EBA\u3001\u7FA4\u804A\u6216\u4F1A\u8BDD"));
    m_globalSearchEdit->setReadOnly(true);
    m_globalSearchEdit->setCursor(Qt::PointingHandCursor);
    m_globalSearchEdit->installEventFilter(this);
    layout->addWidget(m_globalSearchEdit);

    applyGlobalSearchStyle();
    return m_globalSearchHost.data();
}

void MainWindow::applyGlobalSearchStyle()
{
    if (!m_globalSearchHost) return;

    const AppStyle::ThemeMode appMode = AppStyle::currentThemeMode();
    const ElaThemeType::ThemeMode mode = AppStyle::toElaThemeMode(appMode);
    QColor editBackground = ElaThemeColor(mode, BasicBaseDeep);
    if (mode == ElaThemeType::Dark) {
        editBackground = QColor(20, 31, 43, 230);
    } else {
        editBackground.setAlpha(220);
    }
    const QColor border = ElaThemeColor(mode, BasicBorder);
    const QColor borderHover = ElaThemeColor(mode, BasicBorderHover);
    const QColor text = ElaThemeColor(mode, BasicText);
    const QColor muted = ElaThemeColor(mode, BasicDetailsText);

    m_globalSearchHost->setStyleSheet(
        QStringLiteral(
            "QLineEdit#GlobalDirectorySearchEdit{background:%1;color:%2;border:1px solid %3;"
            "border-radius:13px;padding:0 12px;font-size:12px;min-height:25px;}"
            "QLineEdit#GlobalDirectorySearchEdit:focus{border:1px solid %4;}"
            "QLineEdit#GlobalDirectorySearchEdit:placeholder{color:%5;}").arg(
            rgbaString(editBackground),
            text.name(QColor::HexArgb),
            border.name(QColor::HexArgb),
            borderHover.name(QColor::HexArgb),
            muted.name(QColor::HexArgb)));
}

void MainWindow::showGlobalSearchPanel()
{
    if (!m_globalSearchHistory) {
        m_globalSearchHistory = new GlobalSearchHistory(
            QStringLiteral("LeyoChat"), QStringLiteral("LeyoChat"));
    }
    if (!m_globalSearchPanel) {
        m_globalSearchPanel = new GlobalSearchPanel(m_globalSearchHistory, this);
        connect(m_globalSearchPanel, &GlobalSearchPanel::dismissed, this, [this]() {
            if (m_globalSearchEdit) m_globalSearchEdit->clearFocus();
        });
        connect(m_globalSearchPanel, &GlobalSearchPanel::searchRequested, this,
                &MainWindow::globalSearchRequested);
        connect(m_globalSearchPanel, &GlobalSearchPanel::contactActivated, this,
                [this](const QString& clientId, const QString&) {
            navigation(m_conversationsPageKey);
            emit contactSelected(clientId);
        });
        connect(m_globalSearchPanel, &GlobalSearchPanel::groupActivated, this,
                [this](const QString& groupId, const QString& groupName) {
            navigation(m_conversationsPageKey);
            emit conversationSelected(groupId);
            if (m_conversationsPage) {
                m_conversationsPage->showGroupConversation(groupId, groupName);
            }
        });
        connect(m_globalSearchPanel, &GlobalSearchPanel::messageActivated, this,
                [this](const QString& conversationId, const QString& messageId) {
            navigation(m_conversationsPageKey);
            emit searchResultJumpRequested(conversationId, messageId);
        });
        connect(m_globalSearchPanel, &GlobalSearchPanel::fileActivated, this,
                [this](const QString& taskId) {
            emit openTransferFileRequested(taskId);
        });
        connect(m_globalSearchPanel, &GlobalSearchPanel::departmentActivated, this,
                [this](const QString& /*department*/) {
            navigation(m_directoryPageKey);
            if (m_directoryPage) {
                m_directoryPage->switchToOrgTab();
            }
        });
    }
    if (!m_globalSearchEdit) return;
    const QPoint pos = m_globalSearchEdit->mapToGlobal(
        QPoint((m_globalSearchEdit->width() - 600) / 2, m_globalSearchEdit->height() + 4));
    m_globalSearchPanel->popup(pos);
}

void MainWindow::hideGlobalSearchPanel()
{
    if (m_globalSearchPanel) {
        m_globalSearchPanel->dismiss();
    }
}

void MainWindow::setGlobalSearchResults(
    const QVector<GlobalSearchPanel::ContactResult>& contacts,
    const QVector<GlobalSearchPanel::GroupResult>& groups,
    const QVector<GlobalSearchPanel::MessageResult>& messages,
    const QVector<GlobalSearchPanel::FileResult>& files,
    const QVector<GlobalSearchPanel::DepartmentResult>& departments)
{
    if (m_globalSearchPanel) {
        m_globalSearchPanel->setResults(contacts, groups, messages, files, departments);
    }
}

MainWindow::~MainWindow()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    setAcceptDrops(true);
    // ── ElaWindow 导航栏配置 ──
    setNavigationBarDisplayMode(ElaNavigationType::Maximal);
    setNavigationBarWidth(122);
    setAppBarHeight(45);
    setUserInfoCardVisible(true);
    setUserInfoCardTitle(QString());
    setUserInfoCardSubTitle(QString());
    setIsDefaultClosed(true);
    setIsCentralStackedWidgetTransparent(true);
    auto* appBarLeftSpacer = new QWidget(this);
    appBarLeftSpacer->setFixedSize(1, 1);
    setCustomWidget(ElaAppBarType::LeftArea, appBarLeftSpacer);

    // ── 创建 ConversationsPage（承载完整工作区）──
    m_conversationsPage = new ConversationsPage(this, this);

    // ── 页面注册 ──
    {
        addPageNode(QStringLiteral("\u6D88\u606F"), m_conversationsPage, 0, ElaIconType::Comments);
        m_conversationsPageKey = m_conversationsPage->property("ElaPageKey").toString();
        m_navMsgKey = m_conversationsPageKey;

        m_notificationsPage = new NotificationsPage(this);
        addPageNode(QStringLiteral("\u901A\u77E5"), m_notificationsPage, 0, ElaIconType::Bell);
        m_notificationsPageKey = m_notificationsPage->property("ElaPageKey").toString();
        m_navNotifKey = m_notificationsPageKey;

        m_directoryPage = new DirectoryPage(this);
        addPageNode(QStringLiteral("\u901A\u8BAF\u5F55"), m_directoryPage, ElaIconType::AddressBook);
        m_directoryPageKey = m_directoryPage->property("ElaPageKey").toString();
        m_navContactKey = m_directoryPageKey;

        m_knowledgePage = new KnowledgePage(this);
        addPageNode(QStringLiteral("\u77E5\u8BC6"), m_knowledgePage, ElaIconType::Brain);
        m_knowledgePageKey = m_knowledgePage->property("ElaPageKey").toString();
        m_navAiKey = m_knowledgePageKey;
    }

    // ── 设置页（footer 节点）──
    {
        const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        ClientPreferences clientPrefs = ClientPreferencesStore::load(dataRoot);
        Profile defaultProfile;
        m_settingsPage = new SettingsPage(defaultProfile, clientPrefs, dataRoot, false, this);
        buildPageScaffold(m_settingsPage, QStringLiteral("settings"));
        addFooterNode(QStringLiteral("\u8BBE\u7F6E"), m_settingsPage, m_navSettingsKey, 0, ElaIconType::GearComplex);

        connect(m_settingsPage, &SettingsPage::dataExportRequested, this, &MainWindow::dataExportRequested);
        connect(m_settingsPage, &SettingsPage::dataImportRequested, this, &MainWindow::dataImportRequested);
    }

    m_primaryPageKeys.insert(QStringLiteral("messages"), m_conversationsPageKey);
    m_primaryPageKeys.insert(QStringLiteral("directory"), m_directoryPageKey);
    m_primaryPageKeys.insert(QStringLiteral("notifications"), m_notificationsPageKey);
    m_primaryPageKeys.insert(QStringLiteral("knowledge"), m_knowledgePageKey);
    m_primaryPageKeys.insert(QStringLiteral("workbench"), QStringLiteral("workbench"));
    m_primaryPageKeys.insert(QStringLiteral("settings"), m_navSettingsKey);
    setCustomWidget(ElaAppBarType::MiddleArea, buildGlobalSearchWidget());
    // ── 搜索框（注入 AppBar）──
    // 搜索入口放回消息左栏，顶部 AppBar 保持轻量，避免三栏结构被旧版全局搜索打断。

    // ── 导航切换 ──
    // 导航切换：ElaWindow 自动切换页面，此处仅处理副作用
    connect(this, &ElaWindow::navigationNodeClicked, this,
            [this](ElaNavigationType::NavigationNodeType, const QString& nodeKey) {
        if (nodeKey == m_navMsgKey) {
            m_conversationsPage->setGroupWorkspaceMode(false);
            m_conversationsPage->syncConversationSidebarMode();
            emit conversationFilterChanged(0);
        } else if (nodeKey == m_navNotifKey) {
            if (m_notificationsPage) {
                m_notificationsPage->markAllNotificationsRead();
            }
        }
    });

    connect(this, &ElaWindow::userInfoCardClicked, this, [this]() {
        navigation(m_navSettingsKey);
    });

    // ── AppBar 更新按钮 ──
    m_updateBar = new UpdateBar(this);
    m_updateBar->hide();
    setCustomWidget(ElaAppBarType::RightArea, m_updateBar);

    // ── 信号转发：ConversationsPage → MainWindow ──
    connectConversationsPageSignals();

    // ── 窗口初始化 ──
    QSize initialSize(1520, 900);
    QSize minimumSize(1360, 860);
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QSize available = screen->availableGeometry().size() - QSize(1, 1);
        initialSize.setWidth(qMin(initialSize.width(), available.width()));
        initialSize.setHeight(qMin(initialSize.height(), available.height()));
        minimumSize.setWidth(qMin(minimumSize.width(), qMax(960, available.width())));
        minimumSize.setHeight(qMin(minimumSize.height(), qMax(640, available.height())));
    }
    setMinimumSize(minimumSize);
    resize(initialSize.expandedTo(minimumSize));
    setWindowTitle(QStringLiteral("LeyoChat"));
    setWindowIcon(QIcon(QStringLiteral(":/app/leyochat-icon.png")));
    {
        const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        ClientPreferences startupPrefs = ClientPreferencesStore::load(dataRoot);
        applyClientAppearance(this, startupPrefs, true);
    }
    refreshTheme();
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode themeMode) {
        const AppStyle::ThemeMode mode = AppStyle::themeModeFromEla(themeMode);
        if (qApp) {
            qApp->setProperty("leyochat.themeMode", AppStyle::themeModeToString(mode));
            qApp->setPalette(AppStyle::applicationPalette(mode));
        }
        persistThemeMode(mode);
        setStyleSheet(AppStyle::activeStylesheet(mode));
        applyGlobalSearchStyle();
        if (m_conversationsPage) {
            m_conversationsPage->refreshTheme();
        }
        if (m_knowledgePage) {
            m_knowledgePage->refreshTheme();
        }
        if (m_notificationsPage) {
            m_notificationsPage->refreshTheme();
        }
        if (m_directoryPage) {
            m_directoryPage->refreshTheme();
        }
    });
    if (qApp && qApp->styleHints()) {
        connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this](Qt::ColorScheme) {
            if (AppStyle::followsSystemTheme()) {
                refreshTheme();
            }
        });
    }
}

// ═══════════════════════════════════════════════════════════════════════
// 信号转发连接
// ═══════════════════════════════════════════════════════════════════════

void MainWindow::connectConversationsPageSignals()
{
    auto* cp = m_conversationsPage;
    if (!cp) return;

    // 将 ConversationsPage 所有信号转发到 MainWindow 同名信号
    connect(cp, &ConversationsPage::sendRequested, this, &MainWindow::sendRequested);
    connect(cp, &ConversationsPage::stickerSendRequested, this, &MainWindow::stickerSendRequested);
    connect(cp, &ConversationsPage::fileSendRequested, this, &MainWindow::fileSendRequested);
    connect(cp, &ConversationsPage::nudgeRequested, this, &MainWindow::nudgeRequested);
    connect(cp, &ConversationsPage::devOpsInsertRequested, this, &MainWindow::devOpsInsertRequested);
    connect(cp, &ConversationsPage::connectRequested, this, &MainWindow::connectRequested);
    connect(cp, &ConversationsPage::createGroupRequested, this, &MainWindow::createGroupRequested);
    connect(cp, &ConversationsPage::createGroupWithPeerRequested, this, &MainWindow::createGroupWithPeerRequested);
    connect(cp, &ConversationsPage::conversationSelected, this, &MainWindow::conversationSelected);
    connect(cp, &ConversationsPage::composerRecoveryContextChanged,
            this, &MainWindow::composerRecoveryContextChanged);
    connect(cp, &ConversationsPage::composerRecoveryContextCommitted,
            this, &MainWindow::composerRecoveryContextCommitted);
    connect(cp, &ConversationsPage::retryMessageRequested, this, &MainWindow::retryMessageRequested);
    connect(cp, &ConversationsPage::recallMessageRequested, this, &MainWindow::recallMessageRequested);
    connect(cp, &ConversationsPage::editSaveRequested, this, &MainWindow::editSaveRequested);
    connect(cp, &ConversationsPage::reactionRequested, this, &MainWindow::reactionRequested);
    connect(cp, &ConversationsPage::retryTransferRequested, this, &MainWindow::retryTransferRequested);
    connect(cp, &ConversationsPage::deleteTransferRequested, this, &MainWindow::deleteTransferRequested);
    connect(cp, &ConversationsPage::clearPendingTransfersRequested, this, &MainWindow::clearPendingTransfersRequested);
    connect(cp, &ConversationsPage::clearCompletedTransfersRequested, this, &MainWindow::clearCompletedTransfersRequested);
    connect(cp, &ConversationsPage::clearFailedTransfersRequested, this, &MainWindow::clearFailedTransfersRequested);
    connect(cp, &ConversationsPage::openMessageFileRequested, this, &MainWindow::openMessageFileRequested);
    connect(cp, &ConversationsPage::revealMessageFileRequested, this, &MainWindow::revealMessageFileRequested);
    connect(cp, &ConversationsPage::readReceiptDetailRequested, this, &MainWindow::readReceiptDetailRequested);
    connect(cp, &ConversationsPage::pinMessageRequested, this, &MainWindow::pinMessageRequested);
    connect(cp, &ConversationsPage::unpinMessageRequested, this, &MainWindow::unpinMessageRequested);
    connect(cp, &ConversationsPage::replyToMessageRequested, this, &MainWindow::replyToMessageRequested);
    connect(cp, &ConversationsPage::forwardMessageRequested, this, &MainWindow::forwardMessageRequested);
    connect(cp, &ConversationsPage::messageReminderRequested, this, &MainWindow::messageReminderRequested);
    connect(cp, &ConversationsPage::mergedForwardRequested, this, &MainWindow::mergedForwardRequested);
    connect(cp, &ConversationsPage::mergedForwardPackageRequested, this, &MainWindow::mergedForwardPackageRequested);
    connect(cp, &ConversationsPage::openTransferFileRequested, this, &MainWindow::openTransferFileRequested);
    connect(cp, &ConversationsPage::revealTransferFileRequested, this, &MainWindow::revealTransferFileRequested);
    connect(cp, &ConversationsPage::cancelTransferRequested, this, &MainWindow::cancelTransferRequested);
    connect(cp, &ConversationsPage::cancelSameNameTransfersRequested, this, &MainWindow::cancelSameNameTransfersRequested);
    connect(cp, &ConversationsPage::voiceCallRequested, this, &MainWindow::voiceCallRequested);
    connect(cp, &ConversationsPage::conversationFilterChanged, this, &MainWindow::conversationFilterChanged);
    connect(cp, &ConversationsPage::conversationPinToggled, this, &MainWindow::conversationPinToggled);
    connect(cp, &ConversationsPage::conversationStarToggled, this, &MainWindow::conversationStarToggled);
    connect(cp, &ConversationsPage::conversationMuteToggled, this, &MainWindow::conversationMuteToggled);
    connect(cp, &ConversationsPage::conversationMarkUnread, this, &MainWindow::conversationMarkUnread);
    connect(cp, &ConversationsPage::conversationMarkDone, this, &MainWindow::conversationMarkDone);
    connect(cp, &ConversationsPage::groupAnnouncementRequested, this, &MainWindow::groupAnnouncementRequested);
    connect(cp, &ConversationsPage::groupAnnouncementReminderRequested,
            this, &MainWindow::groupAnnouncementReminderRequested);
    connect(cp, &ConversationsPage::groupAddMemberRequested, this, &MainWindow::groupAddMemberRequested);
    connect(cp, &ConversationsPage::groupChatHistoryRequested, this, &MainWindow::groupChatHistoryRequested);
    connect(cp, &ConversationsPage::groupSettingsRequested, this, &MainWindow::groupSettingsRequested);
    connect(cp, &ConversationsPage::groupMemberDirectChatRequested, this, &MainWindow::groupMemberDirectChatRequested);
    connect(cp, &ConversationsPage::groupMemberAdminRequested, this, &MainWindow::groupMemberAdminRequested);
    connect(cp, &ConversationsPage::groupMemberRemoveRequested, this, &MainWindow::groupMemberRemoveRequested);
    connect(cp, &ConversationsPage::groupMemberMuteRequested, this, &MainWindow::groupMemberMuteRequested);
    connect(cp, &ConversationsPage::chatHistoryRequested, this, &MainWindow::chatHistoryRequested);
    connect(cp, &ConversationsPage::closeCurrentConversationRequested, this, &MainWindow::closeCurrentConversationRequested);
    connect(cp, &ConversationsPage::messageUrlOpenRequested, this, &MainWindow::messageUrlOpenRequested);
    connect(cp, &ConversationsPage::olderMessagesRequested, this, &MainWindow::olderMessagesRequested);
    connect(cp, &ConversationsPage::fileServiceDownloadRequested, this, &MainWindow::fileServiceDownloadRequested);
    connect(cp, &ConversationsPage::fileServiceVersionHistoryRequested, this, &MainWindow::fileServiceVersionHistoryRequested);
    connect(cp, &ConversationsPage::groupFileServiceSaveRequested, this, &MainWindow::groupFileServiceSaveRequested);
    connect(cp, &ConversationsPage::groupFileManagerRequested, this, &MainWindow::groupFileManagerRequested);
    connect(cp, &ConversationsPage::groupFileDownloadRequested, this, &MainWindow::groupFileDownloadRequested);
    connect(cp, &ConversationsPage::groupFileOpenRequested, this, &MainWindow::groupFileOpenRequested);
    connect(cp, &ConversationsPage::viewportReachedBottom, this, &MainWindow::viewportReachedBottom);
    connect(cp, &ConversationsPage::contactSelected, this, &MainWindow::contactSelected);
    connect(cp, &ConversationsPage::avatarProfileRequested, this, &MainWindow::avatarProfileRequested);
    connect(cp, &ConversationsPage::conversationAvatarHovered, this, &MainWindow::conversationAvatarHovered);
    connect(cp, &ConversationsPage::retryPendingRequested, this, &MainWindow::retryPendingRequested);
    connect(cp, &ConversationsPage::screenshotRequested, this, &MainWindow::screenshotRequested);
    connect(cp, &ConversationsPage::groupNavSelected, this, &MainWindow::groupNavSelected);

    // ── DirectoryPage 信号转发 ──
    if (m_directoryPage) {
        connect(m_directoryPage, &DirectoryPage::contactSelected, this, &MainWindow::contactSelected);
        connect(m_directoryPage, &DirectoryPage::contactProfileRequested, this, &MainWindow::contactProfileRequested);
        connect(m_directoryPage, &DirectoryPage::contactReminderRequested,
                this, &MainWindow::contactReminderRequested);
        connect(m_directoryPage, &DirectoryPage::avatarProfileRequested, this, &MainWindow::avatarProfileRequested);
        connect(m_directoryPage, &DirectoryPage::contactDeleteRequested, this, &MainWindow::contactDeleteRequested);
        connect(m_directoryPage, &DirectoryPage::openConversationRequested, this, [this](const QString& clientId) {
            navigation(m_conversationsPageKey);
            emit contactSelected(clientId);
        });
        connect(m_directoryPage, &DirectoryPage::groupConversationRequested, this, [this](const QString& groupId) {
            navigation(m_conversationsPageKey);
            emit conversationSelected(groupId);
        });
        connect(m_directoryPage, &DirectoryPage::createGroupRequested, this, &MainWindow::createGroupRequested);
        connect(m_directoryPage, &DirectoryPage::addContactRequested, this, &MainWindow::addContactRequested);
    }

    // ── NotificationsPage 信号转发 ──
    if (m_notificationsPage) {
        connect(m_notificationsPage, &NotificationsPage::notificationMarkedReadRequested, this, &MainWindow::notificationMarkedReadRequested);
        connect(m_notificationsPage, &NotificationsPage::notificationArchivedRequested, this, &MainWindow::notificationArchivedRequested);
        connect(m_notificationsPage, &NotificationsPage::notificationsMarkAllReadRequested, this, &MainWindow::notificationsMarkAllReadRequested);
        connect(m_notificationsPage, &NotificationsPage::reminderDoneRequested, this, &MainWindow::reminderDoneRequested);
        connect(m_notificationsPage, &NotificationsPage::reminderSnoozeRequested, this, &MainWindow::reminderSnoozeRequested);
        connect(m_notificationsPage, &NotificationsPage::messageUrlOpenRequested, this, &MainWindow::messageUrlOpenRequested);
        connect(m_notificationsPage, &NotificationsPage::unreadCountChanged, this, [this](int count) {
            setNodeKeyPoints(m_navNotifKey, qMax(0, count));
        });
    }

    // ── KnowledgePage 信号转发 ──
    if (m_knowledgePage) {
        connect(m_knowledgePage, &KnowledgePage::messageUrlOpenRequested, this, &MainWindow::messageUrlOpenRequested);
    }
}

// ═══════════════════════════════════════════════════════════════════════
// 拖拽事件处理 — 窗口级兜底（当子控件未捕获时）
// ═══════════════════════════════════════════════════════════════════════

void MainWindow::showEvent(QShowEvent* event)
{
    ElaWindow::showEvent(event);
    // 每次 show 都重新注册 OLE IDropTarget。
    // 延迟 50ms 确保 ElaAppBar 的 SetWindowLongPtr 样式变更已完成。
    // singleShot(0) 在某些情况下仍然太早（ElaAppBar 的 eventFilter 可能还没跑完）。
    QTimer::singleShot(50, this, [this]() {
        forceRegisterOleDropTarget();
    });
}

void MainWindow::forceRegisterOleDropTarget()
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    if (!hwnd) {
        qWarning() << "[drag-drop] forceRegisterOleDropTarget: no HWND!";
        return;
    }
    // 1. 先清除任何失效的注册
    HRESULT revokeHr = ::RevokeDragDrop(hwnd);

    // 2. 通过 Qt 的 setAcceptDrops 注册（内部调用 RegisterDragDrop）
    setAcceptDrops(false);
    setAcceptDrops(true);

    // 3. 验证注册是否成功：检查窗口是否有 OLE drop target 属性
    IDropTarget* pdt = nullptr;
    HRESULT hr = S_OK;
    // GetPropW "OleDropTargetInterface" 是 Windows 用来存储 IDropTarget 的标准属性
    pdt = static_cast<IDropTarget*>(
        ::GetPropW(hwnd, L"OleDropTargetInterface"));
    if (!pdt) {
        // Qt 注册失败 — 尝试强制重做一次
        qWarning() << "[drag-drop] OLE registration FAILED after setAcceptDrops."
                   << "revokeHr=" << Qt::hex << (unsigned)revokeHr
                   << "Retrying with explicit RevokeDragDrop...";
        ::RevokeDragDrop(hwnd);
        setAcceptDrops(false);
        setAcceptDrops(true);
        pdt = static_cast<IDropTarget*>(
            ::GetPropW(hwnd, L"OleDropTargetInterface"));
    }

    if (!m_dropTargetRegistered) {
        m_dropTargetRegistered = true;
        qInfo() << "[drag-drop] OLE drop target registered on HWND" << (void*)hwnd
                << "acceptDrops:" << acceptDrops()
                << "IDropTarget:" << (pdt ? "OK" : "MISSING");
    }

    // 4. 补充 WM_DROPFILES 回退 + UIPI 过滤
    ::ChangeWindowMessageFilterEx(hwnd, WM_DROPFILES, MSGFLT_ALLOW, nullptr);
    ::ChangeWindowMessageFilterEx(hwnd, WM_COPYDATA, MSGFLT_ALLOW, nullptr);
    ::ChangeWindowMessageFilterEx(hwnd, 0x0049 /*WM_COPYGLOBALDATA*/, MSGFLT_ALLOW, nullptr);
    ::DragAcceptFiles(hwnd, TRUE);
#else
    setAcceptDrops(true);
#endif
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    qInfo() << "[drag-drop] MainWindow::dragEnterEvent mimeFormats:"
            << (event->mimeData() ? event->mimeData()->formats() : QStringList());
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        ElaWindow::dragEnterEvent(event);
    }
}

void MainWindow::dragMoveEvent(QDragMoveEvent* event)
{
    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        ElaWindow::dragMoveEvent(event);
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!event->mimeData()) {
        ElaWindow::dropEvent(event);
        return;
    }
    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList files;
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) files.append(url.toLocalFile());
    }
    if (!files.isEmpty()) {
        for (const QString& fp : files) {
            emit fileSendRequested(fp);
        }
        event->acceptProposedAction();
    } else {
        ElaWindow::dropEvent(event);
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType == "windows_generic_MSG" && message) {
        const auto* msg = static_cast<const MSG*>(message);
        if (msg->message == WM_DROPFILES) {
            HDROP hDrop = reinterpret_cast<HDROP>(msg->wParam);
            const UINT fileCount = ::DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
            QStringList files;
            files.reserve(static_cast<int>(fileCount));
            for (UINT i = 0; i < fileCount; ++i) {
                WCHAR path[MAX_PATH + 1] = {};
                if (::DragQueryFileW(hDrop, i, path, MAX_PATH)) {
                    files.append(QString::fromWCharArray(path));
                }
            }
            ::DragFinish(hDrop);
            if (!files.isEmpty()) {
                for (const QString& fp : files) {
                    emit fileSendRequested(fp);
                }
            }
            *result = 0;
            return true;
        }
    }
    return ElaWindow::nativeEvent(eventType, message, result);
}
#endif

// ═══════════════════════════════════════════════════════════════════════
// 窗口事件处理（保留在 MainWindow）
// ═══════════════════════════════════════════════════════════════════════

void MainWindow::changeEvent(QEvent* event)
{
    if (event
        && (event->type() == QEvent::WindowStateChange
            || event->type() == QEvent::ActivationChange)) {
        if (uiRestoreTraceEnabled()) {
            const QString eventName = event->type() == QEvent::WindowStateChange
                ? QStringLiteral("WindowStateChange")
                : QStringLiteral("ActivationChange");
            qInfo().noquote()
                << QStringLiteral("[ui-restore] seq=%1 phase=main-window-change-event type=%2 %3")
                       .arg(QString::number(uiRestoreTraceSequence()),
                            eventName,
                            mainWindowStateSummary(this));
        }
        emit windowInteractiveStateChanged();
    }
    ElaWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!event) return;

    const bool forceQuit = qApp && qApp->property("leyochat_force_quit").toBool();
    if (forceQuit || !QSystemTrayIcon::isSystemTrayAvailable()) {
        ElaWindow::closeEvent(event);
        return;
    }

    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    ClientPreferences prefs = ClientPreferencesStore::load(dataRoot);
    ClientCloseAction action = prefs.closeWindowAction;

    if (action == ClientCloseAction::AskEveryTime) {
        CloseToTrayDialog dialog(this);
        dialog.setWindowModality(Qt::WindowModal);
        if (dialog.exec() != QDialog::Accepted) {
            event->ignore();
            return;
        }
        action = dialog.selectedAction();
        if (dialog.dontAskAgain()) {
            prefs.closeWindowAction = action;
            ClientPreferencesStore::save(dataRoot, prefs);
        }
    }

    if (action == ClientCloseAction::MinimizeToTray) {
        event->ignore();
        hide();
        emit windowMinimizedToTrayRequested();
        return;
    }

    // ExitApplication: 必须显式 quit()，因为 quitOnLastWindowClosed(false) 下
    // 单纯关闭窗口不会退出进程，应用会继续在托盘运行。
    qApp->setProperty("leyochat_force_quit", true);
    ElaWindow::closeEvent(event);
    qApp->quit();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    ElaWindow::resizeEvent(event);
    repositionUpdateBar();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_globalSearchEdit && event) {
        if (event->type() == QEvent::MouseButtonPress) {
            showGlobalSearchPanel();
            return true;
        }
    }
    return ElaWindow::eventFilter(watched, event);
}

void MainWindow::repositionUpdateBar()
{
    if (!m_updateBar || m_updateBar->isHidden()) return;
    m_updateBar->adjustSize();
    m_updateBar->updateGeometry();
}

// ═══════════════════════════════════════════════════════════════════════
// 通话管理（保留在 MainWindow）
// ═══════════════════════════════════════════════════════════════════════

void MainWindow::showCallWindow(bool outgoing, const QString& peerName, const QString& avatarPath)
{
    closeCallWindow();
    const auto mode = outgoing ? CallWindow::Mode::Outgoing : CallWindow::Mode::Incoming;
    m_callWindow = new CallWindow(mode, peerName, avatarPath, nullptr);
    connect(m_callWindow, &CallWindow::cancelClicked, this, &MainWindow::callCancelRequested);
    connect(m_callWindow, &CallWindow::answerClicked, this, [this]() { emit callAnswered(QString()); });
    connect(m_callWindow, &CallWindow::rejectClicked, this, [this]() { emit callRejected(QString()); });
    connect(m_callWindow, &CallWindow::hangupClicked, this, &MainWindow::callHungUp);
    connect(m_callWindow, &CallWindow::muteToggled, this, &MainWindow::callMuteToggled);
    connect(m_callWindow, &CallWindow::screenShareClicked, this, &MainWindow::screenShareToggled);
    connect(m_callWindow, &CallWindow::remoteControlClicked, this, &MainWindow::remoteControlToggled);
    connect(m_callWindow, &QObject::destroyed, this, [this]() { m_callWindow = nullptr; });
    m_callWindow->show();
}

void MainWindow::updateCallWindowState(CallSession::State state)
{
    if (m_callWindow) m_callWindow->updateState(state);
}

void MainWindow::closeCallWindow()
{
    if (!m_callWindow) return;
    m_callWindow->disconnect(this);
    m_callWindow->close();
    m_callWindow = nullptr;
}

void MainWindow::setCallWindowMuted(bool muted)
{
    if (m_callWindow) m_callWindow->setAudioMuted(muted);
}

// ═══════════════════════════════════════════════════════════════════════
// 外观管理（保留在 MainWindow）
// ═══════════════════════════════════════════════════════════════════════

void MainWindow::refreshTheme()
{
    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    if (qApp) {
        qApp->setProperty("leyochat.themeMode", AppStyle::themeModeToString(mode));
        const ElaThemeType::ThemeMode elaMode = AppStyle::toElaThemeMode(mode);
        if (eTheme->getThemeMode() != elaMode) {
            eTheme->setThemeMode(elaMode);
        }
        qApp->setPalette(AppStyle::applicationPalette(mode));
    }
    setStyleSheet(AppStyle::activeStylesheet(mode));
    applyGlobalSearchStyle();
    if (m_conversationsPage) m_conversationsPage->refreshTheme();
    if (m_knowledgePage) m_knowledgePage->refreshTheme();
    if (m_notificationsPage) m_notificationsPage->refreshTheme();
    if (m_directoryPage) m_directoryPage->refreshTheme();
}

void MainWindow::reloadAppearanceSettings()
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const ClientPreferences prefs = ClientPreferencesStore::load(dataRoot);
    applyClientAppearance(this, prefs, true);
    refreshTheme();
    if (m_conversationsPage) m_conversationsPage->refreshTheme();
    if (m_knowledgePage) m_knowledgePage->refreshTheme();
    if (m_notificationsPage) m_notificationsPage->refreshTheme();
    if (m_directoryPage) m_directoryPage->refreshTheme();
    update();
}

// ═══════════════════════════════════════════════════════════════════════
// 转发到 ConversationsPage 的公共 API
// ═══════════════════════════════════════════════════════════════════════

void MainWindow::setConversationModel(ConversationListModel* model)
{
    if (m_conversationModel && m_conversationModel != model) {
        QObject::disconnect(m_conversationModel,
                            static_cast<const char*>(nullptr),
                            this,
                            static_cast<const char*>(nullptr));
    }
    m_conversationModel = model;
    if (m_conversationsPage) m_conversationsPage->setConversationModel(model);
}

void MainWindow::setContactModel(ContactListModel* model)
{
    if (m_contactModel && m_contactModel != model) {
        QObject::disconnect(m_contactModel,
                            static_cast<const char*>(nullptr),
                            this,
                            static_cast<const char*>(nullptr));
    }
    m_contactModel = model;
    if (m_conversationsPage) m_conversationsPage->setContactModel(model);
    if (m_directoryPage) m_directoryPage->setContactModel(model);
}
void MainWindow::setMessageModel(MessageListModel* model) { if (m_conversationsPage) m_conversationsPage->setMessageModel(model); }
void MainWindow::setTransferModel(QAbstractItemModel* model) { if (m_conversationsPage) m_conversationsPage->setTransferModel(model); }
void MainWindow::setListenPort(quint16 port) { if (m_conversationsPage) m_conversationsPage->setListenPort(port); }
void MainWindow::setChatHeader(const QString& title, const QString& status) { if (m_conversationsPage) m_conversationsPage->setChatHeader(title, status); }
void MainWindow::setChatHeaderDirect(const QString& name, const QString& status, const QString& signature, const QString& avatarImagePath) { if (m_conversationsPage) m_conversationsPage->setChatHeaderDirect(name, status, signature, avatarImagePath); }
void MainWindow::setChatHeaderGroup(const QString& groupId, const QString& groupName, int memberCount) { if (m_conversationsPage) m_conversationsPage->setChatHeaderGroup(groupId, groupName, memberCount); }
void MainWindow::setStatusMessage(const QString& message, int timeoutMs) { if (m_conversationsPage) m_conversationsPage->setStatusMessage(message, timeoutMs); }
void MainWindow::showChatToast(const QString& message, int timeoutMs) { if (m_conversationsPage) m_conversationsPage->showChatToast(message, timeoutMs); }
void MainWindow::setRuntimeArchitectureSummary(int serviceCount, int workspaceBindingCount, int groupBindingCount, int resourceCount, bool bound, const QString& activeServiceName) { if (m_conversationsPage) m_conversationsPage->setRuntimeArchitectureSummary(serviceCount, workspaceBindingCount, groupBindingCount, resourceCount, bound, activeServiceName); }
void MainWindow::setRuntimeArchitectureSnapshot(const RuntimeArchitectureSnapshot& snapshot) { if (m_conversationsPage) m_conversationsPage->setRuntimeArchitectureSnapshot(snapshot); }
void MainWindow::setNotificationItems(const QVector<SystemNotificationItem>& items) { if (m_notificationsPage) m_notificationsPage->setNotificationItems(items); }
void MainWindow::appendNotificationItem(const SystemNotificationItem& item) { if (m_notificationsPage) m_notificationsPage->appendNotificationItem(item); }
void MainWindow::setActiveReminders(const QVector<ReminderItem>& reminders) { if (m_notificationsPage) m_notificationsPage->setActiveReminders(reminders); }
void MainWindow::setSelectedConversationId(const QString& conversationId) { if (m_conversationsPage) m_conversationsPage->setSelectedConversationId(conversationId); }
void MainWindow::setSelectedContactId(const QString& clientId) { if (m_directoryPage) m_directoryPage->setSelectedContactId(clientId); }
void MainWindow::setSelectedTransferId(const QString& taskId) { if (m_conversationsPage) m_conversationsPage->setSelectedTransferId(taskId); }
void MainWindow::focusChatInput() { if (m_conversationsPage) m_conversationsPage->focusChatInput(); }
bool MainWindow::isShowingWelcomePage() const { return m_conversationsPage && m_conversationsPage->isShowingWelcomePage(); }
bool MainWindow::isShowingChatPage() const { return m_conversationsPage && m_conversationsPage->isShowingChatPage(); }
bool MainWindow::isShowingNotificationPage() const { return false; /* notification is separate page */ }
void MainWindow::showMessagesPage() { navigation(m_conversationsPageKey); }
QString MainWindow::recoveryPageId() const
{
    const QString currentKey = getCurrentNavigationPageKey();
    for (auto it = m_primaryPageKeys.cbegin(); it != m_primaryPageKeys.cend(); ++it) {
        if (it.key() != QStringLiteral("workbench") && it.value() == currentKey) {
            return it.key();
        }
    }
    return QStringLiteral("messages");
}
bool MainWindow::navigateToRecoveryPage(const QString& stablePageId)
{
    const QString pageId = stablePageId.trimmed().toLower();
    if (pageId == QStringLiteral("workbench") || !m_primaryPageKeys.contains(pageId)) {
        return false;
    }
    const QString pageKey = m_primaryPageKeys.value(pageId);
    if (pageKey.isEmpty()) {
        return false;
    }
    navigation(pageKey);
    return true;
}
bool MainWindow::isGroupPanelVisible() const { return m_conversationsPage && m_conversationsPage->isGroupPanelVisible(); }
bool MainWindow::isGroupWorkspaceActive() const { return m_conversationsPage && m_conversationsPage->isGroupWorkspaceActive(); }
void MainWindow::showDirectConversation(const QString& conversationId, const QString& title) { if (m_conversationsPage) m_conversationsPage->showDirectConversation(conversationId, title); }
void MainWindow::showGroupConversation(const QString& conversationId, const QString& title) { if (m_conversationsPage) m_conversationsPage->showGroupConversation(conversationId, title); }
void MainWindow::clearCurrentConversationView() { if (m_conversationsPage) m_conversationsPage->clearCurrentConversationView(); }
void MainWindow::clearComposerDraft(const QString& outgoingConversationId) { if (m_conversationsPage) m_conversationsPage->clearComposerDraft(outgoingConversationId); }
void MainWindow::restoreComposerDraft(const QString& conversationId) { if (m_conversationsPage) m_conversationsPage->restoreComposerDraft(conversationId); }
ComposerRecoveryContext MainWindow::activeComposerRecoveryContext() const
{
    return m_conversationsPage ? m_conversationsPage->activeComposerRecoveryContext()
                               : ComposerRecoveryContext{};
}
QString MainWindow::activeComposerContextId() const
{
    return m_conversationsPage ? m_conversationsPage->activeComposerContextId() : QString();
}
void MainWindow::stageRecoveredComposerContext(
    const QString& conversationId, const ComposerRecoveryContext& context)
{
    if (m_conversationsPage) {
        m_conversationsPage->stageRecoveredComposerContext(conversationId, context);
    }
}
void MainWindow::syncDraftsToModel() { if (m_conversationsPage) m_conversationsPage->syncDraftsToModel(); }
bool MainWindow::importScreenshotPreview(const QImage& image) { return m_conversationsPage ? m_conversationsPage->importScreenshotPreview(image) : false; }
void MainWindow::triggerScreenshot(bool forceHideWindow)
{
    const bool searchPanelActive = (m_globalSearchPanel && m_globalSearchPanel->isVisible())
        || (m_globalSearchEdit && m_globalSearchEdit->hasFocus());
    if (searchPanelActive) {
        hideGlobalSearchPanel();
        if (m_globalSearchEdit) {
            m_globalSearchEdit->clearFocus();
        }
        if (m_conversationsPage) {
            m_conversationsPage->showChatToast(QStringLiteral("已关闭搜索面板，再按一次截图"), 1800);
        }
        return;
    }
    if (m_conversationsPage) {
        QTimer::singleShot(0, this, [this, forceHideWindow]() {
            if (m_conversationsPage) {
                m_conversationsPage->triggerScreenshot(forceHideWindow);
            }
        });
    }
}
int MainWindow::pendingAttachmentCount() const { return m_conversationsPage ? m_conversationsPage->pendingAttachmentCount() : 0; }
void MainWindow::submitCurrentComposer() { if (m_conversationsPage) m_conversationsPage->submitCurrentComposer(); }
void MainWindow::setNavUnreadCount(int count) { setNodeKeyPoints(m_navMsgKey, qMax(0, count)); }
void MainWindow::setNavGroupUnreadCount(int /*count*/) { }
void MainWindow::setNavNotificationCount(int count) { setNodeKeyPoints(m_navNotifKey, qMax(0, count)); }
void MainWindow::showProfileCard(const ProfileCardPopup::ProfileInfo& info, const QPoint& globalPos) { if (m_conversationsPage) m_conversationsPage->showProfileCard(info, globalPos); }
void MainWindow::setGroupInfoPanel(const QString& announcement, const GroupMemberListEntries& members, bool currentUserCanManageMembers) { if (m_conversationsPage) m_conversationsPage->setGroupInfoPanel(announcement, members, currentUserCanManageMembers); }
void MainWindow::setGroupMembers(const GroupMemberListEntries& members, bool currentUserCanManageMembers) { if (m_conversationsPage) m_conversationsPage->setGroupMembers(members, currentUserCanManageMembers); }
void MainWindow::setPinnedMessageCards(const std::vector<PinnedCardInfo>& cards) { if (m_conversationsPage) m_conversationsPage->setPinnedMessageCards(cards); }
void MainWindow::clearPinnedMessageCards() { if (m_conversationsPage) m_conversationsPage->clearPinnedMessageCards(); }
void MainWindow::setCurrentUserIsGroupOwner(bool isOwner) { if (m_conversationsPage) m_conversationsPage->setCurrentUserIsGroupOwner(isOwner); }
void MainWindow::setAiKnowledgeServices(const QVector<KnowledgeServiceConfig>& configs, const QString& preferredServiceId) { if (m_knowledgePage) m_knowledgePage->setAiKnowledgeServices(configs, preferredServiceId); }
void MainWindow::setDirectoryGroups(const QVector<GroupSummary>& groups)
{
    m_directoryGroups = groups;
    if (m_directoryPage) m_directoryPage->setGroups(groups);
}

void MainWindow::setDirectoryOrgData(const QHash<QString, QVector<OrgContactEntry>>& departments)
{
    if (m_directoryPage) m_directoryPage->setOrgData(departments);
}

void MainWindow::setLocalDisplayName(const QString& name)
{
    m_localDisplayName = name;
    setUserInfoCardTitle(name);
    setUserInfoCardSubTitle(QString());
    if (m_conversationsPage) m_conversationsPage->setLocalDisplayName(name);
}

void MainWindow::setAvatarText(const QString& letter)
{
    setUserInfoCardTitle(m_localDisplayName.isEmpty() ? letter : m_localDisplayName);

    // Generate a text avatar to replace the component library placeholder.
    const QString ch = letter.isEmpty() ? QStringLiteral("?") : letter.left(1).toUpper();
    constexpr int kSize = 64;
    static const QColor kPalette[] = {
        QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
        QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
        QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
    };
    const QColor bg = kPalette[qHash(m_localDisplayName) % 6];
    const QString cacheKey = QStringLiteral("mw-avatar|%1|%2").arg(ch, bg.name());
    QPixmap pm;
    if (!QPixmapCache::find(cacheKey, &pm)) {
        pm = QPixmap(kSize, kSize);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawEllipse(0, 0, kSize, kSize);
        QFont f = p.font();
        f.setBold(true);
        f.setPixelSize(kSize / 3);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(QRect(0, 0, kSize, kSize), Qt::AlignCenter, ch);
        p.end();
        QPixmapCache::insert(cacheKey, pm);
    }
    m_currentAvatarPixmap = pm;
    setUserInfoCardPixmap(pm);
}

void MainWindow::setAvatarImagePath(const QString& imagePath)
{
    const QFileInfo info(imagePath);
    if (!info.exists() || !info.isFile()) return;
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) return;
    m_currentAvatarPixmap = pixmap;
    setUserInfoCardPixmap(pixmap);
}

GroupInfoPanel* MainWindow::groupInfoPanel() const { return m_conversationsPage ? m_conversationsPage->groupInfoPanel() : nullptr; }
ChatComposerWidget* MainWindow::chatComposerWidget() const { return m_conversationsPage ? m_conversationsPage->chatComposerWidget() : nullptr; }
ChatHeaderWidget* MainWindow::chatHeaderWidget() const { return m_conversationsPage ? m_conversationsPage->chatHeaderWidget() : nullptr; }
