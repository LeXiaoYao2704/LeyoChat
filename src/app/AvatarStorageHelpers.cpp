#include "app/AvatarStorageHelpers.h"

#include "app/AppPathHelpers.h"
#include "app/AppSettings.h"

#include <QBuffer>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QSettings>

static QString s_cachedAvatarBase64;
static QString s_cachedAvatarPath;
static QHash<QString, QString> s_avatarPathCache;

QString avatarBase64ForImagePath(const QString& imagePath) {
    if (imagePath.trimmed().isEmpty()) {
        return {};
    }

    if (imagePath == s_cachedAvatarPath && !s_cachedAvatarBase64.isEmpty()) {
        return s_cachedAvatarBase64;
    }

    QImage image(imagePath);
    if (image.isNull()) {
        return {};
    }

    image = image.scaled(72,
                         72,
                         Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return {};
    }

    s_cachedAvatarPath = imagePath;
    s_cachedAvatarBase64 = QString::fromLatin1(pngBytes.toBase64());
    return s_cachedAvatarBase64;
}

void invalidateAvatarCache(const QString& clientId) {
    s_avatarPathCache.remove(clientId.trimmed());
}

void invalidateAvatarImageCache()
{
    s_cachedAvatarBase64.clear();
    s_cachedAvatarPath.clear();
}

QString persistAvatarBase64ForClient(const QString& clientId, const QString& avatarBase64) {
    if (clientId.trimmed().isEmpty() || avatarBase64.trimmed().isEmpty()) {
        return {};
    }

    const QByteArray bytes = QByteArray::fromBase64(avatarBase64.toLatin1());
    if (bytes.isEmpty()) {
        return {};
    }

    QImage image;
    if (!image.loadFromData(bytes, "PNG")) {
        return {};
    }

    const QString storedAvatarPath = avatarStoragePathForClient(clientId);
    if (!image.save(storedAvatarPath, "PNG")) {
        return {};
    }

    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    cfg.setValue(QStringLiteral("avatar/") + clientId, storedAvatarPath);
    cfg.sync();
    invalidateAvatarCache(clientId);
    return storedAvatarPath;
}

QString cachedAvatarPathForClient(const QString& clientId) {
    if (clientId.trimmed().isEmpty()) {
        return {};
    }
    const QString key = clientId.trimmed();
    auto it = s_avatarPathCache.constFind(key);
    if (it != s_avatarPathCache.constEnd()) {
        return it.value();
    }
    QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
    const QString path = cfg.value(QStringLiteral("avatar/") + key).toString().trimmed();
    const QString result = QFileInfo::exists(path) ? path : QString();
    s_avatarPathCache.insert(key, result);
    return result;
}
