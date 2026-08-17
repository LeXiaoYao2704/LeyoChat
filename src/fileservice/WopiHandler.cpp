#include "WopiHandler.h"
#include "FileServiceDatabase.h"
#include "FileStorageManager.h"
#include <QDateTime>
#include <QUuid>

WopiHandler::WopiHandler(FileServiceDatabase* db, FileStorageManager* storage)
    : m_db(db), m_storage(storage)
{}

WopiCheckFileInfoResult WopiHandler::checkFileInfo(const QString& fileId, const QString& accessToken)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto tokenRecord = m_db->validateWopiToken(accessToken, now);
    if (!tokenRecord)
        return { {}, 401 };

    if (tokenRecord->fileId != fileId)
        return { {}, 403 };

    auto file = m_db->findFileById(fileId);
    if (!file.has_value())
        return { {}, 404 };

    auto latestVersion = m_db->findCurrentVersion(fileId);
    qint64 fileSize = latestVersion ? latestVersion->fileSize : 0;

    QJsonObject info;
    info[QStringLiteral("BaseFileName")] = file->fileName;
    info[QStringLiteral("Size")] = fileSize;
    info[QStringLiteral("OwnerId")] = file->uploadedById;
    info[QStringLiteral("UserId")] = tokenRecord->clientId;
    info[QStringLiteral("UserFriendlyName")] = tokenRecord->displayName;
    info[QStringLiteral("Version")] = file->currentVersion;
    info[QStringLiteral("UserCanWrite")] = true;
    info[QStringLiteral("UserCanNotWriteRelative")] = true;
    info[QStringLiteral("SupportsLocks")] = true;
    info[QStringLiteral("SupportsUpdate")] = true;

    return { info, 200 };
}

WopiGetFileResult WopiHandler::getFile(const QString& fileId, const QString& accessToken)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto tokenRecord = m_db->validateWopiToken(accessToken, now);
    if (!tokenRecord)
        return { {}, {}, 401 };

    if (tokenRecord->fileId != fileId)
        return { {}, {}, 403 };

    auto latestVersion = m_db->findCurrentVersion(fileId);
    if (!latestVersion)
        return { {}, {}, 404 };

    auto content = m_storage->readFile(latestVersion->storagePath);
    if (!content.has_value())
        return { {}, {}, 404 };

    return { *content, QStringLiteral("application/octet-stream"), 200 };
}

WopiLockResult WopiHandler::handleLock(const QString& fileId, const QString& accessToken,
                                        const QString& wopiOverride, const QString& lockId)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto tokenRecord = m_db->validateWopiToken(accessToken, now);
    if (!tokenRecord)
        return { 401, {} };

    const qint64 lockExpiry = now + 30 * 60 * 1000; // 30 分钟

    if (wopiOverride == QStringLiteral("LOCK")) {
        auto existing = m_db->findFileLock(fileId);
        if (existing.has_value()) {
            if (existing->lockId == lockId) {
                // 相同 lockId → 刷新过期时间：删除旧锁，重新插入
                m_db->deleteFileLock(fileId);
                FileLockRecord refreshed;
                refreshed.fileId = fileId;
                refreshed.lockId = lockId;
                refreshed.lockedBy = tokenRecord->clientId;
                refreshed.lockedAtMs = existing->lockedAtMs;
                refreshed.expiresAtMs = lockExpiry;
                m_db->insertFileLock(refreshed);
                return { 200, {} };
            }
            // 不同 lockId → 冲突
            return { 409, existing->lockId };
        }
        // 无锁 → 创建
        FileLockRecord newLock;
        newLock.fileId = fileId;
        newLock.lockId = lockId;
        newLock.lockedBy = tokenRecord->clientId;
        newLock.lockedAtMs = now;
        newLock.expiresAtMs = lockExpiry;
        m_db->insertFileLock(newLock);
        return { 200, {} };
    }

    if (wopiOverride == QStringLiteral("REFRESH_LOCK")) {
        m_db->deleteFileLock(fileId);
        FileLockRecord refreshed;
        refreshed.fileId = fileId;
        refreshed.lockId = lockId;
        refreshed.lockedBy = tokenRecord->clientId;
        refreshed.lockedAtMs = now;
        refreshed.expiresAtMs = lockExpiry;
        m_db->insertFileLock(refreshed);
        return { 200, {} };
    }

    if (wopiOverride == QStringLiteral("UNLOCK")) {
        m_db->deleteFileLock(fileId);
        return { 200, {} };
    }

    return { 400, {} };
}

int WopiHandler::putFile(const QString& fileId, const QString& accessToken,
                          const QString& lockId, const QByteArray& content)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto tokenRecord = m_db->validateWopiToken(accessToken, now);
    if (!tokenRecord)
        return 401;

    // 验证锁匹配
    auto existing = m_db->findFileLock(fileId);
    if (existing.has_value() && existing->lockId != lockId)
        return 409;

    auto file = m_db->findFileById(fileId);
    if (!file.has_value())
        return 404;

    int nextVersionNum = m_db->nextVersionNumber(fileId);
    QString versionLabel = QStringLiteral("v%1").arg(nextVersionNum);
    QString versionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto storagePath = m_storage->saveFile(fileId, versionId, content);
    if (!storagePath.has_value())
        return 500;

    m_db->beginTransaction();

    FileVersionRecord ver;
    ver.versionId = versionId;
    ver.fileId = fileId;
    ver.versionNumber = nextVersionNum;
    ver.versionLabel = versionLabel;
    ver.uploaderId = tokenRecord->clientId;
    ver.uploaderName = tokenRecord->displayName;
    ver.uploadedAtMs = now;
    ver.fileSize = content.size();
    ver.storagePath = *storagePath;
    ver.changeNote = QStringLiteral("WOPI PutFile");

    bool ok = m_db->insertVersion(ver);
    ok = ok && m_db->updateFileCurrentVersion(fileId, versionId, now);

    if (ok)
        m_db->commit();
    else
        m_db->rollback();

    return ok ? 200 : 500;
}
