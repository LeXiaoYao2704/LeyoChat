#include "network/HttpFileClient.h"
#include <QFileInfo>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>

HttpFileClient::HttpFileClient(QObject* parent)
    : HttpFileClient(parent, 120000)
{
}

HttpFileClient::HttpFileClient(QObject* parent, int uploadStallTimeoutMs)
    : QObject(parent),
      m_uploadStallTimeoutMs(uploadStallTimeoutMs > 0 ? uploadStallTimeoutMs : 120000)
{
    m_uploadStallTimer.setSingleShot(true);
    connect(&m_uploadStallTimer, &QTimer::timeout, this, [this]() {
        if (!m_currentUpload) {
            return;
        }
        m_uploadTimedOut = true;
        qWarning() << "[http-file-upload] STALLED: aborting upload after"
                   << m_uploadStallTimeoutMs << "ms";
        m_currentUpload->abort();
    });
}

bool HttpFileClient::isUploading() const { return m_currentUpload != nullptr; }
bool HttpFileClient::isDownloading() const { return m_currentDownload != nullptr; }

void HttpFileClient::resetUploadStallTimer()
{
    if (m_uploadStallTimeoutMs <= 0) {
        return;
    }
    m_uploadStallTimer.start(m_uploadStallTimeoutMs);
}

void HttpFileClient::uploadFile(const QString& baseUrl, const QString& token,
                                 const QString& workspaceId, const QString& filePath,
                                 const QString& uploaderName, const QString& clientId) {
    qInfo() << "[http-file-upload] start url=" << baseUrl + QStringLiteral("/api/v1/chat-files")
            << "workspaceId=" << workspaceId
            << "file=" << filePath
            << "uploaderName=" << uploaderName
            << "tokenLen=" << token.length();
    if (m_currentUpload) {
        qWarning() << "[http-file-upload] REJECTED: already uploading";
        emit uploadFailed(QStringLiteral("上传已在进行中"));
        return;
    }
    m_uploadFile = new QFile(filePath, this);
    if (!m_uploadFile->open(QIODevice::ReadOnly)) {
        qWarning() << "[http-file-upload] FAILED: cannot open file" << filePath;
        delete m_uploadFile;
        m_uploadFile = nullptr;
        emit uploadFailed(QStringLiteral("无法打开文件"));
        return;
    }
    qInfo() << "[http-file-upload] file open ok, bodySize=" << m_uploadFile->size();

    QNetworkRequest request;
    request.setUrl(QUrl(baseUrl + QStringLiteral("/api/v1/chat-files")));
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());
    request.setRawHeader("X-Workspace-Id", workspaceId.toUtf8());
    request.setRawHeader("X-File-Name", QFileInfo(filePath).fileName().toUtf8());
    request.setRawHeader("X-Uploader-Name", uploaderName.toUtf8());
    if (!clientId.isEmpty())
        request.setRawHeader("X-Client-Id", clientId.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    request.setHeader(QNetworkRequest::ContentLengthHeader, m_uploadFile->size());

    m_uploadTimedOut = false;
    m_currentUpload = m_nam.post(request, m_uploadFile);
    qInfo() << "[http-file-upload] POST sent";
    resetUploadStallTimer();
    connect(m_currentUpload, &QNetworkReply::uploadProgress, this,
            [this](qint64 sent, qint64 total) {
        resetUploadStallTimer();
        qint64 displaySent = sent;
        if (total > 0 && displaySent >= total) {
            displaySent = qMax<qint64>(0, total - 1);
        }
        emit uploadProgress(displaySent, total);
    });
    connect(m_currentUpload, &QNetworkReply::finished, this, [this]() {
        auto* reply = m_currentUpload;
        m_currentUpload = nullptr;
        m_uploadStallTimer.stop();
        const qint64 uploadTotal = m_uploadFile ? m_uploadFile->size() : 0;
        if (m_uploadFile) {
            m_uploadFile->close();
            m_uploadFile->deleteLater();
            m_uploadFile = nullptr;
        }
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            const QString errorText = m_uploadTimedOut
                ? QStringLiteral("upload timeout")
                : reply->errorString();
            m_uploadTimedOut = false;
            qWarning() << "[http-file-upload] FAILED: httpError=" << reply->error()
                       << "errorString=" << errorText
                       << "httpStatus=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                       << "body=" << reply->readAll().left(500);
            emit uploadFailed(errorText);
            return;
        }
        m_uploadTimedOut = false;
        const QByteArray respBody = reply->readAll();
        const QJsonObject json = QJsonDocument::fromJson(respBody).object();
        qInfo() << "[http-file-upload] SUCCESS: httpStatus=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                << "responseBody=" << respBody.left(500);
        if (uploadTotal > 0) {
            emit uploadProgress(uploadTotal, uploadTotal);
        }
        emit uploadFinished(json);
    });
}

void HttpFileClient::startDownload(const QString& url, const QString& token,
                                    const QString& savePath, qint64 existingBytes) {
    if (m_currentDownload) {
        emit downloadFailed(QStringLiteral("下载已在进行中"));
        return;
    }

    m_downloadSavePath = savePath;
    m_downloadOffset = existingBytes;
    const QString partPath = savePath + QStringLiteral(".part");

    m_downloadFile = new QFile(partPath, this);
    QIODevice::OpenMode mode = existingBytes > 0 ? QIODevice::Append : QIODevice::WriteOnly;
    if (!m_downloadFile->open(mode)) {
        delete m_downloadFile;
        m_downloadFile = nullptr;
        emit downloadFailed(QStringLiteral("无法创建临时文件"));
        return;
    }

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());
    if (existingBytes > 0) {
        request.setRawHeader("Range",
            QStringLiteral("bytes=%1-").arg(existingBytes).toUtf8());
    }

    m_currentDownload = m_nam.get(request);
    connect(m_currentDownload, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile) {
            m_downloadFile->write(m_currentDownload->readAll());
        }
    });
    connect(m_currentDownload, &QNetworkReply::downloadProgress, this,
        [this](qint64 received, qint64 total) {
            const qint64 totalAdjusted = (total > 0) ? total + m_downloadOffset : 0;
            emit downloadProgress(received + m_downloadOffset, totalAdjusted);
        });
    connect(m_currentDownload, &QNetworkReply::finished, this, [this]() {
        auto* reply = m_currentDownload;
        m_currentDownload = nullptr;
        reply->deleteLater();
        if (m_downloadFile) {
            m_downloadFile->close();
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        if (reply->error() != QNetworkReply::NoError
            && reply->error() != QNetworkReply::OperationCanceledError) {
            qWarning() << "[http-file-download] FAILED url=" << reply->url().toString()
                       << "error=" << reply->error()
                       << "errorString=" << reply->errorString()
                       << "httpStatus=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            emit downloadFailed(reply->errorString());
            return;
        }
        if (reply->error() == QNetworkReply::OperationCanceledError) {
            return; // 暂停或取消，不发信号
        }
        const QString partPath = m_downloadSavePath + QStringLiteral(".part");
        if (QFile::exists(m_downloadSavePath))
            QFile::remove(m_downloadSavePath);
        if (!QFile::rename(partPath, m_downloadSavePath)) {
            emit downloadFailed(QStringLiteral("文件落盘失败"));
            return;
        }
        emit downloadFinished(m_downloadSavePath);
    });
}

void HttpFileClient::pauseDownload() {
    if (m_currentDownload) {
        m_currentDownload->abort();
        // 保留 .part 文件不删除，下次 resume 用 existingBytes
    }
}

void HttpFileClient::cancelDownload() {
    if (m_currentDownload) {
        m_currentDownload->abort();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        m_downloadFile->remove();
        delete m_downloadFile;
        m_downloadFile = nullptr;
    }
}
