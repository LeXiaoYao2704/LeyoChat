#include "OnlyOfficeCallbackHandler.h"
#include "FileServiceDatabase.h"
#include "FileStorageManager.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUuid>
#include <QDateTime>
#include <QJsonArray>
#include <QtAlgorithms>
#include "integrations/SyncNetworkReply.h"

OnlyOfficeCallbackHandler::OnlyOfficeCallbackHandler(
    FileServiceDatabase* db, FileStorageManager* storage)
    : m_db(db), m_storage(storage)
{}

OnlyOfficeCallbackHandler::~OnlyOfficeCallbackHandler()
{
    qDeleteAll(m_fileMutexes);
}

QMutex* OnlyOfficeCallbackHandler::getFileMutex(const QString& fileId)
{
    QMutexLocker mapLock(&m_mapMutex);
    if (!m_fileMutexes.contains(fileId))
        m_fileMutexes[fileId] = new QMutex();
    return m_fileMutexes[fileId];
}

QByteArray OnlyOfficeCallbackHandler::downloadFromUrl(const QString& url)
{
    QNetworkAccessManager nam;
    QUrl qurl(url);
    QNetworkRequest req(qurl);
    QNetworkReply* reply = nam.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray data;
    if (reply->error() == QNetworkReply::NoError)
        data = reply->readAll();
    deleteSynchronousNetworkReply(reply);
    return data;
}

QJsonObject OnlyOfficeCallbackHandler::handleCallback(
    const QString& fileId, const QString& accessToken, const QJsonObject& body)
{
    // Callback 可能在 token 过期后到达，传 0 跳过过期检查
    auto tokenRecord = m_db->validateWopiToken(accessToken, 0);
    if (!tokenRecord) {
        QJsonObject err;
        err[QStringLiteral("error")] = 1;
        return err;
    }

    const int status = body[QStringLiteral("status")].toInt();

    QJsonObject ok;
    ok[QStringLiteral("error")] = 0;

    switch (status) {
    case 1: // 正在编辑，无需操作
        return ok;
    case 2: // 编辑完成，需保存
        return handleStatus2or6(fileId, body, QStringLiteral("协同编辑保存"));
    case 4: // 无变更关闭
        return handleStatus4(fileId);
    case 6: // 强制保存
        return handleStatus2or6(fileId, body, QStringLiteral("自动保存"));
    default:
        return ok;
    }
}

QJsonObject OnlyOfficeCallbackHandler::handleStatus2or6(
    const QString& fileId, const QJsonObject& body, const QString& changeNote)
{
    QMutexLocker lock(getFileMutex(fileId));

    QJsonObject ok;
    ok[QStringLiteral("error")] = 0;
    QJsonObject err;
    err[QStringLiteral("error")] = 1;

    // 版本防重：检查 key 是否匹配当前版本
    const QString key = body[QStringLiteral("key")].toString();
    auto file = m_db->findFileById(fileId);
    if (!file.has_value())
        return err;

    const QString expectedKey = fileId + QStringLiteral("_") + file->currentVersion;
    if (key != expectedKey)
        return ok; // 已过期的 key → 跳过

    // 下载文件内容
    const QString downloadUrl = body[QStringLiteral("url")].toString();
    QByteArray content = downloadFromUrl(downloadUrl);
    if (content.isEmpty())
        return err;

    // 保存新版本
    const int nextVersionNum = m_db->nextVersionNumber(fileId);
    const QString versionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    auto storagePath = m_storage->saveFile(fileId, versionId, content);
    if (!storagePath.has_value())
        return err;

    // callback 中的 users 列表最后一位作为上传者
    const auto usersArray = body[QStringLiteral("users")].toArray();
    const QString uploaderId = usersArray.isEmpty()
        ? QString() : usersArray.last().toString();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    m_db->beginTransaction();

    FileVersionRecord ver;
    ver.versionId = versionId;
    ver.fileId = fileId;
    ver.versionNumber = nextVersionNum;
    ver.versionLabel = QStringLiteral("v%1").arg(nextVersionNum);
    ver.uploaderId = uploaderId;
    ver.uploaderName = QString();
    ver.uploadedAtMs = now;
    ver.fileSize = content.size();
    ver.storagePath = *storagePath;
    ver.changeNote = changeNote;

    bool success = m_db->insertVersion(ver);
    success = success && m_db->updateFileCurrentVersion(fileId, versionId, now);

    if (success) {
        m_db->commit();
        m_db->deleteFileLock(fileId);
        return ok;
    }

    m_db->rollback();
    return err;
}

QJsonObject OnlyOfficeCallbackHandler::handleStatus4(const QString& fileId)
{
    m_db->deleteFileLock(fileId);
    QJsonObject ok;
    ok[QStringLiteral("error")] = 0;
    return ok;
}
