#include "GlobalSearchPanel.h"
#include "GlobalSearchHistory.h"
#include "AppStyle.h"

#include <ElaFlowLayout.h>
#include <ElaIconButton.h>
#include <ElaLineEdit.h>
#include <ElaListWidget.h>
#include <ElaScrollBar.h>
#include <ElaText.h>
#include <ElaTheme.h>

#include <QDateTime>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputMethod>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kTabHeight = 36;
constexpr int kIndicatorHeight = 3;
constexpr int kSearchBarHeight = 38;

struct TabDef {
    const char16_t* label;
};

const TabDef kTabs[] = {
    {u"综合"},
    {u"联系人"},
    {u"群组"},
    {u"聊天记录"},
    {u"文件"},
    {u"部门"},
};
static_assert(std::size(kTabs) == static_cast<size_t>(GlobalSearchPanel::Tab::_Count));

// item data roles
constexpr int kRoleKind = Qt::UserRole + 200;       // "contact","group","message","file","department","keyword","frequent"
constexpr int kRoleId = Qt::UserRole + 201;
constexpr int kRoleId2 = Qt::UserRole + 202;        // messageId for messages
constexpr int kRoleTitle = Qt::UserRole + 203;

bool shouldRouteToSearchEdit(const QKeyEvent* event)
{
    if (!event) return false;
    if (event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return false;
    }
    if (!event->text().isEmpty()) {
        return true;
    }
    switch (event->key()) {
    case Qt::Key_Backspace:
    case Qt::Key_Delete:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Home:
    case Qt::Key_End:
        return true;
    default:
        return false;
    }
}
} // namespace

GlobalSearchPanel::GlobalSearchPanel(GlobalSearchHistory* history, QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    , m_history(history)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    setFocusPolicy(Qt::StrongFocus);
    setFixedWidth(kPanelWidth);
    setMaximumHeight(kPanelMaxHeight);

    buildUi();
    applyTheme();

    auto* themeInst = eTheme;
    if (themeInst) {
        connect(themeInst, &ElaTheme::themeModeChanged, this, &GlobalSearchPanel::applyTheme);
    }
}

void GlobalSearchPanel::buildUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(16, 14, 16, 14);
    rootLayout->setSpacing(0);

    // ── 搜索输入框 ──
    m_searchEdit = new ElaLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("\u641C\u7D22\u8054\u7CFB\u4EBA\u3001\u7FA4\u7EC4\u3001\u804A\u5929\u8BB0\u5F55\u2026"));
    m_searchEdit->setIsClearButtonEnable(true);
    m_searchEdit->setFixedHeight(kSearchBarHeight);
    m_searchEdit->setBorderRadius(10);
    m_searchEdit->setAttribute(Qt::WA_InputMethodEnabled, true);
    m_searchEdit->setFocusPolicy(Qt::StrongFocus);
    setFocusProxy(m_searchEdit);
    rootLayout->addWidget(m_searchEdit);
    rootLayout->addSpacing(8);

    // ── Tab 栏 ──
    m_tabBar = new QWidget(this);
    m_tabBar->setFixedHeight(kTabHeight);
    auto* tabLayout = new QHBoxLayout(m_tabBar);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(0);

    for (int i = 0; i < static_cast<int>(Tab::_Count); ++i) {
        auto* label = new ElaText(QString::fromUtf16(kTabs[i].label), this);
        label->setFixedHeight(kTabHeight);
        label->setAlignment(Qt::AlignCenter);
        label->setCursor(Qt::PointingHandCursor);
        label->installEventFilter(this);
        m_tabLabels.append(label);
        tabLayout->addWidget(label, 1);
    }
    rootLayout->addWidget(m_tabBar);

    // Tab 指示条
    m_tabIndicator = new QWidget(m_tabBar);
    m_tabIndicator->setFixedHeight(kIndicatorHeight);
    m_tabIndicator->setFixedWidth(40);
    m_indicatorAnim = new QPropertyAnimation(m_tabIndicator, "geometry", this);
    m_indicatorAnim->setDuration(200);
    m_indicatorAnim->setEasingCurve(QEasingCurve::OutCubic);

    rootLayout->addSpacing(6);

    // ── 空态容器 ──
    m_emptyStateWidget = new QWidget(this);
    auto* emptyLayout = new QVBoxLayout(m_emptyStateWidget);
    emptyLayout->setContentsMargins(0, 4, 0, 0);
    emptyLayout->setSpacing(10);

    // 搜索历史
    auto* historyHeader = new QHBoxLayout;
    auto* historyTitle = new ElaText(QStringLiteral("\u641C\u7D22\u5386\u53F2"), this);
    historyTitle->setObjectName(QStringLiteral("sectionTitle"));
    historyHeader->addWidget(historyTitle);
    historyHeader->addStretch();
    auto* clearHistoryBtn = new ElaIconButton(ElaIconType::TrashCan, 14, this);
    clearHistoryBtn->setFixedSize(22, 22);
    clearHistoryBtn->setToolTip(QStringLiteral("\u6E05\u7A7A\u641C\u7D22\u5386\u53F2"));
    historyHeader->addWidget(clearHistoryBtn);
    emptyLayout->addLayout(historyHeader);

    m_keywordFlow = new ElaFlowLayout(nullptr, -1, 8, 6);
    auto* keywordFlowWidget = new QWidget(m_emptyStateWidget);
    keywordFlowWidget->setLayout(m_keywordFlow);
    emptyLayout->addWidget(keywordFlowWidget);

    emptyLayout->addSpacing(6);

    // 常用
    auto* frequentHeader = new QHBoxLayout;
    auto* frequentTitle = new ElaText(QStringLiteral("\u5E38\u7528"), this);
    frequentTitle->setObjectName(QStringLiteral("sectionTitle"));
    frequentHeader->addWidget(frequentTitle);
    frequentHeader->addStretch();
    auto* clearFrequentBtn = new ElaIconButton(ElaIconType::TrashCan, 14, this);
    clearFrequentBtn->setFixedSize(22, 22);
    clearFrequentBtn->setToolTip(QStringLiteral("\u6E05\u7A7A\u5E38\u7528"));
    frequentHeader->addWidget(clearFrequentBtn);
    emptyLayout->addLayout(frequentHeader);

    m_frequentFlow = new ElaFlowLayout(nullptr, -1, 8, 6);
    auto* frequentFlowWidget = new QWidget(m_emptyStateWidget);
    frequentFlowWidget->setLayout(m_frequentFlow);
    emptyLayout->addWidget(frequentFlowWidget);
    emptyLayout->addStretch();

    rootLayout->addWidget(m_emptyStateWidget, 1);

    // ── 结果列表 ──
    m_resultList = new ElaListWidget(this);
    m_resultList->setObjectName(QStringLiteral("globalSearchResultList"));
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resultList->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    new ElaScrollBar(m_resultList->verticalScrollBar(), m_resultList);
    m_resultList->hide();
    rootLayout->addWidget(m_resultList, 1);

    // ── 防抖定时器 ──
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(150);

    // ── 连接 ──
    connect(m_searchEdit, &ElaLineEdit::textChanged, this, &GlobalSearchPanel::onSearchTextChanged);
    connect(m_debounceTimer, &QTimer::timeout, this, [this]() {
        if (m_currentKeyword.isEmpty()) {
            showEmptyState();
        } else {
            emit searchRequested(m_currentKeyword, static_cast<int>(m_currentTab));
        }
    });
    connect(m_resultList, &ElaListWidget::itemClicked, this, &GlobalSearchPanel::onItemClicked);
    connect(clearHistoryBtn, &ElaIconButton::clicked, this, [this]() {
        if (m_history) m_history->clearKeywords();
        showEmptyState();
    });
    connect(clearFrequentBtn, &ElaIconButton::clicked, this, [this]() {
        if (m_history) m_history->clearFrequentContacts();
        showEmptyState();
    });

}

void GlobalSearchPanel::popup(const QPoint& globalPos, const QString& initialText)
{
    m_searchEdit->setText(initialText);
    m_currentKeyword = initialText.trimmed();

    // 计算面板位置，确保不超出屏幕边界
    QPoint pos = globalPos;
    if (auto* screen = QGuiApplication::screenAt(globalPos)) {
        const QRect screenGeo = screen->availableGeometry();
        if (pos.x() + kPanelWidth > screenGeo.right()) {
            pos.setX(screenGeo.right() - kPanelWidth - 4);
        }
        if (pos.x() < screenGeo.left()) {
            pos.setX(screenGeo.left() + 4);
        }
        if (pos.y() + kPanelMaxHeight > screenGeo.bottom()) {
            // 向上弹出
            pos.setY(globalPos.y() - kPanelMaxHeight - 8);
        }
    }
    move(pos);
    show();
    focusSearchEdit(true);
    QTimer::singleShot(0, this, [this]() { focusSearchEdit(); });
    QTimer::singleShot(50, this, [this]() { focusSearchEdit(); });

    if (m_currentKeyword.isEmpty()) {
        showEmptyState();
    } else {
        emit searchRequested(m_currentKeyword, static_cast<int>(m_currentTab));
    }
    adjustSize();

    // 延迟定位指示条（需要等 tab label layout 完成后才有正确宽度）
    QTimer::singleShot(0, this, [this]() { animateIndicator(static_cast<int>(m_currentTab)); });
}

void GlobalSearchPanel::dismiss()
{
    m_debounceTimer->stop();
    hide();
    emit dismissed();
}

void GlobalSearchPanel::switchTab(Tab tab)
{
    if (m_currentTab == tab) return;
    m_currentTab = tab;
    animateIndicator(static_cast<int>(tab));
    // 更新 Tab 标签的选中/未选中样式
    const auto mode = AppStyle::currentThemeMode();
    const QString textColor = AppStyle::textPrimary(mode);
    const QString mutedColor = AppStyle::textSecondary(mode);
    for (int i = 0; i < m_tabLabels.size(); ++i) {
        const bool active = (i == static_cast<int>(tab));
        m_tabLabels[i]->setStyleSheet(QStringLiteral(
            "font-size:13px; font-weight:%1; color:%2; background:transparent; padding:0 8px;")
            .arg(active ? QStringLiteral("700") : QStringLiteral("400"),
                 active ? textColor : mutedColor));
    }
    refreshContent();
}

void GlobalSearchPanel::refreshContent()
{
    if (m_currentKeyword.isEmpty()) {
        showEmptyState();
    } else {
        // Tab 切换时直接用已缓存的结果做本地过滤，不重新发起搜索
        showSearchResults();
    }
}

void GlobalSearchPanel::showEmptyState()
{
    m_resultList->hide();
    m_emptyStateWidget->show();

    // 清除旧 tag — 立即 delete 避免与新 widget 共存
    while (m_keywordFlow->count() > 0) {
        auto* item = m_keywordFlow->takeAt(0);
        if (auto* w = item->widget()) {
            w->removeEventFilter(this);
            delete w;
        }
        delete item;
    }
    while (m_frequentFlow->count() > 0) {
        auto* item = m_frequentFlow->takeAt(0);
        if (auto* w = item->widget()) {
            w->removeEventFilter(this);
            delete w;
        }
        delete item;
    }

    if (!m_history) return;

    // 填充搜索历史 Tag
    const QStringList keywords = m_history->recentKeywords();
    for (const QString& kw : keywords) {
        auto* tag = new ElaText(kw, m_emptyStateWidget);
        tag->setObjectName(QStringLiteral("keywordTag"));
        tag->setCursor(Qt::PointingHandCursor);
        tag->setFixedHeight(26);
        tag->setAlignment(Qt::AlignCenter);
        tag->installEventFilter(this);
        m_keywordFlow->addWidget(tag);
    }

    // 填充常用
    const QVector<FrequentContact> frequents = m_history->frequentContacts();
    for (const auto& fc : frequents) {
        auto* chip = new QWidget(m_emptyStateWidget);
        chip->setObjectName(QStringLiteral("frequentChip"));
        chip->setCursor(Qt::PointingHandCursor);
        chip->setFixedHeight(32);
        chip->setProperty("contactId", fc.id);
        chip->setProperty("contactTitle", fc.title);
        chip->setProperty("contactKind", fc.kind);
        chip->installEventFilter(this);
        auto* chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(6, 2, 10, 2);
        chipLayout->setSpacing(6);

        // 头像
        auto* avatar = new ElaText(chip);
        avatar->setFixedSize(24, 24);
        avatar->setAlignment(Qt::AlignCenter);
        const QString letter = fc.title.isEmpty() ? QStringLiteral("?")
            : QString(fc.title.front()).toUpper();
        avatar->setText(letter);
        avatar->setObjectName(fc.kind == 0 ? QStringLiteral("avatarContact") : QStringLiteral("avatarGroup"));
        chipLayout->addWidget(avatar);

        auto* nameLabel = new ElaText(fc.title, chip);
        nameLabel->setObjectName(QStringLiteral("chipName"));
        chipLayout->addWidget(nameLabel);

        m_frequentFlow->addWidget(chip);
    }

    adjustSize();
}

void GlobalSearchPanel::showSearchResults()
{
    m_emptyStateWidget->hide();
    m_resultList->show();
    m_resultList->clear();

    const auto addSection = [this](const QString& title) {
        auto* item = new QListWidgetItem(m_resultList);
        item->setFlags(Qt::NoItemFlags);
        auto* label = new ElaText(title, m_resultList);
        label->setObjectName(QStringLiteral("resultSection"));
        item->setSizeHint(QSize(0, 28));
        m_resultList->setItemWidget(item, label);
    };

    const auto addContactItem = [this](const ContactResult& c) {
        auto* widget = new QWidget;
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(10);

        auto* avatar = new ElaText(widget);
        avatar->setFixedSize(32, 32);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setText(c.displayName.isEmpty() ? QStringLiteral("?")
            : QString(c.displayName.front()).toUpper());
        avatar->setObjectName(QStringLiteral("avatarContact"));
        layout->addWidget(avatar);

        auto* infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(1);
        auto* nameLabel = new ElaText(highlightKeyword(c.displayName), widget);
        nameLabel->setTextFormat(Qt::RichText);
        nameLabel->setObjectName(QStringLiteral("resultName"));
        infoLayout->addWidget(nameLabel);
        if (!c.detail.isEmpty()) {
            auto* detailLabel = new ElaText(c.detail, widget);
            detailLabel->setObjectName(QStringLiteral("resultDetail"));
            infoLayout->addWidget(detailLabel);
        }
        layout->addLayout(infoLayout, 1);

        // 在线状态
        auto* statusDot = new ElaText(c.isOnline ? QStringLiteral("\u2022") : QStringLiteral("\u25CB"), widget);
        statusDot->setObjectName(c.isOnline ? QStringLiteral("statusOnline") : QStringLiteral("statusOffline"));
        layout->addWidget(statusDot);

        auto* item = new QListWidgetItem(m_resultList);
        item->setData(kRoleKind, QStringLiteral("contact"));
        item->setData(kRoleId, c.clientId);
        item->setData(kRoleTitle, c.displayName);
        item->setSizeHint(QSize(0, 44));
        m_resultList->setItemWidget(item, widget);
    };

    const auto addGroupItem = [this](const GroupResult& g) {
        auto* widget = new QWidget;
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(10);

        auto* avatar = new ElaText(widget);
        avatar->setFixedSize(32, 32);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setText(QStringLiteral("\u7FA4"));
        avatar->setObjectName(QStringLiteral("avatarGroup"));
        layout->addWidget(avatar);

        auto* infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(1);
        auto* nameLabel = new ElaText(highlightKeyword(g.groupName), widget);
        nameLabel->setTextFormat(Qt::RichText);
        nameLabel->setObjectName(QStringLiteral("resultName"));
        infoLayout->addWidget(nameLabel);
        auto* countLabel = new ElaText(QStringLiteral("%1 \u4EBA").arg(g.memberCount), widget);
        countLabel->setObjectName(QStringLiteral("resultDetail"));
        infoLayout->addWidget(countLabel);
        layout->addLayout(infoLayout, 1);

        auto* item = new QListWidgetItem(m_resultList);
        item->setData(kRoleKind, QStringLiteral("group"));
        item->setData(kRoleId, g.groupId);
        item->setData(kRoleTitle, g.groupName);
        item->setSizeHint(QSize(0, 44));
        m_resultList->setItemWidget(item, widget);
    };

    const auto addMessageItem = [this](const MessageResult& m) {
        auto* widget = new QWidget;
        auto* wLayout = new QVBoxLayout(widget);
        wLayout->setContentsMargins(8, 4, 8, 4);
        wLayout->setSpacing(2);
        auto* topRow = new QHBoxLayout;
        topRow->setSpacing(6);
        auto* convLabel = new ElaText(m.conversationTitle, widget);
        convLabel->setObjectName(QStringLiteral("resultName"));
        topRow->addWidget(convLabel, 1);
        const QDateTime ts = QDateTime::fromMSecsSinceEpoch(m.createdAtMs);
        auto* timeLabel = new ElaText(ts.toString(QStringLiteral("MM-dd HH:mm")), widget);
        timeLabel->setObjectName(QStringLiteral("resultDetail"));
        topRow->addWidget(timeLabel);
        wLayout->addLayout(topRow);

        auto* senderLabel = new ElaText(m.senderName, widget);
        senderLabel->setObjectName(QStringLiteral("resultDetail"));
        wLayout->addWidget(senderLabel);

        auto* bodyLabel = new ElaText(highlightKeyword(m.bodyPreview), widget);
        bodyLabel->setTextFormat(Qt::RichText);
        bodyLabel->setObjectName(QStringLiteral("resultBody"));
        bodyLabel->setWordWrap(true);
        bodyLabel->setMaximumHeight(36);
        wLayout->addWidget(bodyLabel);

        auto* item = new QListWidgetItem(m_resultList);
        item->setData(kRoleKind, QStringLiteral("message"));
        item->setData(kRoleId, m.conversationId);
        item->setData(kRoleId2, m.messageId);
        item->setSizeHint(QSize(0, 64));
        m_resultList->setItemWidget(item, widget);
    };

    const auto addFileItem = [this](const FileResult& f) {
        auto* widget = new QWidget;
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(10);
        auto* icon = new ElaText(QStringLiteral("\U0001F4CE"), widget);
        icon->setFixedSize(24, 24);
        icon->setAlignment(Qt::AlignCenter);
        layout->addWidget(icon);
        auto* infoLayout = new QVBoxLayout;
        infoLayout->setSpacing(1);
        auto* nameLabel = new ElaText(highlightKeyword(f.fileName), widget);
        nameLabel->setTextFormat(Qt::RichText);
        nameLabel->setObjectName(QStringLiteral("resultName"));
        infoLayout->addWidget(nameLabel);
        const QDateTime ts = QDateTime::fromMSecsSinceEpoch(f.createdAtMs);
        auto* detail = new ElaText(
            QStringLiteral("%1 \u00B7 %2").arg(f.peerName, ts.toString(QStringLiteral("MM-dd"))), widget);
        detail->setObjectName(QStringLiteral("resultDetail"));
        infoLayout->addWidget(detail);
        layout->addLayout(infoLayout, 1);

        auto* item = new QListWidgetItem(m_resultList);
        item->setData(kRoleKind, QStringLiteral("file"));
        item->setData(kRoleId, f.taskId);
        item->setSizeHint(QSize(0, 40));
        m_resultList->setItemWidget(item, widget);
    };

    const auto addDeptItem = [this](const DepartmentResult& d) {
        auto* widget = new QWidget;
        auto* layout = new QHBoxLayout(widget);
        layout->setContentsMargins(8, 4, 8, 4);
        layout->setSpacing(10);
        auto* icon = new ElaText(QStringLiteral("\U0001F3E2"), widget);
        icon->setFixedSize(24, 24);
        icon->setAlignment(Qt::AlignCenter);
        layout->addWidget(icon);
        auto* nameLabel = new ElaText(highlightKeyword(d.department), widget);
        nameLabel->setTextFormat(Qt::RichText);
        nameLabel->setObjectName(QStringLiteral("resultName"));
        layout->addWidget(nameLabel, 1);
        auto* countLabel = new ElaText(QStringLiteral("%1 \u4EBA").arg(d.memberCount), widget);
        countLabel->setObjectName(QStringLiteral("resultDetail"));
        layout->addWidget(countLabel);

        auto* item = new QListWidgetItem(m_resultList);
        item->setData(kRoleKind, QStringLiteral("department"));
        item->setData(kRoleId, d.department);
        item->setSizeHint(QSize(0, 36));
        m_resultList->setItemWidget(item, widget);
    };

    // 根据当前 Tab 决定显示哪些结果
    if (m_currentTab == Tab::All) {
        // 综合：每类最多 3 条
        if (!m_contacts.isEmpty()) {
            addSection(QStringLiteral("\U0001F464 \u8054\u7CFB\u4EBA"));
            const int n = qMin(3, m_contacts.size());
            for (int i = 0; i < n; ++i) addContactItem(m_contacts[i]);
        }
        if (!m_groups.isEmpty()) {
            addSection(QStringLiteral("\U0001F465 \u7FA4\u7EC4"));
            const int n = qMin(3, m_groups.size());
            for (int i = 0; i < n; ++i) addGroupItem(m_groups[i]);
        }
        if (!m_messages.isEmpty()) {
            addSection(QStringLiteral("\U0001F4AC \u804A\u5929\u8BB0\u5F55"));
            const int n = qMin(5, m_messages.size());
            for (int i = 0; i < n; ++i) addMessageItem(m_messages[i]);
        }
        if (!m_files.isEmpty()) {
            addSection(QStringLiteral("\U0001F4CE \u6587\u4EF6"));
            const int n = qMin(3, m_files.size());
            for (int i = 0; i < n; ++i) addFileItem(m_files[i]);
        }
        if (!m_departments.isEmpty()) {
            addSection(QStringLiteral("\U0001F3E2 \u90E8\u95E8"));
            const int n = qMin(3, m_departments.size());
            for (int i = 0; i < n; ++i) addDeptItem(m_departments[i]);
        }
    } else if (m_currentTab == Tab::Contact) {
        if (!m_contacts.isEmpty()) {
            addSection(QStringLiteral("\U0001F464 \u8054\u7CFB\u4EBA\uFF08%1\uFF09").arg(m_contacts.size()));
            for (const auto& c : m_contacts) addContactItem(c);
        }
    } else if (m_currentTab == Tab::Group) {
        if (!m_groups.isEmpty()) {
            addSection(QStringLiteral("\U0001F465 \u7FA4\u7EC4\uFF08%1\uFF09").arg(m_groups.size()));
            for (const auto& g : m_groups) addGroupItem(g);
        }
    } else if (m_currentTab == Tab::ChatHistory) {
        if (!m_messages.isEmpty()) {
            addSection(QStringLiteral("\U0001F4AC \u804A\u5929\u8BB0\u5F55\uFF08%1\uFF09").arg(m_messages.size()));
            for (const auto& msg : m_messages) addMessageItem(msg);
        }
    } else if (m_currentTab == Tab::File) {
        if (!m_files.isEmpty()) {
            addSection(QStringLiteral("\U0001F4CE \u6587\u4EF6\uFF08%1\uFF09").arg(m_files.size()));
            for (const auto& f : m_files) addFileItem(f);
        }
    } else if (m_currentTab == Tab::Department) {
        if (!m_departments.isEmpty()) {
            addSection(QStringLiteral("\U0001F3E2 \u90E8\u95E8\uFF08%1\uFF09").arg(m_departments.size()));
            for (const auto& d : m_departments) addDeptItem(d);
        }
    }

    // 无结果
    if (m_resultList->count() == 0) {
        auto* item = new QListWidgetItem(m_resultList);
        item->setFlags(Qt::NoItemFlags);
        auto* empty = new ElaText(QStringLiteral("\u672A\u627E\u5230\u5339\u914D\u7ED3\u679C"), m_resultList);
        empty->setAlignment(Qt::AlignCenter);
        empty->setObjectName(QStringLiteral("resultDetail"));
        item->setSizeHint(QSize(0, 48));
        m_resultList->setItemWidget(item, empty);
    }

    adjustSize();
}

void GlobalSearchPanel::setResults(const QVector<ContactResult>& contacts,
                                   const QVector<GroupResult>& groups,
                                   const QVector<MessageResult>& messages,
                                   const QVector<FileResult>& files,
                                   const QVector<DepartmentResult>& departments)
{
    m_contacts = contacts;
    m_groups = groups;
    m_messages = messages;
    m_files = files;
    m_departments = departments;
    showSearchResults();
}

void GlobalSearchPanel::onSearchTextChanged(const QString& text)
{
    m_currentKeyword = text.trimmed();
    if (m_currentKeyword.isEmpty()) {
        m_debounceTimer->stop();
        showEmptyState();
    } else {
        m_debounceTimer->start();
    }
}

void GlobalSearchPanel::onItemClicked(QListWidgetItem* item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled)) return;

    const QString kind = item->data(kRoleKind).toString();
    const QString id = item->data(kRoleId).toString();
    const QString title = item->data(kRoleTitle).toString();

    // 记录搜索历史和常用
    if (m_history && !m_currentKeyword.isEmpty()) {
        m_history->addKeyword(m_currentKeyword);
    }

    if (kind == QLatin1String("contact")) {
        if (m_history) m_history->recordContactHit(id, title, 0);
        emit contactActivated(id, title);
    } else if (kind == QLatin1String("group")) {
        if (m_history) m_history->recordContactHit(id, title, 1);
        emit groupActivated(id, title);
    } else if (kind == QLatin1String("message")) {
        const QString msgId = item->data(kRoleId2).toString();
        emit messageActivated(id, msgId);
    } else if (kind == QLatin1String("file")) {
        emit fileActivated(id);
    } else if (kind == QLatin1String("department")) {
        emit departmentActivated(id);
    }

    dismiss();
}

void GlobalSearchPanel::animateIndicator(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= m_tabLabels.size()) return;
    auto* targetLabel = m_tabLabels[tabIndex];
    const int x = targetLabel->x() + (targetLabel->width() - m_tabIndicator->width()) / 2;
    const int y = kTabHeight - kIndicatorHeight;
    const QRect target(x, y, m_tabIndicator->width(), kIndicatorHeight);

    if (m_indicatorAnim->state() == QAbstractAnimation::Running) {
        m_indicatorAnim->stop();
    }
    m_indicatorAnim->setStartValue(m_tabIndicator->geometry());
    m_indicatorAnim->setEndValue(target);
    m_indicatorAnim->start();
}

void GlobalSearchPanel::focusSearchEdit(bool selectText)
{
    if (!isVisible() || !m_searchEdit) {
        return;
    }

    raise();
    activateWindow();
    m_searchEdit->setFocus(Qt::MouseFocusReason);
    if (selectText && !m_searchEdit->text().isEmpty()) {
        m_searchEdit->selectAll();
    } else if (!m_searchEdit->hasSelectedText()) {
        m_searchEdit->setCursorPosition(m_searchEdit->text().size());
    }
    if (auto* inputMethod = QGuiApplication::inputMethod()) {
        inputMethod->update(Qt::ImCursorRectangle | Qt::ImCursorPosition);
        inputMethod->show();
    }
    m_searchEdit->update();
}

QString GlobalSearchPanel::highlightKeyword(const QString& text) const
{
    if (m_currentKeyword.isEmpty()) return text.toHtmlEscaped();
    QString html;
    int pos = 0;
    const QString lower = text.toLower();
    const QString keyLower = m_currentKeyword.toLower();
    while (pos < text.size()) {
        const int idx = lower.indexOf(keyLower, pos);
        if (idx < 0) {
            html += text.mid(pos).toHtmlEscaped();
            break;
        }
        html += text.mid(pos, idx - pos).toHtmlEscaped();
        html += QStringLiteral("<span style=\"color:#E8700A;font-weight:700;\">")
              + text.mid(idx, m_currentKeyword.size()).toHtmlEscaped()
              + QStringLiteral("</span>");
        pos = idx + m_currentKeyword.size();
    }
    return html;
}

bool GlobalSearchPanel::event(QEvent* e)
{
    if (e->type() == QEvent::Show || e->type() == QEvent::WindowActivate) {
        QTimer::singleShot(0, this, [this]() { focusSearchEdit(); });
    }
    if (e->type() == QEvent::WindowDeactivate) {
        // Qt::Tool 不会自动关闭，手动处理点击外部关闭
        // 延迟检查：避免 IME 候选窗口弹出时误关闭
        QTimer::singleShot(0, this, [this]() {
            if (!isActiveWindow()) {
                dismiss();
            }
        });
        return true;
    }
    if (e->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape) {
            dismiss();
            return true;
        }
        if (ke->key() == Qt::Key_Down || ke->key() == Qt::Key_Up) {
            if (m_resultList->isVisible() && m_resultList->count() > 0) {
                int current = m_resultList->currentRow();
                const int step = (ke->key() == Qt::Key_Down) ? 1 : -1;
                int next = current + step;
                while (next >= 0 && next < m_resultList->count()) {
                    auto* nextItem = m_resultList->item(next);
                    if (nextItem && (nextItem->flags() & Qt::ItemIsEnabled)) break;
                    next += step;
                }
                if (next >= 0 && next < m_resultList->count()) {
                    m_resultList->setCurrentRow(next);
                }
                return true;
            }
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (m_resultList->isVisible() && m_resultList->currentItem()
                && (m_resultList->currentItem()->flags() & Qt::ItemIsEnabled)) {
                onItemClicked(m_resultList->currentItem());
                return true;
            }
        }
        if (m_searchEdit && shouldRouteToSearchEdit(ke)) {
            focusSearchEdit();
            QKeyEvent forwarded(ke->type(),
                                ke->key(),
                                ke->modifiers(),
                                ke->nativeScanCode(),
                                ke->nativeVirtualKey(),
                                ke->nativeModifiers(),
                                ke->text(),
                                ke->isAutoRepeat(),
                                ke->count());
            QCoreApplication::sendEvent(m_searchEdit, &forwarded);
            return true;
        }
    }

    return QWidget::event(e);
}

bool GlobalSearchPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        // Tab 标签点击
        for (int i = 0; i < m_tabLabels.size(); ++i) {
            if (watched == m_tabLabels[i]) {
                switchTab(static_cast<Tab>(i));
                return true;
            }
        }

        // 搜索历史关键词 Tag 点击
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget && widget->objectName() == QLatin1String("keywordTag")) {
            auto* label = qobject_cast<ElaText*>(widget);
            if (label) {
                m_searchEdit->setText(label->text());
                // textChanged 会自动触发 debounce → searchRequested
            }
            return true;
        }

        // 常用联系人 Chip 点击
        if (widget && widget->objectName() == QLatin1String("frequentChip")) {
            const QString contactId = widget->property("contactId").toString();
            const QString contactTitle = widget->property("contactTitle").toString();
            const int kind = widget->property("contactKind").toInt();
            if (kind == 0) {
                emit contactActivated(contactId, contactTitle);
            } else {
                emit groupActivated(contactId, contactTitle);
            }
            dismiss();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void GlobalSearchPanel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool dark = AppStyle::isDarkTheme();
    const QColor bg = dark ? QColor(24, 30, 38, 230) : QColor(248, 250, 253, 235);
    const QColor border = dark ? QColor(60, 70, 85, 180) : QColor(200, 208, 218, 180);

    QPainterPath path;
    path.addRoundedRect(rect().adjusted(1, 1, -1, -1), 14, 14);
    painter.fillPath(path, bg);
    painter.setPen(QPen(border, 1));
    painter.drawPath(path);
}

void GlobalSearchPanel::applyTheme()
{
    const auto mode = AppStyle::currentThemeMode();
    const QString textColor = AppStyle::textPrimary(mode);
    const QString mutedColor = AppStyle::textSecondary(mode);
    const QString accentColor = AppStyle::accent(mode);
    const QString surfaceColor = AppStyle::surfaceMuted(mode);
    const QString hoverColor = AppStyle::hoverBg(mode);

    // Tab 标签样式
    for (int i = 0; i < m_tabLabels.size(); ++i) {
        m_tabLabels[i]->setStyleSheet(QStringLiteral(
            "font-size:13px; font-weight:%1; color:%2; background:transparent; padding:0 8px;")
            .arg(i == static_cast<int>(m_currentTab) ? QStringLiteral("700") : QStringLiteral("400"),
                 i == static_cast<int>(m_currentTab) ? textColor : mutedColor));
    }

    // 指示条颜色
    m_tabIndicator->setStyleSheet(QStringLiteral("background:%1; border-radius:1px;").arg(accentColor));

    // 搜索框
    m_searchEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background:%1; color:%2; border:1px solid %3; border-radius:10px; padding:0 12px; font-size:13px; }"
        "QLineEdit:focus { border:2px solid %4; background:%5; padding:0 11px; }")
        .arg(surfaceColor, textColor, AppStyle::border(mode), accentColor, AppStyle::surface(mode)));

    // 结果列表
    m_resultList->setStyleSheet(QStringLiteral(
        "QListWidget#globalSearchResultList { border:none; outline:none; background:transparent; }"
        "QListWidget#globalSearchResultList::item { padding:0; border-radius:8px; }"
        "QListWidget#globalSearchResultList::item:hover { background:%1; }"
        "QListWidget#globalSearchResultList::item:selected { background:%2; }")
        .arg(hoverColor, AppStyle::selectedBg(mode)));

    // Tag 样式
    const QString tagStyle = QStringLiteral(
        "ElaText#keywordTag { background:%1; color:%2; border-radius:12px;"
        " padding:4px 12px; font-size:12px; }"
        "ElaText#keywordTag:hover { background:%3; }")
        .arg(surfaceColor, textColor, hoverColor);

    // Section title
    const QString sectionStyle = QStringLiteral(
        "ElaText#sectionTitle { font-size:12px; font-weight:700; color:%1; background:transparent; }")
        .arg(mutedColor);

    // Result name / detail
    const QString resultNameStyle = QStringLiteral(
        "ElaText#resultName { font-size:13px; font-weight:600; color:%1; background:transparent; }")
        .arg(textColor);
    const QString resultDetailStyle = QStringLiteral(
        "ElaText#resultDetail { font-size:11px; color:%1; background:transparent; }")
        .arg(mutedColor);
    const QString resultBodyStyle = QStringLiteral(
        "ElaText#resultBody { font-size:12px; color:%1; background:transparent; }")
        .arg(textColor);
    const QString resultSectionStyle = QStringLiteral(
        "ElaText#resultSection { font-size:12px; font-weight:700; color:%1; padding:6px 8px 2px 8px; background:transparent; }")
        .arg(accentColor);

    // Avatar styles
    const QString avatarContactStyle = QStringLiteral(
        "ElaText#avatarContact { background:%1; color:white; border-radius:12px; font-size:13px; font-weight:700; }")
        .arg(accentColor);
    const QString avatarGroupStyle = QStringLiteral(
        "ElaText#avatarGroup { background:%1; color:white; border-radius:12px; font-size:12px; font-weight:700; }")
        .arg(AppStyle::success(mode));

    // Status
    const QString statusOnline = QStringLiteral(
        "ElaText#statusOnline { color:%1; font-size:16px; background:transparent; }").arg(AppStyle::success(mode));
    const QString statusOffline = QStringLiteral(
        "ElaText#statusOffline { color:%1; font-size:14px; background:transparent; }").arg(mutedColor);

    // Chip styles
    const QString chipStyle = QStringLiteral(
        "QWidget#frequentChip { background:%1; border-radius:16px; }"
        "QWidget#frequentChip:hover { background:%2; }")
        .arg(surfaceColor, hoverColor);
    const QString chipNameStyle = QStringLiteral(
        "ElaText#chipName { font-size:12px; color:%1; background:transparent; }").arg(textColor);

    // 合并设置
    setStyleSheet(tagStyle + sectionStyle + resultNameStyle + resultDetailStyle
                  + resultBodyStyle + resultSectionStyle
                  + avatarContactStyle + avatarGroupStyle
                  + statusOnline + statusOffline
                  + chipStyle + chipNameStyle);

    update();
}
