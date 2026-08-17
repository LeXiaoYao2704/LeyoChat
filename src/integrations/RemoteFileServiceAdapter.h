#pragma once
#include <QHash>
#include <QIODevice>
#include <QJsonDocument>
#include <QMap>
#include <QString>
#include <QUrl>
#include <QVector>
#include <memory>
#include <optional>

#include "integrations/RemoteFileServiceContracts.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "domain/ResourceRefPayload.h"

// Transport interface — same pattern as IAzureDevOpsApiTransport
class IRemoteFileServiceTransport {
public:
    virtual ~IRemoteFileServiceTransport() = default;

    virtual std::optional<QJsonDocument> getJson(
        const QUrl& url,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const = 0;

    virtual std::optional<QByteArray> getBytes(
        const QUrl& url,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const = 0;

    // PUT raw bytes, returns JSON response body
    virtual std::optional<QJsonDocument> putBytes(
        const QUrl& url,
        const QByteArray& data,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const = 0;

    virtual std::optional<QJsonDocument> putDevice(
        const QUrl& url,
        QIODevice* device,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const = 0;

    // POST raw bytes, returns JSON response body
    virtual std::optional<QJsonDocument> postBytes(
        const QUrl& url,
        const QByteArray& data,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const = 0;

    virtual std::optional<QJsonDocument> postDevice(
        const QUrl& url,
        QIODevice* device,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const = 0;
};

class NetworkRemoteFileServiceTransport : public IRemoteFileServiceTransport {
public:
    std::optional<QJsonDocument> getJson(
        const QUrl& url,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const override;

    std::optional<QByteArray> getBytes(
        const QUrl& url,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const override;

    std::optional<QJsonDocument> putBytes(
        const QUrl& url,
        const QByteArray& data,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const override;

    std::optional<QJsonDocument> putDevice(
        const QUrl& url,
        QIODevice* device,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const override;

    std::optional<QJsonDocument> postBytes(
        const QUrl& url,
        const QByteArray& data,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const override;

    std::optional<QJsonDocument> postDevice(
        const QUrl& url,
        QIODevice* device,
        const QMap<QByteArray, QByteArray>& extraHeaders,
        const RemoteFileServiceConnectionSettings& settings,
        QString* errorMessage) const override;
};

class RemoteFileServiceAdapter {
public:
    explicit RemoteFileServiceAdapter(
        RemoteFileServiceConnectionSettings settings = {},
        std::shared_ptr<IRemoteFileServiceTransport> transport =
            std::make_shared<NetworkRemoteFileServiceTransport>());

    bool testConnection(QString* errorMessage = nullptr) const;

    // Upload new file, returns {fileId, versionId} on success
    std::optional<RemoteFileUploadResult> uploadFile(
        const QString& workspaceId,
        const QString& localFilePath,
        const QString& changeNote,
        const QString& uploaderName,
        const QString& clientId = {},
        QString* errorMessage = nullptr) const;

    // Upload new version of existing file
    std::optional<RemoteFileUploadResult> uploadNewVersion(
        const QString& fileId,
        const QString& localFilePath,
        const QString& changeNote,
        const QString& uploaderName,
        const QString& clientId = {},
        QString* errorMessage = nullptr) const;

    // Download latest version to saveToDir, returns saved file path
    std::optional<QString> downloadFile(
        const QString& fileId,
        const QString& fileName,
        const QString& saveToDir,
        QString* errorMessage = nullptr) const;

    // Download from a full URL (URL comes from the card payload's action target)
    std::optional<QString> downloadByUrl(
        const QString& fullUrl,
        const QString& saveFileName,
        const QString& saveToDir,
        QString* errorMessage = nullptr) const;

    std::optional<QString> downloadVersion(
        const QString& fileId,
        const QString& versionId,
        const QString& fileName,
        const QString& saveToDir,
        QString* errorMessage = nullptr) const;

    QVector<RemoteFileInfo>    listFiles(const QString& workspaceId,
                                         QString* errorMessage = nullptr) const;
    QVector<RemoteFileVersion> getVersionHistory(const QString& fileId,
                                                  QString* errorMessage = nullptr) const;

    // Build ResourceRefPayload from fileId (uses makePayload from SharedFileResourceContracts)
    // Non-const: writes to cache
    std::optional<ResourceRefPayload> payloadForFileId(
        const QString& fileId,
        const QString& workspaceId,
        const QString& fileName,
        const QString& uploaderName,
        qint64 fileSizeBytes,
        QString* errorMessage = nullptr);

private:
    QUrl filesUrl() const;
    QUrl fileUrl(const QString& fileId) const;
    QUrl downloadUrl(const QString& fileId) const;
    QUrl versionsUrl(const QString& fileId) const;
    QUrl versionDownloadUrl(const QString& fileId, const QString& versionId) const;
    QUrl pingUrl() const;

    QString bearerAuthHeader() const;

    RemoteFileServiceConnectionSettings          m_settings;
    std::shared_ptr<IRemoteFileServiceTransport> m_transport;
    QHash<QString, ResourceRefPayload>           m_payloadCache;
};
