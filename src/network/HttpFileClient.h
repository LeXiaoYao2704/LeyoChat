#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QJsonObject>
#include <QTimer>

class HttpFileClient : public QObject {
    Q_OBJECT
public:
    explicit HttpFileClient(QObject* parent = nullptr);
    HttpFileClient(QObject* parent, int uploadStallTimeoutMs);

    // 上传文件到 FileService
    void uploadFile(const QString& baseUrl, const QString& token,
                    const QString& workspaceId, const QString& filePath,
                    const QString& uploaderName, const QString& clientId = {});

    // 下载文件（支持断点续传）
    void startDownload(const QString& url, const QString& token,
                       const QString& savePath, qint64 existingBytes = 0);
    void pauseDownload();
    void cancelDownload();

    bool isUploading() const;
    bool isDownloading() const;

signals:
    void uploadProgress(qint64 bytesSent, qint64 bytesTotal);
    void uploadFinished(const QJsonObject& response);
    void uploadFailed(const QString& errorText);

    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString& savePath);
    void downloadFailed(const QString& errorText);

private:
    void resetUploadStallTimer();

    QNetworkAccessManager m_nam;
    QNetworkReply* m_currentUpload = nullptr;
    QNetworkReply* m_currentDownload = nullptr;
    QFile* m_uploadFile = nullptr;
    QFile* m_downloadFile = nullptr;
    QTimer m_uploadStallTimer;
    int m_uploadStallTimeoutMs = 120000;
    bool m_uploadTimedOut = false;
    QString m_downloadSavePath;
    qint64 m_downloadOffset = 0;
};
