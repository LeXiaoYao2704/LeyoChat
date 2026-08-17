// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增40行/修改0行/删除0行; 总行数93行
// @AI-LastModified: 2026-04-15 22:51:14

#include "FileStorageManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>

FileStorageManager::FileStorageManager(const QString& storageRoot)
    : m_storageRoot(storageRoot)
{
}

std::optional<QString> FileStorageManager::saveFile(const QString& fileId,
                                                      const QString& versionId,
                                                      const QByteArray& data) const
{
    QDir dir(m_storageRoot);
    if (!dir.mkpath(fileId)) {
        qWarning() << "FileStorageManager: failed to create directory for fileId:" << fileId;
        return std::nullopt;
    }

    const QString relativePath = fileId + QStringLiteral("/") + versionId + QStringLiteral(".bin");
    const QString fullPath     = m_storageRoot + QStringLiteral("/") + relativePath;

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "FileStorageManager: failed to open file for writing:" << fullPath;
        return std::nullopt;
    }
    file.write(data);
    file.close();
    return relativePath;
}

std::optional<QByteArray> FileStorageManager::readFile(const QString& relativePath) const
{
    const QString fullPath = absolutePath(relativePath);
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "FileStorageManager: failed to open file for reading:" << fullPath;
        return std::nullopt;
    }
    const QByteArray data = file.readAll();
    file.close();
    return data;
}

QString FileStorageManager::absolutePath(const QString& relativePath) const
{
    return m_storageRoot + QStringLiteral("/") + relativePath;
}

bool FileStorageManager::deleteFile(const QString& relativePath) const
{
    const QString abs = absolutePath(relativePath);
    if (!QFile::exists(abs))
        return false;
    return QFile::remove(abs);
}

std::optional<FileStorageManager::FileRangeResult> FileStorageManager::readFileRange(
    const QString& relativePath, qint64 offset, qint64 length) const
{
    const QString fullPath = absolutePath(relativePath);
    QFile file(fullPath);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const qint64 totalSize = file.size();
    const qint64 start = std::min(offset, totalSize);
    const qint64 end = (length < 0) ? totalSize - 1
                                     : std::min(start + length - 1, totalSize - 1);
    if (start > end)
        return FileRangeResult{{}, totalSize, start, start};
    file.seek(start);
    FileRangeResult result;
    result.data = file.read(end - start + 1);
    result.totalSize = totalSize;
    result.rangeStart = start;
    result.rangeEnd = start + result.data.size() - 1;
    return result;
}

qint64 FileStorageManager::fileSize(const QString& relativePath) const
{
    QFileInfo info(absolutePath(relativePath));
    return info.exists() ? info.size() : -1;
}

std::optional<QString> FileStorageManager::saveFileFromPath(const QString& chatFileId,
                                                              const QString& sourcePath) const
{
    QDir dir(m_storageRoot);
    const QString subDir = QStringLiteral("chat-files/") + chatFileId;
    if (!dir.mkpath(subDir))
        return std::nullopt;
    const QString fileName = QFileInfo(sourcePath).fileName();
    const QString relativePath = subDir + QStringLiteral("/") + fileName;
    const QString fullPath = m_storageRoot + QStringLiteral("/") + relativePath;
    if (!QFile::copy(sourcePath, fullPath))
        return std::nullopt;
    return relativePath;
}
