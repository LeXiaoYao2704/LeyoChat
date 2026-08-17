#include "services/GroupFileTransferService.h"
#include <QFileInfo>

GroupFileTransferService::GroupFileTransferService(QObject* parent) : QObject(parent) {}

void GroupFileTransferService::sendGroupFile(const QString& groupId,
                                              const QString& filePath,
                                              const GroupFileServiceConfig& config,
                                              const QString& uploaderName,
                                              const QString& clientId) {
    qInfo() << "[group-file-transfer] sendGroupFile called"
            << "groupId=" << groupId
            << "file=" << filePath
            << "config.enabled=" << config.enabled
            << "config.baseUrl=" << config.baseUrl
            << "config.workspaceId=" << config.workspaceId
            << "config.bearerToken.len=" << config.bearerToken.length();
    if (!config.enabled || config.baseUrl.trimmed().isEmpty()) {
        qWarning() << "[group-file-transfer] fallback to P2P: config disabled or baseUrl empty";
        emit fallbackToP2P(groupId, filePath);
        return;
    }
    if (m_uploader) {
        qWarning() << "[group-file-transfer] REJECTED: upload already in progress";
        emit uploadFailed(groupId, QStringLiteral("正在上传中, 请稍候"));
        return;
    }
    m_uploader = new HttpFileClient(this);
    connect(m_uploader, &HttpFileClient::uploadProgress, this,
        [this, groupId](qint64 sent, qint64 total) { emit uploadProgress(groupId, sent, total); });
    connect(m_uploader, &HttpFileClient::uploadFinished, this,
        [this, groupId, filePath, uploaderName](const QJsonObject& resp) {
            qInfo() << "[group-file-transfer] uploadFinished signal received"
                    << "groupId=" << groupId << "resp=" << resp;
            m_uploader->deleteLater();
            m_uploader = nullptr;
            QJsonObject fileCard;
            fileCard[QStringLiteral("channel")]    = QStringLiteral("fileservice");
            fileCard[QStringLiteral("file_id")]    = resp.value(QStringLiteral("file_id")).toString();
            fileCard[QStringLiteral("file_name")]  = resp.value(QStringLiteral("file_name")).toString();
            fileCard[QStringLiteral("file_size")]  = resp.value(QStringLiteral("file_size")).toInteger();
            fileCard[QStringLiteral("file_hash")]  = resp.value(QStringLiteral("file_hash")).toString();
            fileCard[QStringLiteral("uploader_name")] = uploaderName;
            fileCard[QStringLiteral("sender_file_path")] = filePath;
            emit uploadFinished(groupId, fileCard);
        });
    connect(m_uploader, &HttpFileClient::uploadFailed, this,
        [this, groupId, filePath](const QString& err) {
            qWarning() << "[group-file-transfer] uploadFailed → fallback to P2P"
                       << "groupId=" << groupId << "error=" << err;
            m_uploader->deleteLater();
            m_uploader = nullptr;
            emit fallbackToP2P(groupId, filePath);
        });
    m_uploader->uploadFile(config.baseUrl, config.bearerToken,
                           config.workspaceId, filePath, uploaderName, clientId);
}

void GroupFileTransferService::startDownload(const QString& messageId,
                                              const QString& baseUrl,
                                              const QString& token,
                                              const QString& fileId,
                                              const QString& /*fileName*/,
                                              const QString& savePath,
                                              qint64 existingBytes) {
    if (m_activeDownloads.contains(messageId))
        return;
    if (m_activeDownloads.size() >= kMaxConcurrentDownloads) {
        emit fileDownloadFailed(messageId, QStringLiteral("下载队列已满, 请稍候重试"));
        return;
    }
    auto* client = new HttpFileClient(this);
    m_activeDownloads.insert(messageId, client);
    connect(client, &HttpFileClient::downloadProgress, this,
        [this, messageId](qint64 recv, qint64 total) {
            emit fileDownloadProgress(messageId, recv, total);
        });
    connect(client, &HttpFileClient::downloadFinished, this,
        [this, messageId](const QString& path) {
            if (auto* c = m_activeDownloads.take(messageId))
                c->deleteLater();
            emit fileDownloadFinished(messageId, path);
        });
    connect(client, &HttpFileClient::downloadFailed, this,
        [this, messageId](const QString& err) {
            if (auto* c = m_activeDownloads.take(messageId))
                c->deleteLater();
            emit fileDownloadFailed(messageId, err);
        });
    const QString url = baseUrl + QStringLiteral("/api/v1/chat-files/") + fileId;
    qInfo() << "[group-file-download] startDownload url=" << url
            << "savePath=" << savePath << "existingBytes=" << existingBytes;
    client->startDownload(url, token, savePath, existingBytes);
}

void GroupFileTransferService::pauseDownload(const QString& messageId) {
    if (auto* client = m_activeDownloads.value(messageId))
        client->pauseDownload();
}

void GroupFileTransferService::cancelDownload(const QString& messageId) {
    if (auto* client = m_activeDownloads.take(messageId)) {
        client->cancelDownload();
        client->deleteLater();
    }
}
