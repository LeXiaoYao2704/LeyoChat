#include "ui/GroupInfoPanel.h"

#include "ui/AppStyle.h"

#include <ElaListWidget.h>
#include <ElaPushButton.h>
#include <ElaText.h>

#include <algorithm>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <ElaFrame.h>
#include <ElaStackedWidget.h>
#include <QColor>
#include <QContextMenuEvent>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStackedWidget>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>
#include <functional>
#include <utility>

namespace {
constexpr int kMemberClientIdRole = Qt::UserRole + 1;

QColor memberAvatarColor(const QString& key, bool dark)
{
    const QVector<QColor> colors = dark
        ? QVector<QColor>{QColor(QStringLiteral("#4EA1FF")),
                          QColor(QStringLiteral("#A78BFA")),
                          QColor(QStringLiteral("#38BDF8")),
                          QColor(QStringLiteral("#34D399")),
                          QColor(QStringLiteral("#FDBA74")),
                          QColor(QStringLiteral("#F472B6"))}
        : QVector<QColor>{QColor(QStringLiteral("#0078D4")),
                          QColor(QStringLiteral("#7C3AED")),
                          QColor(QStringLiteral("#0284C7")),
                          QColor(QStringLiteral("#059669")),
                          QColor(QStringLiteral("#EA580C")),
                          QColor(QStringLiteral("#DB2777"))};
    return colors.at(static_cast<int>(qHash(key) % colors.size()));
}

class MemberAvatarWidget final : public QWidget
{
public:
    MemberAvatarWidget(QString key,
                       QString displayText,
                       QString avatarImagePath,
                       bool online,
                       QWidget* parent = nullptr)
        : QWidget(parent),
          m_key(std::move(key)),
          m_displayText(std::move(displayText)),
          m_online(online)
    {
        setFixedSize(34, 34);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setCursor(Qt::PointingHandCursor);
        const QFileInfo avatarInfo(avatarImagePath);
        if (avatarInfo.exists() && avatarInfo.isFile()) {
            QPixmap raw(avatarImagePath);
            if (!raw.isNull()) {
                m_avatarPixmap = raw;
            }
        }
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        const bool dark = eTheme && eTheme->getThemeMode() == ElaThemeType::Dark;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF avatarRect = rect().adjusted(1, 1, -1, -1);

        QPainterPath clip;
        clip.addEllipse(avatarRect);
        if (!m_avatarPixmap.isNull()) {
            const QPixmap scaled = m_avatarPixmap.scaled(size(),
                                                         Qt::KeepAspectRatioByExpanding,
                                                         Qt::SmoothTransformation);
            painter.save();
            painter.setClipPath(clip);
            if (!m_online) {
                painter.setOpacity(0.42);
            }
            painter.drawPixmap(rect(), scaled, scaled.rect());
            painter.restore();
            if (!m_online) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(dark ? QColor(31, 41, 55, 145)
                                      : QColor(243, 244, 246, 165));
                painter.drawEllipse(avatarRect);
            }
        } else {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_online ? memberAvatarColor(m_key, dark)
                                      : (dark ? QColor(QStringLiteral("#4B5563"))
                                              : QColor(QStringLiteral("#E5E7EB"))));
            painter.drawEllipse(avatarRect);

            QFont textFont = font();
            textFont.setPixelSize(14);
            textFont.setWeight(QFont::DemiBold);
            painter.setFont(textFont);
            painter.setPen(m_online ? QColor(QStringLiteral("#FFFFFF"))
                                    : (dark ? QColor(QStringLiteral("#D1D5DB"))
                                            : QColor(QStringLiteral("#6B7280"))));
            painter.drawText(rect(), Qt::AlignCenter, m_displayText.left(1).toUpper());
        }
    }

private:
    QString m_key;
    QString m_displayText;
    QPixmap m_avatarPixmap;
    bool m_online = false;
};

class GroupMemberRowWidget final : public QWidget
{
public:
    explicit GroupMemberRowWidget(QString clientId, QWidget* parent = nullptr)
        : QWidget(parent),
          m_clientId(std::move(clientId))
    {
        setObjectName(QStringLiteral("groupMemberRow"));
        setAttribute(Qt::WA_StyledBackground, true);
        setAttribute(Qt::WA_Hover, true);
        setMouseTracking(true);
    }

    void setAvatarWidget(QWidget* avatar)
    {
        if (m_avatar == avatar) {
            return;
        }
        if (m_avatar) {
            m_avatar->removeEventFilter(this);
        }
        m_avatar = avatar;
        if (m_avatar) {
            m_avatar->installEventFilter(this);
        }
    }

    void setActivateCallback(std::function<void(const QString&)> callback)
    {
        m_activateCallback = std::move(callback);
    }

    void setAvatarHoverCallback(std::function<void(const QString&, const QPoint&)> callback)
    {
        m_avatarHoverCallback = std::move(callback);
    }

    void setAvatarLeaveCallback(std::function<void()> callback)
    {
        m_avatarLeaveCallback = std::move(callback);
    }

    void setContextMenuCallback(std::function<void(const QString&, const QPoint&)> callback)
    {
        m_contextMenuCallback = std::move(callback);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_avatar && event) {
            if (event->type() == QEvent::Enter) {
                if (!m_clientId.isEmpty() && m_avatarHoverCallback) {
                    const QPoint globalPos =
                        m_avatar->mapToGlobal(QPoint(m_avatar->width() + 8, 0));
                    m_avatarHoverCallback(m_clientId, globalPos);
                }
            } else if (event->type() == QEvent::Leave) {
                if (m_avatarLeaveCallback) {
                    m_avatarLeaveCallback();
                }
            } else if (event->type() == QEvent::MouseButtonDblClick) {
                auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (mouseEvent->button() == Qt::LeftButton) {
                    activateMember();
                    mouseEvent->accept();
                    return true;
                }
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        if (event && event->button() == Qt::LeftButton) {
            activateMember();
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (event && !m_clientId.isEmpty() && m_contextMenuCallback) {
            m_contextMenuCallback(m_clientId, event->globalPos());
            event->accept();
            return;
        }
        QWidget::contextMenuEvent(event);
    }

private:
    void activateMember()
    {
        if (!m_clientId.isEmpty() && m_activateCallback) {
            m_activateCallback(m_clientId);
        }
    }

    QString m_clientId;
    QWidget* m_avatar = nullptr;
    std::function<void(const QString&)> m_activateCallback;
    std::function<void(const QString&, const QPoint&)> m_avatarHoverCallback;
    std::function<void()> m_avatarLeaveCallback;
    std::function<void(const QString&, const QPoint&)> m_contextMenuCallback;
};

ElaText* makeSectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new ElaText(text, parent);
    label->setObjectName(QStringLiteral("groupSectionTitle"));
    return label;
}

QFrame* makeDivider(QWidget* parent)
{
    auto* divider = new ElaFrame(parent);
    divider->setFrameShape(QFrame::NoFrame);
    divider->setFixedHeight(1);
    divider->setStyleSheet(QStringLiteral("background:transparent;"));
    return divider;
}

QString memberRoleText(const GroupMemberListEntry& member)
{
    QStringList roles;
    if (member.isOwner) {
        roles.append(QStringLiteral("\u7FA4\u4E3B"));
    } else if (member.isAdmin) {
        roles.append(QStringLiteral("\u7BA1\u7406\u5458"));
    } else {
        roles.append(QStringLiteral("\u7FA4\u6210\u5458"));
    }
    if (member.isSelf) {
        roles.append(QStringLiteral("\u6211"));
    }
    return roles.join(QStringLiteral(" \u00B7 "));
}

QWidget* makeMemberRow(const GroupMemberListEntry& member,
                       QWidget* parent,
                       std::function<void(const QString&)> activateMember,
                       std::function<void(const QString&, const QPoint&)> avatarHovered,
                       std::function<void()> avatarHoverLeft,
                       std::function<void(const QString&, const QPoint&)> contextMenuRequested)
{
    auto* row = new GroupMemberRowWidget(member.clientId.trimmed(), parent);
    row->setActivateCallback(std::move(activateMember));
    row->setAvatarHoverCallback(std::move(avatarHovered));
    row->setAvatarLeaveCallback(std::move(avatarHoverLeft));
    row->setContextMenuCallback(std::move(contextMenuRequested));
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(2, 4, 2, 4);
    layout->setSpacing(10);

    const QString trimmedName = member.displayName.trimmed().isEmpty()
                                    ? QStringLiteral("\u672A\u547D\u540D\u6210\u5458")
                                    : member.displayName.trimmed();

    auto* avatar = new MemberAvatarWidget(
        member.clientId.trimmed().isEmpty() ? trimmedName : member.clientId.trimmed(),
        trimmedName,
        member.avatarImagePath,
        member.isOnline,
        row);
    avatar->setObjectName(QStringLiteral("groupMemberAvatar"));
    row->setAvatarWidget(avatar);
    avatar->setToolTip(QString());

    auto* textLayout = new QVBoxLayout;
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(1);

    auto* nameLabel = new ElaText(trimmedName, row);
    nameLabel->setObjectName(QStringLiteral("groupMemberName"));
    nameLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto* metaLabel = new ElaText(memberRoleText(member), row);
    metaLabel->setObjectName(QStringLiteral("groupMemberMeta"));
    metaLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    const bool dark = eTheme && eTheme->getThemeMode() == ElaThemeType::Dark;
    const AppStyle::ThemeMode mode = dark ? AppStyle::ThemeMode::Dark
                                          : AppStyle::ThemeMode::Light;
    const QString rowHoverBg = dark ? QStringLiteral("#333A44")
                                    : QStringLiteral("#EEF5FF");
    const QString nameColor = member.isOnline
                                  ? AppStyle::accent(mode)
                                  : (dark ? QStringLiteral("#8A8F98")
                                          : QStringLiteral("#8A94A3"));
    const QString metaColor = member.isOnline
                                  ? AppStyle::textPrimary(mode)
                                  : (dark ? QStringLiteral("#6B7280")
                                          : QStringLiteral("#9CA3AF"));
    row->setStyleSheet(QStringLiteral(
                           "QWidget#groupMemberRow {"
                           "  background:transparent;"
                           "  border:none;"
                           "}"
                           "QWidget#groupMemberRow:hover {"
                           "  background:%1;"
                           "  border-radius:10px;"
                           "}").arg(rowHoverBg));
    nameLabel->setStyleSheet(QStringLiteral(
                                 "QLabel {"
                                 "  background:transparent;"
                                 "  border:none;"
                                 "  color:%1;"
                                 "  font-size:13px;"
                                 "  font-weight:650;"
                                 "}").arg(nameColor));
    metaLabel->setStyleSheet(QStringLiteral(
                                 "QLabel {"
                                 "  background:transparent;"
                                 "  border:none;"
                                 "  color:%1;"
                                 "  font-size:11px;"
                                 "}").arg(metaColor));

    textLayout->addWidget(nameLabel);
    textLayout->addWidget(metaLabel);

    layout->addWidget(avatar, 0, Qt::AlignTop);
    layout->addLayout(textLayout, 1);

    // 在线状态小圆点
    if (member.isOnline && row->property("_showLegacyOnlineDot").toBool()) {
        auto* onlineDot = new ElaText(row);
        onlineDot->setObjectName(QStringLiteral("groupMemberOnlineDot"));
        onlineDot->setFixedSize(8, 8);
        onlineDot->raise();
        // 放在头像右下角
        onlineDot->move(avatar->x() + 22, avatar->y() + 22);
    }

    return row;
}

} // namespace

GroupInfoPanel::GroupInfoPanel(QWidget* parent)
    : QWidget(parent)
{
    const QFont baseFont = font();
    const int panelWidth = qMax(300, QFontMetrics(AppStyle::bodyFont(baseFont))
                                         .horizontalAdvance(QStringLiteral("\u7FA4\u516C\u544A\u4E0E\u6210\u5458"))
                                     + 170);

    setObjectName(QStringLiteral("groupInfoPanel"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(false);
    setMinimumWidth(0);
    setMaximumWidth(420);
    resize(panelWidth, height());
    refreshTheme();

    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]() { refreshTheme(); });

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Content area
    m_stackedWidget = new ElaStackedWidget(this);
    m_stackedWidget->setObjectName(QStringLiteral("groupInfoStackedWidget"));
    m_stackedWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_stackedWidget->setAutoFillBackground(false);
    m_detailPage = buildDetailPage();
    m_stackedWidget->addWidget(m_detailPage);
    root->addWidget(m_stackedWidget);
}

void GroupInfoPanel::switchToTab(int index)
{
    if (!m_stackedWidget || m_stackedWidget->count() <= 0) {
        return;
    }
    m_stackedWidget->setCurrentIndex(qBound(0, index, m_stackedWidget->count() - 1));
}

void GroupInfoPanel::showDetailView()
{
    switchToTab(0);
}

void GroupInfoPanel::showFileServiceSettingsView()
{
    showDetailView();
}

bool GroupInfoPanel::isShowingFileServiceSettingsView() const
{
    return false;
}

QWidget* GroupInfoPanel::buildDetailPage()
{
    auto* page = new QWidget;
    page->setObjectName(QStringLiteral("groupInfoDetailPage"));
    page->setAttribute(Qt::WA_StyledBackground, true);
    page->setAutoFillBackground(false);

    auto* root = new QVBoxLayout(page);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(12);

    auto* topBand = new ElaFrame(page);
    topBand->setObjectName(QStringLiteral("groupInfoTopBand"));
    topBand->setFrameShape(QFrame::StyledPanel);
    topBand->setLineWidth(0);
    auto* topBandLayout = new QHBoxLayout(topBand);
    topBandLayout->setContentsMargins(12, 8, 12, 8);
    topBandLayout->setSpacing(8);

    m_modeChipLabel = new ElaText(QStringLiteral("\u7FA4\u8BE6\u60C5"), topBand);
    m_modeChipLabel->setObjectName(QStringLiteral("groupInfoModeChip"));
    m_consoleChipLabel = new ElaText(QStringLiteral("\u534F\u4F5C\u63A7\u5236\u53F0"), topBand);
    m_consoleChipLabel->setObjectName(QStringLiteral("groupInfoConsoleChip"));
    m_topHintLabel = new ElaText(QStringLiteral("\u4E0A\u4E0B\u6587\u5DF2\u540C\u6B65"), topBand);
    m_topHintLabel->setObjectName(QStringLiteral("groupInfoTopHint"));

    topBandLayout->addWidget(m_modeChipLabel);
    topBandLayout->addWidget(m_consoleChipLabel);
    topBandLayout->addStretch();
    topBandLayout->addWidget(m_topHintLabel);

    auto* overviewSection = new ElaFrame(page);
    overviewSection->setObjectName(QStringLiteral("groupOverviewSection"));
    auto* overviewLayout = new QVBoxLayout(overviewSection);
    overviewLayout->setContentsMargins(0, 0, 0, 0);
    overviewLayout->setSpacing(12);
    overviewLayout->addWidget(topBand);

    m_groupAvatarLabel = new ElaText(QStringLiteral("H"), overviewSection);
    m_groupAvatarLabel->setObjectName(QStringLiteral("groupInfoAvatar"));
    m_groupAvatarLabel->setVisible(false);

    m_groupTitleLabel = new ElaText(QStringLiteral("\u672A\u547D\u540D\u7FA4\u804A"), overviewSection);
    m_groupTitleLabel->setObjectName(QStringLiteral("groupInfoTitle"));
    m_groupTitleLabel->setVisible(false);
    m_groupSubtitleLabel = new ElaText(
        QStringLiteral("\u7FA4\u516C\u544A\u3001\u5171\u4EAB\u6587\u4EF6\u4E0E\u6210\u5458\u6982\u89C8"),
        overviewSection);
    m_groupSubtitleLabel->setObjectName(QStringLiteral("groupInfoSubtitle"));
    m_groupSubtitleLabel->setVisible(false);

    m_memberCountLabel = new ElaText(QStringLiteral("0 \u4F4D\u6210\u5458"), overviewSection);
    m_memberCountLabel->setObjectName(QStringLiteral("groupInfoChip"));
    m_memberCountLabel->setVisible(false);

    auto* overviewSummaryRow = new QHBoxLayout;
    overviewSummaryRow->setContentsMargins(0, 0, 0, 0);
    overviewSummaryRow->setSpacing(10);
    auto* overviewTextLayout = new QVBoxLayout;
    overviewTextLayout->setContentsMargins(0, 0, 0, 0);
    overviewTextLayout->setSpacing(4);
    overviewTextLayout->addWidget(m_groupTitleLabel);
    overviewTextLayout->addWidget(m_groupSubtitleLabel);

    overviewSummaryRow->addWidget(m_groupAvatarLabel, 0, Qt::AlignTop);
    overviewSummaryRow->addLayout(overviewTextLayout, 1);
    overviewSummaryRow->addWidget(m_memberCountLabel, 0, Qt::AlignTop);
    overviewLayout->addLayout(overviewSummaryRow);

    auto* filesSection = new ElaFrame(page);
    filesSection->setObjectName(QStringLiteral("groupFilesSection"));
    auto* filesSectionLayout = new QVBoxLayout(filesSection);
    filesSectionLayout->setContentsMargins(0, 0, 0, 0);
    filesSectionLayout->setSpacing(8);

    auto* sharedFilesTitle = makeSectionTitle(QStringLiteral("\u5171\u4EAB\u6587\u4EF6"), filesSection);

    auto* sharedFilesCard = new ElaFrame(filesSection);
    m_sharedFilesCard = sharedFilesCard;
    sharedFilesCard->setObjectName(QStringLiteral("groupSharedFilesCard"));
    sharedFilesCard->setFrameShape(QFrame::StyledPanel);
    sharedFilesCard->setLineWidth(0);
    sharedFilesCard->setCursor(Qt::PointingHandCursor);
    sharedFilesCard->installEventFilter(this);
    auto* sharedFilesLayout = new QHBoxLayout(sharedFilesCard);
    sharedFilesLayout->setContentsMargins(14, 10, 14, 10);
    sharedFilesLayout->setSpacing(8);
    m_runtimeChipLabel = new ElaText(QStringLiteral("\u5C1A\u672A\u5F00\u542F\u7FA4\u6587\u4EF6\u670D\u52A1"), sharedFilesCard);
    m_runtimeChipLabel->setObjectName(QStringLiteral("groupRuntimeChip"));
    m_runtimeDetailLabel = new ElaText(sharedFilesCard);
    m_runtimeDetailLabel->setObjectName(QStringLiteral("groupRuntimeDetail"));
    m_runtimeDetailLabel->setVisible(false);
    sharedFilesLayout->addWidget(m_runtimeChipLabel, 1);
    m_sharedFilesArrow = new ElaText(QStringLiteral("\u203A"), sharedFilesCard);
    m_sharedFilesArrow->setObjectName(QStringLiteral("groupSharedFilesArrow"));
    m_sharedFilesArrow->setVisible(false);
    sharedFilesLayout->addWidget(m_sharedFilesArrow);
    sharedFilesCard->setCursor(Qt::ArrowCursor);
    filesSectionLayout->addWidget(sharedFilesTitle);
    filesSectionLayout->addWidget(sharedFilesCard);

    auto* announcementSection = new QWidget(page);
    announcementSection->setObjectName(QStringLiteral("groupAnnouncementSection"));
    announcementSection->setAttribute(Qt::WA_StyledBackground, true);
    announcementSection->setAutoFillBackground(false);
    auto* announcementSectionLayout = new QVBoxLayout(announcementSection);
    announcementSectionLayout->setContentsMargins(0, 0, 0, 0);
    announcementSectionLayout->setSpacing(8);

    auto* announcementTitleRow = new QHBoxLayout;
    announcementTitleRow->setContentsMargins(0, 0, 0, 0);
    announcementTitleRow->setSpacing(8);
    auto* announcementTitle = makeSectionTitle(QStringLiteral("\u7FA4\u516C\u544A"), announcementSection);
    m_announcementReminderButton = new ElaPushButton(QStringLiteral("提醒"), announcementSection);
    m_announcementReminderButton->setObjectName(QStringLiteral("groupAnnouncementReminderButton"));
    m_announcementReminderButton->setCursor(Qt::PointingHandCursor);
    m_announcementReminderButton->setToolTip(QStringLiteral("为群公告设置本机提醒"));
    connect(m_announcementReminderButton, &QAbstractButton::clicked, this, [this]() {
        const QString groupId = m_currentGroupId.trimmed();
        if (groupId.isEmpty()) {
            return;
        }
        emit groupAnnouncementReminderRequested(
            groupId,
            groupTitleText().trimmed(),
            announcementText().trimmed());
    });
    m_announcementEditButton = new QPushButton(QStringLiteral("\u7F16\u8F91"), announcementSection);
    m_announcementEditButton->setCursor(Qt::PointingHandCursor);
    m_announcementEditButton->setFlat(true);
    m_announcementEditButton->setStyleSheet(QStringLiteral(
        "QPushButton { color:%1; font-size:12px; border:none; background:transparent; padding:0 4px; }"
        "QPushButton:hover { color:%2; }")
        .arg(AppStyle::textSecondary(), AppStyle::accent()));
    m_announcementEditButton->hide();
    connect(m_announcementEditButton, &QPushButton::clicked, this, &GroupInfoPanel::announcementEditRequested);
    announcementTitleRow->addWidget(announcementTitle);
    announcementTitleRow->addStretch();
    announcementTitleRow->addWidget(m_announcementReminderButton);
    announcementTitleRow->addWidget(m_announcementEditButton);

    auto* announcementCard = new QFrame(announcementSection);
    announcementCard->setObjectName(QStringLiteral("announcementCard"));
    announcementCard->setFrameShape(QFrame::NoFrame);
    announcementCard->setAttribute(Qt::WA_StyledBackground, true);
    announcementCard->setLineWidth(0);
    auto* announcementLayout = new QVBoxLayout(announcementCard);
    announcementLayout->setContentsMargins(14, 14, 14, 14);
    announcementLayout->setSpacing(0);

    m_announcementTextLabel = new ElaText(QStringLiteral("\u6682\u65E0\u516C\u544A"), announcementCard);
    m_announcementTextLabel->setObjectName(QStringLiteral("announcementBody"));
    m_announcementTextLabel->setWordWrap(true);
    m_announcementTextLabel->setMaximumHeight(60);
    announcementLayout->addWidget(m_announcementTextLabel);
    announcementSectionLayout->addLayout(announcementTitleRow);
    announcementSectionLayout->addWidget(announcementCard);

    auto* membersSection = new QWidget(page);
    membersSection->setObjectName(QStringLiteral("groupMembersSection"));
    membersSection->setAttribute(Qt::WA_StyledBackground, true);
    membersSection->setAutoFillBackground(false);
    auto* membersSectionLayout = new QVBoxLayout(membersSection);
    membersSectionLayout->setContentsMargins(0, 0, 0, 0);
    membersSectionLayout->setSpacing(8);

    auto* membersRow = new QHBoxLayout;
    membersRow->setContentsMargins(0, 0, 0, 0);
    membersRow->setSpacing(8);
    auto* membersTitle = makeSectionTitle(QStringLiteral("\u6210\u5458"), membersSection);
    m_memberSectionLabel = new ElaText(QStringLiteral("0"), membersSection);
    m_memberSectionLabel->setObjectName(QStringLiteral("groupMemberSectionCount"));
    membersRow->addWidget(membersTitle);
    membersRow->addStretch();
    membersRow->addWidget(m_memberSectionLabel);

    m_memberListWidget = new ElaListWidget(membersSection);
    m_memberListWidget->setObjectName(QStringLiteral("groupMembersList"));
    m_memberListWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_memberListWidget->setFrameShape(QFrame::NoFrame);
    m_memberListWidget->setResizeMode(QListView::Adjust);
    m_memberListWidget->setSpacing(2);
    m_memberListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    if (m_memberListWidget->viewport()) {
        m_memberListWidget->viewport()->setAutoFillBackground(false);
    }
    m_memberListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_membersCard = new QFrame(membersSection);
    m_membersCard->setObjectName(QStringLiteral("groupMembersCard"));
    m_membersCard->setFrameShape(QFrame::NoFrame);
    m_membersCard->setAttribute(Qt::WA_StyledBackground, true);
    m_membersCard->setLineWidth(0);
    auto* membersCardLayout = new QVBoxLayout(m_membersCard);
    membersCardLayout->setContentsMargins(14, 12, 14, 12);
    membersCardLayout->setSpacing(10);
    membersCardLayout->addLayout(membersRow);
    membersCardLayout->addWidget(m_memberListWidget);
    membersSectionLayout->addWidget(m_membersCard, 1);

    overviewSection->hide();
    filesSection->hide();
    root->addWidget(announcementSection);
    root->addWidget(membersSection, 1);

    connect(m_memberListWidget, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item) {
                if (!item) {
                    return;
                }
                emit memberActivated(item->data(kMemberClientIdRole).toString());
            });
    connect(m_memberListWidget, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& pos) {
                auto* item = m_memberListWidget->itemAt(pos);
                if (!item) {
                    return;
                }
                emit memberContextMenuRequested(item->data(kMemberClientIdRole).toString(),
                                                m_memberListWidget->viewport()->mapToGlobal(pos));
            });

    return page;
}

void GroupInfoPanel::refreshTheme()
{
    const bool dark = eTheme && eTheme->getThemeMode() == ElaThemeType::Dark;
    const AppStyle::ThemeMode mode = dark ? AppStyle::ThemeMode::Dark
                                          : AppStyle::ThemeMode::Light;
    const QString detailCardBg = dark ? QStringLiteral("rgba(45,45,45,0.78)") : QStringLiteral("rgba(255,255,255,0.76)");
    const QString detailHoverBg = dark ? QStringLiteral("#333A44") : QStringLiteral("#EEF5FF");
    const QString detailPanelBg = dark ? QStringLiteral("rgba(32,32,32,0.68)") : QStringLiteral("rgba(248,251,255,0.64)");

    setStyleSheet(QStringLiteral(
                      "QWidget#groupInfoPanel {"
                      "  background:%1;"
                      "}"
                      "QStackedWidget#groupInfoStackedWidget,"
                      "QWidget#groupInfoDetailPage,"
                      "QWidget#groupInfoSettingsPage,"
                      "QWidget#groupAnnouncementSection,"
                      "QWidget#groupMembersSection {"
                      "  background:transparent;"
                      "}"
                      "QFrame#announcementCard,"
                      "QFrame#groupMembersCard,"
                      "QFrame#groupFileServiceCard {"
                      "  background:%6;"
                      "  border:1px solid BORDER_COLOR;"
                      "  border-radius:14px;"
                      "}"
                      "QLabel#groupInfoAvatar {"
                      "  background:%2;"
                      "  color:%3;"
                      "  border-radius:24px;"
                      "  font-size:18px;"
                      "  font-weight:700;"
                      "  min-width:48px;"
                      "  min-height:48px;"
                      "}"
                      "QLabel#groupInfoTitle {"
                      "  font-size:16px;"
                      "  font-weight:700;"
                      "  color:%4;"
                      "}"
                      "QLabel#groupInfoSubtitle {"
                      "  font-size:12px;"
                      "  color:%5;"
                      "}"
                      "QLabel#groupInfoChip {"
                      "  background:%2;"
                      "  color:%3;"
                      "  border-radius:11px;"
                      "  font-size:12px;"
                      "  font-weight:600;"
                      "  padding:3px 10px;"
                      "}"
                      "QLabel#groupInfoModeChip {"
                      "  background:%8;"
                      "  color:%3;"
                      "  border:none;"
                      "  border-radius:999px;"
                      "  font-size:11px;"
                      "  font-weight:700;"
                      "  padding:4px 10px;"
                      "}"
                      "QLabel#groupInfoConsoleChip {"
                      "  background:%9;"
                      "  color:%7;"
                      "  border:none;"
                      "  border-radius:999px;"
                      "  font-size:11px;"
                      "  font-weight:600;"
                      "  padding:4px 10px;"
                      "}"
                      "QLabel#groupInfoTopHint {"
                      "  color:%5;"
                      "  font-size:11px;"
                      "  font-weight:600;"
                      "}"
                      "QLabel#groupSettingsFileSectionLabel {"
                      "  font-size:14px;"
                      "  font-weight:700;"
                      "  color:%4;"
                      "}"
                      "QLabel#groupSettingsHelperLabel {"
                      "  font-size:12px;"
                      "  color:%5;"
                      "  line-height:1.4;"
                      "}"
                      "QPushButton#groupSettingsBackButton {"
                      "  background:transparent;"
                      "  border:none;"
                      "  color:%7;"
                      "  font-size:12px;"
                      "  font-weight:600;"
                      "  padding:4px 0;"
                      "}"
                      "QPushButton#groupSettingsBackButton:hover {"
                      "  color:%3;"
                      "}"
                      "QLabel#announcementBody {"
                      "  font-size:13px;"
                      "  color:%4;"
                      "  line-height:1.4;"
                      "}"
                      "QLabel#groupRuntimeChip {"
                      "  background:%2;"
                      "  color:%3;"
                      "  border-radius:10px;"
                      "  font-size:11px;"
                      "  font-weight:700;"
                      "  padding:4px 10px;"
                      "}"
                      "QLabel#groupRuntimeDetail {"
                      "  font-size:12px;"
                      "  color:%5;"
                      "  line-height:1.4;"
                      "}"
                      "QLabel#entryChip, QLabel[groupEntryRole=\"chip\"] {"
                      "  background:%6;"
                      "  color:%7;"
                      "  border-radius:9px;"
                      "  font-size:12px;"
                      "  padding:4px 10px;"
                      "}"
                      "QListWidget {"
                      "  background:transparent;"
                      "  border:none;"
                      "  font-size:13px;"
                      "  color:%4;"
                      "  outline:none;"
                      "}"
                      "QListWidget::item {"
                      "  padding:0;"
                      "  margin:0;"
                      "  border:none;"
                      "  background:transparent;"
                      "}"
                      "QListWidget::item:hover {"
                      "  background:HOVER_BG;"
                      "}"
                      "QListWidget::item:selected,"
                      "QListWidget::item:selected:active {"
                      "  background:transparent;"
                      "  color:%4;"
                      "}"
                      "QWidget#groupMemberRow {"
                      "  background:transparent;"
                      "  border:none;"
                      "}"
                      "QWidget#groupMemberRow:hover {"
                      "  background:HOVER_BG;"
                      "  border-radius:10px;"
                      "}"
                      "QLabel#groupMemberAvatar {"
                      "  background:%2;"
                      "  color:%3;"
                      "  border:none;"
                      "  border-radius:17px;"
                      "  font-size:13px;"
                      "  font-weight:700;"
                      "}"
                      "QLabel#groupMemberName {"
                      "  background:transparent;"
                      "  border:none;"
                      "  color:%4;"
                      "  font-size:13px;"
                      "  font-weight:650;"
                      "}"
                      "QLabel#groupMemberMeta {"
                      "  background:transparent;"
                      "  border:none;"
                      "  color:%5;"
                      "  font-size:11px;"
                      "}"
                      "QLabel#groupMemberOnlineDot {"
                      "  background:SUCCESS_COLOR;"
                      "  border:1px solid %6;"
                      "  border-radius:4px;"
                      "}"
                      "QLabel#groupSectionTitle {"
                      "  font-size:12px;"
                      "  color:%5;"
                      "  font-weight:700;"
                      "  letter-spacing:0.5px;"
                      "}"
                      "QLabel#groupMemberSectionCount {"
                      "  font-size:12px;"
                      "  color:%5;"
                      "  font-weight:600;"
                      "}"
                      "QLabel#groupSharedFilesArrow {"
                      "  font-size:16px;"
                      "  color:%5;"
                      "}")
                .arg(detailPanelBg)                  // %1
                .arg(AppStyle::accentSoft(mode))      // %2
                .arg(AppStyle::accent(mode))          // %3
                .arg(AppStyle::textPrimary(mode))     // %4
                .arg(AppStyle::textMuted(mode))       // %5
                .arg(detailCardBg)                    // %6
                .arg(AppStyle::textSecondary(mode))   // %7
                .arg(AppStyle::surfaceMuted(mode))    // %8
                .arg(detailCardBg)                    // %9
                );
    // Replace custom placeholders after %1-%9 have been resolved
    {
        QString resolved = styleSheet();
        resolved.replace(QLatin1String("SUCCESS_COLOR"), AppStyle::success(mode));
        resolved.replace(QLatin1String("HOVER_BG"), detailHoverBg);
        resolved.replace(QLatin1String("SELECTED_BG"), AppStyle::selectedBg(mode));
        resolved.replace(QLatin1String("BORDER_COLOR"), AppStyle::border(mode));
        setStyleSheet(resolved);
    }
    const QString cardStyle = QStringLiteral(
        "QFrame#%1 {"
        "  background:%2;"
        "  border:1px solid %3;"
        "  border-radius:14px;"
        "}");
    if (auto* announcementCard = findChild<QFrame*>(QStringLiteral("announcementCard"))) {
        announcementCard->setAttribute(Qt::WA_StyledBackground, true);
        announcementCard->setAutoFillBackground(false);
        QPalette pal = announcementCard->palette();
        pal.setColor(QPalette::Window, QColor(detailCardBg));
        announcementCard->setPalette(pal);
        announcementCard->setStyleSheet(cardStyle.arg(QStringLiteral("announcementCard"),
                                                       detailCardBg,
                                                       AppStyle::border(mode)));
    }
    if (m_membersCard) {
        m_membersCard->setAttribute(Qt::WA_StyledBackground, true);
        m_membersCard->setAutoFillBackground(false);
        QPalette pal = m_membersCard->palette();
        pal.setColor(QPalette::Window, QColor(detailCardBg));
        m_membersCard->setPalette(pal);
        m_membersCard->setStyleSheet(cardStyle.arg(QStringLiteral("groupMembersCard"),
                                                   detailCardBg,
                                                   AppStyle::border(mode)));
    }
    if (m_memberListWidget) {
        m_memberListWidget->setStyleSheet(QStringLiteral(
            "QListWidget#groupMembersList {"
            "  background:transparent;"
            "  border:none;"
            "  outline:none;"
            "  color:%1;"
            "}"
            "QListWidget#groupMembersList::item {"
            "  background:transparent;"
            "  border:none;"
            "  padding:0;"
            "  margin:0;"
            "}").arg(AppStyle::textPrimary(mode)));
        if (m_memberListWidget->viewport()) {
            m_memberListWidget->viewport()->setAutoFillBackground(false);
            QPalette viewportPalette = m_memberListWidget->viewport()->palette();
            viewportPalette.setColor(QPalette::Base, QColor(Qt::transparent));
            viewportPalette.setColor(QPalette::Window, QColor(Qt::transparent));
            m_memberListWidget->viewport()->setPalette(viewportPalette);
            m_memberListWidget->viewport()->setStyleSheet(QStringLiteral("background:transparent;"));
        }
    }

    if (m_groupTitleLabel && m_announcementTextLabel) {
        setGroupSummary(m_groupTitleLabel->text(),
                        m_announcementTextLabel->text(),
                        m_memberEntries,
                        m_currentUserCanManageMembers);
    }

    // Note: settings-page form widgets (fsEnabledCheck, fsBaseUrlEdit, etc.) use
    // standard Qt palette and do not require explicit theme refresh.
}

void GroupInfoPanel::setGroupSummary(const QString& groupName,
                                     const QString& announcement,
                                     const GroupMemberListEntries& members,
                                     bool currentUserCanManageMembers)
{
    const QString title = groupName.trimmed().isEmpty()
                              ? QStringLiteral("\u672A\u547D\u540D\u7FA4\u804A")
                              : groupName.trimmed();
    const QString announceText = announcement.trimmed().isEmpty()
                                     ? QStringLiteral("\u6682\u65E0\u516C\u544A")
                                     : announcement.trimmed();

    if (m_groupTitleLabel
        && m_announcementTextLabel
        && m_groupTitleLabel->text() == title
        && m_announcementTextLabel->text() == announceText
        && m_memberEntries == members
        && m_currentUserCanManageMembers == currentUserCanManageMembers) {
        return;
    }

    m_memberEntries = members;
    m_currentUserCanManageMembers = currentUserCanManageMembers;
    m_announcementEditButton->setVisible(currentUserCanManageMembers);
    m_groupTitleLabel->setText(title);
    m_groupAvatarLabel->setText(title.left(1).toUpper());
    const int onlineCount = std::count_if(members.cbegin(), members.cend(),
                                          [](const GroupMemberListEntry& e) { return e.isOnline; });
    m_memberCountLabel->setText(QStringLiteral("%1 \u4F4D\u6210\u5458").arg(members.size()));
    m_memberSectionLabel->setText(QStringLiteral("%1 \u4EBA").arg(members.size()));
    m_announcementTextLabel->setText(announceText);
    m_consoleChipLabel->setText(members.isEmpty() ? QStringLiteral("\u534F\u4F5C\u63A7\u5236\u53F0")
                                                  : QStringLiteral("%1/%2 \u5728\u7EBF")
                                                        .arg(onlineCount)
                                                        .arg(members.size()));
    m_topHintLabel->setText(announcement.trimmed().isEmpty()
                                ? QStringLiteral("\u516C\u544A\u9762\u677F\u5F85\u66F4\u65B0")
                                : QStringLiteral("\u516C\u544A\u5DF2\u52A0\u8F7D"));

    m_memberListWidget->clear();
    for (const GroupMemberListEntry& member : members) {
        auto* item = new QListWidgetItem();
        item->setSizeHint(QSize(0, 44));
        item->setData(kMemberClientIdRole, member.clientId);
        m_memberListWidget->addItem(item);
        m_memberListWidget->setItemWidget(
            item,
            makeMemberRow(
                member,
                m_memberListWidget,
                [this](const QString& clientId) {
                    emit memberActivated(clientId);
                },
                [this](const QString& clientId, const QPoint& globalPos) {
                    emit memberAvatarHovered(clientId, globalPos);
                },
                [this]() {
                    emit memberAvatarHoverLeft();
                },
                [this](const QString& clientId, const QPoint& globalPos) {
                    emit memberContextMenuRequested(clientId, globalPos);
                }));
    }
}

void GroupInfoPanel::setHybridRuntimeSummary(const QString& badge, const QString& detail)
{
    const bool hasService = !badge.trimmed().isEmpty();
    m_fileServiceAvailable = hasService;
    if (m_runtimeChipLabel) {
        m_runtimeChipLabel->setText(hasService
                                        ? badge.trimmed()
                                        : QStringLiteral("\u5C1A\u672A\u5F00\u542F\u7FA4\u6587\u4EF6\u670D\u52A1"));
    }
    if (m_runtimeDetailLabel) {
        m_runtimeDetailLabel->setText(detail.trimmed());
        m_runtimeDetailLabel->setVisible(!detail.trimmed().isEmpty());
    }
    if (m_sharedFilesArrow) {
        m_sharedFilesArrow->setVisible(hasService);
    }
    if (m_sharedFilesCard) {
        m_sharedFilesCard->setCursor(hasService ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void GroupInfoPanel::setGroupId(const QString& groupId)
{
    m_currentGroupId = groupId;
}

bool GroupInfoPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (event && event->type() == QEvent::MouseButtonRelease) {
        auto* frame = qobject_cast<QFrame*>(watched);
        if (frame && frame->objectName() == QStringLiteral("groupSharedFilesCard")) {
            if (m_fileServiceAvailable) {
                emit sharedFilesClicked();
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void GroupInfoPanel::setGroupFileServiceConfig(const GroupFileServiceConfig& config, bool canEdit)
{
    Q_UNUSED(config);
    Q_UNUSED(canEdit);
}

QString GroupInfoPanel::groupTitleText() const
{
    return m_groupTitleLabel->text();
}

QString GroupInfoPanel::announcementText() const
{
    return m_announcementTextLabel->text();
}

int GroupInfoPanel::memberCount() const
{
    return m_memberListWidget->count();
}

QString GroupInfoPanel::memberDisplayName(const QString& clientId) const
{
    for (const GroupMemberListEntry& member : m_memberEntries) {
        if (member.clientId == clientId) {
            return member.displayName;
        }
    }
    return {};
}

void GroupInfoPanel::updateSyncStatus(const QString& text)
{
    Q_UNUSED(text);
}
