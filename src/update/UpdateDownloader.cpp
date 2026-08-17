#include "update/UpdateDownloader.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtConcurrent>

UpdateDownloader::UpdateDownloader(QObject* parent)
    : QObject(parent)
{
}

QString UpdateDownloader::localInstallerPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
        .filePath(QStringLiteral("LeyoChat-update-setup.exe"));
}

void UpdateDownloader::download(const QString& sourceDir,
                                const QString& fileName,
                                const QString& expectedSha256)
{
    const QString remotePath = QDir(sourceDir).filePath(fileName);
    const QString localPath = localInstallerPath();

    (void)QtConcurrent::run([this, remotePath, localPath, expectedSha256]() {
        QFile src(remotePath);
        if (!src.open(QIODevice::ReadOnly)) {
            QMetaObject::invokeMethod(this, [this, remotePath]() {
                emit downloadFailed(
                    QStringLiteral("无法打开升级文件：%1").arg(remotePath));
            });
            return;
        }

        const qint64 totalSize = src.size();
        if (totalSize <= 0) {
            src.close();
            QMetaObject::invokeMethod(this, [this]() {
                emit downloadFailed(QStringLiteral("升级文件大小无效"));
            });
            return;
        }

        QFile dst(localPath);
        if (!dst.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            src.close();
            QMetaObject::invokeMethod(this, [this, localPath]() {
                emit downloadFailed(
                    QStringLiteral("无法写入本地文件：%1").arg(localPath));
            });
            return;
        }

        QCryptographicHash hash(QCryptographicHash::Sha256);
        qint64 copied = 0;
        constexpr qint64 chunkSize = 256 * 1024;
        int lastPercent = -1;

        while (!src.atEnd()) {
            const QByteArray chunk = src.read(chunkSize);
            if (chunk.isEmpty()) break;
            dst.write(chunk);
            hash.addData(chunk);
            copied += chunk.size();

            const int percent = static_cast<int>(copied * 100 / totalSize);
            if (percent != lastPercent) {
                lastPercent = percent;
                QMetaObject::invokeMethod(this, [this, percent]() {
                    emit progressChanged(percent);
                });
            }
        }

        src.close();
        dst.close();

        const QString actualSha256 = QString::fromLatin1(hash.result().toHex());
        if (!expectedSha256.isEmpty()
            && actualSha256.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
            QFile::remove(localPath);
            QMetaObject::invokeMethod(this, [this]() {
                emit downloadFailed(
                    QStringLiteral("安装包校验失败，文件可能损坏，请重试"));
            });
            return;
        }

        QMetaObject::invokeMethod(this, [this, localPath]() {
            emit downloadFinished(localPath);
        });
    });
}
