#include "app/FileOpenHelpers.h"

#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

bool openFilePath(const QString& filePath) {
    if (filePath.trimmed().isEmpty()) {
        qWarning() << "[FileOpen] openFilePath: path is empty";
        return false;
    }
    if (!QFileInfo::exists(filePath)) {
        qWarning() << "[FileOpen] openFilePath: file does not exist:" << filePath;
        return false;
    }
    qInfo() << "[FileOpen] openFilePath:" << filePath
            << "size=" << QFileInfo(filePath).size();

    if (QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
        return true;
    }

#ifdef Q_OS_WIN
    return QProcess::startDetached(QStringLiteral("cmd.exe"),
                                   {QStringLiteral("/c"),
                                    QStringLiteral("start"),
                                    QString(),
                                    QDir::toNativeSeparators(filePath)});
#else
    return false;
#endif
}

bool openParentDirectory(const QString& filePath) {
    const QFileInfo info(filePath);
    if (filePath.trimmed().isEmpty()) {
        qWarning() << "[FileOpen] openParentDirectory: path is empty";
        return false;
    }
    if (!info.exists()) {
        qWarning() << "[FileOpen] openParentDirectory: file does not exist:" << filePath;
        return false;
    }
    qInfo() << "[FileOpen] openParentDirectory:" << filePath;

#ifdef Q_OS_WIN
    // Open with explorer /select so Windows highlights the target file.
    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
    return QProcess::startDetached(QStringLiteral("explorer.exe"),
                                   {QStringLiteral("/select,") + nativePath});
#else
    if (QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()))) {
        return true;
    }
    return false;
#endif
}
