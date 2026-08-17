#pragma once

#include <QString>

QString avatarBase64ForImagePath(const QString& imagePath);
void invalidateAvatarCache(const QString& clientId);
void invalidateAvatarImageCache();
QString persistAvatarBase64ForClient(const QString& clientId, const QString& avatarBase64);
QString cachedAvatarPathForClient(const QString& clientId);
