#include "DirectoryPage.h"

#include "ui/AppStyle.h"
#include "ui/AlphabetIndexBar.h"
#include "ui/ContactCardDelegate.h"
#include "ui/ContactListModel.h"
#include "ui/LeyoDialog.h"
#include "ui/OrgTreeDelegate.h"

#include <ElaFrame.h>
#include <ElaLineEdit.h>
#include <ElaListWidget.h>
#include <ElaListView.h>
#include <ElaMenu.h>
#include <ElaPushButton.h>
#include <ElaText.h>
#include <ElaTheme.h>
#include <ElaTreeView.h>

#include <QAbstractItemModel>
#include <QAbstractScrollArea>
#include <QDateTime>
#include <algorithm>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QStandardItemModel>
#include <QFrame>
#include <QStyledItemDelegate>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString directoryGlass(const AppStyle::ThemeMode mode, int lightAlpha = 122, int darkAlpha = 150)
{
    return AppStyle::isDarkTheme(mode)
        ? QStringLiteral("rgba(18,31,43,%1)").arg(darkAlpha)
        : QStringLiteral("rgba(255,255,255,%1)").arg(lightAlpha);
}

QString directorySoftGlass(const AppStyle::ThemeMode mode, int lightAlpha = 82, int darkAlpha = 118)
{
    return AppStyle::isDarkTheme(mode)
        ? QStringLiteral("rgba(25,42,55,%1)").arg(darkAlpha)
        : QStringLiteral("rgba(255,255,255,%1)").arg(lightAlpha);
}

QColor directorySoftGlassColor(const AppStyle::ThemeMode mode, int lightAlpha = 82, int darkAlpha = 118)
{
    return AppStyle::isDarkTheme(mode)
        ? QColor(25, 42, 55, darkAlpha)
        : QColor(255, 255, 255, lightAlpha);
}

ElaFrame* createStatCard(QWidget* parent,
                         const QString& title,
                         QLabel** valueLabel,
                         QLabel** metaLabel = nullptr)
{
    auto* card = new ElaFrame(parent);
    card->setObjectName(QStringLiteral("directoryStatCard"));
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(3);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName(QStringLiteral("directoryStatTitle"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto* value = new QLabel(QStringLiteral("0"), card);
    value->setObjectName(QStringLiteral("directoryStatValue"));
    value->setAlignment(Qt::AlignCenter);
    layout->addWidget(value);
    if (valueLabel) *valueLabel = value;

    auto* meta = new QLabel(QString(), card);
    meta->setObjectName(QStringLiteral("directoryStatMeta"));
    meta->setAlignment(Qt::AlignCenter);
    layout->addWidget(meta);
    if (metaLabel) *metaLabel = meta;

    return card;
}

void styleDirectoryActionButton(ElaPushButton* button,
                                const AppStyle::ThemeMode mode,
                                bool accent = false)
{
    if (!button) return;
    button->setBorderRadius(8);
    if (accent) {
        button->setLightDefaultColor(QColor(AppStyle::accent(mode)));
        button->setDarkDefaultColor(QColor(AppStyle::accent(mode)));
        button->setLightHoverColor(QColor(AppStyle::accentHover(mode)));
        button->setDarkHoverColor(QColor(AppStyle::accentHover(mode)));
        button->setLightPressColor(QColor(AppStyle::accentPressed(mode)));
        button->setDarkPressColor(QColor(AppStyle::accentPressed(mode)));
        button->setLightTextColor(Qt::white);
        button->setDarkTextColor(Qt::white);
        return;
    }
    button->setLightDefaultColor(QColor(255, 255, 255, 38));
    button->setDarkDefaultColor(QColor(25, 42, 55, 120));
    button->setLightHoverColor(QColor(255, 255, 255, 92));
    button->setDarkHoverColor(QColor(32, 54, 68, 170));
    button->setLightPressColor(QColor(255, 255, 255, 120));
    button->setDarkPressColor(QColor(22, 38, 50, 190));
    button->setLightTextColor(QColor(AppStyle::textPrimary(mode)));
    button->setDarkTextColor(QColor(AppStyle::textPrimary(mode)));
}

enum DirectoryGroupRole {
    GroupNameRole = Qt::UserRole + 10,
    GroupOwnerRole,
    GroupMemberCountRole,
    GroupUnreadCountRole,
    GroupIdRole,
};

class DirectoryGroupDelegate final : public QStyledItemDelegate {
public:
    explicit DirectoryGroupDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return QSize(0, 74);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const QRect cardRect = option.rect.adjusted(6, 5, -8, -5);

        QColor cardBg = directorySoftGlassColor(mode, 70, 105);
        QColor cardBorder(AppStyle::border(mode));
        if (selected) {
            cardBg = QColor(AppStyle::selectedBg(mode));
            cardBorder = QColor(AppStyle::accent(mode));
        } else if (hovered) {
            cardBg = QColor(AppStyle::hoverBg(mode));
            cardBorder = QColor(AppStyle::accent(mode));
        }
        painter->setPen(QPen(cardBorder, selected || hovered ? 1.2 : 1.0));
        painter->setBrush(cardBg);
        painter->drawRoundedRect(cardRect, 8, 8);

        const QString groupId = index.data(GroupIdRole).toString();
        const QString groupName = index.data(GroupNameRole).toString();
        const QString owner = index.data(GroupOwnerRole).toString();
        const int memberCount = index.data(GroupMemberCountRole).toInt();
        const int unreadCount = index.data(GroupUnreadCountRole).toInt();
        const QString title = groupName.trimmed().isEmpty()
            ? QStringLiteral("\u672A\u547D\u540D\u7FA4\u7EC4")
            : groupName.trimmed();

        const int avatarSize = 42;
        const int avatarX = cardRect.left() + 12;
        const int avatarY = cardRect.top() + (cardRect.height() - avatarSize) / 2;
        const QColor accent(AppStyle::accent(mode));
        const int hueShift = static_cast<int>(qHash(groupId) % 80);
        const QColor avatarBg = QColor::fromHsv((accent.hue() + hueShift + 360) % 360,
                                                qMin(220, accent.saturation() + 20),
                                                qMax(150, accent.value()));
        painter->setPen(Qt::NoPen);
        painter->setBrush(avatarBg);
        painter->drawEllipse(QRect(avatarX, avatarY, avatarSize, avatarSize));

        QFont avatarFont = option.font;
        avatarFont.setPixelSize(15);
        avatarFont.setBold(true);
        painter->setFont(avatarFont);
        painter->setPen(Qt::white);
        painter->drawText(QRect(avatarX, avatarY, avatarSize, avatarSize),
                          Qt::AlignCenter,
                          title.left(1));

        const int textX = avatarX + avatarSize + 10;
        const int rightReserve = unreadCount > 0 ? 96 : 36;
        const int textW = qMax(40, cardRect.right() - textX - rightReserve);
        const int row1Y = cardRect.top() + 10;

        QFont nameFont = option.font;
        nameFont.setPixelSize(14);
        nameFont.setBold(true);
        painter->setFont(nameFont);
        painter->setPen(QColor(AppStyle::textPrimary(mode)));
        const QFontMetrics nameFm(nameFont);
        const QString elidedTitle = nameFm.elidedText(title, Qt::ElideRight, textW - 58);
        painter->drawText(textX, row1Y, textW, 20, Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

        const int dotX = qMin(textX + nameFm.horizontalAdvance(elidedTitle) + 10, textX + textW - 44);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0x35, 0xD0, 0x7F));
        painter->drawEllipse(QRect(dotX, row1Y + 7, 6, 6));

        QFont onlineFont = option.font;
        onlineFont.setPixelSize(12);
        painter->setFont(onlineFont);
        painter->setPen(QColor(0x35, 0xD0, 0x7F));
        painter->drawText(dotX + 10, row1Y, 44, 20, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("\u5728\u7EBF"));

        const QString meta = owner.trimmed().isEmpty()
            ? QStringLiteral("%1 \u4F4D\u6210\u5458").arg(memberCount)
            : QStringLiteral("%1 \u4F4D\u6210\u5458 \u00B7 %2").arg(memberCount).arg(owner.trimmed());
        QFont metaFont = option.font;
        metaFont.setPixelSize(12);
        painter->setFont(metaFont);
        painter->setPen(QColor(AppStyle::textMuted(mode)));
        painter->drawText(textX, cardRect.top() + 36, textW, 18,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(metaFont).elidedText(meta, Qt::ElideRight, textW));

        if (unreadCount > 0) {
            const QString badgeText = QStringLiteral("\u672A\u8BFB %1").arg(unreadCount);
            QFont badgeFont = option.font;
            badgeFont.setPixelSize(11);
            badgeFont.setBold(true);
            const QFontMetrics badgeFm(badgeFont);
            const int badgeW = qMax(50, badgeFm.horizontalAdvance(badgeText) + 16);
            const QRect badgeRect(cardRect.right() - badgeW - 32,
                                  cardRect.top() + (cardRect.height() - 22) / 2,
                                  badgeW,
                                  22);
            painter->setFont(badgeFont);
            painter->setPen(QPen(QColor(AppStyle::accent(mode)), 1));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(badgeRect, 11, 11);
            painter->setPen(QColor(AppStyle::accent(mode)));
            painter->drawText(badgeRect, Qt::AlignCenter, badgeText);
        }

        QFont moreFont = option.font;
        moreFont.setPixelSize(18);
        moreFont.setBold(true);
        painter->setFont(moreFont);
        painter->setPen(QColor(AppStyle::textMuted(mode)));
        painter->drawText(QRect(cardRect.right() - 26, cardRect.top(), 20, cardRect.height()),
                          Qt::AlignCenter,
                          QStringLiteral("\u22EE"));

        painter->restore();
    }
};

}

DirectoryPage::DirectoryPage(QWidget* parent)
    : ElaScrollPage(parent)
{
    setWindowTitle(QStringLiteral("通讯录"));
    setTitleVisible(false);

    const QFont baseUiFont = font();
    const int searchHeight = qMax(32, QFontMetrics(AppStyle::bodyFont(baseUiFont)).height() + 16);

    auto* contentWidget = new QWidget(this);
    contentWidget->setObjectName(QStringLiteral("directoryPageRoot"));
    contentWidget->setAttribute(Qt::WA_StyledBackground, true);
    contentWidget->setAutoFillBackground(false);
    contentWidget->setStyleSheet(QStringLiteral("QWidget#directoryPageRoot { background: transparent; }"));
    auto* rootLayout = new QVBoxLayout(contentWidget);
    rootLayout->setContentsMargins(12, 14, 12, 12);
    rootLayout->setSpacing(12);

    // ── Hero 面板 ──
    auto* heroPanel = new ElaFrame(contentWidget);
    heroPanel->setObjectName(QStringLiteral("directoryHeroPanel"));
    heroPanel->setMinimumHeight(108);
    heroPanel->setMaximumHeight(124);
    auto* heroLayout = new QVBoxLayout(heroPanel);
    heroLayout->setContentsMargins(18, 12, 18, 12);
    heroLayout->setSpacing(8);

    auto* heroTitleRow = new QHBoxLayout;
    heroTitleRow->setContentsMargins(0, 0, 0, 0);
    heroTitleRow->setSpacing(10);
    m_directoryTitleLabel = new QLabel(QStringLiteral("\u901A\u8BAF\u5F55"), heroPanel);
    m_directoryTitleLabel->setObjectName(QStringLiteral("directoryHeroTitle"));
    heroTitleRow->addWidget(m_directoryTitleLabel);
    m_contactsModeChip = new ElaText(QStringLiteral("\u8054\u7CFB\u4EBA\u76EE\u5F55"), heroPanel);
    m_contactsModeChip->setObjectName(QStringLiteral("contactsModeChip"));
    m_contactsModeChip->setProperty("surfaceChipRole", QStringLiteral("mode"));
    m_contactsModeChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_contactsModeChip->setMinimumWidth(
        QFontMetrics(m_contactsModeChip->font()).horizontalAdvance(m_contactsModeChip->text()) + 28);
    m_contactsStatusChip = new ElaText(QStringLiteral("\u6682\u65E0\u8054\u7CFB\u4EBA"), heroPanel);
    m_contactsStatusChip->setObjectName(QStringLiteral("contactsStatusChip"));
    m_contactsStatusChip->setProperty("surfaceChipRole", QStringLiteral("status"));
    m_contactsStatusChip->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_contactsStatusChip->setMinimumWidth(
        QFontMetrics(m_contactsStatusChip->font()).horizontalAdvance(m_contactsStatusChip->text()) + 28);
    heroTitleRow->addWidget(m_contactsModeChip);
    heroTitleRow->addWidget(m_contactsStatusChip);
    heroTitleRow->addStretch();
    heroLayout->addLayout(heroTitleRow);

    auto* statRow = new QHBoxLayout;
    statRow->setContentsMargins(0, 0, 0, 0);
    statRow->setSpacing(0);
    QLabel* onlineMeta = nullptr;
    QLabel* groupUnreadMeta = nullptr;
    QLabel* myGroupMeta = nullptr;
    QLabel* departmentMeta = nullptr;
    statRow->addWidget(createStatCard(heroPanel, QStringLiteral("\u8054\u7CFB\u4EBA"), &m_contactTotalValueLabel, &onlineMeta), 1);
    statRow->addWidget(createStatCard(heroPanel, QStringLiteral("\u7FA4\u7EC4"), &m_groupTotalValueLabel, &groupUnreadMeta), 1);
    statRow->addWidget(createStatCard(heroPanel, QStringLiteral("\u6211\u7684\u7FA4\u7EC4"), &m_myGroupValueLabel, &myGroupMeta), 1);
    statRow->addWidget(createStatCard(heroPanel, QStringLiteral("\u7EC4\u7EC7\u67B6\u6784"), &m_departmentValueLabel, &departmentMeta), 1);
    m_contactOnlineValueLabel = onlineMeta;
    m_groupUnreadValueLabel = groupUnreadMeta;
    Q_UNUSED(myGroupMeta);
    Q_UNUSED(departmentMeta);
    heroLayout->addLayout(statRow);

    m_heroSummaryLabel = new QLabel(heroPanel);
    m_heroSummaryLabel->setObjectName(QStringLiteral("directoryHeroSummary"));
    m_heroSummaryLabel->hide();

    m_heroRuntimeLabel = new QLabel(heroPanel);
    m_heroRuntimeLabel->setObjectName(QStringLiteral("directoryHeroRuntime"));
    m_heroRuntimeLabel->setWordWrap(true);
    m_heroRuntimeLabel->hide();

    rootLayout->addWidget(heroPanel);

    // ── 搜索栏 + Tab 按钮 ──
    auto* toolBar = new QWidget(contentWidget);
    toolBar->setObjectName(QStringLiteral("directoryToolbar"));
    auto* toolBarLayout = new QVBoxLayout(toolBar);
    toolBarLayout->setContentsMargins(0, 0, 0, 0);
    toolBarLayout->setSpacing(10);
    auto* searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(8);

    m_searchEdit = new ElaLineEdit(toolBar);
    m_searchEdit->setObjectName(QStringLiteral("directorySearchEdit"));
    m_searchEdit->setPlaceholderText(QStringLiteral("\u641C\u7D22\u8054\u7CFB\u4EBA\u6216\u7FA4\u7EC4..."));
    m_searchEdit->setIsClearButtonEnable(true);
    m_searchEdit->setFixedHeight(searchHeight);
    searchRow->addWidget(m_searchEdit, 1);

    m_presenceFilterTabs = new ElaFrame(toolBar);
    m_presenceFilterTabs->setObjectName(QStringLiteral("directoryPresenceFilterTabs"));
    m_presenceFilterTabs->setFixedHeight(searchHeight);
    m_presenceFilterTabs->setFixedWidth(198);
    auto* presenceFilterLayout = new QHBoxLayout(m_presenceFilterTabs);
    presenceFilterLayout->setContentsMargins(3, 3, 3, 3);
    presenceFilterLayout->setSpacing(3);

    m_allFilterBtn = new ElaPushButton(QStringLiteral("\u5168\u90E8"), m_presenceFilterTabs);
    m_allFilterBtn->setFixedHeight(searchHeight - 6);
    m_allFilterBtn->setBorderRadius(7);
    presenceFilterLayout->addWidget(m_allFilterBtn, 1);

    m_onlineFilterBtn = new ElaPushButton(QStringLiteral("\u5728\u7EBF"), m_presenceFilterTabs);
    m_onlineFilterBtn->setFixedHeight(searchHeight - 6);
    m_onlineFilterBtn->setBorderRadius(7);
    presenceFilterLayout->addWidget(m_onlineFilterBtn, 1);

    m_offlineFilterBtn = new ElaPushButton(QStringLiteral("\u79BB\u7EBF"), m_presenceFilterTabs);
    m_offlineFilterBtn->setFixedHeight(searchHeight - 6);
    m_offlineFilterBtn->setBorderRadius(7);
    presenceFilterLayout->addWidget(m_offlineFilterBtn, 1);
    searchRow->addWidget(m_presenceFilterTabs);
    toolBarLayout->addLayout(searchRow);

    m_directorySegmentedTabs = new ElaFrame(toolBar);
    m_directorySegmentedTabs->setObjectName(QStringLiteral("directorySegmentedTabs"));
    m_directorySegmentedTabs->setFixedWidth(540);
    auto* segmentedLayout = new QHBoxLayout(m_directorySegmentedTabs);
    segmentedLayout->setContentsMargins(4, 4, 4, 4);
    segmentedLayout->setSpacing(4);
    auto* segmentedRow = new QHBoxLayout;
    segmentedRow->setContentsMargins(0, 0, 0, 0);
    segmentedRow->setSpacing(0);
    segmentedRow->addStretch();
    segmentedRow->addWidget(m_directorySegmentedTabs);
    segmentedRow->addStretch();
    toolBarLayout->addLayout(segmentedRow);

    m_contactsTabBtn = new ElaPushButton(QStringLiteral("\u8054\u7CFB\u4EBA"), m_directorySegmentedTabs);
    m_contactsTabBtn->setFixedHeight(30);
    m_contactsTabBtn->setMinimumWidth(140);
    m_contactsTabBtn->setBorderRadius(6);
    segmentedLayout->addWidget(m_contactsTabBtn, 1);

    m_groupsTabBtn = new ElaPushButton(QStringLiteral("\u7FA4\u7EC4"), m_directorySegmentedTabs);
    m_groupsTabBtn->setFixedHeight(30);
    m_groupsTabBtn->setMinimumWidth(140);
    m_groupsTabBtn->setBorderRadius(6);
    segmentedLayout->addWidget(m_groupsTabBtn, 1);

    m_orgTabBtn = new ElaPushButton(QStringLiteral("\u7EC4\u7EC7\u67B6\u6784"), m_directorySegmentedTabs);
    m_orgTabBtn->setFixedHeight(30);
    m_orgTabBtn->setMinimumWidth(140);
    m_orgTabBtn->setBorderRadius(6);
    segmentedLayout->addWidget(m_orgTabBtn, 1);

    rootLayout->addWidget(toolBar);

    // ── Tab 内容栈 ──
    m_tabStack = new QStackedWidget(contentWidget);

    // ── 联系人视图 (Tab 0) ──
    auto* contactsPage = new QWidget(m_tabStack);
    contactsPage->setObjectName(QStringLiteral("directoryContactSurface"));
    contactsPage->setAttribute(Qt::WA_StyledBackground, true);
    contactsPage->setStyleSheet(QStringLiteral("QWidget#directoryContactSurface { background: transparent; }"));
    auto* contactsLayout = new QVBoxLayout(contactsPage);
    contactsLayout->setContentsMargins(0, 0, 0, 0);
    contactsLayout->setSpacing(0);

    m_contactEmptyLabel = new ElaText(
        QStringLiteral("\u6682\u65E0\u53EF\u7528\u8054\u7CFB\u4EBA\n\u4F60\u53EF\u4EE5\u6DFB\u52A0\u8054\u7CFB\u4EBA\u3001\u624B\u52A8\u8FDE\u63A5 IP \u6216\u7B49\u5F85\u5C40\u57DF\u7F51\u53D1\u73B0"),
        contactsPage);
    m_contactEmptyLabel->setAlignment(Qt::AlignCenter);
    m_contactEmptyLabel->setWordWrap(true);
    m_contactEmptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_contactEmptyLabel->setStyleSheet(QStringLiteral(
        "font-size:13px; color:%1; background:transparent; border:none; padding:18px 20px;")
        .arg(AppStyle::textMuted()));
    contactsLayout->addWidget(m_contactEmptyLabel, 1);

    m_contactList = new ElaListView(contactsPage);
    m_contactList->setObjectName(QStringLiteral("directoryContactList"));
    m_contactList->setAutoFillBackground(false);
    m_contactList->setFrameShape(QFrame::NoFrame);
    m_contactList->setFocusPolicy(Qt::NoFocus);
    if (m_contactList->viewport()) {
        m_contactList->viewport()->setAutoFillBackground(false);
    }

    auto* contactListRow = new QWidget(contactsPage);
    auto* contactListRowLayout = new QHBoxLayout(contactListRow);
    contactListRowLayout->setContentsMargins(0, 0, 0, 0);
    contactListRowLayout->setSpacing(0);
    contactListRowLayout->addWidget(m_contactList, 1);
    m_alphabetIndexBar = new AlphabetIndexBar(contactListRow);
    m_alphabetIndexBar->setListView(m_contactList);
    m_alphabetIndexBar->hide();
    contactsLayout->addWidget(contactListRow, 1);

    auto* addContactBtn = new ElaPushButton(QStringLiteral("\u6DFB\u52A0\u8054\u7CFB\u4EBA"), contactsPage);
    addContactBtn->setFixedHeight(36);
    addContactBtn->setBorderRadius(8);
    addContactBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(43,92,230,0.08); color:%1; border:1px solid rgba(43,92,230,0.25);"
        " border-radius:8px; font-size:13px; padding:0 18px; }"
        "QPushButton:hover { background: rgba(43,92,230,0.16); }"
        "QPushButton:pressed { background: rgba(43,92,230,0.24); }")
        .arg(AppStyle::accent()));
    auto* addContactBtnRow = new QHBoxLayout();
    addContactBtnRow->setContentsMargins(12, 8, 12, 12);
    addContactBtnRow->addStretch();
    addContactBtnRow->addWidget(addContactBtn);
    contactsLayout->addLayout(addContactBtnRow);

    m_tabStack->addWidget(contactsPage); // index 0

    // ── 群组视图 (Tab 1) ──
    auto* groupsPage = new QWidget(m_tabStack);
    groupsPage->setObjectName(QStringLiteral("directoryGroupSurface"));
    groupsPage->setAttribute(Qt::WA_StyledBackground, true);
    groupsPage->setStyleSheet(QStringLiteral("QWidget#directoryGroupSurface { background: transparent; }"));
    auto* groupsLayout = new QVBoxLayout(groupsPage);
    groupsLayout->setContentsMargins(0, 0, 0, 0);
    groupsLayout->setSpacing(0);

    m_groupEmptyLabel = new QLabel(
        QStringLiteral("\u6682\u65E0\u7FA4\u7EC4\n\u4F60\u53EF\u4EE5\u521B\u5EFA\u7FA4\u7EC4\u6216\u7B49\u5F85\u88AB\u9080\u8BF7"),
        groupsPage);
    m_groupEmptyLabel->setAlignment(Qt::AlignCenter);
    m_groupEmptyLabel->setWordWrap(true);
    m_groupEmptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_groupEmptyLabel->setStyleSheet(QStringLiteral(
        "font-size:13px; color:%1; background:transparent; border:none; padding:18px 20px;")
        .arg(AppStyle::textMuted()));
    groupsLayout->addWidget(m_groupEmptyLabel, 1);

    m_groupListWidget = new ElaListWidget(groupsPage);
    m_groupListWidget->setObjectName(QStringLiteral("directoryGroupList"));
    m_groupListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_groupListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_groupListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_groupListWidget->setItemDelegate(new DirectoryGroupDelegate(m_groupListWidget));
    m_groupListWidget->setSpacing(2);
    m_groupListWidget->setStyleSheet(QStringLiteral(
        "QListWidget { background:transparent; border:none; outline:0; }"
        "QListWidget::item { padding:8px 12px; border-radius:8px; }"
        "QListWidget::item:selected { background:%1; }"
        "QListWidget::item:hover { background:%2; }")
        .arg(AppStyle::selectedBg(), AppStyle::hoverBg()));
    groupsLayout->addWidget(m_groupListWidget, 1);

    auto* createGroupBtn = new ElaPushButton(QStringLiteral("\u521B\u5EFA\u7FA4\u7EC4"), groupsPage);
    createGroupBtn->setFixedHeight(36);
    createGroupBtn->setBorderRadius(8);
    createGroupBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(43,92,230,0.08); color:%1; border:1px solid rgba(43,92,230,0.25);"
        " border-radius:8px; font-size:13px; padding:0 18px; }"
        "QPushButton:hover { background: rgba(43,92,230,0.16); }"
        "QPushButton:pressed { background: rgba(43,92,230,0.24); }")
        .arg(AppStyle::accent()));
    auto* createGroupBtnRow = new QHBoxLayout();
    createGroupBtnRow->setContentsMargins(12, 8, 12, 12);
    createGroupBtnRow->addStretch();
    createGroupBtnRow->addWidget(createGroupBtn);
    groupsLayout->addLayout(createGroupBtnRow);

    m_tabStack->addWidget(groupsPage); // index 1

    // ── 组织架构视图 (Tab 2) ──
    auto* orgPage = new QWidget(m_tabStack);
    orgPage->setObjectName(QStringLiteral("directoryOrgSurface"));
    orgPage->setAttribute(Qt::WA_StyledBackground, true);
    orgPage->setStyleSheet(QStringLiteral("QWidget#directoryOrgSurface { background: transparent; }"));
    auto* orgLayout = new QVBoxLayout(orgPage);
    orgLayout->setContentsMargins(0, 0, 0, 0);
    orgLayout->setSpacing(0);

    m_orgEmptyLabel = new ElaText(
        QStringLiteral("\u6682\u65E0\u7EC4\u7EC7\u67B6\u6784\u6570\u636E\n\u5F85\u8054\u7CFB\u4EBA\u4E0A\u7EBF\u540E\u81EA\u52A8\u751F\u6210"),
        orgPage);
    m_orgEmptyLabel->setAlignment(Qt::AlignCenter);
    m_orgEmptyLabel->setWordWrap(true);
    m_orgEmptyLabel->setTextStyle(ElaTextType::Body);
    m_orgEmptyLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_orgEmptyLabel->setStyleSheet(QStringLiteral(
        "font-size:13px; color:%1; background:transparent; border:none; padding:18px 20px;")
        .arg(AppStyle::textMuted()));
    orgLayout->addWidget(m_orgEmptyLabel, 1);

    m_orgTreeWidget = new ElaTreeView(orgPage);
    m_orgTreeWidget->setObjectName(QStringLiteral("directoryOrgTree"));
    m_orgTreeWidget->setHeaderHidden(true);
    m_orgTreeWidget->setRootIsDecorated(true);
    m_orgTreeWidget->setExpandsOnDoubleClick(true);
    m_orgTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_orgTreeWidget->setFrameShape(QFrame::NoFrame);
    m_orgTreeWidget->setAutoFillBackground(false);
    if (m_orgTreeWidget->viewport()) {
        m_orgTreeWidget->viewport()->setAutoFillBackground(false);
    }
    m_orgTreeWidget->setStyleSheet(QStringLiteral(
        "QTreeView { background:transparent; border:none; outline:0; }"
        "QTreeView::item { padding:6px 4px; border-radius:6px; }"
        "QTreeView::item:selected { background:%1; }"
        "QTreeView::item:hover { background:%2; }")
        .arg(AppStyle::selectedBg(), AppStyle::hoverBg()));
    m_orgTreeWidget->hide();
    orgLayout->addWidget(m_orgTreeWidget, 1);

    m_orgTreeModel = new QStandardItemModel(this);
    m_orgTreeWidget->setModel(m_orgTreeModel);
    m_orgTreeWidget->setItemDelegate(new OrgTreeDelegate(m_orgTreeWidget));

    // 双击组织架构中的联系人 → 发起私聊
    connect(m_orgTreeWidget, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QString clientId = index.data(OrgTreeDelegate::ClientIdRole).toString();
        if (!clientId.isEmpty()) {
            emit openConversationRequested(clientId);
        }
    });
    // 右键菜单
    m_orgTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_orgTreeWidget, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex index = m_orgTreeWidget->indexAt(pos);
        if (!index.isValid()) return;
        const QString clientId = index.data(OrgTreeDelegate::ClientIdRole).toString();
        if (clientId.isEmpty()) return; // 部门节点，不弹菜单

        QMenu menu(this);
        menu.setStyleSheet(QStringLiteral(
            "QMenu { background:%1; border:1px solid %2; border-radius:8px; padding:4px; }"
            "QMenu::item { padding:6px 16px; color:%3; border-radius:4px; }"
            "QMenu::item:selected { background:%4; color:%5; }"
            "QMenu::separator { height:1px; background:%2; margin:4px 0; }")
            .arg(AppStyle::surface(), AppStyle::border(), AppStyle::textPrimary(),
                 AppStyle::hoverBg(), AppStyle::accent()));
        auto* chatAction = menu.addAction(QStringLiteral("\u53D1\u8D77\u79C1\u804A"));
        auto* profileAction = menu.addAction(QStringLiteral("\u67E5\u770B\u8D44\u6599"));
        const QAction* chosen = menu.exec(m_orgTreeWidget->viewport()->mapToGlobal(pos));
        if (chosen == chatAction) {
            emit openConversationRequested(clientId);
        } else if (chosen == profileAction) {
            emit contactProfileRequested(clientId);
        }
    });

    m_tabStack->addWidget(orgPage); // index 2

    rootLayout->addWidget(m_tabStack, 1);

    addCentralWidget(contentWidget);

    // ── 主题变化 ──
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]() {
        refreshTheme();
    });
    // 初始修正 viewport 背景
    QTimer::singleShot(0, this, [this]() { refreshTheme(); });

    // ── Tab 切换样式 ──
    updateTabStyle();

    // ── 信号连接 ──
    connect(m_contactsTabBtn, &QPushButton::clicked, this, [this]() {
        m_tabStack->setCurrentIndex(0);
        m_activeTabIndex = 0;
        updateTabStyle();
    });
    connect(m_groupsTabBtn, &QPushButton::clicked, this, [this]() {
        m_tabStack->setCurrentIndex(1);
        m_activeTabIndex = 1;
        updateTabStyle();
    });
    connect(m_orgTabBtn, &QPushButton::clicked, this, [this]() {
        m_tabStack->setCurrentIndex(2);
        m_activeTabIndex = 2;
        updateTabStyle();
    });

    connect(m_searchEdit, &ElaLineEdit::textChanged, this, [this](const QString& text) {
        auto* contactModel = qobject_cast<ContactListModel*>(m_contactList->model());
        if (contactModel) {
            contactModel->setSearchText(text.trimmed());
        }
    });

    const auto applyPresenceFilter = [this](int index) {
        m_presenceFilterIndex = index;
        auto* contactModel = qobject_cast<ContactListModel*>(m_contactList->model());
        if (contactModel) {
            const auto filter = index == 1
                ? ContactListModel::PresenceFilter::Online
                : index == 2
                    ? ContactListModel::PresenceFilter::Offline
                    : ContactListModel::PresenceFilter::All;
            contactModel->setPresenceFilter(filter);
        }
        updatePresenceFilterStyle();
        syncContactWorkspaceStatus();
    };
    connect(m_allFilterBtn, &QPushButton::clicked, this, [applyPresenceFilter]() { applyPresenceFilter(0); });
    connect(m_onlineFilterBtn, &QPushButton::clicked, this, [applyPresenceFilter]() { applyPresenceFilter(1); });
    connect(m_offlineFilterBtn, &QPushButton::clicked, this, [applyPresenceFilter]() { applyPresenceFilter(2); });

    connect(createGroupBtn, &QPushButton::clicked, this, &DirectoryPage::createGroupRequested);
    connect(addContactBtn, &QPushButton::clicked, this, &DirectoryPage::addContactRequested);

    m_contactList->setItemDelegate(new ContactCardDelegate(m_contactList));
    m_contactList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_contactList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_contactList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_contactList->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(m_contactList, &QListView::clicked, this, [this](const QModelIndex& index) {
        const QString clientId = index.data(ContactListModel::ClientIdRole).toString();
        if (!clientId.isEmpty()) {
            emit contactSelected(clientId);
        }
    });

    connect(m_contactList, &QListView::doubleClicked, this, [this](const QModelIndex& index) {
        const QString clientId = index.data(ContactListModel::ClientIdRole).toString();
        if (!clientId.isEmpty()) {
            emit openConversationRequested(clientId);
        }
    });

    connect(m_contactList, &QListView::customContextMenuRequested, this, [this](const QPoint& pos) {
        const QModelIndex idx = m_contactList->indexAt(pos);
        if (!idx.isValid()) return;
        if (idx.data(ContactListModel::IsSectionHeaderRole).toBool()) return;

        const QString clientId = idx.data(ContactListModel::ClientIdRole).toString().trimmed();
        const QString displayName = idx.data(ContactListModel::DisplayNameRole).toString().trimmed();
        if (clientId.isEmpty()) return;

        ElaMenu menu(this);
        menu.setStyleSheet(QStringLiteral(
            "QMenu { background:%1; border:none; border-radius:8px; padding:4px 0; }"
            "QMenu::item { padding:7px 20px; font-size:13px; color:%3; }"
            "QMenu::item:selected { background:%4; color:%5; }"
            "QMenu::separator { height:1px; background:%2; margin:4px 0; }")
            .arg(AppStyle::surface(), AppStyle::border(), AppStyle::textPrimary(),
                 AppStyle::hoverBg(), AppStyle::accent()));

        auto* chatAction = menu.addAction(QStringLiteral("\u53D1\u8D77\u79C1\u804A"));
        auto* profileAction = menu.addAction(QStringLiteral("\u67E5\u770B\u8D44\u6599"));
        auto* followUpAction = menu.addAction(QStringLiteral("明天跟进"));

        auto* contactModel = qobject_cast<ContactListModel*>(m_contactList->model());
        const bool isFav = contactModel && contactModel->isFavorite(clientId);
        auto* favAction = menu.addAction(isFav ? QStringLiteral("\u53D6\u6D88\u6536\u85CF")
                                               : QStringLiteral("\u2605 \u6536\u85CF\u8054\u7CFB\u4EBA"));

        const QString currentAlias = contactModel ? contactModel->aliasFor(clientId) : QString();
        auto* aliasAction = menu.addAction(currentAlias.isEmpty()
                                               ? QStringLiteral("\u8BBE\u7F6E\u5907\u6CE8\u540D")
                                               : QStringLiteral("\u4FEE\u6539\u5907\u6CE8\u540D"));

        menu.addSeparator();
        auto* deleteAction = menu.addAction(QStringLiteral("\u5220\u9664\u8054\u7CFB\u4EBA"));

        const QAction* chosen = menu.exec(m_contactList->viewport()->mapToGlobal(pos));
        if (!chosen) return;

        if (chosen == chatAction) {
            emit openConversationRequested(clientId);
        } else if (chosen == profileAction) {
            emit contactProfileRequested(clientId);
        } else if (chosen == followUpAction) {
            emit contactReminderRequested(
                clientId,
                displayName,
                idx.data(ContactListModel::StatusTextRole).toString().trimmed());
        } else if (chosen == favAction) {
            if (contactModel) contactModel->toggleFavorite(clientId);
        } else if (chosen == aliasAction) {
            if (contactModel) {
                bool ok = false;
                const QString alias = LeyoDialog::getText(
                    this,
                    QStringLiteral("\u8BBE\u7F6E\u5907\u6CE8\u540D"),
                    QStringLiteral("\u8BF7\u8F93\u5165\u5907\u6CE8\u540D\uFF08\u7559\u7A7A\u5219\u6E05\u9664\uFF09\uFF1A"),
                    currentAlias, &ok);
                if (ok) contactModel->setAlias(clientId, alias.trimmed());
            }
        } else if (chosen == deleteAction) {
            const QString targetLabel = displayName.isEmpty() ? clientId : displayName;
            if (LeyoDialog::question(this,
                    QStringLiteral("\u5220\u9664\u8054\u7CFB\u4EBA"),
                    QStringLiteral("\u786E\u8BA4\u5220\u9664\u8054\u7CFB\u4EBA\u201C%1\u201D\u5417\uFF1F").arg(targetLabel))) {
                emit contactDeleteRequested(clientId);
            }
        }
    });

    const auto openGroupFromItem = [this](QListWidgetItem* item) {
        if (!item) return;
        const QString groupId = item->data(Qt::UserRole).toString();
        if (!groupId.isEmpty()) {
            emit groupConversationRequested(groupId);
        }
    };
    connect(m_groupListWidget, &QListWidget::itemDoubleClicked, this, openGroupFromItem);
}

void DirectoryPage::setContactModel(ContactListModel* model)
{
    m_contactList->setModel(model);
    if (model) {
        const auto filter = m_presenceFilterIndex == 1
            ? ContactListModel::PresenceFilter::Online
            : m_presenceFilterIndex == 2
                ? ContactListModel::PresenceFilter::Offline
                : ContactListModel::PresenceFilter::All;
        model->setPresenceFilter(filter);
    }
    if (m_alphabetIndexBar) {
        m_alphabetIndexBar->setModel(model);
    }
    const auto syncContactState = [this, model]() {
        if (!m_contactEmptyLabel) return;
        const bool hasItems = model && model->rowCount() > 0;
        m_contactEmptyLabel->setVisible(!hasItems);
        m_contactList->setVisible(hasItems);
        if (m_alphabetIndexBar) {
            m_alphabetIndexBar->setVisible(false);
            m_alphabetIndexBar->refresh();
        }
        syncContactWorkspaceStatus();
    };
    if (model) {
        connect(model, &QAbstractItemModel::modelReset, this, syncContactState);
        connect(model, &QAbstractItemModel::rowsInserted, this, syncContactState);
        connect(model, &QAbstractItemModel::rowsRemoved, this, syncContactState);
        connect(model, &QAbstractItemModel::layoutChanged, this, syncContactState);
        connect(model, &QAbstractItemModel::dataChanged, this, syncContactState);
    }
    syncContactState();
}

void DirectoryPage::setSelectedContactId(const QString& clientId)
{
    if (!m_contactList || !m_contactList->model()) return;
    for (int row = 0; row < m_contactList->model()->rowCount(); ++row) {
        const QModelIndex index = m_contactList->model()->index(row, 0);
        if (index.data(ContactListModel::ClientIdRole).toString() == clientId.trimmed()) {
            if (m_contactList->currentIndex() == index) return;
            m_contactList->setCurrentIndex(index);
            m_contactList->selectionModel()->select(index,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            return;
        }
    }
    m_contactList->clearSelection();
}

void DirectoryPage::syncContactWorkspaceStatus()
{
    if (!m_contactList) return;

    auto* model = m_contactList->model();
    if (!model || model->rowCount() == 0) {
        if (m_contactsStatusChip) {
            m_contactsStatusChip->setText(QStringLiteral("\u5C40\u57DF\u7F51\u5728\u7EBF"));
        }
        if (m_heroSummaryLabel)
            m_heroSummaryLabel->setText(QStringLiteral("\u5171 0 \u4F4D\u8054\u7CFB\u4EBA \u00B7 0 \u4F4D\u5728\u7EBF \u00B7 %1 \u4E2A\u7FA4\u7EC4").arg(m_groups.size()));
        if (m_heroRuntimeLabel)
            m_heroRuntimeLabel->setText(QStringLiteral("\u5F53\u524D\u6682\u65E0\u5728\u7EBF\u8054\u7CFB\u4EBA\uFF0C\u5DF2\u8BC6\u522B\u8054\u7CFB\u4EBA\u4ECD\u4FDD\u7559\u5728\u901A\u8BAF\u5F55\u4E2D\u3002"));
        updateDirectoryStats();
        return;
    }

    int onlineCount = 0;
    int totalCount = 0;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(ContactListModel::IsSectionHeaderRole).toBool()) continue;
        ++totalCount;
        if (index.data(ContactListModel::StatusTextRole).toString()
            == QStringLiteral("\u5728\u7EBF")) {
            ++onlineCount;
        }
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kOnlineStatusHoldMs = 3000;
    int displayOnlineCount = onlineCount;
    if (onlineCount > 0) {
        m_lastStableOnlineCount = onlineCount;
        m_contactOnlineHoldUntilMs = nowMs + kOnlineStatusHoldMs;
    } else if (m_lastStableOnlineCount > 0 && nowMs < m_contactOnlineHoldUntilMs) {
        displayOnlineCount = m_lastStableOnlineCount;
    } else {
        m_lastStableOnlineCount = 0;
        m_contactOnlineHoldUntilMs = 0;
    }

    const QString chipText = QStringLiteral("%1/%2 \u5728\u7EBF").arg(displayOnlineCount).arg(totalCount);
    if (m_contactsStatusChip) {
        m_contactsStatusChip->setText(chipText);
        m_contactsStatusChip->setMinimumWidth(
            QFontMetrics(m_contactsStatusChip->font()).horizontalAdvance(chipText) + 28);
    }

    // 更新 Hero 面板
    if (m_heroSummaryLabel) {
        m_heroSummaryLabel->setText(
            QStringLiteral("\u5171 %1 \u4F4D\u8054\u7CFB\u4EBA \u00B7 %2 \u4F4D\u5728\u7EBF \u00B7 %3 \u4E2A\u7FA4\u7EC4 \u00B7 \u53CC\u51FB\u8054\u7CFB\u4EBA\u53EF\u65B0\u5EFA\u4F1A\u8BDD")
                .arg(totalCount)
                .arg(displayOnlineCount)
                .arg(m_groups.size()));
    }
    if (m_heroRuntimeLabel) {
        m_heroRuntimeLabel->setText(
            displayOnlineCount > 0
                ? QStringLiteral("\u5DF2\u8BC6\u522B\u7528\u6237\u4F1A\u81EA\u52A8\u4FDD\u7559\u5728\u901A\u8BAF\u5F55\uFF1B\u641C\u7D22\u4E0D\u533A\u5206\u5728\u7EBF\u72B6\u6001\uFF0C\u79BB\u7EBF\u8054\u7CFB\u4EBA\u4E5F\u53EF\u76F4\u63A5\u6253\u5F00\u4F1A\u8BDD\u3002")
                : QStringLiteral("\u5F53\u524D\u6682\u65E0\u5728\u7EBF\u8054\u7CFB\u4EBA\uFF0C\u4F46\u5DF2\u8BC6\u522B\u8054\u7CFB\u4EBA\u4ECD\u4FDD\u7559\u5728\u901A\u8BAF\u5F55\u548C\u641C\u7D22\u4E2D\u3002"));
    }
    updateDirectoryStats();
}

void DirectoryPage::setGroups(const QVector<GroupSummary>& groups)
{
    m_groups = groups;
    rebuildGroupList();
    syncContactWorkspaceStatus(); // refresh hero stats
    updateDirectoryStats();
}

void DirectoryPage::updateDirectoryStats()
{
    int totalCount = 0;
    int onlineCount = 0;
    if (m_contactList && m_contactList->model()) {
        auto* model = m_contactList->model();
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            if (index.data(ContactListModel::IsSectionHeaderRole).toBool()) continue;
            ++totalCount;
            if (index.data(ContactListModel::StatusTextRole).toString() == QStringLiteral("\u5728\u7EBF")) {
                ++onlineCount;
            }
        }
    }

    int unreadGroups = 0;
    int groupMembers = 0;
    for (const GroupSummary& group : std::as_const(m_groups)) {
        if (group.unreadCount > 0) ++unreadGroups;
        groupMembers += qMax(0, group.memberCount);
    }

    if (m_contactTotalValueLabel) m_contactTotalValueLabel->setText(QString::number(totalCount));
    if (m_contactOnlineValueLabel) {
        m_contactOnlineValueLabel->setText(QStringLiteral("\u5728\u7EBF %1").arg(onlineCount));
    }
    if (m_groupTotalValueLabel) m_groupTotalValueLabel->setText(QString::number(m_groups.size()));
    if (m_groupUnreadValueLabel) {
        m_groupUnreadValueLabel->setText(QStringLiteral("\u672A\u8BFB %1").arg(unreadGroups));
    }
    if (m_myGroupValueLabel) m_myGroupValueLabel->setText(QString::number(m_groups.size()));
    if (m_departmentValueLabel) m_departmentValueLabel->setText(QString::number(m_departmentCount));

    if (m_heroSummaryLabel) {
        m_heroSummaryLabel->setText(QStringLiteral("\u8054\u7CFB\u4EBA %1 \u00B7 \u5728\u7EBF %2 \u00B7 \u7FA4\u6210\u5458 %3")
            .arg(totalCount)
            .arg(onlineCount)
            .arg(groupMembers));
    }
}

void DirectoryPage::rebuildGroupList()
{
    if (!m_groupListWidget || !m_groupEmptyLabel) return;

    m_groupListWidget->clear();
    const bool hasGroups = !m_groups.isEmpty();
    m_groupEmptyLabel->setVisible(!hasGroups);
    m_groupListWidget->setVisible(hasGroups);
    if (!hasGroups) return;

    QVector<GroupSummary> sorted = m_groups;
    std::sort(sorted.begin(), sorted.end(), [](const GroupSummary& a, const GroupSummary& b) {
        const bool aUnread = a.unreadCount > 0;
        const bool bUnread = b.unreadCount > 0;
        if (aUnread != bUnread) return aUnread;
        return a.lastActiveMs > b.lastActiveMs;
    });

    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();

    for (const GroupSummary& g : sorted) {
        auto* item = new QListWidgetItem();
        item->setData(Qt::UserRole, g.groupId);
        item->setData(GroupIdRole, g.groupId);
        item->setData(GroupNameRole, g.groupName);
        item->setData(GroupOwnerRole, g.ownerDisplayName);
        item->setData(GroupMemberCountRole, g.memberCount);
        item->setData(GroupUnreadCountRole, g.unreadCount);
        item->setSizeHint(QSize(0, 74));
        const QString title = g.groupName.trimmed().isEmpty()
            ? QStringLiteral("\u672A\u547D\u540D\u7FA4\u7EC4")
            : g.groupName.trimmed();
        const QString meta = g.ownerDisplayName.isEmpty()
            ? QStringLiteral("%1 \u4F4D\u6210\u5458").arg(g.memberCount)
            : QStringLiteral("%1 \u4F4D\u6210\u5458 \u00B7 %2").arg(g.memberCount).arg(g.ownerDisplayName);
        item->setText(g.unreadCount > 0
            ? QStringLiteral("%1    \u672A\u8BFB %2\n%3").arg(title).arg(g.unreadCount).arg(meta)
            : QStringLiteral("%1\n%2").arg(title, meta));
        item->setForeground(QColor(AppStyle::textPrimary(mode)));
        m_groupListWidget->addItem(item);
    }
}

void DirectoryPage::updateTabStyle()
{
    if (!m_contactsTabBtn || !m_groupsTabBtn || !m_orgTabBtn) return;

    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    auto apply = [&](ElaPushButton* button, bool active) {
        button->setBorderRadius(8);
        if (active) {
            button->setLightDefaultColor(QColor(AppStyle::accent(mode)));
            button->setDarkDefaultColor(QColor(AppStyle::accent(mode)));
            button->setLightHoverColor(QColor(AppStyle::accentHover(mode)));
            button->setDarkHoverColor(QColor(AppStyle::accentHover(mode)));
            button->setLightPressColor(QColor(AppStyle::accentPressed(mode)));
            button->setDarkPressColor(QColor(AppStyle::accentPressed(mode)));
            button->setLightTextColor(Qt::white);
            button->setDarkTextColor(Qt::white);
        } else {
            button->setLightDefaultColor(QColor(255, 255, 255, 0));
            button->setDarkDefaultColor(QColor(255, 255, 255, 0));
            button->setLightHoverColor(QColor(255, 255, 255, 62));
            button->setDarkHoverColor(QColor(32, 54, 68, 150));
            button->setLightPressColor(QColor(255, 255, 255, 92));
            button->setDarkPressColor(QColor(22, 38, 50, 180));
            button->setLightTextColor(QColor(AppStyle::textSecondary(mode)));
            button->setDarkTextColor(QColor(AppStyle::textSecondary(mode)));
        }
        button->update();
    };
    apply(m_contactsTabBtn, m_activeTabIndex == 0);
    apply(m_groupsTabBtn, m_activeTabIndex == 1);
    apply(m_orgTabBtn, m_activeTabIndex == 2);
}

void DirectoryPage::updatePresenceFilterStyle()
{
    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    auto apply = [&](ElaPushButton* button, bool active) {
        if (!button) return;
        button->setBorderRadius(7);
        if (active) {
            button->setLightDefaultColor(QColor(AppStyle::accent(mode)));
            button->setDarkDefaultColor(QColor(AppStyle::accent(mode)));
            button->setLightHoverColor(QColor(AppStyle::accentHover(mode)));
            button->setDarkHoverColor(QColor(AppStyle::accentHover(mode)));
            button->setLightPressColor(QColor(AppStyle::accentPressed(mode)));
            button->setDarkPressColor(QColor(AppStyle::accentPressed(mode)));
            button->setLightTextColor(Qt::white);
            button->setDarkTextColor(Qt::white);
        } else {
            button->setLightDefaultColor(QColor(255, 255, 255, 0));
            button->setDarkDefaultColor(QColor(255, 255, 255, 0));
            button->setLightHoverColor(QColor(255, 255, 255, 62));
            button->setDarkHoverColor(QColor(32, 54, 68, 150));
            button->setLightPressColor(QColor(255, 255, 255, 92));
            button->setDarkPressColor(QColor(22, 38, 50, 180));
            button->setLightTextColor(QColor(AppStyle::textSecondary(mode)));
            button->setDarkTextColor(QColor(AppStyle::textSecondary(mode)));
        }
        button->update();
    };
    apply(m_allFilterBtn, m_presenceFilterIndex == 0);
    apply(m_onlineFilterBtn, m_presenceFilterIndex == 1);
    apply(m_offlineFilterBtn, m_presenceFilterIndex == 2);
}

void DirectoryPage::refreshTheme()
{
    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();

    // Fix list viewport backgrounds for dark mode — use chatStageBg
    const QColor panelBg(Qt::transparent);
    auto fixViewport = [&](QAbstractScrollArea* area) {
        if (!area || !area->viewport()) return;
        QPalette vp = area->viewport()->palette();
        area->setAutoFillBackground(false);
        area->viewport()->setAutoFillBackground(false);
        vp.setColor(QPalette::Base, panelBg);
        vp.setColor(QPalette::Window, panelBg);
        area->viewport()->setPalette(vp);
        area->viewport()->update();
    };
    fixViewport(m_contactList);
    fixViewport(m_groupListWidget);
    fixViewport(m_orgTreeWidget);

    const QString shell = directoryGlass(mode);
    const QString soft = directorySoftGlass(mode);
    setStyleSheet(QStringLiteral(
        "DirectoryPage, QWidget#directoryPageRoot, QWidget#directoryContactSurface, QWidget#directoryGroupSurface, QWidget#directoryOrgSurface { background:transparent; }"
        "QStackedWidget { background:transparent; border:none; }"
        "QWidget#directoryToolbar { background:transparent; border:none; }"
        "QLabel#directoryHeroTitle { font-size:18px; font-weight:700; color:%1; background:transparent; }"
        "ElaFrame#directoryHeroPanel { background:%2; border:1px solid %3; border-radius:12px; }"
        "ElaFrame#directoryStatCard { background:transparent; border-right:1px solid %3; border-radius:0; }"
        "QLabel#directoryStatTitle { font-size:11px; color:%4; background:transparent; }"
        "QLabel#directoryStatValue { font-size:18px; font-weight:800; color:%1; background:transparent; }"
        "QLabel#directoryStatMeta { font-size:11px; color:%4; background:transparent; }"
        "ElaFrame#sideSurfaceBand { background:transparent; border:none; border-radius:0; }"
        "ElaFrame#directorySegmentedTabs { background:%5; border:1px solid %3; border-radius:10px; }"
        "ElaFrame#directoryPresenceFilterTabs { background:%5; border:1px solid %3; border-radius:10px; }"
        "ElaLineEdit#directorySearchEdit { background:%5; border:1px solid %3; border-radius:10px; color:%1; padding-left:10px; }"
        "QListView#directoryContactList { background:transparent; border:none; outline:0; }"
        "QListView#directoryContactList::item { background:transparent; border:none; padding:0; margin:0; }"
        "QListView#directoryContactList::item:selected { background:transparent; border:none; }"
        "QListView#directoryContactList::item:selected:active { background:transparent; border:none; }"
        "QListView#directoryContactList::item:hover { background:transparent; border:none; }"
        "QListWidget#directoryGroupList { background:transparent; border:none; outline:0; }"
        "QListWidget#directoryGroupList::item { padding:6px 0; border-radius:8px; }"
        "QListWidget#directoryGroupList::item:selected { background:%6; }"
        "QListWidget#directoryGroupList::item:hover { background:%7; }"
        "QTreeView#directoryOrgTree { background:transparent; border:none; outline:0; }"
        "QTreeView#directoryOrgTree::item { padding:5px 4px; border-radius:6px; }"
        "QTreeView#directoryOrgTree::item:selected { background:%6; }"
        "QTreeView#directoryOrgTree::item:hover { background:%7; }"
        "QTreeView#directoryOrgTree::branch { background:transparent; }"
        "QLabel { selection-background-color:%6; }")
        .arg(AppStyle::textPrimary(mode),
             shell,
             AppStyle::border(mode),
             AppStyle::textSecondary(mode),
             soft,
             AppStyle::selectedBg(mode),
             AppStyle::hoverBg(mode)));

    updateTabStyle();
    updatePresenceFilterStyle();

    // Re-apply hero panel styles
    if (m_heroSummaryLabel) {
        m_heroSummaryLabel->setStyleSheet(QStringLiteral(
            "font-size:13px; color:%1; background:transparent;")
            .arg(AppStyle::textMuted(mode)));
    }
    if (m_heroRuntimeLabel) {
        m_heroRuntimeLabel->setStyleSheet(QStringLiteral(
            "font-size:12px; color:%1; background:transparent;")
            .arg(AppStyle::textMuted(mode)));
    }
    if (m_groupEmptyLabel) {
        m_groupEmptyLabel->setStyleSheet(QStringLiteral(
            "font-size:13px; color:%1; background:transparent; border:none; padding:18px 20px;")
            .arg(AppStyle::textMuted(mode)));
    }
    if (m_orgEmptyLabel) {
        m_orgEmptyLabel->setStyleSheet(QStringLiteral(
            "font-size:13px; color:%1; background:transparent; border:none; padding:18px 20px;")
            .arg(AppStyle::textMuted(mode)));
    }
    if (m_orgTreeWidget) {
        m_orgTreeWidget->setStyleSheet(QStringLiteral(
            "QTreeView { background:transparent; border:none; outline:0; }"
            "QTreeView::item { padding:5px 4px; border-radius:6px; }"
            "QTreeView::item:selected { background:%1; }"
            "QTreeView::item:hover { background:%2; }"
            "QTreeView::branch { background:transparent; }")
            .arg(AppStyle::selectedBg(mode), AppStyle::hoverBg(mode)));
    }

    // Rebuild group cards (they embed theme colors)
    rebuildGroupList();
    updateDirectoryStats();

    // Force contact list repaint (delegate reads AppStyle::currentThemeMode())
    if (m_contactList && m_contactList->viewport())
        m_contactList->viewport()->update();
}

void DirectoryPage::switchToOrgTab()
{
    if (m_tabStack) {
        m_tabStack->setCurrentIndex(2);
        m_activeTabIndex = 2;
        updateTabStyle();
    }
}

void DirectoryPage::setOrgData(const QHash<QString, QVector<OrgContactEntry>>& departments)
{
    m_departmentCount = departments.size();
    updateDirectoryStats();

    if (!m_orgTreeModel || !m_orgTreeWidget || !m_orgEmptyLabel) return;

    m_orgTreeModel->clear();

    if (departments.isEmpty()) {
        m_orgEmptyLabel->show();
        m_orgTreeWidget->hide();
        return;
    }
    m_orgEmptyLabel->hide();
    m_orgTreeWidget->show();

    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    const QColor deptColor(AppStyle::textPrimary(mode));

    // 按部门名排序，"未分组"放最后
    QStringList deptNames = departments.keys();
    std::sort(deptNames.begin(), deptNames.end(), [](const QString& a, const QString& b) {
        const bool aEmpty = a.isEmpty() || a == QStringLiteral("\u672A\u5206\u7EC4");
        const bool bEmpty = b.isEmpty() || b == QStringLiteral("\u672A\u5206\u7EC4");
        if (aEmpty != bEmpty) return bEmpty;
        return a.localeAwareCompare(b) < 0;
    });

    for (const QString& dept : deptNames) {
        const auto& members = departments.value(dept);
        const QString displayDept = dept.isEmpty() ? QStringLiteral("\u672A\u5206\u7EC4") : dept;

        int onlineCount = 0;
        for (const auto& m : members) {
            if (m.isOnline) ++onlineCount;
        }

        auto* deptItem = new QStandardItem(
            QStringLiteral("%1  (%2\u4EBA, \u5728\u7EBF%3)")
                .arg(displayDept)
                .arg(members.size())
                .arg(onlineCount));
        deptItem->setEditable(false);
        QFont deptFont;
        deptFont.setPixelSize(14);
        deptFont.setBold(true);
        deptItem->setFont(deptFont);
        deptItem->setForeground(deptColor);

        for (const auto& member : members) {
            auto* memberItem = new QStandardItem(member.displayName);
            memberItem->setEditable(false);
            memberItem->setData(member.clientId, OrgTreeDelegate::ClientIdRole);
            memberItem->setData(member.displayName, OrgTreeDelegate::DisplayNameRole);
            memberItem->setData(member.jobTitle, OrgTreeDelegate::JobTitleRole);
            memberItem->setData(member.isOnline, OrgTreeDelegate::IsOnlineRole);
            deptItem->appendRow(memberItem);
        }
        m_orgTreeModel->appendRow(deptItem);
    }

    // 默认展开所有部门
    m_orgTreeWidget->expandAll();
}
