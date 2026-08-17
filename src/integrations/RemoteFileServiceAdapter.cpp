#include "integrations/RemoteFileServiceAdapter.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QDebug>

#include "integrations/SharedFileResourceContracts.h"
#include "integrations/SyncNetworkReply.h"

namespace {

QString chooseFirstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return {};
}

}  // namespace

// ---------------------------------------------------------------------------
// NetworkRemoteFileServiceTransport
// ---------------------------------------------------------------------------

std::optional<QJsonDocument> NetworkRemoteFileServiceTransport::getJson(
    const QUrl& url,
    const RemoteFileServiceConnectionSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(settings.bearerToken).toUtf8());

    QPointer<QNetworkReply> reply(manager.get(request));
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(15000);
    loop.exec();

    if (!reply) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务请求对象提前释放");
        }
        return std::nullopt;
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务请求超时");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                QString::fromUtf8(body).trimmed(),
                reply->errorString(),
                QStringLiteral("远程文件服务请求失败"),
            });
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务返回了无法解析的 JSON");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return document;
}

std::optional<QByteArray> NetworkRemoteFileServiceTransport::getBytes(
    const QUrl& url,
    const RemoteFileServiceConnectionSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(settings.bearerToken).toUtf8());

    QPointer<QNetworkReply> reply(manager.get(request));
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000);
    loop.exec();

    if (!reply) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务请求对象提前释放");
        }
        return std::nullopt;
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务下载超时");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                reply->errorString(),
                QStringLiteral("远程文件服务下载失败"),
            });
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return body;
}

std::optional<QJsonDocument> NetworkRemoteFileServiceTransport::putBytes(
    const QUrl& url,
    const QByteArray& data,
    const QMap<QByteArray, QByteArray>& extraHeaders,
    const RemoteFileServiceConnectionSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(settings.bearerToken).toUtf8());
    for (auto it = extraHeaders.cbegin(); it != extraHeaders.cend(); ++it) {
        request.setRawHeader(it.key(), it.value());
    }

    QPointer<QNetworkReply> reply(manager.put(request, data));
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000);
    loop.exec();

    if (!reply) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务请求对象提前释放");
        }
        return std::nullopt;
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务上传超时");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                QString::fromUtf8(body).trimmed(),
                reply->errorString(),
                QStringLiteral("远程文件服务上传失败"),
            });
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务返回了无法解析的 JSON");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return document;
}

std::optional<QJsonDocument> NetworkRemoteFileServiceTransport::putDevice(
    const QUrl& url,
    QIODevice* device,
    const QMap<QByteArray, QByteArray>& extraHeaders,
    const RemoteFileServiceConnectionSettings& settings,
    QString* errorMessage) const
{
    if (!device) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务上传设备无效");
        }
        return std::nullopt;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(settings.bearerToken).toUtf8());
    for (auto it = extraHeaders.cbegin(); it != extraHeaders.cend(); ++it) {
        request.setRawHeader(it.key(), it.value());
    }

    QPointer<QNetworkReply> reply(manager.put(request, device));
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000);
    loop.exec();

    if (!reply) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务请求对象提前释放");
        }
        return std::nullopt;
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务上传超时");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                QString::fromUtf8(body).trimmed(),
                reply->errorString(),
                QStringLiteral("远程文件服务上传失败"),
            });
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务返回了无法解析的 JSON");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return document;
}

std::optional<QJsonDocument> NetworkRemoteFileServiceTransport::postBytes(
    const QUrl& url,
    const QByteArray& data,
    const QMap<QByteArray, QByteArray>& extraHeaders,
    const RemoteFileServiceConnectionSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(settings.bearerToken).toUtf8());
    for (auto it = extraHeaders.cbegin(); it != extraHeaders.cend(); ++it) {
        request.setRawHeader(it.key(), it.value());
    }

    QPointer<QNetworkReply> reply(manager.post(request, data));
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000);
    loop.exec();

    if (!reply) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务请求对象提前释放");
        }
        return std::nullopt;
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务上传超时");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                QString::fromUtf8(body).trimmed(),
                reply->errorString(),
                QStringLiteral("远程文件服务上传失败"),
            });
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务返回了无法解析的 JSON");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return document;
}

std::optional<QJsonDocument> NetworkRemoteFileServiceTransport::postDevice(
    const QUrl& url,
    QIODevice* device,
    const QMap<QByteArray, QByteArray>& extraHeaders,
    const RemoteFileServiceConnectionSettings& settings,
    QString* errorMessage) const
{
    if (!device) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务上传设备无效");
        }
        return std::nullopt;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Authorization",
                         QStringLiteral("Bearer %1").arg(settings.bearerToken).toUtf8());
    for (auto it = extraHeaders.cbegin(); it != extraHeaders.cend(); ++it) {
        request.setRawHeader(it.key(), it.value());
    }

    QPointer<QNetworkReply> reply(manager.post(request, device));
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(30000);
    loop.exec();

    if (!reply) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务请求对象提前释放");
        }
        return std::nullopt;
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务上传超时");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (errorMessage) {
            *errorMessage = chooseFirstNonEmpty({
                QString::fromUtf8(body).trimmed(),
                reply->errorString(),
                QStringLiteral("远程文件服务上传失败"),
            });
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务返回了无法解析的 JSON");
        }
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return document;
}

// ---------------------------------------------------------------------------
// RemoteFileServiceAdapter — constructor and URL helpers
// ---------------------------------------------------------------------------

RemoteFileServiceAdapter::RemoteFileServiceAdapter(
    RemoteFileServiceConnectionSettings settings,
    std::shared_ptr<IRemoteFileServiceTransport> transport)
    : m_settings(std::move(settings))
    , m_transport(std::move(transport))
{
}

QUrl RemoteFileServiceAdapter::pingUrl() const
{
    QString base = m_settings.baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return QUrl(base + QStringLiteral("/api/v1/ping"));
}

QUrl RemoteFileServiceAdapter::filesUrl() const
{
    QString base = m_settings.baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return QUrl(base + QStringLiteral("/api/v1/files"));
}

QUrl RemoteFileServiceAdapter::fileUrl(const QString& fileId) const
{
    QString base = m_settings.baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return QUrl(base + QStringLiteral("/api/v1/files/") + fileId.trimmed());
}

QUrl RemoteFileServiceAdapter::downloadUrl(const QString& fileId) const
{
    return QUrl(fileUrl(fileId).toString() + QStringLiteral("/download"));
}

QUrl RemoteFileServiceAdapter::versionsUrl(const QString& fileId) const
{
    return QUrl(fileUrl(fileId).toString() + QStringLiteral("/versions"));
}

QUrl RemoteFileServiceAdapter::versionDownloadUrl(const QString& fileId,
                                                   const QString& versionId) const
{
    return QUrl(versionsUrl(fileId).toString() + QStringLiteral("/") + versionId.trimmed()
                + QStringLiteral("/download"));
}

// ---------------------------------------------------------------------------
// RemoteFileServiceAdapter — public API
// ---------------------------------------------------------------------------

bool RemoteFileServiceAdapter::testConnection(QString* errorMessage) const
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写远程文件服务地址和 Bearer Token");
        }
        return false;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务传输层未初始化");
        }
        return false;
    }

    const auto document = m_transport->getJson(pingUrl(), m_settings, errorMessage);
    const bool ok = document.has_value();
    if (ok) {
        qInfo().noquote()
            << QStringLiteral("[integrations][remote-file-service] connection test succeeded");
    } else {
        qWarning().noquote()
            << QStringLiteral("[integrations][remote-file-service] connection test failed: %1")
                   .arg(errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
    }
    return ok;
}

std::optional<RemoteFileUploadResult> RemoteFileServiceAdapter::uploadFile(
    const QString& workspaceId,
    const QString& localFilePath,
    const QString& changeNote,
    const QString& uploaderName,
    const QString& clientId,
    QString* errorMessage) const
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写远程文件服务地址和 Bearer Token");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务传输层未初始化");
        }
        return std::nullopt;
    }

    QFile file(localFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取本地文件: %1").arg(localFilePath);
        }
        return std::nullopt;
    }

    const QString fileName = QFileInfo(localFilePath).fileName();
    const QUrl url = QUrl(filesUrl().toString() + QStringLiteral("/") + fileName);

    QMap<QByteArray, QByteArray> headers;
    headers.insert("X-Workspace-Id",  workspaceId.trimmed().toUtf8());
    headers.insert("X-Change-Note",   changeNote.trimmed().toUtf8());
    headers.insert("X-Uploader-Name", uploaderName.trimmed().toUtf8());
    if (!clientId.trimmed().isEmpty())
        headers.insert("X-Client-Id", clientId.trimmed().toUtf8());
    headers.insert("Content-Length", QByteArray::number(file.size()));

    const auto document = m_transport->putDevice(url, &file, headers, m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        qWarning().noquote()
            << QStringLiteral("[integrations][remote-file-service] uploadFile failed for %1")
                   .arg(fileName);
        return std::nullopt;
    }

    const QJsonObject object = document->object();
    RemoteFileUploadResult result;
    result.fileId    = object.value(QStringLiteral("file_id")).toString().trimmed();
    result.versionId = object.value(QStringLiteral("version_id")).toString().trimmed();

    if (result.fileId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务未返回 file_id");
        }
        return std::nullopt;
    }

    qInfo().noquote()
        << QStringLiteral("[integrations][remote-file-service] uploaded file %1 -> fileId=%2")
               .arg(fileName, result.fileId);
    return result;
}

std::optional<RemoteFileUploadResult> RemoteFileServiceAdapter::uploadNewVersion(
    const QString& fileId,
    const QString& localFilePath,
    const QString& changeNote,
    const QString& uploaderName,
    const QString& clientId,
    QString* errorMessage) const
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写远程文件服务地址和 Bearer Token");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务传输层未初始化");
        }
        return std::nullopt;
    }

    QFile file(localFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法读取本地文件: %1").arg(localFilePath);
        }
        return std::nullopt;
    }

    const QString fileName = QFileInfo(localFilePath).fileName();

    QMap<QByteArray, QByteArray> headers;
    headers.insert("X-Change-Note",   changeNote.trimmed().toUtf8());
    headers.insert("X-Uploader-Name", uploaderName.trimmed().toUtf8());
    if (!clientId.trimmed().isEmpty())
        headers.insert("X-Client-Id", clientId.trimmed().toUtf8());
    headers.insert("Content-Length", QByteArray::number(file.size()));

    const auto document =
        m_transport->postDevice(versionsUrl(fileId), &file, headers, m_settings, errorMessage);
    if (!document.has_value() || !document->isObject()) {
        qWarning().noquote()
            << QStringLiteral(
                   "[integrations][remote-file-service] uploadNewVersion failed for fileId=%1")
                   .arg(fileId);
        return std::nullopt;
    }

    const QJsonObject object = document->object();
    RemoteFileUploadResult result;
    result.fileId    = object.value(QStringLiteral("file_id")).toString().trimmed();
    result.versionId = object.value(QStringLiteral("version_id")).toString().trimmed();

    if (result.fileId.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务未返回 file_id");
        }
        return std::nullopt;
    }

    qInfo().noquote()
        << QStringLiteral(
               "[integrations][remote-file-service] uploaded new version of %1 -> versionId=%2")
               .arg(fileName, result.versionId);
    return result;
}

std::optional<QString> RemoteFileServiceAdapter::downloadFile(
    const QString& fileId,
    const QString& fileName,
    const QString& saveToDir,
    QString* errorMessage) const
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写远程文件服务地址和 Bearer Token");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务传输层未初始化");
        }
        return std::nullopt;
    }

    const auto bytes = m_transport->getBytes(downloadUrl(fileId), m_settings, errorMessage);
    if (!bytes.has_value()) {
        return std::nullopt;
    }

    const QString savePath = QDir(saveToDir).filePath(fileName);
    QFile outFile(savePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入文件: %1").arg(savePath);
        }
        return std::nullopt;
    }
    outFile.write(*bytes);
    outFile.close();

    qInfo().noquote()
        << QStringLiteral("[integrations][remote-file-service] downloaded fileId=%1 -> %2")
               .arg(fileId, savePath);
    return savePath;
}

std::optional<QString> RemoteFileServiceAdapter::downloadByUrl(
    const QString& fullUrl,
    const QString& saveFileName,
    const QString& saveToDir,
    QString* errorMessage) const
{
    const auto bytes = m_transport->getBytes(QUrl(fullUrl), m_settings, errorMessage);
    if (!bytes.has_value()) return std::nullopt;

    const QString savePath = QDir(saveToDir).filePath(saveFileName);
    QFile f(savePath);
    if (!f.open(QIODevice::WriteOnly)) {
        if (errorMessage) *errorMessage = QStringLiteral("无法写入文件：") + savePath;
        return std::nullopt;
    }
    f.write(*bytes);
    return savePath;
}

std::optional<QString> RemoteFileServiceAdapter::downloadVersion(
    const QString& fileId,
    const QString& versionId,
    const QString& fileName,
    const QString& saveToDir,
    QString* errorMessage) const
{
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写远程文件服务地址和 Bearer Token");
        }
        return std::nullopt;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务传输层未初始化");
        }
        return std::nullopt;
    }

    const auto bytes =
        m_transport->getBytes(versionDownloadUrl(fileId, versionId), m_settings, errorMessage);
    if (!bytes.has_value()) {
        return std::nullopt;
    }

    const QString savePath = QDir(saveToDir).filePath(fileName);
    QFile outFile(savePath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入文件: %1").arg(savePath);
        }
        return std::nullopt;
    }
    outFile.write(*bytes);
    outFile.close();

    qInfo().noquote()
        << QStringLiteral(
               "[integrations][remote-file-service] downloaded fileId=%1 versionId=%2 -> %3")
               .arg(fileId, versionId, savePath);
    return savePath;
}

QVector<RemoteFileInfo> RemoteFileServiceAdapter::listFiles(const QString& workspaceId,
                                                             QString* errorMessage) const
{
    QVector<RemoteFileInfo> files;
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写远程文件服务地址和 Bearer Token");
        }
        return files;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务传输层未初始化");
        }
        return files;
    }

    const QUrl url = QUrl(filesUrl().toString()
                          + QStringLiteral("?workspaceId=") + workspaceId.trimmed());
    const auto document = m_transport->getJson(url, m_settings, errorMessage);
    if (!document.has_value()) {
        qWarning().noquote()
            << QStringLiteral(
                   "[integrations][remote-file-service] listFiles failed for workspaceId=%1: %2")
                   .arg(workspaceId.trimmed(),
                        errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
        return files;
    }

    const QJsonArray array = document->array();
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        RemoteFileInfo info;
        info.fileId         = obj.value(QStringLiteral("file_id")).toString().trimmed();
        info.workspaceId    = obj.value(QStringLiteral("workspace_id")).toString().trimmed();
        info.fileName       = obj.value(QStringLiteral("file_name")).toString().trimmed();
        info.currentVersion = obj.value(QStringLiteral("current_version")).toString().trimmed();
        info.uploadedById   = obj.value(QStringLiteral("uploaded_by_id")).toString().trimmed();
        info.uploadedByName = obj.value(QStringLiteral("uploaded_by_name")).toString().trimmed();
        info.createdAtMs    = static_cast<qint64>(
            obj.value(QStringLiteral("created_at_ms")).toDouble());
        info.updatedAtMs    = static_cast<qint64>(
            obj.value(QStringLiteral("updated_at_ms")).toDouble());
        if (!info.fileId.isEmpty()) {
            files.push_back(info);
        }
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    qInfo().noquote()
        << QStringLiteral("[integrations][remote-file-service] listed %1 files for workspaceId=%2")
               .arg(QString::number(files.size()), workspaceId.trimmed());
    return files;
}

QVector<RemoteFileVersion> RemoteFileServiceAdapter::getVersionHistory(
    const QString& fileId,
    QString* errorMessage) const
{
    QVector<RemoteFileVersion> versions;
    if (!m_settings.hasCredentialConfiguration()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("请先填写远程文件服务地址和 Bearer Token");
        }
        return versions;
    }
    if (!m_transport) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("远程文件服务传输层未初始化");
        }
        return versions;
    }

    const auto document = m_transport->getJson(versionsUrl(fileId), m_settings, errorMessage);
    if (!document.has_value()) {
        qWarning().noquote()
            << QStringLiteral(
                   "[integrations][remote-file-service] getVersionHistory failed for fileId=%1: %2")
                   .arg(fileId.trimmed(),
                        errorMessage ? errorMessage->trimmed() : QStringLiteral("unknown"));
        return versions;
    }

    const QJsonArray array = document->array();
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        RemoteFileVersion ver;
        ver.versionId     = obj.value(QStringLiteral("version_id")).toString().trimmed();
        ver.fileId        = obj.value(QStringLiteral("file_id")).toString().trimmed();
        ver.versionNumber = obj.value(QStringLiteral("version_number")).toInt();
        ver.versionLabel  = obj.value(QStringLiteral("version_label")).toString().trimmed();
        ver.uploaderId    = obj.value(QStringLiteral("uploader_id")).toString().trimmed();
        ver.uploaderName  = obj.value(QStringLiteral("uploader_name")).toString().trimmed();
        ver.uploadedAtMs  = static_cast<qint64>(
            obj.value(QStringLiteral("uploaded_at_ms")).toDouble());
        ver.fileSizeBytes = static_cast<qint64>(
            obj.value(QStringLiteral("file_size_bytes")).toDouble());
        ver.changeNote    = obj.value(QStringLiteral("change_note")).toString().trimmed();
        if (!ver.versionId.isEmpty()) {
            versions.push_back(ver);
        }
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    qInfo().noquote()
        << QStringLiteral("[integrations][remote-file-service] fetched %1 versions for fileId=%2")
               .arg(QString::number(versions.size()), fileId.trimmed());
    return versions;
}

std::optional<ResourceRefPayload> RemoteFileServiceAdapter::payloadForFileId(
    const QString& fileId,
    const QString& workspaceId,
    const QString& fileName,
    const QString& uploaderName,
    qint64 fileSizeBytes,
    QString* errorMessage)
{
    const QString key = fileId.trimmed();
    if (key.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("fileId 不能为空");
        }
        return std::nullopt;
    }

    const auto it = m_payloadCache.constFind(key);
    if (it != m_payloadCache.cend()) {
        return it.value();
    }

    SharedFileResource resource;
    resource.serviceId      = QStringLiteral("remote-file-service");
    resource.workspaceId    = workspaceId.trimmed();
    resource.resourceId     = key;
    resource.title          = fileName.trimmed();
    resource.ownerName      = uploaderName.trimmed();
    resource.version        = QStringLiteral("latest");
    resource.sizeBytes      = fileSizeBytes;
    resource.downloadTarget = downloadUrl(key).toString();
    resource.openTarget     = versionsUrl(key).toString();

    const ResourceRefPayload payload = SharedFileResourceContracts::makePayload(resource);
    m_payloadCache.insert(key, payload);
    return payload;
}
