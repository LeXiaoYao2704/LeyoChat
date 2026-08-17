#include "app/AppPathHelpers.h"

#include "app/AppSettings.h"
#include "app/TestModeContext.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

QString databasePath() {
    const TestModeContext testModeContext = TestModeContext::current();
    const QString basePath = testModeContext.appDataRoot().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        : testModeContext.appDataRoot();
    if (basePath.isEmpty()) {
        return QStringLiteral("leyochat.db");
    }

    QDir().mkpath(basePath);
    return QDir(basePath).filePath(QStringLiteral("leyochat.db"));
}

QString avatarDirectoryPath() {
    const TestModeContext testModeContext = TestModeContext::current();
    QString basePath = testModeContext.appDataRoot();
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (basePath.isEmpty()) {
        basePath = QDir::currentPath();
    }
    const QString avatarRoot = QDir(basePath).filePath(QStringLiteral("avatars"));
    QDir().mkpath(avatarRoot);
    return avatarRoot;
}

QString avatarStoragePathForClient(const QString& clientId) {
    const QString safeClientId = clientId.trimmed().isEmpty()
        ? QStringLiteral("unknown")
        : clientId.trimmed().replace(QRegularExpression(QStringLiteral(R"([^A-Za-z0-9._-])")),
                                     QStringLiteral("_"));
    return QDir(avatarDirectoryPath()).filePath(QStringLiteral("peer-%1.png").arg(safeClientId));
}

QString ensureIncomingFilesDirectory() {
    QString basePath = TestModeContext::current().incomingFilesDirectoryPath();
    if (!basePath.isEmpty()) {
        QDir().mkpath(basePath);
        return basePath;
    }

    {
        QSettings cfg(AppSettings::organizationName(), AppSettings::applicationName());
        const QString customPath = cfg.value(QStringLiteral("file/incomingFilesPath")).toString().trimmed();
        if (!customPath.isEmpty()) {
            QDir().mkpath(customPath);
            return customPath;
        }
    }

    basePath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    if (basePath.isEmpty()) {
        basePath = QDir::currentPath();
    }

    QDir directory(basePath);
    directory.mkpath(QStringLiteral("LeyoChat/Received"));
    return directory.filePath(QStringLiteral("LeyoChat/Received"));
}

QString sanitizeSenderDirectoryName(const QString& senderName)
{
    QString safeName = senderName.trimmed();
    if (safeName.isEmpty()) {
        safeName = QStringLiteral("unknown");
    }
    safeName.replace(QRegularExpression(QStringLiteral(R"([\\/:*?"<>|\x00-\x1F])")),
                     QStringLiteral("_"));
    while (!safeName.isEmpty() && (safeName.endsWith(QLatin1Char('.'))
                                   || safeName.endsWith(QLatin1Char(' ')))) {
        safeName.chop(1);
    }
    if (safeName.isEmpty()) {
        safeName = QStringLiteral("unknown");
    }
    return safeName;
}

QString ensureIncomingFilesDirectoryForSender(const QString& senderName) {
    const QString rootPath = ensureIncomingFilesDirectory();
    QDir rootDir(rootPath);
    const QString senderFolder = sanitizeSenderDirectoryName(senderName);
    rootDir.mkpath(senderFolder);
    return rootDir.filePath(senderFolder);
}

QString uniqueFilePath(const QString& directoryPath, const QString& requestedName) {
    QFileInfo info(requestedName);
    const QString completeName = info.fileName().trimmed().isEmpty() ? QStringLiteral("received.bin")
                                                                      : info.fileName();
    QDir directory(directoryPath);
    QString candidate = directory.filePath(completeName);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }

    const QString baseName = QFileInfo(completeName).completeBaseName();
    const QString suffix = QFileInfo(completeName).suffix();
    int index = 1;
    while (true) {
        const QString numberedName = suffix.isEmpty()
                                         ? QStringLiteral("%1_%2").arg(baseName, QString::number(index))
                                         : QStringLiteral("%1_%2.%3").arg(baseName, QString::number(index), suffix);
        candidate = directory.filePath(numberedName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
        ++index;
    }
}
