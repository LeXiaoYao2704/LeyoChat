#pragma once
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QSet>
#include <QMutex>
#include <functional>
#include "FileServiceDatabase.h"
#include "FileStorageManager.h"
#include "FileServiceAuth.h"
#include "WopiHandler.h"
#include "OnlyOfficeCallbackHandler.h"

class FileServiceHttpServer : public QObject {
    Q_OBJECT
public:
    using RouteRegistrar = std::function<void(QHttpServer&)>;

    explicit FileServiceHttpServer(FileServiceDatabase* db,
                                   FileStorageManager* storage,
                                   FileServiceAuth* auth,
                                   const QString& onlyOfficeUrl = QString(),
                                   const QString& externalUrl = QString(),
                                   const QString& jwtSecret = QString(),
                                   QObject* parent = nullptr);
    FileServiceHttpServer(FileServiceDatabase* db,
                          FileStorageManager* storage,
                          FileServiceAuth* auth,
                          const QString& onlyOfficeUrl,
                          const QString& externalUrl,
                          const QString& jwtSecret,
                          RouteRegistrar routeRegistrar,
                          QObject* parent = nullptr);

    bool listen(const QHostAddress& address, quint16 port);

    // 聊天文件清理配置
    void setChatFileCleanupConfig(int ttlDays, int quotaMb);
    void setLegacyFileAccessEnabled(bool enabled);
    bool legacyFileAccessEnabled() const;

private:
    void setupRoutes();
    void startChatFileCleanup();
    std::optional<AuthenticatedClient> authenticateFileRequest(
        const QHttpServerRequest& request,
        const char* routeName) const;
    QString buildEditorHtml(const QString& fileId, const QString& fileName,
                            const QString& fileType, const QString& documentType,
                            const QString& version, const QString& token,
                            const QString& userId, const QString& userName,
                            const QString& clientHost) const;

    QHttpServer       m_server;
    FileServiceDatabase*  m_db;
    FileStorageManager*   m_storage;
    FileServiceAuth*      m_auth;
    WopiHandler*          m_wopiHandler = nullptr;
    OnlyOfficeCallbackHandler* m_callbackHandler = nullptr;
    static QByteArray generateJwt(const QJsonObject& payload, const QByteArray& secret);

    QString               m_onlyOfficeUrl;
    QString               m_externalUrl;
    QByteArray            m_jwtSecret;

    // 聊天文件清理
    QSet<QString>  m_activeDownloads;
    QMutex         m_activeDownloadsMutex;
    int m_chatFileTtlDays  = 7;
    int m_chatFileQuotaMb  = 2048;
    bool m_legacyFileAccessEnabled = true;
    RouteRegistrar m_routeRegistrar;
};
