#pragma once

#include <QString>

QString databasePath();
QString avatarDirectoryPath();
QString avatarStoragePathForClient(const QString& clientId);
QString ensureIncomingFilesDirectory();
QString sanitizeSenderDirectoryName(const QString& senderName);
QString ensureIncomingFilesDirectoryForSender(const QString& senderName);
QString uniqueFilePath(const QString& directoryPath, const QString& requestedName);
