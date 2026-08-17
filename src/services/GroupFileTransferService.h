#pragma once
#include <QObject>
#include <QMap>
#include <QJsonObject>
#include "network/HttpFileClient.h"
#include "integrations/RemoteFileServiceSettings.h"

class GroupFileTransferService : public QObject {
    Q_OBJECT
public:
    explicit GroupFileTransferService(QObject* parent = nullptr);

    // 发送群文件 — 自动选择通道
    void sendGroupFile(const QString& groupId, const QString& filePath,
                       const GroupFileServiceConfig& config,
                       const QString& uploaderName,
                       const QString& clientId = {});

    // 下载群文件
    void startDownload(const QString& messageId, const QString& baseUrl,
                       const QString& token, const QString& fileId,
                       const QString& fileName, const QString& savePath,
                       qint64 existingBytes = 0);
    void pauseDownload(const QString& messageId);
    void cancelDownload(const QString& messageId);

    static constexpr int kMaxConcurrentDownloads = 3;

signals:
    // 上传完成 → 调用方广播 group_file_card
    void uploadFinished(const QString& groupId, const QJsonObject& fileCard);
    void uploadProgress(const QString& groupId, qint64 bytesSent, qint64 bytesTotal);
    void uploadFailed(const QString& groupId, const QString& errorText);
    // 降级为 P2P
    void fallbackToP2P(const QString& groupId, const QString& filePath);

    // 下载信号
    void fileDownloadProgress(const QString& messageId, qint64 bytesReceived, qint64 bytesTotal);
    void fileDownloadFinished(const QString& messageId, const QString& localPath);
    void fileDownloadFailed(const QString& messageId, const QString& errorText);

private:
    QMap<QString, HttpFileClient*> m_activeDownloads;
    HttpFileClient* m_uploader = nullptr;
};
