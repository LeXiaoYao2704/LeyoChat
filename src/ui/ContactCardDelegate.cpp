#include "ContactCardDelegate.h"
#include "ui/AppStyle.h"
#include "ui/ContactListModel.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace {

constexpr int kCardHeight = 76;
constexpr int kSectionHeight = 28;
constexpr int kAvatarSize = 44;
constexpr int kLeftPadding = 12;
constexpr int kAvatarGap = 10;

static const QColor kAvatarPalette[] = {
    QColor(0x52, 0x73, 0xE8), QColor(0x2F, 0xA4, 0x84),
    QColor(0xD9, 0x96, 0x3A), QColor(0x7B, 0x68, 0xE6),
    QColor(0xD8, 0x5A, 0x9A), QColor(0x32, 0x96, 0xC4),
};

QColor avatarColorFor(const QString& seed)
{
    return kAvatarPalette[qHash(seed) % 6];
}

QPixmap letterAvatar(const QString& name, const QColor& bg, int size)
{
    const QString trimmed = name.trimmed();
    const QString letter = trimmed.isEmpty() ? QStringLiteral("?") : trimmed.left(1).toUpper();
    const QString cacheKey = QStringLiteral("ccd-letter|%1|%2|%3").arg(letter, bg.name()).arg(size);

    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) return cached;

    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(0, 0, size, size);

    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(qMax(12, size / 3));
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, letter);
    p.end();

    QPixmapCache::insert(cacheKey, pm);
    return pm;
}

QPixmap offlineLetterAvatar(const QString& name, const QColor& bg, int size)
{
    const QString cacheKey = QStringLiteral("ccd-off|%1|%2|%3")
                                 .arg(name.trimmed().left(1).toUpper(), bg.name()).arg(size);
    QPixmap cached;
    if (QPixmapCache::find(cacheKey, &cached)) return cached;

    QImage img = letterAvatar(name, bg, size).toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < img.height(); ++y) {
        auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const int gray = qGray(line[x]);
            line[x] = qRgba(gray, gray, gray, qAlpha(line[x]));
        }
    }

    QPixmap result = QPixmap::fromImage(img);
    QPixmapCache::insert(cacheKey, result);
    return result;
}

} // namespace

ContactCardDelegate::ContactCardDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ContactCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    const QRect rect = option.rect;

    if (index.data(ContactListModel::IsSectionHeaderRole).toBool()) {
        const QString section = index.data(ContactListModel::SectionRole).toString();
        QFont sectionFont = option.font;
        sectionFont.setPixelSize(12);
        sectionFont.setBold(true);
        painter->setFont(sectionFont);
        painter->setPen(QColor(AppStyle::textMuted(mode)));
        painter->drawText(rect.adjusted(kLeftPadding, 0, 0, 0),
                          Qt::AlignLeft | Qt::AlignVCenter, section);
        painter->restore();
        return;
    }

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    const QRect cardRect = rect.adjusted(6, 5, -8, -5);
    const QColor baseCardBg = AppStyle::isDarkTheme(mode)
        ? QColor(25, 42, 55, 105)
        : QColor(255, 255, 255, 70);
    QColor cardBg = baseCardBg;
    QColor cardBorder(AppStyle::border(mode));
    if (selected) {
        cardBg = AppStyle::isDarkTheme(mode)
            ? QColor(25, 42, 55, 135)
            : QColor(255, 255, 255, 96);
        cardBorder = QColor(AppStyle::accent(mode));
    } else if (hovered) {
        cardBg = AppStyle::isDarkTheme(mode)
            ? QColor(25, 42, 55, 125)
            : QColor(255, 255, 255, 86);
        cardBorder = QColor(AppStyle::accent(mode));
    }

    painter->setPen(QPen(cardBorder, selected || hovered ? 1.2 : 1.0));
    painter->setBrush(cardBg);
    painter->drawRoundedRect(cardRect, 8, 8);

    const QString clientId = index.data(ContactListModel::ClientIdRole).toString();
    const QString displayName = index.data(ContactListModel::DisplayNameRole).toString();
    const QString host = index.data(ContactListModel::HostRole).toString();
    const int port = index.data(ContactListModel::PortRole).toInt();
    const int presence = index.data(ContactListModel::PresenceRole).toInt();
    const bool isFavorite = index.data(ContactListModel::IsFavoriteRole).toBool();

    const bool isOnline = (presence == 0);
    const bool isAway = (presence == 1);

    const int avatarX = rect.x() + kLeftPadding + 6;
    const int avatarY = rect.y() + (rect.height() - kAvatarSize) / 2;
    const QColor avatarBg = avatarColorFor(clientId);
    const QPixmap avatar = isOnline || isAway
        ? letterAvatar(displayName, avatarBg, kAvatarSize)
        : offlineLetterAvatar(displayName, avatarBg, kAvatarSize);
    painter->drawPixmap(avatarX, avatarY, avatar);

    const int textX = avatarX + kAvatarSize + kAvatarGap;
    const int textW = rect.right() - textX - 54;
    const int row1Y = rect.y() + 14;
    const int row2Y = rect.y() + 42;

    QFont nameFont = option.font;
    nameFont.setPixelSize(14);
    nameFont.setBold(true);
    painter->setFont(nameFont);
    painter->setPen(QColor(AppStyle::textPrimary(mode)));
    const QFontMetrics nameFm(nameFont);
    const QString fallbackName = displayName.trimmed().isEmpty() ? clientId : displayName;
    const QString elidedName = nameFm.elidedText(fallbackName, Qt::ElideRight, textW - 92);
    painter->drawText(textX, row1Y, textW, 20, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    const QString statusText = isOnline ? QStringLiteral("\u5728\u7EBF")
                            : isAway ? QStringLiteral("\u79BB\u5F00")
                                     : QStringLiteral("\u79BB\u7EBF");
    const QColor statusColor = isOnline ? QColor(0x35, 0xD0, 0x7F)
                             : isAway ? QColor(0xE5, 0xAA, 0x28)
                                      : QColor(AppStyle::textMuted(mode));
    int statusX = textX + nameFm.horizontalAdvance(elidedName) + 10;
    const int maxStatusX = textX + textW - 64;
    statusX = qMin(statusX, maxStatusX);

    painter->setPen(Qt::NoPen);
    painter->setBrush(statusColor);
    painter->drawEllipse(QRect(statusX, row1Y + 7, 6, 6));

    QFont statusFont = option.font;
    statusFont.setPixelSize(12);
    painter->setFont(statusFont);
    painter->setPen(statusColor);
    painter->drawText(statusX + 10, row1Y, 42, 20, Qt::AlignLeft | Qt::AlignVCenter, statusText);

    if (isFavorite) {
        QFont starFont = option.font;
        starFont.setPixelSize(14);
        painter->setFont(starFont);
        painter->setPen(QColor(0xF5, 0xA6, 0x23));
        painter->drawText(statusX + 54, row1Y, 20, 20, Qt::AlignCenter, QStringLiteral("\u2605"));
    }

    QFont metaFont = option.font;
    metaFont.setPixelSize(12);
    painter->setFont(metaFont);
    painter->setPen(QColor(AppStyle::textMuted(mode)));
    const QString meta = port > 0 ? QStringLiteral("%1:%2").arg(host).arg(port) : host;
    painter->drawText(textX, row2Y, textW, 18, Qt::AlignLeft | Qt::AlignVCenter,
                      meta.trimmed().isEmpty() ? clientId : meta);

    QFont moreFont = option.font;
    moreFont.setPixelSize(18);
    moreFont.setBold(true);
    painter->setFont(moreFont);
    painter->setPen(QColor(AppStyle::textMuted(mode)));
    painter->drawText(rect.adjusted(0, 0, -18, 0),
                      Qt::AlignRight | Qt::AlignVCenter,
                      QStringLiteral("\u22EE"));

    painter->restore();
}

QSize ContactCardDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                    const QModelIndex& index) const
{
    if (index.data(ContactListModel::IsSectionHeaderRole).toBool())
        return QSize(0, kSectionHeight);
    return QSize(0, kCardHeight);
}
