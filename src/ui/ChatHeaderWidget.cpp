#include "ui/ChatHeaderWidget.h"

#include "ui/AppStyle.h"
#include "ui/UiIcons.h"

#include <ElaIcon.h>
#include <ElaIconButton.h>
#include <ElaText.h>
#include <ElaToolButton.h>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFrame>
#include <ElaFrame.h>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QStackedWidget>
#include <ElaStackedWidget.h>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString avatarStyle(const QString& background)
{
    return QStringLiteral(
               "QLabel {"
               "  background:%1;"
               "  color:#FFFFFF;"
               "  border-radius:22px;"
               "  font-size:17px;"
               "  font-weight:700;"
               "}")
        .arg(background);
}

ElaToolButton* makeActionButton(ElaIconType::IconName icon, QWidget* parent, const QString& tooltip = QString())
{
    auto* button = new ElaToolButton(parent);
    button->setObjectName(QStringLiteral("HeaderElaToolButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(36, 36);
    button->setIconSize(QSize(16, 16));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setProperty("headerIcon", static_cast<int>(icon));
    button->setIcon(ElaIcon::getInstance()->getElaIcon(icon, 18, QColor(AppStyle::textPrimary())));
    button->setIsTransparent(true);
    button->setBorderRadius(8);
    button->setFocusPolicy(Qt::NoFocus);
    if (!tooltip.isEmpty()) button->setToolTip(tooltip);
    return button;
}

ElaToolButton* makeIconActionButton(ElaIconType::IconName icon, QWidget* parent, const QString& tooltip = QString())
{
    auto* button = new ElaToolButton(parent);
    button->setObjectName(QStringLiteral("HeaderElaToolButton"));
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(36, 36);
    button->setIconSize(QSize(16, 16));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setProperty("headerIcon", static_cast<int>(icon));
    button->setIcon(ElaIcon::getInstance()->getElaIcon(icon, 18, QColor(AppStyle::textPrimary())));
    button->setIsTransparent(true);
    button->setBorderRadius(8);
    button->setFocusPolicy(Qt::NoFocus);
    if (!tooltip.isEmpty()) button->setToolTip(tooltip);
    return button;
}

void applyAvatarLabel(QLabel* label,
                      const QString& avatarImagePath,
                      const QString& fallbackText,
                      const QString& fallbackStyle)
{
    if (!label) {
        return;
    }

    const QFileInfo info(avatarImagePath);
    if (info.exists() && info.isFile()) {
        QPixmap raw(avatarImagePath);
        if (!raw.isNull()) {
            const QSize targetSize = label->size();
            QPixmap scaled = raw.scaled(targetSize,
                                        Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
            QPixmap rounded(targetSize);
            rounded.fill(Qt::transparent);
            QPainter painter(&rounded);
            painter.setRenderHint(QPainter::Antialiasing);
            QPainterPath clipPath;
            clipPath.addEllipse(rounded.rect());
            painter.setClipPath(clipPath);
            painter.drawPixmap(0, 0, scaled);
            painter.end();
            label->setPixmap(rounded);
            label->setText(QString());
            return;
        }
    }

    label->setPixmap(QPixmap());
    label->setText(fallbackText);
    label->setStyleSheet(fallbackStyle);
}

} // namespace

ChatHeaderWidget::ChatHeaderWidget(QWidget* parent)
    : QWidget(parent)
{
    const QFont baseFont = font();
    const int avatarSize = AppStyle::avatarSizeForFont(baseFont);
    const int actionHeight = qMax(32, QFontMetrics(AppStyle::bodyFont(baseFont)).height() + 12);
    const int headerMinHeight = qMax(64, avatarSize + 12);

    setObjectName(QStringLiteral("chatHeaderWidget"));
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(headerMinHeight);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 8, 16, 10);
    root->setSpacing(8);

    m_controlBand = new ElaFrame(this);
    m_controlBand->setObjectName(QStringLiteral("headerControlBand"));
    m_controlBand->setFrameShape(QFrame::NoFrame);
    m_controlBand->hide();
    auto* bandLayout = new QHBoxLayout(m_controlBand);
    bandLayout->setContentsMargins(12, 8, 12, 8);
    bandLayout->setSpacing(8);

    m_modeChipLabel = new ElaText(QStringLiteral("直连会话"), m_controlBand);
    m_modeChipLabel->setObjectName(QStringLiteral("headerModeChip"));
    m_contextChipLabel = new ElaText(QStringLiteral("已连接"), m_controlBand);
    m_contextChipLabel->setObjectName(QStringLiteral("headerContextChip"));
    m_statusStrip = new ElaFrame(m_controlBand);
    m_statusStrip->setObjectName(QStringLiteral("headerStatusStrip"));
    m_statusStrip->setFrameShape(QFrame::NoFrame);
    auto* statusStripLayout = new QHBoxLayout(m_statusStrip);
    statusStripLayout->setContentsMargins(8, 4, 10, 4);
    statusStripLayout->setSpacing(6);
    auto* statusPulse = new ElaFrame(m_statusStrip);
    statusPulse->setObjectName(QStringLiteral("headerStatusPulse"));
    statusPulse->setFrameShape(QFrame::NoFrame);
    statusPulse->setFixedSize(8, 8);
    m_statusValueLabel = new ElaText(QStringLiteral("直连在线"), m_statusStrip);
    m_statusValueLabel->setObjectName(QStringLiteral("headerStatusValue"));
    statusStripLayout->addWidget(statusPulse, 0, Qt::AlignVCenter);
    statusStripLayout->addWidget(m_statusValueLabel);
    m_consoleHintLabel = new ElaText(QStringLiteral("会话控制台"), m_controlBand);
    m_consoleHintLabel->setObjectName(QStringLiteral("headerConsoleHint"));

    bandLayout->addWidget(m_modeChipLabel);
    bandLayout->addWidget(m_contextChipLabel);
    bandLayout->addWidget(m_statusStrip);
    bandLayout->addStretch();
    bandLayout->addWidget(m_consoleHintLabel);
    root->addWidget(m_controlBand);

    m_stack = new ElaStackedWidget(this);
    m_stack->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_stack);

    m_directPage = new QWidget(m_stack);
    auto* directLayout = new QHBoxLayout(m_directPage);
    directLayout->setContentsMargins(0, 0, 0, 0);
    directLayout->setSpacing(10);

    // --- 圆角信息卡片（类似 EchoChat 的对话对象框）---
    auto* directInfoCard = new ElaFrame(m_directPage);
    directInfoCard->setObjectName(QStringLiteral("headerInfoCard"));
    directInfoCard->setFrameShape(QFrame::NoFrame);
    directInfoCard->setAttribute(Qt::WA_StyledBackground, true);
    auto* directInfoCardLayout = new QHBoxLayout(directInfoCard);
    directInfoCardLayout->setContentsMargins(14, 10, 14, 10);
    directInfoCardLayout->setSpacing(8);

    m_directAvatarLabel = new ElaText(QStringLiteral("?"), directInfoCard);
    m_directAvatarLabel->setAlignment(Qt::AlignCenter);
    m_directAvatarLabel->setFixedSize(avatarSize, avatarSize);

    auto* directInfo = new QVBoxLayout;
    directInfo->setContentsMargins(0, 0, 0, 0);
    directInfo->setSpacing(2);

    m_directTitleLabel = new ElaText(directInfoCard);
    m_directSubtitleLabel = new ElaText(directInfoCard);
    m_directSecondaryLabel = new ElaText(directInfoCard);
    m_directSecondaryLabel->setVisible(false);

    directInfo->addWidget(m_directTitleLabel);
    directInfo->addWidget(m_directSubtitleLabel);
    directInfo->addWidget(m_directSecondaryLabel);

    directInfoCardLayout->addWidget(m_directAvatarLabel);
    directInfoCardLayout->addLayout(directInfo, 1);

    // --- 右侧图标按钮区（在卡片内部，匹配 EchoChat 布局）---
    m_directHistoryButton = makeActionButton(ElaIconType::MagnifyingGlass, directInfoCard, QStringLiteral("搜索聊天记录"));
    m_directHistoryButton->setFixedSize(36, 36);
    m_directVoiceCallButton = makeActionButton(ElaIconType::Phone, directInfoCard, QStringLiteral("通话"));
    m_directVoiceCallButton->setFixedSize(36, 36);
    m_directCreateGroupButton = makeActionButton(ElaIconType::UserGroup, directInfoCard, QStringLiteral("创建群聊"));
    m_directCreateGroupButton->setFixedSize(36, 36);
    m_directCloseButton = makeActionButton(ElaIconType::Xmark, directInfoCard, QStringLiteral("关闭当前会话"));
    m_directCloseButton->setFixedSize(36, 36);

    directInfoCardLayout->addWidget(m_directHistoryButton);
    directInfoCardLayout->addWidget(m_directVoiceCallButton);
    directInfoCardLayout->addWidget(m_directCreateGroupButton);
    directInfoCardLayout->addWidget(m_directCloseButton);

    directLayout->addWidget(directInfoCard, 1);

    m_groupPage = new QWidget(m_stack);
    auto* groupLayout = new QHBoxLayout(m_groupPage);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->setSpacing(10);

    // --- 群聊圆角信息卡片 ---
    auto* groupInfoCard = new ElaFrame(m_groupPage);
    groupInfoCard->setObjectName(QStringLiteral("headerInfoCard"));
    groupInfoCard->setFrameShape(QFrame::NoFrame);
    groupInfoCard->setAttribute(Qt::WA_StyledBackground, true);
    auto* groupInfoCardLayout = new QHBoxLayout(groupInfoCard);
    groupInfoCardLayout->setContentsMargins(14, 10, 14, 10);
    groupInfoCardLayout->setSpacing(8);

    m_groupAvatarLabel = new ElaText(UiIcons::navContacts(), groupInfoCard);
    m_groupAvatarLabel->setAlignment(Qt::AlignCenter);
    m_groupAvatarLabel->setFixedSize(avatarSize, avatarSize);

    auto* groupInfo = new QVBoxLayout;
    groupInfo->setContentsMargins(0, 0, 0, 0);
    groupInfo->setSpacing(2);

    m_groupTitleLabel = new ElaText(groupInfoCard);
    m_groupSubtitleLabel = new ElaText(groupInfoCard);
    groupInfo->addWidget(m_groupTitleLabel);
    groupInfo->addWidget(m_groupSubtitleLabel);

    groupInfoCardLayout->addWidget(m_groupAvatarLabel);
    groupInfoCardLayout->addLayout(groupInfo, 1);

    // --- 群聊右侧图标按钮（在卡片内部）---

    m_groupAnnouncementButton = makeActionButton(ElaIconType::Bullhorn, groupInfoCard, QStringLiteral("群公告"));
    m_groupAddMemberButton = makeActionButton(ElaIconType::UserPlus, groupInfoCard, QStringLiteral("加入"));
    m_groupHistoryButton = makeActionButton(ElaIconType::ClockRotateLeft, groupInfoCard, QStringLiteral("聊天记录"));
    m_groupSettingsButton = makeActionButton(ElaIconType::CircleInfo, groupInfoCard, QStringLiteral("群信息"));
    m_groupSettingsButton->setObjectName(QStringLiteral("groupInfoButton"));
    m_groupFileServiceSettingsButton =
        makeActionButton(ElaIconType::Gear, groupInfoCard, QStringLiteral("设置群文件服务"));
    m_groupFileManagerButton = makeActionButton(ElaIconType::FolderOpen, groupInfoCard, QStringLiteral("群文件"));
    m_groupCloseButton = makeActionButton(ElaIconType::Xmark, groupInfoCard, QStringLiteral("关闭当前会话"));
    m_groupMoreButton = makeIconActionButton(ElaIconType::Gear, groupInfoCard, QStringLiteral("群设置"));
    m_groupMoreButton->setObjectName(QStringLiteral("groupSettingsMenuButton"));
    QObject::connect(m_groupMoreButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::groupSettingsRequested);

    groupInfoCardLayout->addWidget(m_groupAnnouncementButton);
    groupInfoCardLayout->addWidget(m_groupAddMemberButton);
    groupInfoCardLayout->addWidget(m_groupFileManagerButton);
    groupInfoCardLayout->addWidget(m_groupHistoryButton);
    groupInfoCardLayout->addWidget(m_groupMoreButton);
    groupInfoCardLayout->addWidget(m_groupCloseButton);

    groupLayout->addWidget(groupInfoCard, 1);

    m_stack->addWidget(m_directPage);
    m_stack->addWidget(m_groupPage);
    m_stack->setCurrentWidget(m_directPage);

    for (ElaToolButton* button : {m_directHistoryButton,
                                m_directCreateGroupButton,
                                m_directCloseButton,
                                m_groupAnnouncementButton,
                                m_groupAddMemberButton,
                                m_groupHistoryButton,
                                m_groupSettingsButton,
                                m_groupFileServiceSettingsButton,
                                m_groupFileManagerButton,
                                m_groupCloseButton,
                                m_groupMoreButton}) {
        if (button) {
            button->setFixedHeight(actionHeight);
        }
    }

    m_groupSettingsButton->hide();
    m_groupFileServiceSettingsButton->hide();

    connect(m_directHistoryButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::directHistoryRequested);
    connect(m_directVoiceCallButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::directVoiceCallRequested);
    connect(m_directCreateGroupButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::directCreateGroupRequested);
    connect(m_directCloseButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::closeRequested);
    connect(m_groupAnnouncementButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::groupAnnouncementRequested);
    connect(m_groupAddMemberButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::groupAddMemberRequested);
    connect(m_groupHistoryButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::groupHistoryRequested);
    connect(m_groupSettingsButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::groupInfoPanelRequested);
    connect(m_groupFileServiceSettingsButton,
            &QAbstractButton::clicked,
            this,
            &ChatHeaderWidget::groupFileServiceSettingsRequested);
    connect(m_groupFileManagerButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::groupFileManagerRequested);
    connect(m_groupCloseButton, &QAbstractButton::clicked, this, &ChatHeaderWidget::closeRequested);

    refreshTheme();

    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]() { refreshTheme(); });
}

void ChatHeaderWidget::refreshTheme()
{
    const auto refreshHeaderIcon = [](ElaToolButton* button) {
        if (!button) {
            return;
        }
        const QVariant iconValue = button->property("headerIcon");
        if (!iconValue.isValid()) {
            return;
        }
        button->setIcon(ElaIcon::getInstance()->getElaIcon(
            static_cast<ElaIconType::IconName>(iconValue.toInt()),
            18,
            QColor(AppStyle::textPrimary())));
    };

    setStyleSheet(QStringLiteral(
                      "QWidget#chatHeaderWidget {"
                      "  background:transparent;"
                      "}"
                      "QFrame#headerInfoCard {"
                      "  background:%7;"
                      "  border:1px solid %8;"
                      "  border-radius:16px;"
                      "}"
                      "QFrame#headerControlBand {"
                  "  background:transparent;"
                      "  border:none;"
                      "}"
                      "QLabel#headerModeChip {"
                  "  background:%1;"
                  "  color:%2;"
                      "  border:none;"
                      "  border-radius:999px;"
                      "  font-size:11px;"
                      "  font-weight:700;"
                      "  padding:4px 10px;"
                      "}"
                      "QLabel#headerContextChip {"
                      "  background:%3;"
                  "  color:%4;"
                      "  border:none;"
                      "  border-radius:999px;"
                      "  font-size:11px;"
                      "  font-weight:600;"
                      "  padding:4px 10px;"
                      "}"
                      "QLabel#headerConsoleHint {"
                  "  color:%5;"
                      "  font-size:11px;"
                      "  font-weight:600;"
                      "}"
                      "QFrame#headerStatusStrip {"
                      "  background:%3;"
                      "  border:none;"
                      "}"
                      "QFrame#headerStatusPulse {"
                  "  background:%2;"
                      "  border-radius:4px;"
                      "}"
                      "QLabel#headerStatusValue {"
                  "  color:%4;"
                      "  font-size:11px;"
                      "  font-weight:700;"
                      "}"
                      "QFrame#headerActionTray {"
                  "  background:transparent;"
                      "  border:none;"
                      "}"
                      "QToolButton#HeaderElaToolButton {"
                      "  background:transparent;"
                      "  border:none;"
                      "  color:%9;"
                      "  border-radius:8px;"
                      "  padding:0;"
                      "  min-width:36px; min-height:36px;"
                      "}"
                      "QToolButton#HeaderElaToolButton:hover {"
                      "  background:%3;"
                      "  border:none;"
                      "}"
                      "QToolButton#HeaderElaToolButton:pressed {"
                      "  background:%6;"
                      "}")
                  .arg(AppStyle::accentSoft(),
                      AppStyle::accent(),
                      AppStyle::surfaceMuted(),
                      AppStyle::textSecondary(),
                      AppStyle::textMuted(),
                      AppStyle::surfaceAlt(),
                      AppStyle::chatCardBg(),
                      AppStyle::border(),
                      AppStyle::textPrimary()));

    for (ElaToolButton* button : {m_directHistoryButton,
                                  m_directVoiceCallButton,
                                  m_directCreateGroupButton,
                                  m_directCloseButton,
                                  m_groupAnnouncementButton,
                                  m_groupAddMemberButton,
                                  m_groupHistoryButton,
                                  m_groupSettingsButton,
                                  m_groupFileServiceSettingsButton,
                                  m_groupFileManagerButton,
                                  m_groupCloseButton,
                                  m_groupMoreButton}) {
        refreshHeaderIcon(button);
    }

    const QString titleStyle =
        QStringLiteral("font-size:16px; font-weight:700; color:%1;").arg(AppStyle::textPrimary());
    const QString subtitleAccentStyle =
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::accent());
    const QString subtitleMutedStyle =
        QStringLiteral("font-size:12px; color:%1;").arg(AppStyle::textMuted());

    if (m_directTitleLabel) {
        m_directTitleLabel->setStyleSheet(titleStyle);
    }
    if (m_directSubtitleLabel) {
        m_directSubtitleLabel->setStyleSheet(subtitleAccentStyle);
    }
    if (m_directSecondaryLabel) {
        m_directSecondaryLabel->setStyleSheet(subtitleMutedStyle);
    }
    if (m_groupTitleLabel) {
        m_groupTitleLabel->setStyleSheet(titleStyle);
    }
    if (m_groupSubtitleLabel) {
        m_groupSubtitleLabel->setStyleSheet(subtitleMutedStyle);
    }

    applyAvatarLabel(m_directAvatarLabel,
                     QString(),
                     m_directTitleLabel ? m_directTitleLabel->text().left(1).toUpper()
                                        : QStringLiteral("?"),
                     avatarStyle(AppStyle::accent()));
    if (m_groupAvatarLabel) {
        m_groupAvatarLabel->setStyleSheet(avatarStyle(AppStyle::accent()));
    }
}

void ChatHeaderWidget::setDirectChatState(const QString& name,
                                         const QString& status,
                                         const QString& signature,
                                         const QString& avatarImagePath)
{
    // 清除正在输入指示器状态
    m_savedDirectStatus.clear();
    if (m_typingTimer) m_typingTimer->stop();

    const QString trimmedName = name.trimmed();
    const QString title = trimmedName.isEmpty() ? QStringLiteral("未命名会话") : trimmedName;
    const QString detail = status.trimmed().isEmpty() ? QStringLiteral("已连接") : status.trimmed();
    Q_UNUSED(signature);

    m_directTitleLabel->setText(title);
    m_directSubtitleLabel->setText(detail);
    m_directSecondaryLabel->clear();
    m_directSecondaryLabel->hide();
    applyAvatarLabel(m_directAvatarLabel,
                     avatarImagePath,
                     title.left(1).toUpper(),
                     avatarStyle(AppStyle::accent()));
    if (m_statusValueLabel) {
        m_statusValueLabel->setText(QStringLiteral("直连在线"));
    }
    m_modeChipLabel->setText(QStringLiteral("直连会话"));
    m_contextChipLabel->setText(detail);
    m_consoleHintLabel->setText(QStringLiteral("实时通讯通道"));
    m_stack->setCurrentWidget(m_directPage);
}

void ChatHeaderWidget::setGroupChatState(const QString& groupName, const QString& memberSummary)
{
    const QString title =
        groupName.trimmed().isEmpty() ? QStringLiteral("未命名群聊") : groupName.trimmed();
    const QString summary =
        memberSummary.trimmed().isEmpty() ? QStringLiteral("暂无成员") : memberSummary.trimmed();

    m_groupTitleLabel->setText(title);
    m_groupSubtitleLabel->setText(summary);
    m_groupAvatarLabel->setText(title.left(1).toUpper());
    if (m_statusValueLabel) {
        m_statusValueLabel->setText(m_groupRuntimeStatusValue.trimmed().isEmpty()
                                        ? QStringLiteral("群协作进行中")
                                        : m_groupRuntimeStatusValue.trimmed());
    }
    m_modeChipLabel->setText(QStringLiteral("群协作空间"));
    m_contextChipLabel->setText(m_groupRuntimeContextChip.trimmed().isEmpty()
                                    ? summary
                                    : m_groupRuntimeContextChip.trimmed());
    m_consoleHintLabel->setText(m_groupRuntimeConsoleHint.trimmed().isEmpty()
                                    ? QStringLiteral("群上下文已就绪")
                                    : m_groupRuntimeConsoleHint.trimmed());
    m_stack->setCurrentWidget(m_groupPage);
}

void ChatHeaderWidget::setGroupRuntimeState(const QString& statusValue,
                                            const QString& consoleHint,
                                            const QString& contextChip)
{
    m_groupRuntimeStatusValue = statusValue.trimmed();
    m_groupRuntimeConsoleHint = consoleHint.trimmed();
    m_groupRuntimeContextChip = contextChip.trimmed();

    if (isGroupMode()) {
        if (m_statusValueLabel) {
            m_statusValueLabel->setText(m_groupRuntimeStatusValue.isEmpty()
                                            ? QStringLiteral("群协作进行中")
                                            : m_groupRuntimeStatusValue);
        }
        if (m_consoleHintLabel) {
            m_consoleHintLabel->setText(m_groupRuntimeConsoleHint.isEmpty()
                                            ? QStringLiteral("群上下文已就绪")
                                            : m_groupRuntimeConsoleHint);
        }
        if (m_contextChipLabel && !m_groupRuntimeContextChip.isEmpty()) {
            m_contextChipLabel->setText(m_groupRuntimeContextChip);
        }
    }
}

void ChatHeaderWidget::clearGroupRuntimeState()
{
    m_groupRuntimeStatusValue.clear();
    m_groupRuntimeConsoleHint.clear();
    m_groupRuntimeContextChip.clear();

    if (isGroupMode()) {
        if (m_statusValueLabel) {
            m_statusValueLabel->setText(QStringLiteral("群协作进行中"));
        }
        if (m_consoleHintLabel) {
            m_consoleHintLabel->setText(QStringLiteral("群上下文已就绪"));
        }
    }
}

QString ChatHeaderWidget::titleText() const
{
    return isGroupMode() ? m_groupTitleLabel->text() : m_directTitleLabel->text();
}

QString ChatHeaderWidget::subtitleText() const
{
    return isGroupMode() ? m_groupSubtitleLabel->text() : m_directSubtitleLabel->text();
}

QString ChatHeaderWidget::secondaryText() const
{
    return isGroupMode() ? QString() : m_directSecondaryLabel->text();
}

bool ChatHeaderWidget::isGroupMode() const
{
    return m_stack->currentWidget() == m_groupPage;
}

ElaToolButton* ChatHeaderWidget::directHistoryButton() const { return m_directHistoryButton; }
ElaToolButton* ChatHeaderWidget::directVoiceCallButton() const { return m_directVoiceCallButton; }
ElaToolButton* ChatHeaderWidget::directCreateGroupButton() const { return m_directCreateGroupButton; }
ElaToolButton* ChatHeaderWidget::groupAnnouncementButton() const { return m_groupAnnouncementButton; }
ElaToolButton* ChatHeaderWidget::groupAddMemberButton() const { return m_groupAddMemberButton; }
ElaToolButton* ChatHeaderWidget::groupHistoryButton() const { return m_groupHistoryButton; }
ElaToolButton* ChatHeaderWidget::groupInfoButton() const { return m_groupSettingsButton; }
ElaToolButton* ChatHeaderWidget::groupSettingsButton() const { return m_groupSettingsButton; }
ElaToolButton* ChatHeaderWidget::groupFileServiceSettingsButton() const { return m_groupFileServiceSettingsButton; }
ElaToolButton* ChatHeaderWidget::groupFileManagerButton() const { return m_groupFileManagerButton; }
ElaToolButton* ChatHeaderWidget::directCloseButton() const { return m_directCloseButton; }
ElaToolButton* ChatHeaderWidget::groupCloseButton() const { return m_groupCloseButton; }

void ChatHeaderWidget::setGroupFileManagerVisible(bool visible)
{
    m_groupFileManagerButton->setVisible(visible);
}

void ChatHeaderWidget::showTypingIndicator()
{
    if (isGroupMode()) return;
    if (!m_typingTimer) {
        m_typingTimer = new QTimer(this);
        m_typingTimer->setSingleShot(true);
        connect(m_typingTimer, &QTimer::timeout, this, &ChatHeaderWidget::clearTypingIndicator);
    }
    if (m_savedDirectStatus.isEmpty()) {
        m_savedDirectStatus = m_directSubtitleLabel->text();
    }
    m_directSubtitleLabel->setText(QStringLiteral("\u6B63\u5728\u8F93\u5165..."));
    m_typingTimer->start(5000);
}

void ChatHeaderWidget::clearTypingIndicator()
{
    if (m_typingTimer) {
        m_typingTimer->stop();
    }
    if (!m_savedDirectStatus.isEmpty()) {
        m_directSubtitleLabel->setText(m_savedDirectStatus);
        m_savedDirectStatus.clear();
    }
}
