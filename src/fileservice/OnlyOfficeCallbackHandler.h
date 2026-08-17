#pragma once
#include <QJsonObject>
#include <QString>
#include <QMutex>
#include <QMap>

class FileServiceDatabase;
class FileStorageManager;

class OnlyOfficeCallbackHandler
{
public:
    OnlyOfficeCallbackHandler(FileServiceDatabase* db, FileStorageManager* storage);
    virtual ~OnlyOfficeCallbackHandler();

    // 返回 JSON 响应体 {"error": 0|1}
    QJsonObject handleCallback(const QString& fileId, const QString& accessToken,
                                const QJsonObject& callbackBody);

protected:
    // 可在测试中 override 以避免真实网络请求
    virtual QByteArray downloadFromUrl(const QString& url);

private:
    QJsonObject handleStatus2or6(const QString& fileId, const QJsonObject& body,
                                  const QString& changeNote);
    QJsonObject handleStatus4(const QString& fileId);

    FileServiceDatabase* m_db;
    FileStorageManager* m_storage;
    QMap<QString, QMutex*> m_fileMutexes;
    QMutex m_mapMutex;

    QMutex* getFileMutex(const QString& fileId);
};
