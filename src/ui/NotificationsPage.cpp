#include "NotificationsPage.h"

#include "app/ReminderActionRouting.h"
#include "ui/AppStyle.h"
#include "ui/LeyoDialog.h"

#include <ElaFrame.h>
#include <ElaListWidget.h>
#include <ElaPushButton.h>
#include <ElaSplitter.h>
#include <ElaText.h>

#include <QClipboard>
#include <QAbstractScrollArea>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QDateTime>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

#ifdef LEYOCHAT_HAS_WEBENGINE
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineNavigationRequest>
#endif

namespace {

QString shellGlass()
{
    return AppStyle::isDarkTheme()
        ? QStringLiteral("rgba(30,34,40,145)")
        : QStringLiteral("rgba(255,255,255,112)");
}

QString softGlass()
{
    return AppStyle::isDarkTheme()
        ? QStringLiteral("rgba(38,43,50,118)")
        : QStringLiteral("rgba(255,255,255,92)");
}

QString reminderTargetLabel(const QString& targetType)
{
    const QString normalized = targetType.trimmed();
    if (normalized == QStringLiteral("message")) {
        return QStringLiteral("消息");
    }
    if (normalized == QStringLiteral("contact")) {
        return QStringLiteral("联系人");
    }
    if (normalized == QStringLiteral("group_announcement")) {
        return QStringLiteral("群公告");
    }
    if (normalized == QStringLiteral("group_file")) {
        return QStringLiteral("群文件");
    }
    return QStringLiteral("提醒");
}

QString reminderDueTimeText(qint64 dueAtMs)
{
    if (dueAtMs <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(dueAtMs).toString(QStringLiteral("MM-dd HH:mm"));
}

}

NotificationsPage::NotificationsPage(QWidget* parent)
    : ElaScrollPage(parent)
{
    setTitleVisible(false);

    auto* contentWidget = new QWidget(this);
    contentWidget->setObjectName(QStringLiteral("notificationsPageRoot"));
    contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    contentWidget->setAutoFillBackground(false);
    contentWidget->setStyleSheet(QStringLiteral("QWidget#notificationsPageRoot { background:transparent; }"));
    auto* rootLayout = new QHBoxLayout(contentWidget);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* splitter = new ElaSplitter(Qt::Horizontal, contentWidget);
    splitter->setObjectName(QStringLiteral("notificationSplitter"));
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(1);
    rootLayout->addWidget(splitter);

    // ══════════════ 左侧：通知列表 ══════════════
    auto* sidePanel = new ElaFrame(splitter);
    sidePanel->setObjectName(QStringLiteral("notificationListSurface"));
    auto* sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->setSpacing(0);

    auto* notificationHeader = new ElaFrame(sidePanel);
    notificationHeader->setObjectName(QStringLiteral("sideHeaderBar"));
    auto* notificationHeaderLayout = new QVBoxLayout(notificationHeader);
    notificationHeaderLayout->setContentsMargins(12, 12, 12, 12);
    notificationHeaderLayout->setSpacing(10);

    auto* notificationHeaderCard = new ElaFrame(notificationHeader);
    notificationHeaderCard->setObjectName(QStringLiteral("notificationHeaderCard"));
    auto* notificationHeaderCardLayout = new QVBoxLayout(notificationHeaderCard);
    notificationHeaderCardLayout->setContentsMargins(16, 14, 16, 14);
    notificationHeaderCardLayout->setSpacing(6);

    // 头部行：[通知 xx] [未读 xx]
    auto* notificationTitleRow = new QHBoxLayout;
    notificationTitleRow->setContentsMargins(0, 0, 0, 0);
    notificationTitleRow->setSpacing(8);

    m_notificationModeChip = new ElaText(QStringLiteral("\u901A\u77E5"), notificationHeaderCard);
    m_notificationModeChip->setObjectName(QStringLiteral("notificationModeChip"));
    m_notificationModeChip->setProperty("surfaceChipRole", QStringLiteral("mode"));
    m_notificationModeChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_notificationModeChip->setMinimumWidth(
        QFontMetrics(m_notificationModeChip->font()).horizontalAdvance(QStringLiteral("\u901A\u77E5")) + 28);

    m_notificationStatusChip = new ElaText(QStringLiteral("\u6682\u65E0\u672A\u8BFB"), notificationHeaderCard);
    m_notificationStatusChip->setObjectName(QStringLiteral("notificationStatusChip"));
    m_notificationStatusChip->setProperty("surfaceChipRole", QStringLiteral("status"));
    m_notificationStatusChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_notificationStatusChip->setMinimumWidth(
        QFontMetrics(m_notificationStatusChip->font()).horizontalAdvance(QStringLiteral("\u6682\u65E0\u672A\u8BFB")) + 28);

    notificationTitleRow->addWidget(m_notificationModeChip);
    notificationTitleRow->addWidget(m_notificationStatusChip);
    notificationTitleRow->addStretch();

    notificationHeaderCardLayout->addLayout(notificationTitleRow);
    notificationHeaderLayout->addWidget(notificationHeaderCard);
    sideLayout->addWidget(notificationHeader);

    m_activeReminderSection = new ElaFrame(sidePanel);
    m_activeReminderSection->setObjectName(QStringLiteral("activeRemindersSection"));
    auto* activeReminderLayout = new QVBoxLayout(m_activeReminderSection);
    activeReminderLayout->setContentsMargins(12, 0, 12, 10);
    activeReminderLayout->setSpacing(8);

    m_activeReminderStatus = new ElaText(QStringLiteral("提醒 0"), m_activeReminderSection);
    m_activeReminderStatus->setObjectName(QStringLiteral("activeReminderStatus"));
    m_activeReminderStatus->setTextStyle(ElaTextType::Caption);
    activeReminderLayout->addWidget(m_activeReminderStatus);

    m_activeReminderList = new ElaListWidget(m_activeReminderSection);
    m_activeReminderList->setObjectName(QStringLiteral("activeReminderList"));
    m_activeReminderList->setIsTransparent(true);
    m_activeReminderList->setFrameShape(QFrame::NoFrame);
    m_activeReminderList->setSelectionMode(QAbstractItemView::NoSelection);
    m_activeReminderList->setUniformItemSizes(false);
    m_activeReminderList->setSpacing(4);
    m_activeReminderList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_activeReminderList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_activeReminderList->setFocusPolicy(Qt::NoFocus);
    activeReminderLayout->addWidget(m_activeReminderList);
    sideLayout->addWidget(m_activeReminderSection);

    // notification list body
    auto* notificationsBodyWidget = new ElaFrame(sidePanel);
    notificationsBodyWidget->setObjectName(QStringLiteral("notificationListBody"));
    auto* notificationsBodyLayout = new QVBoxLayout(notificationsBodyWidget);
    notificationsBodyLayout->setContentsMargins(0, 0, 0, 0);
    notificationsBodyLayout->setSpacing(0);
    m_notificationEmptyLabel = new ElaText(
        QStringLiteral("\u6682\u65E0\u7CFB\u7EDF\u901A\u77E5\nOutlook \u548C\u540E\u7EED\u96C6\u6210\u7684\u63D0\u9192\u4F1A\u51FA\u73B0\u5728\u8FD9\u91CC"),
        notificationsBodyWidget);
    m_notificationEmptyLabel->setObjectName(QStringLiteral("notificationEmptyLabel"));
    m_notificationEmptyLabel->setAlignment(Qt::AlignCenter);
    m_notificationEmptyLabel->setWordWrap(true);
    m_notificationEmptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_notificationEmptyLabel->setTextPixelSize(13);

    m_notificationList = new ElaListWidget(this);
    m_notificationList->setObjectName(QStringLiteral("notificationList"));
    m_notificationList->setIsTransparent(true);
    m_notificationList->setFrameShape(QFrame::NoFrame);
    m_notificationList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_notificationList->setUniformItemSizes(false);
    m_notificationList->setWordWrap(true);
    m_notificationList->setSpacing(2);
    m_notificationList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_notificationList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_notificationList->setFocusPolicy(Qt::NoFocus);
    notificationsBodyLayout->addWidget(m_notificationEmptyLabel, 1);
    notificationsBodyLayout->addWidget(m_notificationList, 1);
    sideLayout->addWidget(notificationsBodyWidget, 1);
    splitter->addWidget(sidePanel);

    // ══════════════ 右侧：通知详情 ══════════════
    auto* detailPanel = new ElaFrame(splitter);
    detailPanel->setObjectName(QStringLiteral("notificationDetailPanel"));
    auto* notificationPageLayout = new QVBoxLayout(detailPanel);
    notificationPageLayout->setContentsMargins(24, 20, 24, 24);
    notificationPageLayout->setSpacing(12);

    auto* notificationHero = new ElaFrame(detailPanel);
    notificationHero->setObjectName(QStringLiteral("notificationHeroCard"));
    auto* notificationHeroLayout = new QHBoxLayout(notificationHero);
    notificationHeroLayout->setContentsMargins(20, 18, 20, 18);
    notificationHeroLayout->setSpacing(16);
    auto* notificationHeroTextLayout = new QVBoxLayout;
    notificationHeroTextLayout->setContentsMargins(0, 0, 0, 0);
    notificationHeroTextLayout->setSpacing(6);

    auto* notificationHeroKicker = new ElaText(QStringLiteral("\u7CFB\u7EDF\u63D0\u9192"), notificationHero);
    notificationHeroKicker->setTextStyle(ElaTextType::Caption);
    auto* notificationHeroTitle = new ElaText(QStringLiteral("\u901A\u77E5\u8BE6\u60C5"), notificationHero);
    notificationHeroTitle->setTextStyle(ElaTextType::TitleLarge);
    notificationHeroTitle->setWordWrap(true);
    auto* notificationHeroBody = new ElaText(
        QStringLiteral("\u4ECE\u5DE6\u4FA7\u9009\u62E9\u4E00\u6761\u63D0\u9192\u67E5\u770B\u6458\u8981\u3001\u539F\u59CB\u94FE\u63A5\u548C\u540E\u7EED\u5904\u7406\u52A8\u4F5C\u3002"),
        notificationHero);
    notificationHeroBody->setTextStyle(ElaTextType::Body);
    notificationHeroBody->setWordWrap(true);
    notificationHeroTextLayout->addWidget(notificationHeroKicker, 0, Qt::AlignLeft);
    notificationHeroTextLayout->addWidget(notificationHeroTitle);
    notificationHeroTextLayout->addWidget(notificationHeroBody);
    notificationHeroLayout->addLayout(notificationHeroTextLayout, 1);
    auto* notificationHeroMeta = new ElaText(
        QStringLiteral("\u81EA\u52A8\u63D0\u9192\u9ED8\u8BA4\u90FD\u4F1A\u8FDB\u5165\u8FD9\u91CC\uFF0C\u800C\u4E0D\u662F\u53D1\u5230\u67D0\u4E2A\u79C1\u804A\u6216\u7FA4\u804A\u4F1A\u8BDD"),
        notificationHero);
    notificationHeroMeta->setTextStyle(ElaTextType::Caption);
    notificationHeroMeta->setAlignment(Qt::AlignRight | Qt::AlignTop);
    notificationHeroMeta->setWordWrap(true);
    notificationHeroMeta->setMaximumWidth(200);
    notificationHeroLayout->addWidget(notificationHeroMeta, 0, Qt::AlignTop);

    auto* notificationDetailCard = new ElaFrame(detailPanel);
    notificationDetailCard->setObjectName(QStringLiteral("notificationDetailSurface"));
    m_notificationDetailCard = notificationDetailCard;
    auto* notificationDetailLayout = new QVBoxLayout(notificationDetailCard);
    notificationDetailLayout->setContentsMargins(20, 18, 20, 18);
    notificationDetailLayout->setSpacing(10);

    m_notificationDetailSource = new ElaText(QStringLiteral("\u7B49\u5F85\u901A\u77E5"), notificationDetailCard);
    m_notificationDetailSource->setObjectName(QStringLiteral("notificationDetailSource"));
    m_notificationDetailSource->setTextStyle(ElaTextType::Caption);
    m_notificationDetailTitle = new ElaText(QStringLiteral("\u8FD8\u6CA1\u6709\u7CFB\u7EDF\u63D0\u9192"), notificationDetailCard);
    m_notificationDetailTitle->setObjectName(QStringLiteral("notificationDetailTitle"));
    m_notificationDetailTitle->setTextStyle(ElaTextType::Title);
    m_notificationDetailTitle->setWordWrap(true);
    m_notificationDetailSummary = new ElaText(
        QStringLiteral("\u5F53 Outlook \u4EA7\u751F\u63D0\u9192\u65F6\uFF0C\u8BE6\u60C5\u4F1A\u663E\u793A\u5728\u8FD9\u91CC\u3002"),
        notificationDetailCard);
    m_notificationDetailSummary->setObjectName(QStringLiteral("notificationDetailSummary"));
    m_notificationDetailSummary->setTextStyle(ElaTextType::Body);
    m_notificationDetailSummary->setWordWrap(true);
    m_notificationDetailDetail = new ElaText(QString(), notificationDetailCard);
    m_notificationDetailDetail->setObjectName(QStringLiteral("notificationDetailDetail"));
    m_notificationDetailDetail->setTextStyle(ElaTextType::Body);
    m_notificationDetailDetail->setWordWrap(true);
    m_notificationDetailTimestamp = new ElaText(QString(), notificationDetailCard);
    m_notificationDetailTimestamp->setObjectName(QStringLiteral("notificationDetailTimestamp"));
    m_notificationDetailTimestamp->setTextPixelSize(12);

    m_notificationOpenButton = new ElaPushButton(QStringLiteral("\u6253\u5F00\u539F\u59CB\u9875\u9762"), notificationDetailCard);
    m_notificationOpenButton->setObjectName(QStringLiteral("notificationOpenButton"));
    m_notificationOpenButton->hide();
    m_notificationMarkReadButton = new ElaPushButton(QStringLiteral("\u6807\u8BB0\u5DF2\u8BFB"), notificationDetailCard);
    m_notificationMarkReadButton->setObjectName(QStringLiteral("notificationMarkReadButton"));
    m_notificationMarkReadButton->hide();
    m_notificationArchiveButton = new ElaPushButton(QStringLiteral("\u5220\u9664\u901A\u77E5"), notificationDetailCard);
    m_notificationArchiveButton->setObjectName(QStringLiteral("notificationArchiveButton"));
    m_notificationArchiveButton->hide();
    m_notificationCopyLinkButton = new ElaPushButton(QStringLiteral("\u590D\u5236\u94FE\u63A5"), notificationDetailCard);
    m_notificationCopyLinkButton->setObjectName(QStringLiteral("notificationCopyLinkButton"));
    m_notificationCopyLinkButton->hide();

    auto* notificationActionRow = new QHBoxLayout;
    notificationActionRow->setContentsMargins(0, 4, 0, 0);
    notificationActionRow->setSpacing(10);
    notificationActionRow->addWidget(m_notificationMarkReadButton, 0, Qt::AlignLeft);
    notificationActionRow->addWidget(m_notificationArchiveButton, 0, Qt::AlignLeft);
    notificationActionRow->addWidget(m_notificationOpenButton, 0, Qt::AlignLeft);
    notificationActionRow->addWidget(m_notificationCopyLinkButton, 0, Qt::AlignLeft);
    notificationActionRow->addStretch();

    // action button connections
    connect(m_notificationOpenButton, &QAbstractButton::clicked, this, [this]() {
        for (const SystemNotificationItem& item : std::as_const(m_notificationItems)) {
            if (item.notificationId == m_selectedNotificationId
                && !item.actionUrl.trimmed().isEmpty()) {
                markNotificationReadLocally(item.notificationId);
                emit notificationMarkedReadRequested(item.notificationId);
                emit messageUrlOpenRequested(item.actionUrl.trimmed());
                break;
            }
        }
    });
    connect(m_notificationCopyLinkButton, &QAbstractButton::clicked, this, [this]() {
        for (const SystemNotificationItem& item : std::as_const(m_notificationItems)) {
            if (item.notificationId == m_selectedNotificationId
                && !item.actionUrl.trimmed().isEmpty()) {
                if (QClipboard* clipboard = QGuiApplication::clipboard())
                    clipboard->setText(item.actionUrl.trimmed());
                break;
            }
        }
    });
    connect(m_notificationMarkReadButton, &QAbstractButton::clicked, this, [this]() {
        const QString notificationId = m_selectedNotificationId.trimmed();
        if (notificationId.isEmpty()) return;
        markNotificationReadLocally(notificationId);
        emit notificationMarkedReadRequested(notificationId);
    });
    connect(m_notificationArchiveButton, &QAbstractButton::clicked, this, [this]() {
        const QString notificationId = m_selectedNotificationId.trimmed();
        if (notificationId.isEmpty()) return;
        const bool requiresConfirm =
            QGuiApplication::platformName().compare(QStringLiteral("offscreen"), Qt::CaseInsensitive) != 0;
        if (requiresConfirm
            && !LeyoDialog::question(this,
                QStringLiteral("\u5220\u9664\u901A\u77E5"),
                QStringLiteral("\u786E\u5B9A\u8981\u5220\u9664\u8FD9\u6761\u901A\u77E5\u5417\uFF1F\u5220\u9664\u540E\u4E0D\u53EF\u6062\u590D\u3002"))) {
            return;
        }
        archiveNotificationLocally(notificationId);
        emit notificationArchivedRequested(notificationId);
    });

    notificationDetailLayout->addWidget(m_notificationDetailSource);
    notificationDetailLayout->addWidget(m_notificationDetailTitle);
    notificationDetailLayout->addWidget(m_notificationDetailSummary);
    notificationDetailLayout->addWidget(m_notificationDetailDetail);
    notificationDetailLayout->addWidget(m_notificationDetailTimestamp);
    notificationDetailLayout->addLayout(notificationActionRow);
    notificationDetailLayout->addStretch();

    notificationPageLayout->addWidget(notificationHero);
    notificationPageLayout->addWidget(notificationDetailCard, 1);
    splitter->addWidget(detailPanel);

    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 800});
    addCentralWidget(contentWidget);

    // ── 列表信号连接 ──
    connect(m_notificationList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                if (!current) {
                    m_selectedNotificationId.clear();
                    updateNotificationDetailPane();
                    return;
                }
                m_selectedNotificationId = current->data(Qt::UserRole).toString();
                markNotificationReadLocally(m_selectedNotificationId);
                emit notificationMarkedReadRequested(m_selectedNotificationId);
                updateNotificationDetailPane();
            });
    connect(m_notificationList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
                if (!item) return;
                const QString notificationId = item->data(Qt::UserRole).toString().trimmed();
                for (SystemNotificationItem& notification : m_notificationItems) {
                    if (notification.notificationId != notificationId) continue;
                    if (!notification.actionUrl.trimmed().isEmpty()) {
                        markNotificationReadLocally(notification.notificationId);
                        emit notificationMarkedReadRequested(notification.notificationId);
                        const QUrl url(notification.actionUrl.trimmed());
                        if (url.isValid()) QDesktopServices::openUrl(url);
                        emit messageUrlOpenRequested(notification.actionUrl.trimmed());
                    }
                    return;
                }
            });

    refreshTheme();
    refreshActiveReminderList();
    refreshNotificationList();
}

// ═════════════════════════════════════════════════════════════════
// 业务方法
// ═════════════════════════════════════════════════════════════════

void NotificationsPage::setStatusMessage(const QString& /*message*/, int /*timeoutMs*/)
{
    // Status messages are handled by MainWindow/ConversationsPage
}

void NotificationsPage::refreshTheme()
{
    setStyleSheet(QStringLiteral(
        "NotificationsPage, QWidget#notificationsPageRoot, ElaFrame#notificationListSurface, ElaFrame#notificationDetailPanel { background:transparent; border:none; }"
        "ElaFrame#sideHeaderBar { background:transparent; border:none; }"
        "ElaFrame#notificationHeaderCard, ElaFrame#notificationHeroCard,"
        "ElaFrame#notificationDetailSurface { background:%1; border:1px solid %2; border-radius:12px; }"
        "ElaFrame#notificationListBody { background:transparent; border:none; }"
        "ElaListWidget { background:transparent; border:none; outline:none; }"
        "ElaListWidget::item { border:none; outline:none; margin:0; padding:0; background:transparent; }"
        "ElaListWidget::item:selected { background:transparent; border:none; outline:none; }"
        "ElaListWidget::item:hover { background:transparent; border:none; outline:none; }"
        "ElaListWidget::item:focus { outline:none; border:none; }"
        "ElaText { color:%3; }")
        .arg(shellGlass(), AppStyle::border(), AppStyle::textPrimary()));

    for (auto* area : findChildren<QAbstractScrollArea*>()) {
        if (!area || !area->viewport()) continue;
        area->setAutoFillBackground(false);
        area->viewport()->setAutoFillBackground(false);
        QPalette palette = area->viewport()->palette();
        palette.setColor(QPalette::Base, Qt::transparent);
        palette.setColor(QPalette::Window, Qt::transparent);
        area->viewport()->setPalette(palette);
    }

    const QString chipStyle = QStringLiteral(
        "ElaText { background:%1; color:%2; border:1px solid %3; border-radius:12px; padding:4px 10px; }")
        .arg(softGlass(), AppStyle::textSecondary(), AppStyle::border());
    if (m_notificationModeChip) m_notificationModeChip->setStyleSheet(chipStyle);
    if (m_notificationStatusChip) m_notificationStatusChip->setStyleSheet(chipStyle);
    if (m_activeReminderStatus) m_activeReminderStatus->setStyleSheet(chipStyle);
    if (m_notificationEmptyLabel) {
        m_notificationEmptyLabel->setStyleSheet(
            QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::textMuted()));
    }
    if (m_notificationDetailSource) {
        m_notificationDetailSource->setStyleSheet(
            QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::accent()));
    }
    if (m_notificationDetailTitle) {
        m_notificationDetailTitle->setStyleSheet(
            QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::textPrimary()));
    }
    if (m_notificationDetailSummary) {
        m_notificationDetailSummary->setStyleSheet(
            QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::textSecondary()));
    }
    if (m_notificationDetailDetail) {
        m_notificationDetailDetail->setStyleSheet(
            QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::textSecondary()));
    }
    if (m_notificationDetailTimestamp) {
        m_notificationDetailTimestamp->setStyleSheet(
            QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::textMuted()));
    }

    const QString actionButtonStyle = QStringLiteral(
        "QPushButton, ElaPushButton {"
        "  background:%1;"
        "  color:%2;"
        "  border:1px solid %3;"
        "  border-radius:8px;"
        "  padding:6px 12px;"
        "  min-height:32px;"
        "}"
        "QPushButton:hover, ElaPushButton:hover {"
        "  background:%4;"
        "  border-color:%5;"
        "}"
        "QPushButton:pressed, ElaPushButton:pressed {"
        "  background:%6;"
        "}")
        .arg(softGlass(),
             AppStyle::textPrimary(),
             AppStyle::border(),
             shellGlass(),
             AppStyle::accent(),
             AppStyle::isDarkTheme() ? QStringLiteral("rgba(20,24,30,180)")
                                     : QStringLiteral("rgba(235,240,247,180)"));
    const QString dangerButtonStyle = QStringLiteral(
        "QPushButton, ElaPushButton {"
        "  background:#E81123;"
        "  color:white;"
        "  border:1px solid rgba(0,0,0,0);"
        "  border-radius:8px;"
        "  padding:6px 12px;"
        "  min-height:32px;"
        "}"
        "QPushButton:hover, ElaPushButton:hover { background:#C50F1F; }"
        "QPushButton:pressed, ElaPushButton:pressed { background:#A10D19; }");

    if (m_notificationOpenButton) m_notificationOpenButton->setStyleSheet(actionButtonStyle);
    if (m_notificationCopyLinkButton) m_notificationCopyLinkButton->setStyleSheet(actionButtonStyle);
    if (m_notificationMarkReadButton) m_notificationMarkReadButton->setStyleSheet(actionButtonStyle);
    if (m_notificationArchiveButton) m_notificationArchiveButton->setStyleSheet(dangerButtonStyle);
}

void NotificationsPage::setNotificationItems(const QVector<SystemNotificationItem>& items)
{
    m_notificationItems = items;
    refreshNotificationList();
}

void NotificationsPage::appendNotificationItem(const SystemNotificationItem& item)
{
    m_notificationItems.prepend(item);
    while (m_notificationItems.size() > 200) m_notificationItems.removeLast();
    refreshNotificationList();
}

void NotificationsPage::setActiveReminders(const QVector<ReminderItem>& reminders)
{
    m_activeReminders = reminders;
    std::sort(m_activeReminders.begin(), m_activeReminders.end(), [](const ReminderItem& lhs, const ReminderItem& rhs) {
        if (lhs.dueAtMs == rhs.dueAtMs) {
            return lhs.reminderId < rhs.reminderId;
        }
        return lhs.dueAtMs < rhs.dueAtMs;
    });
    refreshActiveReminderList();
}

void NotificationsPage::refreshActiveReminderList()
{
    if (!m_activeReminderSection || !m_activeReminderList || !m_activeReminderStatus) {
        return;
    }

    m_activeReminderList->clear();
    for (const ReminderItem& item : std::as_const(m_activeReminders)) {
        auto* widgetItem = new QListWidgetItem;
        widgetItem->setData(Qt::UserRole, item.reminderId);
        widgetItem->setSizeHint(QSize(0, 118));
        m_activeReminderList->addItem(widgetItem);
        m_activeReminderList->setItemWidget(widgetItem, createActiveReminderCard(item));
    }

    const int count = m_activeReminders.size();
    m_activeReminderStatus->setText(QStringLiteral("提醒 %1").arg(count));
    m_activeReminderSection->setVisible(count > 0);
}

void NotificationsPage::refreshNotificationList()
{
    if (!m_notificationList) return;

    const QString previousSelection = m_selectedNotificationId;
    const int previousScrollValue = m_notificationList->verticalScrollBar()
                                        ? m_notificationList->verticalScrollBar()->value() : 0;
    QSignalBlocker blocker(m_notificationList);
    m_notificationList->clear();

    for (const SystemNotificationItem& item : std::as_const(m_notificationItems)) {
        auto* widgetItem = new QListWidgetItem;
        widgetItem->setData(Qt::UserRole, item.notificationId);
        widgetItem->setSizeHint(QSize(0, 92));
        m_notificationList->addItem(widgetItem);
        m_notificationList->setItemWidget(widgetItem, createNotificationItemCard(item));
        if (!previousSelection.trimmed().isEmpty() && previousSelection == item.notificationId)
            m_notificationList->setCurrentItem(widgetItem);
    }

    if (!m_notificationList->currentItem()) {
        m_selectedNotificationId.clear();
        m_notificationList->clearSelection();
    }
    if (m_notificationList->verticalScrollBar() && previousScrollValue > 0)
        m_notificationList->verticalScrollBar()->setValue(previousScrollValue);
    updateNotificationDetailPane();
    syncNotificationWorkspaceStatus();
}

QWidget* NotificationsPage::createActiveReminderCard(const ReminderItem& item)
{
    auto* card = new QWidget(m_activeReminderList);
    card->setObjectName(QStringLiteral("activeReminderItem"));
    card->setStyleSheet(QStringLiteral(
        "QWidget#activeReminderItem { background:%1; border:1px solid %2; border-radius:8px; }"
        "ElaText { background:transparent; }")
        .arg(softGlass(), AppStyle::border()));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 8, 10, 8);
    layout->setSpacing(5);

    auto* title = new ElaText(item.titleSnapshot.trimmed().isEmpty()
                                  ? QStringLiteral("本机提醒")
                                  : item.titleSnapshot.trimmed(),
                              card);
    title->setTextPixelSize(13);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("ElaText { color:%1; font-weight:700; }").arg(AppStyle::textPrimary()));
    layout->addWidget(title);

    auto* preview = new ElaText(item.previewSnapshot.trimmed(), card);
    preview->setTextPixelSize(12);
    preview->setWordWrap(false);
    preview->setStyleSheet(QStringLiteral("ElaText { color:%1; }").arg(AppStyle::textSecondary()));
    layout->addWidget(preview);

    const QString dueText = reminderDueTimeText(item.dueAtMs);
    auto* meta = new ElaText(
        dueText.isEmpty()
            ? reminderTargetLabel(item.targetType)
            : QStringLiteral("%1  ·  %2").arg(reminderTargetLabel(item.targetType), dueText),
        card);
    meta->setTextPixelSize(11);
    meta->setStyleSheet(QStringLiteral("ElaText { color:%1; }").arg(AppStyle::textMuted()));
    layout->addWidget(meta);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 2, 0, 0);
    actions->setSpacing(6);
    auto* openButton = new ElaPushButton(QStringLiteral("查看"), card);
    auto* doneButton = new ElaPushButton(QStringLiteral("完成"), card);
    auto* snoozeButton = new ElaPushButton(QStringLiteral("稍后"), card);
    openButton->setObjectName(QStringLiteral("activeReminderOpenButton"));
    doneButton->setObjectName(QStringLiteral("activeReminderDoneButton"));
    snoozeButton->setObjectName(QStringLiteral("activeReminderSnoozeButton"));
    openButton->setFixedHeight(26);
    doneButton->setFixedHeight(26);
    snoozeButton->setFixedHeight(26);
    const QString reminderId = item.reminderId.trimmed();
    connect(openButton, &QAbstractButton::clicked, this, [this, reminderId]() {
        if (!reminderId.isEmpty()) {
            emit messageUrlOpenRequested(reminderActionUrl(QStringLiteral("open"), reminderId));
        }
    });
    connect(doneButton, &QAbstractButton::clicked, this, [this, reminderId]() {
        if (!reminderId.isEmpty()) {
            emit reminderDoneRequested(reminderId);
        }
    });
    connect(snoozeButton, &QAbstractButton::clicked, this, [this, reminderId]() {
        if (!reminderId.isEmpty()) {
            emit reminderSnoozeRequested(reminderId, 30);
        }
    });
    actions->addWidget(openButton);
    actions->addWidget(doneButton);
    actions->addWidget(snoozeButton);
    actions->addStretch();
    layout->addLayout(actions);

    return card;
}

QWidget* NotificationsPage::createNotificationItemCard(const SystemNotificationItem& item)
{
    auto* card = new ElaFrame(m_notificationList);
    card->setObjectName(QStringLiteral("notificationItemCard"));
    card->setFrameShape(QFrame::NoFrame);
    card->setCursor(Qt::PointingHandCursor);
    card->setStyleSheet(QStringLiteral(
        "ElaFrame#notificationItemCard { background:transparent; border:none; border-radius:8px; }"
        "ElaFrame#notificationItemCard:hover { background:%1; }"
        "ElaText { background:transparent; }")
        .arg(softGlass()));

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(8);
    auto* unreadDot = new ElaText(item.unread ? QStringLiteral("\u25CF") : QStringLiteral("\u25CB"), card);
    unreadDot->setStyleSheet(QStringLiteral("ElaText { color:%1; background:transparent; font-weight:700; }")
                                 .arg(item.unread ? AppStyle::accent() : AppStyle::textMuted()));
    auto* title = new ElaText(item.title.trimmed().isEmpty()
                                  ? QStringLiteral("\u7CFB\u7EDF\u63D0\u9192")
                                  : item.title.trimmed(),
                              card);
    title->setTextPixelSize(14);
    title->setWordWrap(false);
    title->setStyleSheet(QStringLiteral("ElaText { color:%1; background:transparent; font-weight:%2; }")
                             .arg(AppStyle::textPrimary(), item.unread ? QStringLiteral("700") : QStringLiteral("500")));
    titleRow->addWidget(unreadDot, 0, Qt::AlignVCenter);
    titleRow->addWidget(title, 1);
    layout->addLayout(titleRow);

    auto* summary = new ElaText(item.summary.trimmed().isEmpty()
                                    ? item.detail.trimmed()
                                    : item.summary.trimmed(),
                                card);
    summary->setTextPixelSize(12);
    summary->setWordWrap(false);
    summary->setStyleSheet(QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::textSecondary()));
    layout->addWidget(summary);

    const QString source = item.sourceLabel.trimmed().isEmpty() ? QStringLiteral("Outlook") : item.sourceLabel.trimmed();
    const QString timeText = item.occurredAtMs > 0
        ? QDateTime::fromMSecsSinceEpoch(item.occurredAtMs).toString(QStringLiteral("MM-dd HH:mm"))
        : QString();
    auto* meta = new ElaText(timeText.isEmpty() ? source : QStringLiteral("%1  \u00B7  %2").arg(source, timeText), card);
    meta->setTextPixelSize(11);
    meta->setStyleSheet(QStringLiteral("ElaText { color:%1; background:transparent; }").arg(AppStyle::textMuted()));
    layout->addWidget(meta);

    return card;
}

void NotificationsPage::selectNotificationById(const QString& notificationId)
{
    if (!m_notificationList || notificationId.trimmed().isEmpty()) return;
    for (int row = 0; row < m_notificationList->count(); ++row) {
        QListWidgetItem* item = m_notificationList->item(row);
        if (item && item->data(Qt::UserRole).toString() == notificationId.trimmed()) {
            m_notificationList->setCurrentItem(item);
            return;
        }
    }
}

bool NotificationsPage::markNotificationReadLocally(const QString& notificationId)
{
    const QString trimmedId = notificationId.trimmed();
    if (trimmedId.isEmpty()) return false;

    bool changed = false;
    for (SystemNotificationItem& item : m_notificationItems) {
        if (item.notificationId != trimmedId || !item.unread) continue;
        item.unread = false;
        changed = true;
        break;
    }
    if (changed) refreshNotificationList();
    else syncNotificationWorkspaceStatus();
    return changed;
}

bool NotificationsPage::archiveNotificationLocally(const QString& notificationId)
{
    const QString trimmedId = notificationId.trimmed();
    if (trimmedId.isEmpty()) return false;

    const auto it = std::remove_if(m_notificationItems.begin(), m_notificationItems.end(),
                                   [&](const SystemNotificationItem& item) {
                                       return item.notificationId == trimmedId;
                                   });
    if (it == m_notificationItems.end()) return false;

    m_notificationItems.erase(it, m_notificationItems.end());
    if (m_selectedNotificationId == trimmedId) m_selectedNotificationId.clear();
    refreshNotificationList();
    return true;
}

#ifdef LEYOCHAT_HAS_WEBENGINE
void NotificationsPage::ensureNotificationBodyView()
{
    if (m_notificationBodyView || !m_notificationDetailCard) return;
    auto* layout = qobject_cast<QVBoxLayout*>(m_notificationDetailCard->layout());
    if (!layout) return;

    // 保存主窗口几何信息：QWebEngineView 首次创建原生子窗口时
    // 可能触发父窗口链原生句柄重建，导致窗口跳到主屏
    QWidget* topLevel = window();
    const QRect savedGeometry = topLevel ? topLevel->geometry() : QRect();
    const bool wasMaximized = topLevel && topLevel->isMaximized();

    m_notificationBodyView = new QWebEngineView(m_notificationDetailCard);
    m_notificationBodyView->setObjectName(QStringLiteral("notificationBodyView"));
    m_notificationBodyView->hide();
    m_notificationBodyView->setContextMenuPolicy(Qt::NoContextMenu);
    m_notificationBodyView->page()->setBackgroundColor(Qt::transparent);
    connect(m_notificationBodyView->page(), &QWebEnginePage::navigationRequested,
            this, [](QWebEngineNavigationRequest& request) {
                if (request.navigationType() == QWebEngineNavigationRequest::LinkClickedNavigation) {
                    QDesktopServices::openUrl(request.url());
                    request.reject();
                } else {
                    request.accept();
                }
            });
    const int stretchIdx = layout->count() - 1;
    layout->insertWidget(stretchIdx, m_notificationBodyView, 1);

    // 恢复主窗口位置（防止跳屏）
    if (topLevel && savedGeometry.isValid()) {
        if (wasMaximized) {
            topLevel->showMaximized();
        } else {
            topLevel->setGeometry(savedGeometry);
        }
    }
}
#endif

void NotificationsPage::markAllNotificationsRead()
{
    bool changed = false;
    for (SystemNotificationItem& item : m_notificationItems) {
        if (item.unread) { item.unread = false; changed = true; }
    }
    if (changed) emit notificationsMarkAllReadRequested();
    if (changed) refreshNotificationList();
    else { syncNotificationWorkspaceStatus(); updateNotificationDetailPane(); }
}

void NotificationsPage::updateNotificationDetailPane()
{
    if (!m_notificationDetailSource || !m_notificationDetailTitle
        || !m_notificationDetailSummary || !m_notificationDetailDetail
        || !m_notificationDetailTimestamp || !m_notificationMarkReadButton
        || !m_notificationArchiveButton || !m_notificationOpenButton
        || !m_notificationCopyLinkButton) return;

    const auto it = std::find_if(m_notificationItems.cbegin(), m_notificationItems.cend(),
                                 [this](const SystemNotificationItem& item) {
                                     return item.notificationId == m_selectedNotificationId;
                                 });
    if (it == m_notificationItems.cend()) {
        m_notificationDetailSource->setText(QStringLiteral("\u7B49\u5F85\u901A\u77E5"));
        m_notificationDetailTitle->setText(QStringLiteral("\u4ECE\u5DE6\u4FA7\u9009\u62E9\u4E00\u6761\u63D0\u9192"));
        m_notificationDetailSummary->setText(
            QStringLiteral("\u8FD9\u91CC\u4F1A\u663E\u793A\u63D0\u9192\u6458\u8981\u3001\u7CFB\u7EDF\u6765\u6E90\u548C\u540E\u7EED\u5904\u7406\u52A8\u4F5C\u3002"));
        m_notificationDetailDetail->setText(
            QStringLiteral("\u5982\u679C\u8FD8\u6CA1\u6709\u8FDE\u4E0A Outlook\uFF0C\u53EF\u4EE5\u5148\u5728\u8BBE\u7F6E\u91CC\u6253\u5F00\u5BF9\u5E94\u96C6\u6210\u3002"));
        m_notificationDetailTimestamp->clear();
        m_notificationMarkReadButton->hide();
        m_notificationArchiveButton->hide();
        m_notificationOpenButton->hide();
        m_notificationCopyLinkButton->hide();
#ifdef LEYOCHAT_HAS_WEBENGINE
        if (m_notificationBodyView) { m_notificationBodyView->hide(); m_notificationBodyView->setHtml(QString()); }
#endif
        return;
    }

    const bool hasHtmlBody = !it->htmlBody.trimmed().isEmpty();
    m_notificationDetailSource->setText(it->sourceLabel.trimmed().isEmpty()
        ? QStringLiteral("\u7CFB\u7EDF\u901A\u77E5") : it->sourceLabel.trimmed());
    m_notificationDetailTitle->setText(it->title.trimmed().isEmpty()
        ? QStringLiteral("\u7CFB\u7EDF\u63D0\u9192") : it->title.trimmed());
    m_notificationDetailSummary->setVisible(!hasHtmlBody);
    m_notificationDetailDetail->setVisible(!hasHtmlBody);
    if (!hasHtmlBody) {
        m_notificationDetailSummary->setText(it->summary.trimmed().isEmpty()
            ? QStringLiteral("\u6682\u65E0\u9644\u52A0\u6458\u8981") : it->summary.trimmed());
        m_notificationDetailDetail->setText(it->detail.trimmed());
    }
#ifdef LEYOCHAT_HAS_WEBENGINE
    if (hasHtmlBody) ensureNotificationBodyView();
    if (m_notificationBodyView) {
        if (hasHtmlBody) {
            // 深色模式下注入 CSS 反转邮件背景/文字颜色
            QString html = it->htmlBody;
            if (AppStyle::isDarkTheme()) {
                const QString darkCss = QStringLiteral(
                    "<style>"
                    "body, html, div, table, td, th, p, span, li, ul, ol {"
                    "  background-color: %1 !important;"
                    "  color: %2 !important;"
                    "}"
                    "a { color: %3 !important; }"
                    "img { opacity: 0.9; }"
                    "hr { border-color: %4 !important; }"
                    "</style>")
                    .arg(AppStyle::surface(), AppStyle::textPrimary(),
                         AppStyle::accent(), AppStyle::border());
                // 尝试注入到 <head> 或 <body> 前
                if (html.contains(QStringLiteral("</head>"), Qt::CaseInsensitive)) {
                    html.replace(QStringLiteral("</head>"),
                                 darkCss + QStringLiteral("</head>"),
                                 Qt::CaseInsensitive);
                } else if (html.contains(QStringLiteral("<body"), Qt::CaseInsensitive)) {
                    const int bodyIdx = html.indexOf(QStringLiteral("<body"), 0, Qt::CaseInsensitive);
                    const int bodyEnd = html.indexOf(QLatin1Char('>'), bodyIdx);
                    if (bodyEnd >= 0) {
                        html.insert(bodyEnd + 1, darkCss);
                    }
                } else {
                    html.prepend(darkCss);
                }
            }
            m_notificationBodyView->setHtml(html);
            m_notificationBodyView->show();
        } else {
            m_notificationBodyView->hide();
            m_notificationBodyView->setHtml(QString());
        }
    }
#endif
    if (it->occurredAtMs > 0) {
        m_notificationDetailTimestamp->setText(
            QDateTime::fromMSecsSinceEpoch(it->occurredAtMs).toString(QStringLiteral("yyyy-MM-dd HH:mm")));
    } else {
        m_notificationDetailTimestamp->clear();
    }
    m_notificationMarkReadButton->setVisible(it->unread);
    m_notificationArchiveButton->setVisible(true);
    m_notificationOpenButton->setText(it->actionLabel.trimmed().isEmpty()
        ? QStringLiteral("\u6253\u5F00\u539F\u59CB\u9875\u9762") : it->actionLabel.trimmed());
    m_notificationOpenButton->setVisible(!it->actionUrl.trimmed().isEmpty());
    m_notificationCopyLinkButton->setVisible(!it->actionUrl.trimmed().isEmpty());
}

void NotificationsPage::syncNotificationWorkspaceStatus()
{
    if (!m_notificationStatusChip) return;

    const int unreadCount = std::count_if(m_notificationItems.cbegin(), m_notificationItems.cend(),
                                          [](const SystemNotificationItem& item) { return item.unread; });
    const int totalCount = m_notificationItems.size();

    // modeChip: "通知 xx"
    if (m_notificationModeChip) {
        const QString modeText = QStringLiteral("\u901A\u77E5 %1").arg(totalCount);
        m_notificationModeChip->setText(modeText);
        m_notificationModeChip->setMinimumWidth(
            QFontMetrics(m_notificationModeChip->font()).horizontalAdvance(modeText) + 28);
    }

    // statusChip: 有未读时 "未读 xx"，否则隐藏
    if (unreadCount > 0) {
        const QString statusText = QStringLiteral("\u672A\u8BFB %1").arg(unreadCount);
        m_notificationStatusChip->setText(statusText);
        m_notificationStatusChip->setMinimumWidth(
            QFontMetrics(m_notificationStatusChip->font()).horizontalAdvance(statusText) + 28);
        m_notificationStatusChip->show();
    } else {
        m_notificationStatusChip->hide();
    }

    emit unreadCountChanged(unreadCount);

    if (m_notificationEmptyLabel) {
        m_notificationEmptyLabel->setVisible(totalCount == 0);
        if (totalCount == 0) {
            m_notificationEmptyLabel->setText(QStringLiteral(
                "\u6682\u65E0\u7CFB\u7EDF\u901A\u77E5\nOutlook \u548C\u540E\u7EED\u96C6\u6210\u7684\u63D0\u9192\u4F1A\u51FA\u73B0\u5728\u8FD9\u91CC"));
        }
    }
    if (m_notificationList) m_notificationList->setVisible(totalCount > 0);
}
