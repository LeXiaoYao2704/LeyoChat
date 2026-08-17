#include "ui/ConversationListDelegate.h"

#include "ui/AppStyle.h"
#include "ui/ConversationListModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QStyleOptionViewItem>

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
    for (const QChar ch : seed) {
        hash = (hash * 31 + ch.unicode()) & 0x7FFF'FFFF;
    }
    return palette[hash % (sizeof(palette) / sizeof(palette[0]))];
}

void drawTag(QPainter* painter, QRect rect, const QString& text, const QColor& bg, const QColor& fg)
{
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(bg);
    painter->drawRoundedRect(rect, 7, 7);
    painter->setPen(fg);
    painter->drawText(rect, Qt::AlignCenter, text);
    painter->restore();
}

} // namespace

ConversationListDelegate::ConversationListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize ConversationListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                         const QModelIndex& /*index*/) const
{
    return {0, AppStyle::conversationRowHeightForFont(option.font)};
}

void ConversationListDelegate::paint(QPainter* painter,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    // — 扁平行背景（不再使用卡片）—
    if (selected) {
        painter->fillRect(option.rect, QColor(AppStyle::selectedBg()));
    } else if (hovered) {
        painter->fillRect(option.rect, QColor(AppStyle::hoverBg()));
    }

    // 选中指示条（左侧 3px 蓝色竖线）
    if (selected) {
        const QRect railRect(option.rect.left() + 2,
                             option.rect.top() + 8,
                             3,
                             option.rect.height() - 16);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(AppStyle::accent()));
        painter->drawRoundedRect(railRect, 1.5, 1.5);
    }

    const QString title = index.data(ConversationListModel::TitleRole).toString();
    const QString preview = index.data(ConversationListModel::PreviewRole).toString();
    const QString timeLabel = index.data(ConversationListModel::TimeLabelRole).toString();
    const bool hasUnread = index.data(ConversationListModel::HasUnreadRole).toBool();
    const bool isPinned = index.data(ConversationListModel::IsPinnedRole).toBool();
    const bool isStarred = index.data(ConversationListModel::IsStarredRole).toBool();
    const bool isMuted = index.data(ConversationListModel::IsMutedRole).toBool();
    const bool hasMentionMe = index.data(ConversationListModel::HasMentionMeRole).toBool();
    const QString avatarPath = index.data(ConversationListModel::AvatarPathRole).toString();

    const int leftPad = 14;
    const int rightPad = 14;
    const int avatarSize = AppStyle::avatarSizeForFont(option.font);
    const QRect avatarRect(option.rect.left() + leftPad,
                           option.rect.top() + (option.rect.height() - avatarSize) / 2,
                           avatarSize,
                           avatarSize);

    bool avatarDrawn = false;
    if (!avatarPath.isEmpty()) {
        QPixmap pix;
        const QString cacheKey = QStringLiteral("convAvatar:%1:%2").arg(avatarPath).arg(avatarSize);
        if (!QPixmapCache::find(cacheKey, &pix)) {
            QPixmap raw(avatarPath);
            if (!raw.isNull()) {
                pix = raw.scaled(avatarSize, avatarSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                QPixmapCache::insert(cacheKey, pix);
            }
        }
        if (!pix.isNull()) {
            painter->save();
            QPainterPath clipPath;
            clipPath.addEllipse(avatarRect);
            painter->setClipPath(clipPath);
            painter->drawPixmap(avatarRect.x(), avatarRect.y(), avatarSize, avatarSize, pix);
            painter->restore();
            avatarDrawn = true;
        }
    }

    if (!avatarDrawn) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(avatarColor(title));
        painter->drawEllipse(avatarRect);

        QFont avatarFont = option.font;
        avatarFont.setBold(true);
        avatarFont.setPixelSize(avatarSize >= 44 ? 17 : 15);
        painter->setFont(avatarFont);
        painter->setPen(Qt::white);
        const QString avatarText = title.trimmed().isEmpty() ? QStringLiteral("?")
                                                             : QString(title.trimmed().front()).toUpper();
        painter->drawText(avatarRect, Qt::AlignCenter, avatarText);
    }

    const int textLeft = avatarRect.right() + 10;
    QFont titleFont = AppStyle::titleFont(option.font);
    titleFont.setPointSizeF(qMax(10.5, titleFont.pointSizeF() - 0.5));
    const QFontMetrics titleMetrics(titleFont);
    QFont metaFont = AppStyle::captionFont(option.font);
    const QFontMetrics metaMetrics(metaFont);
    const int metaWidth = qBound(28, metaMetrics.horizontalAdvance(timeLabel) + 6, 42);
    const int textWidth = qMax(72, option.rect.width() - (textLeft - option.rect.left()) - rightPad - metaWidth);

    const int contentTop =
        option.rect.top()
        + qMax(AppStyle::kSpace8,
               (option.rect.height() - (titleMetrics.height() + AppStyle::kSpace4 + metaMetrics.height())) / 2);

    painter->setFont(titleFont);
    painter->setPen(QColor(AppStyle::textPrimary()));
    const QRect titleRect(textLeft, contentTop, textWidth, titleMetrics.height());
    painter->drawText(titleRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(titleFont).elidedText(title, Qt::ElideRight, textWidth));

    painter->setFont(metaFont);
    painter->setPen(QColor(AppStyle::textMuted()));
    const QRect timeRect(option.rect.right() - rightPad - metaWidth,
                         contentTop,
                         metaWidth,
                         metaMetrics.height());
    painter->drawText(timeRect, Qt::AlignRight | Qt::AlignVCenter, timeLabel);

    const int badgeLaneWidth = 44;
    const int previewWidth = qMax(64, option.rect.width() - (textLeft - option.rect.left()) - rightPad - badgeLaneWidth);
    const QRect previewRect(textLeft,
                            contentTop + titleMetrics.height() + AppStyle::kSpace4,
                            previewWidth,
                            metaMetrics.height());

    const QString draftText = index.data(ConversationListModel::DraftTextRole).toString();

    if (!draftText.isEmpty()) {
        // 草稿模式：显示 "[草稿] 内容" ，前缀用橙色
        const QString draftTag = QStringLiteral("[\u8349\u7A3F] ");
        const int tagWidth = QFontMetrics(metaFont).horizontalAdvance(draftTag);

        painter->setPen(QColor(AppStyle::warning()));
        painter->drawText(previewRect, Qt::AlignLeft | Qt::AlignVCenter, draftTag);

        const QRect remainingRect(previewRect.left() + tagWidth,
                                  previewRect.top(),
                                  previewRect.width() - tagWidth,
                                  previewRect.height());
        painter->setPen(QColor(AppStyle::textMuted()));
        painter->drawText(remainingRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(metaFont).elidedText(draftText, Qt::ElideRight, remainingRect.width()));
    } else if (hasMentionMe && hasUnread) {
        // 当 hasMentionMe 为 true 时，在预览文本前加 "[有人@我] " 高亮前缀
        const QString mentionTag = QStringLiteral("[\u6709\u4EBA@\u6211] ");
        const int tagWidth = QFontMetrics(metaFont).horizontalAdvance(mentionTag);

        painter->setPen(QColor(AppStyle::danger()));
        painter->drawText(previewRect, Qt::AlignLeft | Qt::AlignVCenter, mentionTag);

        const QRect remainingRect(previewRect.left() + tagWidth,
                                  previewRect.top(),
                                  previewRect.width() - tagWidth,
                                  previewRect.height());
        painter->setPen(QColor(AppStyle::textMuted()));
        painter->drawText(remainingRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(metaFont).elidedText(preview, Qt::ElideRight, remainingRect.width()));
    } else {
        painter->drawText(previewRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(metaFont).elidedText(preview, Qt::ElideRight, previewWidth));
    }

    int badgeRight = option.rect.right() - rightPad;
    if (hasUnread) {
        const int dotSize = qMax(10, metaMetrics.height() - 2);
        const QRect dotRect(badgeRight - dotSize,
                            previewRect.center().y() - dotSize / 2,
                            dotSize,
                            dotSize);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(AppStyle::danger()));
        painter->drawEllipse(dotRect);
        badgeRight -= 18;
    }

    painter->setFont(metaFont);
    const int chipHeight = qMax(18, metaMetrics.height() + 6);
    const int chipY = previewRect.center().y() - chipHeight / 2;
    if (isMuted) {
        const QRect rect(badgeRight - 24, chipY, 24, chipHeight);
        drawTag(painter, rect, QStringLiteral("\u9759"), QColor(AppStyle::surfaceAlt()), QColor(AppStyle::textMuted()));
        badgeRight -= 30;
    }
    if (isStarred) {
        const QRect rect(badgeRight - 24, chipY, 24, chipHeight);
        drawTag(painter, rect, QStringLiteral("\u661F"), QColor(0xFE, 0xF4, 0xDA), QColor(0xAF, 0x73, 0x10));
        badgeRight -= 30;
    }
    if (isPinned) {
        const QRect rect(badgeRight - 24, chipY, 24, chipHeight);
        drawTag(painter, rect, QStringLiteral("\u9876"), QColor(AppStyle::accentSoft()), QColor(AppStyle::accent()));
    }

    if (!selected) {
        // separator line removed for Echo-style flat design
    }

    painter->restore();
}
