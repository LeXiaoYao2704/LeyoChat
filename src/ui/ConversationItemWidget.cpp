// ConversationItemWidget.cpp — 会话列表项 Widget 实现
#include "ui/ConversationItemWidget.h"

#include "ui/AppStyle.h"
#include "ui/ConversationListModel.h"

#include <QContextMenuEvent>
#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QStyle>
#include <QVBoxLayout>

namespace {

QColor avatarColor(const QString& seed)
{
    static const QColor palette[] = {
        QColor(0x52, 0x73, 0xE8),
        QColor(0x2F, 0xA4, 0x84),
        QColor(0xD9, 0x96, 0x3A),
        QColor(0x7B, 0x68, 0xE6),
        QColor(0xD8, 0x5A, 0x9A),
        QColor(0x32, 0x96, 0xC4),
    };
    int hash = 0;
    for (const QChar ch : seed)
        hash = (hash * 31 + ch.unicode()) & 0x7FFF'FFFF;
    return palette[hash % (sizeof(palette) / sizeof(palette[0]))];
}

QPixmap roundedPixmap(const QPixmap& src, int size)
{
    QPixmap result(size, size);
    result.fill(Qt::transparent);
    QPainter p(&result);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    p.setClipPath(path);
    p.drawPixmap(0, 0, size, size, src);
    return result;
}

QPixmap letterAvatarPixmap(const QString& title, int size, bool online)
{
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor baseColor = avatarColor(title);
    const QColor fillColor = online ? baseColor : QColor::fromHsl(baseColor.hslHue(), 0, 160);
    p.setPen(Qt::NoPen);
    p.setBrush(fillColor);
    p.drawEllipse(0, 0, size, size);

    QFont f;
    f.setBold(true);
    f.setPixelSize(size >= 40 ? 17 : 15);
    p.setFont(f);
    p.setPen(Qt::white);
    const QString ch = title.trimmed().isEmpty()
                           ? QStringLiteral("?")
                           : QString(title.trimmed().front()).toUpper();
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, ch);
    return pix;
}

} // namespace

ConversationItemWidget::ConversationItemWidget(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("ConversationCardHost"));
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover, true);
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const int avatarSize = 36;

    m_cardFrame = new QFrame(this);
    m_cardFrame->setObjectName(QStringLiteral("ConversationCard"));
    m_cardFrame->setAttribute(Qt::WA_StyledBackground, true);
    m_cardFrame->setAttribute(Qt::WA_Hover, true);
    m_cardFrame->setProperty("hovered", false);
    m_cardFrame->setFrameShape(QFrame::NoFrame);
    m_cardFrame->setFrameShadow(QFrame::Plain);
    m_cardFrame->setMouseTracking(true);
    m_cardFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // 选中指示条
    m_selectionBar = new QFrame(m_cardFrame);
    m_selectionBar->setObjectName(QStringLiteral("ConvSelectionBar"));
    m_selectionBar->setFixedWidth(3);

    // 头像
    m_avatarLabel = new QLabel(m_cardFrame);
    m_avatarLabel->setObjectName(QStringLiteral("ConvAvatarLabel"));
    m_avatarLabel->setFixedSize(avatarSize, avatarSize);
    m_avatarLabel->setScaledContents(false);
    m_avatarLabel->setAlignment(Qt::AlignCenter);
    m_avatarLabel->setStyleSheet(QStringLiteral(
        "QLabel#ConvAvatarLabel { background: transparent; border: none; padding: 0; }"));
    m_avatarLabel->installEventFilter(this);

    // 标题
    m_titleLabel = new QLabel(m_cardFrame);
    m_titleLabel->setObjectName(QStringLiteral("ConvTitleLabel"));
    m_titleLabel->setTextFormat(Qt::PlainText);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_titleLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    {
        QFont f = m_titleLabel->font();
        f.setPixelSize(13);
        f.setWeight(QFont::Bold);
        m_titleLabel->setFont(f);
    }

    // 状态标签（置顶/收藏/静音）
    m_badgeLabel = new QLabel(m_cardFrame);
    m_badgeLabel->setObjectName(QStringLiteral("ConvBadgeLabel"));
    m_badgeLabel->setAlignment(Qt::AlignCenter);
    m_badgeLabel->hide();

    // 时间
    m_timeLabel = new QLabel(m_cardFrame);
    m_timeLabel->setObjectName(QStringLiteral("ConvTimeLabel"));
    m_timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    // 草稿标签
    m_draftTag = new QLabel(QStringLiteral("[\u8349\u7A3F]"), m_cardFrame);
    m_draftTag->setObjectName(QStringLiteral("ConvDraftTag"));
    m_draftTag->hide();

    // @提及标签
    m_mentionTag = new QLabel(QStringLiteral("[\u6709\u4EBA@\u6211]"), m_cardFrame);
    m_mentionTag->setObjectName(QStringLiteral("ConvMentionTag"));
    m_mentionTag->hide();

    // 预览文本
    m_previewLabel = new QLabel(m_cardFrame);
    m_previewLabel->setObjectName(QStringLiteral("ConvPreviewLabel"));
    m_previewLabel->setTextFormat(Qt::PlainText);
    m_previewLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_previewLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    {
        QFont f = m_previewLabel->font();
        f.setPixelSize(12);
        m_previewLabel->setFont(f);
    }

    // 未读指示（小红点）
    m_unreadDot = new QLabel(m_cardFrame);
    m_unreadDot->setObjectName(QStringLiteral("ConvUnreadDot"));
    m_unreadDot->setAlignment(Qt::AlignCenter);
    m_unreadDot->setFixedSize(10, 10);
    m_unreadDot->hide();

    // === 布局 ===
    auto* headerRow = new QHBoxLayout;
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(4);
    headerRow->addWidget(m_titleLabel, 1);
    headerRow->addWidget(m_badgeLabel, 0);
    headerRow->addWidget(m_timeLabel, 0);

    auto* previewRow = new QHBoxLayout;
    previewRow->setContentsMargins(0, 0, 0, 0);
    previewRow->setSpacing(4);
    previewRow->addWidget(m_draftTag, 0);
    previewRow->addWidget(m_mentionTag, 0);
    previewRow->addWidget(m_previewLabel, 1);
    previewRow->addWidget(m_unreadDot, 0);

    auto* bodyLayout = new QVBoxLayout;
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(2);
    bodyLayout->addLayout(headerRow);
    bodyLayout->addLayout(previewRow);

    auto* cardLayout = new QHBoxLayout(m_cardFrame);
    cardLayout->setContentsMargins(8, 7, 8, 7);
    cardLayout->setSpacing(8);
    cardLayout->addWidget(m_selectionBar, 0);
    cardLayout->addWidget(m_avatarLabel, 0, Qt::AlignVCenter);
    cardLayout->addLayout(bodyLayout, 1);

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(30, 2, 30, 2);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_cardFrame, 1);

    applyThemeStyleSheet();
}

void ConversationItemWidget::populateFromIndex(const QModelIndex& index)
{
    const QString title = index.data(ConversationListModel::TitleRole).toString();
    const QString preview = index.data(ConversationListModel::PreviewRole).toString();
    const QString timeLabel = index.data(ConversationListModel::TimeLabelRole).toString();
    const bool hasUnread = index.data(ConversationListModel::HasUnreadRole).toBool();
    const bool isPinned = index.data(ConversationListModel::IsPinnedRole).toBool();
    const bool isStarred = index.data(ConversationListModel::IsStarredRole).toBool();
    const bool isMuted = index.data(ConversationListModel::IsMutedRole).toBool();
    const bool hasMentionMe = index.data(ConversationListModel::HasMentionMeRole).toBool();
    const QString avatarPath = index.data(ConversationListModel::AvatarPathRole).toString();
    const QString draftText = index.data(ConversationListModel::DraftTextRole).toString();
    const bool isOnline = index.data(ConversationListModel::IsOnlineRole).toBool();
    const QString convId = index.data(ConversationListModel::ConversationIdRole).toString();

    setConversationId(convId);
    const bool directConversation = convId.contains(QLatin1Char('|'));
    m_avatarLabel->setCursor(directConversation ? Qt::PointingHandCursor : Qt::ArrowCursor);
    m_avatarLabel->setToolTip(QString());
    setTitle(title);
    setPreview(preview);
    setTimeLabel(timeLabel);
    setHasUnread(hasUnread);
    setIsPinned(isPinned);
    setIsStarred(isStarred);
    setIsMuted(isMuted);
    setHasMentionMe(hasMentionMe && hasUnread);
    setAvatarPath(avatarPath);
    setDraftText(draftText);
    setIsOnline(isOnline);

    buildAvatarPixmap(title, avatarPath, isOnline);
}

void ConversationItemWidget::setConversationId(const QString& id) { m_conversationId = id; }

void ConversationItemWidget::setTitle(const QString& title)
{
    m_titleLabel->setText(title);
}

void ConversationItemWidget::setPreview(const QString& preview)
{
    m_previewLabel->setText(preview);
}

void ConversationItemWidget::setTimeLabel(const QString& time)
{
    m_timeLabel->setText(time);
}

void ConversationItemWidget::setHasUnread(bool unread)
{
    m_unreadDot->setVisible(unread);
}

void ConversationItemWidget::setIsPinned(bool pinned)
{
    if (pinned) {
        m_badgeLabel->setText(QStringLiteral("\u9876"));
        m_badgeLabel->show();
    } else if (m_badgeLabel->text() == QStringLiteral("\u9876")) {
        m_badgeLabel->hide();
    }
}

void ConversationItemWidget::setIsStarred(bool starred)
{
    if (starred && !m_badgeLabel->isVisible()) {
        m_badgeLabel->setText(QStringLiteral("\u2605"));
        m_badgeLabel->show();
    } else if (!starred && m_badgeLabel->text() == QStringLiteral("\u2605")) {
        m_badgeLabel->hide();
    }
}

void ConversationItemWidget::setIsMuted(bool muted)
{
    if (muted && !m_badgeLabel->isVisible()) {
        m_badgeLabel->setText(QStringLiteral("\u9759"));
        m_badgeLabel->show();
    } else if (!muted && m_badgeLabel->text() == QStringLiteral("\u9759")) {
        m_badgeLabel->hide();
    }
}

void ConversationItemWidget::setHasMentionMe(bool mention)
{
    m_mentionTag->setVisible(mention);
}

void ConversationItemWidget::setAvatarPath(const QString& path)
{
    // Actual pixmap building done in buildAvatarPixmap
    Q_UNUSED(path);
}

void ConversationItemWidget::setDraftText(const QString& draft)
{
    m_draftTag->setVisible(!draft.isEmpty());
    if (!draft.isEmpty()) {
        m_previewLabel->setText(draft);
    }
}

void ConversationItemWidget::setIsOnline(bool online)
{
    m_isOnline = online;
}

void ConversationItemWidget::setSelected(bool selected)
{
    if (m_selected == selected)
        return;
    m_selected = selected;
    if (m_cardFrame)
        m_cardFrame->setProperty("selected", selected);
    m_selectionBar->setProperty("selected", selected);
    m_titleLabel->setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
    if (m_cardFrame) {
        m_cardFrame->style()->unpolish(m_cardFrame);
        m_cardFrame->style()->polish(m_cardFrame);
    }
    m_selectionBar->style()->unpolish(m_selectionBar);
    m_selectionBar->style()->polish(m_selectionBar);
    update();
}

void ConversationItemWidget::applyThemeStyleSheet()
{
    // 缓存：所有 ConversationItemWidget 共享相同样式表，仅在主题切换时重建
    static QString s_cachedSheet;
    static QString s_cachedTheme;
    const QString currentTheme = AppStyle::themeModeToString(AppStyle::currentThemeMode());
    if (s_cachedTheme != currentTheme) {
        s_cachedTheme = currentTheme;
        const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
        const bool dark = AppStyle::isDarkTheme(mode);
        const QString accentColor = AppStyle::accent();
        const QString cardHoverBg = dark
            ? QStringLiteral("rgba(255,255,255,30)")
            : QStringLiteral("rgba(255,255,255,116)");
        const QString cardSelectedBg = dark
            ? QStringLiteral("rgba(255,255,255,46)")
            : QStringLiteral("rgba(255,255,255,190)");
        const QString cardSelectedBorder = dark ? AppStyle::borderStrong(mode) : AppStyle::border(mode);
        const QString textPrimary = AppStyle::textPrimary(mode);
        const QString textMuted = AppStyle::textMuted(mode);
        const QString warningColor = AppStyle::warning();
        const QString dangerColor = QStringLiteral("#E81123");

        s_cachedSheet = QStringLiteral(
            "QFrame#ConversationCardHost {"
            "  background: transparent;"
            "  border: none;"
            "}"
            "QFrame#ConversationCard {"
            "  background: transparent;"
            "  border: 1px solid transparent;"
            "  border-radius: 14px;"
            "}"
            "QFrame#ConversationCard:hover {"
            "  background: %2;"
            "  border: 1px solid transparent;"
            "}"
            "QFrame#ConversationCard[hovered=\"true\"] {"
            "  background: %2;"
            "  border: 1px solid transparent;"
            "}"
            "QFrame#ConversationCard[selected=\"true\"] {"
            "  background: %3;"
            "  border: 1px solid %7;"
            "}"
            "QFrame#ConvSelectionBar {"
            "  background: transparent;"
            "  border-radius: 2px;"
            "  min-height: 30px;"
            "}"
            "QFrame#ConvSelectionBar[selected=\"true\"] {"
            "  background: %1;"
            "}"
            "#ConvTitleLabel {"
            "  color: %4;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0;"
            "}"
            "#ConvPreviewLabel {"
            "  color: %5;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0;"
            "}"
            "QLabel#ConvTimeLabel {"
            "  color: %5;"
            "  font-size: 11px;"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0;"
            "}"
            "QLabel#ConvBadgeLabel {"
            "  color: %5;"
            "  font-size: 11px;"
            "  font-weight: 700;"
            "  background: transparent;"
            "  padding: 0 2px;"
            "  border: none;"
            "}"
            "QLabel#ConvDraftTag {"
            "  color: %6;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "  background: transparent;"
            "}"
            "QLabel#ConvMentionTag {"
            "  color: %1;"
            "  font-size: 11px;"
            "  font-weight: 600;"
            "  background: transparent;"
            "}"
            "QLabel#ConvUnreadDot {"
            "  background: %8;"
            "  border-radius: 5px;"
            "  min-width: 10px;"
            "  max-width: 10px;"
            "  min-height: 10px;"
            "  max-height: 10px;"
            "}"
            "QLabel#ConvAvatarLabel {"
            "  background: transparent;"
            "  border: none;"
            "  padding: 0;"
            "}"
            ).arg(accentColor, cardHoverBg, cardSelectedBg, textPrimary, textMuted, warningColor, cardSelectedBorder).arg(dangerColor);
    }
    setStyleSheet(s_cachedSheet);
}

void ConversationItemWidget::buildAvatarPixmap(const QString& title,
                                                const QString& path,
                                                bool online)
{
    const int size = 36;
    // 群聊没有在线/离线概念，永远按在线（彩色）渲染
    const bool isDirectChat = m_conversationId.contains(QLatin1Char('|'));
    const bool effectiveOnline = isDirectChat ? online : true;

    if (!path.isEmpty()) {
        QPixmap pix;
        // 缓存 key 包含在线状态，避免离线灰度每次重新计算
        const QString cacheKey = QStringLiteral("convAvatar:%1:%2:%3").arg(path).arg(size).arg(effectiveOnline ? 1 : 0);
        if (!QPixmapCache::find(cacheKey, &pix)) {
            QPixmap raw(path);
            if (!raw.isNull()) {
                pix = raw.scaled(size, size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                if (!effectiveOnline) {
                    QImage grayImg = pix.toImage().convertToFormat(QImage::Format_Grayscale8);
                    pix = QPixmap::fromImage(grayImg);
                }
                pix = roundedPixmap(pix, size);
                QPixmapCache::insert(cacheKey, pix);
            }
        }
        if (!pix.isNull()) {
            m_avatarLabel->setPixmap(pix);
            return;
        }
    }

    // 字母头像
    m_avatarLabel->setPixmap(letterAvatarPixmap(title, size, effectiveOnline));
}

void ConversationItemWidget::setHovered(bool hovered)
{
    if (!m_cardFrame || m_cardFrame->property("hovered").toBool() == hovered) {
        return;
    }
    m_cardFrame->setProperty("hovered", hovered);
    m_cardFrame->style()->unpolish(m_cardFrame);
    m_cardFrame->style()->polish(m_cardFrame);
    m_cardFrame->update();
}

bool ConversationItemWidget::event(QEvent* event)
{
    if (event) {
        if (event->type() == QEvent::Enter) {
            setHovered(true);
        } else if (event->type() == QEvent::Leave) {
            setHovered(false);
        }
    }
    return QFrame::event(event);
}

bool ConversationItemWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_avatarLabel && event) {
        if (event->type() == QEvent::Enter
            && m_conversationId.contains(QLatin1Char('|'))
            && m_avatarLabel->rect().contains(m_avatarLabel->mapFromGlobal(QCursor::pos()))) {
            const QPoint globalPos =
                m_avatarLabel->mapToGlobal(QPoint(m_avatarLabel->width() + 8, 0));
            emit avatarHovered(m_conversationId, globalPos);
        } else if (event->type() == QEvent::Leave) {
            emit avatarHoverLeft();
        }
    }
    return QFrame::eventFilter(watched, event);
}

void ConversationItemWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(m_conversationId);
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void ConversationItemWidget::contextMenuEvent(QContextMenuEvent* event)
{
    emit contextMenuRequested(m_conversationId, event->globalPos());
    event->accept();
}
