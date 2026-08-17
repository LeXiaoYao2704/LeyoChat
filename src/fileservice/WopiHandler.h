#pragma once
#include <QJsonObject>
#include <QByteArray>
#include <QString>

class FileServiceDatabase;
class FileStorageManager;

struct WopiCheckFileInfoResult {
    QJsonObject json;
    int statusCode = 200;
};

struct WopiGetFileResult {
    QByteArray content;
    QString contentType;
    int statusCode = 200;
};

struct WopiLockResult {
    int statusCode = 200;
    QString existingLockId;  // 仅 409 时填充
};

class WopiHandler
{
public:
    WopiHandler(FileServiceDatabase* db, FileStorageManager* storage);

    WopiCheckFileInfoResult checkFileInfo(const QString& fileId, const QString& accessToken);
    WopiGetFileResult getFile(const QString& fileId, const QString& accessToken);
    int putFile(const QString& fileId, const QString& accessToken,
                const QString& lockId, const QByteArray& content);
    WopiLockResult handleLock(const QString& fileId, const QString& accessToken,
                              const QString& wopiOverride, const QString& lockId);

private:
    FileServiceDatabase* m_db;
    FileStorageManager* m_storage;
};
