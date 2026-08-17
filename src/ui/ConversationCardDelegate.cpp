// ConversationCardDelegate.cpp — 会话列表项 QPainter delegate 实现
#include "ui/ConversationCardDelegate.h"

#include "ui/AppStyle.h"
#include "ui/ConversationListModel.h"

#include <QAbstractItemView>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace {

// === 布局常量（精确复刻 ConversationItemWidget 的 margins/spacing） ===
// mainLayout: margins(30, 2, 30, 2)
constexpr int kMainMarginH = 30;
constexpr int kMainMarginV = 2;
// cardLayout: margins(8, 7, 8, 7), spacing=8
constexpr int kCardPaddingH = 8;
constexpr int kCardPaddingV = 7;
constexpr int kCardSpacing = 8;
// cardFrame borderRadius
constexpr int kCardRadius = 14;
// selectionBar: width=3, min-height=30, borderRadius=2
constexpr int kSelBarWidth = 3;
// avatar size
constexpr int kAvatarSize = 36;
// bodyLayout: spacing=2
constexpr int kBodySpacing = 2;
// headerRow/previewRow: spacing=4
constexpr int kRowSpacing = 4;
// unreadDot: 10x10
constexpr int kUnreadDotSize = 10;

// 与 ConversationItemWidget 中相同的调色板
static const QColor kAvatarPalette[] = {
    QColor(0x52, 0x73, 0xE8),
    QColor(0x2F, 0xA4, 0x84),
    QColor(0xD9, 0x96, 0x3A),
    QColor(0x7B, 0x68, 0xE6),
    QColor(0xD8, 0x5A, 0x9A),
    QColor(0x32, 0x96, 0xC4),
};

QColor avatarColor(const QString& seed)
{
    int hash = 0;
    for (const QChar ch : seed)
        hash = (hash * 31 + ch.unicode()) & 0x7FFF'FFFF;
    return kAvatarPalette[hash % 6];
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
    const QColor baseColor = avatarColor(title);
    const QColor fillColor = online ? baseColor
                                    : QColor::fromHsl(baseColor.hslHue(), 0, 160);
    const QString ch = title.trimmed().isEmpty()
                           ? QStringLiteral("?")
                           : QString(title.trimmed().front()).toUpper();
    const QString cacheKey = QStringLiteral("ccdLetter:%1:%2:%3:%4")
                                 .arg(ch, fillColor.name()).arg(size).arg(online ? 1 : 0);
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached))
        return cached;

    QPixmap pix(size, size);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(fillColor);
    p.drawEllipse(0, 0, size, size);

    QFont f;
    f.setBold(true);
    f.setPixelSize(size >= 40 ? 17 : 15);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, ch);
    p.end();

    QPixmapCache::insert(cacheKey, pix);
    return pix;
}

QPixmap resolveAvatarPixmap(const QString& path, const QString& title,
                            const QString& convId, int size, bool online)
{
    // 群聊无在线/离线概念，始终按在线(彩色)渲染
    const bool isDirectChat = convId.contains(QLatin1Char('|'));
    const bool effectiveOnline = isDirectChat ? online : true;

    if (!path.isEmpty()) {
        const QString cacheKey = QStringLiteral("convAvatar:%1:%2:%3")
                                     .arg(path).arg(size).arg(effectiveOnline ? 1 : 0);
        QPixmap pix;
        if (!QPixmapCache::find(cacheKey, &pix)) {
            QPixmap raw(path);
            if (!raw.isNull()) {
                pix = raw.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                 Qt::SmoothTransformation);
                if (!effectiveOnline) {
                    QImage grayImg = pix.toImage().convertToFormat(QImage::Format_Grayscale8);
                    pix = QPixmap::fromImage(grayImg);
                }
                pix = roundedPixmap(pix, size);
                QPixmapCache::insert(cacheKey, pix);
            }
        }
        if (!pix.isNull())
            return pix;
    }
    return letterAvatarPixmap(title, size, effectiveOnline);
}

} // namespace

// ============================================================================

ConversationCardDelegate::ConversationCardDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ConversationCardDelegate::initStyleOption(QStyleOptionViewItem* option,
                                               const QModelIndex& index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    // 移除焦点框和选中高亮（由 paint() 完全自绘）
    option->state &= ~(QStyle::State_HasFocus | QStyle::State_Selected);
}

QSize ConversationCardDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                         const QModelIndex& /*index*/) const
{
    return QSize(0, AppStyle::kConversationRowHeight);
}

void ConversationCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    const bool dark = AppStyle::isDarkTheme(mode);

    // --- 读取 model 数据 ---
    const QString convId = index.data(ConversationListModel::ConversationIdRole).toString();
    const QString title = index.data(ConversationListModel::TitleRole).toString();
    const QString preview = index.data(ConversationListModel::PreviewRole).toString();
    const QString timeText = index.data(ConversationListModel::TimeLabelRole).toString();
    const bool hasUnread = index.data(ConversationListModel::HasUnreadRole).toBool();
    const bool isPinned = index.data(ConversationListModel::IsPinnedRole).toBool();
    const bool isStarred = index.data(ConversationListModel::IsStarredRole).toBool();
    const bool isMuted = index.data(ConversationListModel::IsMutedRole).toBool();
    const bool hasMentionMe = index.data(ConversationListModel::HasMentionMeRole).toBool();
    const QString avatarPath = index.data(ConversationListModel::AvatarPathRole).toString();
    const QString draftText = index.data(ConversationListModel::DraftTextRole).toString();
    const bool isOnline = index.data(ConversationListModel::IsOnlineRole).toBool();

    const bool selected = (convId == m_selectedConvId);
    const bool hovered = (index == m_hoveredIndex);

    // --- 1. 卡片背景 ---
    // mainLayout margins: option.rect.adjusted(30, 2, -30, -2) = cardFrame 区域
    const QRect cardRect = option.rect.adjusted(kMainMarginH, kMainMarginV,
                                                -kMainMarginH, -kMainMarginV);
    QColor cardBg = Qt::transparent;
    QPen cardPen(Qt::transparent, 1.0);

    if (selected) {
        cardBg = dark ? QColor(255, 255, 255, 46) : QColor(255, 255, 255, 190);
        cardPen = QPen(QColor(dark ? AppStyle::borderStrong(mode) : AppStyle::border(mode)), 1.0);
    } else if (hovered) {
        cardBg = dark ? QColor(255, 255, 255, 30) : QColor(255, 255, 255, 116);
        cardPen = QPen(Qt::transparent, 1.0);
    }

    painter->setPen(cardPen);
    painter->setBrush(cardBg);
    painter->drawRoundedRect(cardRect, kCardRadius, kCardRadius);

    // --- 内部坐标基于 cardRect + cardLayout margins(8,7,8,7) ---
    const int innerLeft = cardRect.x() + kCardPaddingH;
    const int innerTop = cardRect.y() + kCardPaddingV;
    const int innerRight = cardRect.right() - kCardPaddingH;
    const int innerH = cardRect.height() - 2 * kCardPaddingV;

    int curX = innerLeft;

    // --- 2. 选中指示条 ---
    if (selected) {
        // selectionBar: width=3, min-height=30, borderRadius=2
        const int barH = qMin(30, innerH);
        const int barY = innerTop + (innerH - barH) / 2;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(AppStyle::accent(mode)));
        painter->drawRoundedRect(QRect(curX, barY, kSelBarWidth, barH), 2, 2);
    }
    // selectionBar 总是占位（哪怕不可见也参与布局：width=3 + spacing=8）
    curX += kSelBarWidth + kCardSpacing;

    // --- 3. 头像 ---
    const int avatarY = innerTop + (innerH - kAvatarSize) / 2;
    const QPixmap avatar = resolveAvatarPixmap(avatarPath, title, convId, kAvatarSize, isOnline);
    painter->drawPixmap(curX, avatarY, avatar);
    curX += kAvatarSize + kCardSpacing;

    // --- 文本区域 ---
    const int textX = curX;
    const int textW = innerRight - textX;

    // 两行布局：行高由字体决定
    // headerRow: titleLabel(13px Bold) + badgeLabel(11px Bold) + timeLabel(11px)
    // previewRow: draftTag(11px) + mentionTag(11px) + previewLabel(12px) + unreadDot
    const int row1Y = innerTop;
    const int rowH = (innerH - kBodySpacing) / 2; // 每行占一半
    const int row2Y = row1Y + rowH + kBodySpacing;

    // --- 4. 时间标签（右侧对齐，先画以便知道其宽度） ---
    QFont timeFont = option.font;
    timeFont.setPixelSize(11);
    const QFontMetrics timeFm(timeFont);
    const int timeWidth = timeFm.horizontalAdvance(timeText);

    painter->setFont(timeFont);
    painter->setPen(QColor(AppStyle::textMuted(mode)));
    painter->drawText(textX + textW - timeWidth, row1Y, timeWidth, rowH,
                      Qt::AlignRight | Qt::AlignVCenter, timeText);

    // --- 5. 状态徽章（顶/★/静） ---
    QString badgeText;
    if (isPinned)
        badgeText = QStringLiteral("\u9876"); // "顶"
    else if (isStarred)
        badgeText = QStringLiteral("\u2605"); // "★"
    else if (isMuted)
        badgeText = QStringLiteral("\u9759"); // "静"

    QFont badgeFont = option.font;
    badgeFont.setPixelSize(11);
    badgeFont.setWeight(QFont::Bold);
    const QFontMetrics badgeFm(badgeFont);
    const int badgeWidth = badgeText.isEmpty() ? 0 : badgeFm.horizontalAdvance(badgeText) + 4;

    if (!badgeText.isEmpty()) {
        const int badgeX = textX + textW - timeWidth - kRowSpacing - badgeWidth;
        painter->setFont(badgeFont);
        painter->setPen(QColor(AppStyle::textMuted(mode)));
        painter->drawText(badgeX, row1Y, badgeWidth, rowH,
                          Qt::AlignCenter, badgeText);
    }

    // --- 6. 标题文本 ---
    QFont titleFont = option.font;
    titleFont.setPixelSize(13);
    titleFont.setBold(true);
    const QFontMetrics titleFm(titleFont);
    const int titleAvailW = textW - timeWidth - badgeWidth
                            - (badgeWidth > 0 ? kRowSpacing : 0) - kRowSpacing;
    const QString elidedTitle = titleFm.elidedText(title, Qt::ElideRight, titleAvailW);

    painter->setFont(titleFont);
    painter->setPen(QColor(AppStyle::textPrimary(mode)));
    painter->drawText(textX, row1Y, titleAvailW, rowH,
                      Qt::AlignLeft | Qt::AlignVCenter, elidedTitle);

    // --- 第二行：草稿标签 + @提及标签 + 预览文本 + 未读红点 ---
    int previewX = textX;

    // --- 7. 草稿标签 ---
    if (!draftText.isEmpty()) {
        QFont draftFont = option.font;
        draftFont.setPixelSize(11);
        draftFont.setWeight(QFont::DemiBold);
        painter->setFont(draftFont);
        painter->setPen(QColor(AppStyle::warning(mode)));
        const QString draftTag = QStringLiteral("[\u8349\u7A3F]");
        const int draftTagW = QFontMetrics(draftFont).horizontalAdvance(draftTag);
        painter->drawText(previewX, row2Y, draftTagW, rowH,
                          Qt::AlignLeft | Qt::AlignVCenter, draftTag);
        previewX += draftTagW + kRowSpacing;
    }

    // --- 8. @提及标签（仅当有未读且无草稿时显示） ---
    if (hasMentionMe && hasUnread && draftText.isEmpty()) {
        QFont mentionFont = option.font;
        mentionFont.setPixelSize(11);
        mentionFont.setWeight(QFont::DemiBold);
        painter->setFont(mentionFont);
        painter->setPen(QColor(AppStyle::accent(mode)));
        const QString mentionTag = QStringLiteral("[\u6709\u4EBA@\u6211]");
        const int mentionTagW = QFontMetrics(mentionFont).horizontalAdvance(mentionTag);
        painter->drawText(previewX, row2Y, mentionTagW, rowH,
                          Qt::AlignLeft | Qt::AlignVCenter, mentionTag);
        previewX += mentionTagW + kRowSpacing;
    }

    // --- 9. 预览文本 ---
    QFont previewFont = option.font;
    previewFont.setPixelSize(12);
    painter->setFont(previewFont);
    painter->setPen(QColor(AppStyle::textMuted(mode)));

    const int unreadSpace = hasUnread ? (kUnreadDotSize + kRowSpacing) : 0;
    const int previewAvailW = textX + textW - previewX - unreadSpace;
    const QString previewSource = !draftText.isEmpty() ? draftText : preview;
    const QString elidedPreview = QFontMetrics(previewFont)
                                      .elidedText(previewSource, Qt::ElideRight, previewAvailW);
    painter->drawText(previewX, row2Y, previewAvailW, rowH,
                      Qt::AlignLeft | Qt::AlignVCenter, elidedPreview);

    // --- 10. 未读红点 ---
    if (hasUnread) {
        const int dotX = textX + textW - kUnreadDotSize;
        const int dotY = row2Y + (rowH - kUnreadDotSize) / 2;
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0xE8, 0x11, 0x23));
        painter->drawEllipse(QRect(dotX, dotY, kUnreadDotSize, kUnreadDotSize));
    }

    painter->restore();
}

// ============================================================================

bool ConversationCardDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                           const QStyleOptionViewItem& option,
                                           const QModelIndex& index)
{
    if (!event || !index.isValid())
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    const QString convId = index.data(ConversationListModel::ConversationIdRole).toString();

    switch (event->type()) {
    case QEvent::MouseMove: {
        if (m_hoveredIndex != index) {
            const QModelIndex prev = m_hoveredIndex;
            m_hoveredIndex = index;
            if (auto* view = qobject_cast<QAbstractItemView*>(
                    const_cast<QWidget*>(option.widget))) {
                if (prev.isValid())
                    view->update(prev);
                view->update(index);
            }
        }
        // 头像区域 hover → 弹出 profile card（仅私聊）
        if (convId.contains(QLatin1Char('|'))) {
            auto* me = static_cast<QMouseEvent*>(event);
            const QRect cardRect = option.rect.adjusted(kMainMarginH, kMainMarginV,
                                                        -kMainMarginH, -kMainMarginV);
            const bool selected = (convId == m_selectedConvId);
            const QRect avatarRect = computeAvatarRect(cardRect, selected);
            auto* viewport = const_cast<QWidget*>(option.widget);
            if (avatarRect.contains(me->pos())) {
                viewport->setCursor(Qt::PointingHandCursor);
                const QPoint globalPos = option.widget->mapToGlobal(
                    avatarRect.topRight() + QPoint(8, 0));
                emit avatarHovered(convId, globalPos);
            } else {
                viewport->setCursor(Qt::ArrowCursor);
                emit avatarHoverLeft();
            }
        } else {
            const_cast<QWidget*>(option.widget)->setCursor(Qt::ArrowCursor);
            emit avatarHoverLeft();
        }
        break;
    }
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::RightButton) {
            emit contextMenuRequested(convId, option.widget->mapToGlobal(me->pos()));
            return true;
        }
        if (me->button() == Qt::LeftButton) {
            emit clicked(convId);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool ConversationCardDelegate::eventFilter(QObject* obj, QEvent* event)
{
    if (event && event->type() == QEvent::Leave && m_hoveredIndex.isValid()) {
        const QModelIndex prev = m_hoveredIndex;
        m_hoveredIndex = QModelIndex();
        if (auto* view = qobject_cast<QAbstractItemView*>(obj->parent())) {
            view->update(prev);
        }
        if (auto* w = qobject_cast<QWidget*>(obj))
            w->setCursor(Qt::ArrowCursor);
        emit avatarHoverLeft();
    }
    return QStyledItemDelegate::eventFilter(obj, event);
}

void ConversationCardDelegate::setSelectedConversationId(const QString& id)
{
    m_selectedConvId = id;
}

QRect ConversationCardDelegate::computeAvatarRect(const QRect& cardRect, bool /*selected*/) const
{
    const int innerLeft = cardRect.x() + kCardPaddingH;
    const int innerTop = cardRect.y() + kCardPaddingV;
    const int innerH = cardRect.height() - 2 * kCardPaddingV;
    // selectionBar 总是占位
    const int avatarX = innerLeft + kSelBarWidth + kCardSpacing;
    const int avatarY = innerTop + (innerH - kAvatarSize) / 2;
    return QRect(avatarX, avatarY, kAvatarSize, kAvatarSize);
}
