#include "ui/ContactListDelegate.h"

#include "app/AppSettings.h"

#include "ui/AppStyle.h"
#include "ui/ContactListModel.h"

#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QSettings>

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

} // namespace

ContactListDelegate::ContactListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize ContactListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    // 分组头行高度较小
    if (index.data(ContactListModel::IsSectionHeaderRole).toBool()) {
        return {0, qMax(26, QFontMetrics(AppStyle::captionFont(option.font)).height() + 12)};
    }
    return {0, AppStyle::contactRowHeightForFont(option.font)};
}

void ContactListDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // ===== 分组头绘制 =====
    if (index.data(ContactListModel::IsSectionHeaderRole).toBool()) {
        const QString section = index.data(ContactListModel::SectionRole).toString();
        QFont sectionFont = AppStyle::captionFont(option.font);
        sectionFont.setBold(true);
        const QFontMetrics fm(sectionFont);
        painter->setFont(sectionFont);
        painter->setPen(QColor(AppStyle::textMuted()));
        painter->drawText(QRect(option.rect.left() + 14,
                                option.rect.top(),
                                option.rect.width() - 28,
                                option.rect.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, section);
        // 分组头右侧淡色分隔线 removed for Echo-style flat design
        painter->restore();
        return;
    }

    // ===== 普通联系人行 =====
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    if (selected) {
        painter->fillRect(option.rect, QColor(AppStyle::selectedBg()));
    } else if (hovered) {
        painter->fillRect(option.rect, QColor(AppStyle::hoverBg()));
    }

    if (selected) {
        const QRect railRect(option.rect.left() + 2,
                             option.rect.top() + 8,
                             3,
                             option.rect.height() - 16);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(AppStyle::accent()));
        painter->drawRoundedRect(railRect, 1.5, 1.5);
    }

    const QString displayName = index.data(ContactListModel::DisplayNameRole).toString();
    const QString statusText = index.data(ContactListModel::StatusTextRole).toString();
    const QString clientId = index.data(ContactListModel::ClientIdRole).toString();
    const QString hostText = index.data(ContactListModel::HostRole).toString();
    const bool isFavorite = index.data(ContactListModel::IsFavoriteRole).toBool();
    const bool isOnline = statusText == QStringLiteral("\u5728\u7EBF");
    const bool isAway = statusText == QStringLiteral("\u79bb\u5f00");

    const int leftPad = 14;
    const int rightPad = 14;
    const int avatarSize = AppStyle::avatarSizeForFont(option.font);
    const QRect avatarRect(option.rect.left() + leftPad,
                           option.rect.top() + (option.rect.height() - avatarSize) / 2,
                           avatarSize,
                           avatarSize);

    // 缓存头像路径，避免在 paint() 中每行都读 QSettings（Windows 注册表 I/O）
    static QHash<QString, QString> sAvatarPathCache;
    QString avatarPath;
    {
        auto it = sAvatarPathCache.constFind(clientId);
        if (it != sAvatarPathCache.constEnd()) {
            avatarPath = it.value();
        } else {
            QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
            avatarPath = cfg.value(QStringLiteral("avatar/") + clientId).toString();
            sAvatarPathCache.insert(clientId, avatarPath);
        }
    }
    bool drewCustomAvatar = false;
    if (!avatarPath.isEmpty() && QFileInfo::exists(avatarPath)) {
        const QString cacheKey = QStringLiteral("contact_") + avatarPath;
        QPixmap pixmap;
        if (!QPixmapCache::find(cacheKey, &pixmap)) {
            QPixmap raw(avatarPath);
            if (!raw.isNull()) {
                pixmap = raw.scaled(avatarSize,
                                    avatarSize,
                                    Qt::KeepAspectRatioByExpanding,
                                    Qt::SmoothTransformation)
                             .copy(0, 0, avatarSize, avatarSize);
                QPixmapCache::insert(cacheKey, pixmap);
            }
        }

        if (!pixmap.isNull()) {
            QPainterPath clipPath;
            clipPath.addEllipse(avatarRect);
            painter->save();
            painter->setClipPath(clipPath);
            painter->drawPixmap(avatarRect, pixmap);
            painter->restore();
            drewCustomAvatar = true;
        }
    }

    if (!drewCustomAvatar) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(avatarColor(clientId.isEmpty() ? displayName : clientId));
        painter->drawEllipse(avatarRect);

        QFont avatarFont = option.font;
        avatarFont.setBold(true);
        avatarFont.setPixelSize(avatarSize >= 44 ? 17 : 15);
        painter->setFont(avatarFont);
        painter->setPen(Qt::white);
        const QString initial = displayName.trimmed().isEmpty() ? QStringLiteral("?")
                                                                : QString(displayName.trimmed().front()).toUpper();
        painter->drawText(avatarRect, Qt::AlignCenter, initial);
    }

    const QRect onlineDotRect(avatarRect.right() - 8, avatarRect.bottom() - 8, 10, 10);
    painter->setPen(QColor(AppStyle::surface()));
    painter->setBrush(isOnline ? QColor(AppStyle::success())
                               : isAway ? QColor(0xF0, 0xB4, 0x29)
                                        : QColor(AppStyle::surfaceMuted()));
    painter->drawEllipse(onlineDotRect);

    const int textLeft = avatarRect.right() + 10;
    const int chipWidth = 60;
    const int textWidth = option.rect.width() - (textLeft - option.rect.left()) - rightPad - chipWidth;

    QFont titleFont = AppStyle::titleFont(option.font);
    titleFont.setPointSizeF(qMax(11.0, titleFont.pointSizeF()));
    const QFontMetrics titleMetrics(titleFont);
    QFont metaFont = AppStyle::captionFont(option.font);
    const QFontMetrics metaMetrics(metaFont);
    const int contentTop =
        option.rect.top()
        + qMax(AppStyle::kSpace10,
               (option.rect.height() - (titleMetrics.height() + AppStyle::kSpace4 + metaMetrics.height())) / 2);

    painter->setFont(titleFont);
    painter->setPen(QColor(AppStyle::textPrimary()));
    painter->drawText(QRect(textLeft, contentTop, textWidth, titleMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(titleFont).elidedText(displayName, Qt::ElideRight, textWidth));

    painter->setFont(metaFont);
    painter->setPen(QColor(AppStyle::textMuted()));
    // 副文本：显示 IP 地址而非重复的状态文本
    const QString subtitle = hostText.isEmpty() ? clientId : hostText;
    painter->drawText(QRect(textLeft,
                            contentTop + titleMetrics.height() + AppStyle::kSpace4,
                            textWidth,
                            metaMetrics.height()),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(metaFont).elidedText(subtitle, Qt::ElideRight, textWidth));

    // 收藏星标（名称左侧）
    if (isFavorite) {
        QFont starFont = option.font;
        starFont.setPixelSize(12);
        painter->setFont(starFont);
        painter->setPen(QColor(0xF5, 0x9E, 0x0B));
        painter->drawText(QRect(textLeft + QFontMetrics(titleFont).horizontalAdvance(
                                    QFontMetrics(titleFont).elidedText(displayName, Qt::ElideRight, textWidth)) + 4,
                                contentTop, 16, titleMetrics.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QStringLiteral("\u2605"));
    }

    const int chipHeight = qMax(22, metaMetrics.height() + 8);
    const QRect chipRect(option.rect.right() - rightPad - chipWidth,
                         option.rect.center().y() - chipHeight / 2,
                         chipWidth,
                         chipHeight);
    painter->setPen(Qt::NoPen);
    if (isOnline) {
        painter->setBrush(QColor(AppStyle::accentSoft()));
    } else if (isAway) {
        painter->setBrush(QColor(AppStyle::hoverBg()));
    } else {
        painter->setBrush(QColor(AppStyle::surfaceAlt()));
    }
    painter->drawRoundedRect(chipRect, chipHeight / 2, chipHeight / 2);
    if (isOnline) {
        painter->setPen(QColor(AppStyle::success()));
    } else if (isAway) {
        painter->setPen(QColor(AppStyle::warning()));
    } else {
        painter->setPen(QColor(AppStyle::textMuted()));
    }
    painter->drawText(chipRect, Qt::AlignCenter, statusText);

    if (!selected) {
        // separator line removed for Echo-style flat design
    }

    painter->restore();
}
