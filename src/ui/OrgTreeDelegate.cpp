#include "OrgTreeDelegate.h"
#include "ui/AppStyle.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>

namespace {

constexpr int kMemberHeight = 52;
constexpr int kDeptHeight = 36;
constexpr int kAvatarSize = 36;
constexpr int kLeftPadding = 8;
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
    const QString cacheKey = QStringLiteral("org-letter|%1|%2|%3").arg(letter, bg.name()).arg(size);

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
    f.setPixelSize(qMax(11, size / 3));
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, letter);
    p.end();

    QPixmapCache::insert(cacheKey, pm);
    return pm;
}

QPixmap offlineLetterAvatar(const QString& name, const QColor& bg, int size)
{
    const QString cacheKey = QStringLiteral("org-off|%1|%2|%3")
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

OrgTreeDelegate::OrgTreeDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void OrgTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                            const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    const AppStyle::ThemeMode mode = AppStyle::currentThemeMode();
    const QRect rect = option.rect;

    // 判断是否为部门节点（父节点：没有 parent 或 parent 是 invisible root）
    const bool isDepartment = !index.parent().isValid();

    if (isDepartment) {
        // ── 部门行：粗体文本 + 人数统计 ──
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(AppStyle::hoverBg(mode)));
            painter->drawRoundedRect(rect.adjusted(2, 1, -2, -1), 6, 6);
        }

        QFont deptFont = option.font;
        deptFont.setPixelSize(14);
        deptFont.setBold(true);
        painter->setFont(deptFont);
        painter->setPen(QColor(AppStyle::textPrimary(mode)));
        painter->drawText(rect.adjusted(kLeftPadding + 4, 0, -8, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(Qt::DisplayRole).toString());
        painter->restore();
        return;
    }

    // ── 成员行：头像卡片式 ──
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;

    // 绘制悬停/选中背景（半透明，配合Ela主题）
    if (selected || hovered) {
        QColor bgColor = selected
            ? QColor(AppStyle::selectedBg(mode))
            : QColor(AppStyle::hoverBg(mode));
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawRoundedRect(rect.adjusted(4, 1, -4, -1), 8, 8);
    }

    const QString clientId = index.data(ClientIdRole).toString();
    const QString displayName = index.data(DisplayNameRole).toString();
    const QString jobTitle = index.data(JobTitleRole).toString();
    const bool isOnline = index.data(IsOnlineRole).toBool();

    // 头像
    const int avatarX = rect.x() + kLeftPadding + 4;
    const int avatarY = rect.y() + (rect.height() - kAvatarSize) / 2;
    const QColor avatarBg = avatarColorFor(clientId);
    const QPixmap avatar = isOnline
        ? letterAvatar(displayName, avatarBg, kAvatarSize)
        : offlineLetterAvatar(displayName, avatarBg, kAvatarSize);
    painter->drawPixmap(avatarX, avatarY, avatar);

    // 在线状态小圆点（右下角叠加在头像上）
    const int dotSize = 10;
    const int dotX = avatarX + kAvatarSize - dotSize + 1;
    const int dotY = avatarY + kAvatarSize - dotSize + 1;
    const QColor dotColor = isOnline ? QColor(0x35, 0xD0, 0x7F) : QColor(AppStyle::textMuted(mode));
    // 白色边框
    painter->setPen(Qt::NoPen);
    painter->setBrush(AppStyle::isDarkTheme(mode) ? QColor(30, 34, 40) : Qt::white);
    painter->drawEllipse(dotX - 1, dotY - 1, dotSize + 2, dotSize + 2);
    painter->setBrush(dotColor);
    painter->drawEllipse(dotX, dotY, dotSize, dotSize);

    // 文本区域
    const int textX = avatarX + kAvatarSize + kAvatarGap;
    const int textW = rect.right() - textX - 12;

    // 第一行：姓名
    const int row1Y = rect.y() + 8;
    QFont nameFont = option.font;
    nameFont.setPixelSize(14);
    nameFont.setBold(false);
    painter->setFont(nameFont);
    painter->setPen(QColor(isOnline ? AppStyle::textPrimary(mode) : AppStyle::textSecondary(mode)));
    const QFontMetrics nameFm(nameFont);
    const QString fallbackName = displayName.trimmed().isEmpty() ? clientId : displayName;
    const QString elidedName = nameFm.elidedText(fallbackName, Qt::ElideRight, textW);
    painter->drawText(textX, row1Y, textW, 20, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    // 第二行：职位
    const int row2Y = rect.y() + 28;
    QFont jobFont = option.font;
    jobFont.setPixelSize(12);
    painter->setFont(jobFont);
    painter->setPen(QColor(AppStyle::textMuted(mode)));
    const QString jobDisplay = jobTitle.isEmpty()
        ? (isOnline ? QStringLiteral("\u5728\u7EBF") : QStringLiteral("\u79BB\u7EBF"))
        : jobTitle;
    const QFontMetrics jobFm(jobFont);
    const QString elidedJob = jobFm.elidedText(jobDisplay, Qt::ElideRight, textW);
    painter->drawText(textX, row2Y, textW, 18, Qt::AlignLeft | Qt::AlignVCenter, elidedJob);

    painter->restore();
}

QSize OrgTreeDelegate::sizeHint(const QStyleOptionViewItem& /*option*/,
                                const QModelIndex& index) const
{
    const bool isDepartment = !index.parent().isValid();
    return QSize(0, isDepartment ? kDeptHeight : kMemberHeight);
}
