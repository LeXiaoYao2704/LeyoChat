#include "FileServiceHttpServer.h"
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QUrl>
#include <QUuid>
#include <QDateTime>
#include <QHostAddress>
#include <QFileInfo>
#include <QDebug>
#include <QTimer>
#include <QMessageAuthenticationCode>
#include <QMutexLocker>
#include <QtConcurrentRun>
#include <QSqlDatabase>
#include <utility>

FileServiceHttpServer::FileServiceHttpServer(FileServiceDatabase* db,
                                             FileStorageManager* storage,
                                             FileServiceAuth* auth,
                                             const QString& onlyOfficeUrl,
                                             const QString& externalUrl,
                                             const QString& jwtSecret,
                                             QObject* parent)
    : FileServiceHttpServer(db, storage, auth, onlyOfficeUrl, externalUrl,
                            jwtSecret, RouteRegistrar{}, parent)
{
}

FileServiceHttpServer::FileServiceHttpServer(FileServiceDatabase* db,
                                             FileStorageManager* storage,
                                             FileServiceAuth* auth,
                                             const QString& onlyOfficeUrl,
                                             const QString& externalUrl,
                                             const QString& jwtSecret,
                                             RouteRegistrar routeRegistrar,
                                             QObject* parent)
    : QObject(parent), m_db(db), m_storage(storage), m_auth(auth),
      m_onlyOfficeUrl(onlyOfficeUrl), m_externalUrl(externalUrl),
      m_jwtSecret(jwtSecret.toUtf8()),
      m_routeRegistrar(std::move(routeRegistrar))
{
    m_wopiHandler = new WopiHandler(db, storage);
    m_callbackHandler = new OnlyOfficeCallbackHandler(db, storage);
    setupRoutes();
    if (m_routeRegistrar)
        m_routeRegistrar(m_server);

    // 启动时清理过期记录
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_db->deleteExpiredFileLocks(now);
    m_db->deleteExpiredWopiTokens(now);

    // 每小时清理一次
    auto* cleanupTimer = new QTimer(this);
    connect(cleanupTimer, &QTimer::timeout, this, [this]() {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        const int locks = m_db->deleteExpiredFileLocks(nowMs);
        const int tokens = m_db->deleteExpiredWopiTokens(nowMs);
        if (locks > 0 || tokens > 0)
            qDebug() << "Cleanup: removed" << locks << "expired locks," << tokens << "expired tokens";
        startChatFileCleanup();
    });
    cleanupTimer->start(3600 * 1000);
}

bool FileServiceHttpServer::listen(const QHostAddress& address, quint16 port)
{
    const quint16 boundPort = m_server.listen(address, port);
    return boundPort != 0;
}

void FileServiceHttpServer::setLegacyFileAccessEnabled(bool enabled)
{
    m_legacyFileAccessEnabled = enabled;
}

bool FileServiceHttpServer::legacyFileAccessEnabled() const
{
    return m_legacyFileAccessEnabled;
}

std::optional<AuthenticatedClient> FileServiceHttpServer::authenticateFileRequest(
    const QHttpServerRequest& request,
    const char* routeName) const
{
    const QByteArray authHeader = request.value("Authorization");
    const auto clientOpt = m_auth->validate(QString::fromLatin1(authHeader));
    if (clientOpt)
        return clientOpt;

    if (!m_legacyFileAccessEnabled)
        return std::nullopt;

    qWarning().noquote()
        << QStringLiteral("[legacy-file-auth] accepted request without valid bearer route=%1 authHeaderPresent=%2")
               .arg(QString::fromLatin1(routeName ? routeName : "unknown"))
               .arg(!authHeader.trimmed().isEmpty());

    return AuthenticatedClient{
        QStringLiteral("legacy-file-client"),
        QStringLiteral("*"),
        QStringLiteral("member"),
        QStringLiteral("*")
    };
}

static QJsonObject fileRecordToJson(const FileRecord& r)
{
    return QJsonObject{
        {QStringLiteral("file_id"),          r.fileId},
        {QStringLiteral("workspace_id"),     r.workspaceId},
        {QStringLiteral("file_name"),        r.fileName},
        {QStringLiteral("current_version"),  r.currentVersion},
        {QStringLiteral("uploaded_by_id"),   r.uploadedById},
        {QStringLiteral("uploaded_by_name"), r.uploadedByName},
        {QStringLiteral("created_at_ms"),    r.createdAtMs},
        {QStringLiteral("updated_at_ms"),    r.updatedAtMs},
        {QStringLiteral("folder_id"),        r.folderId},
        {QStringLiteral("file_size"),        r.fileSize}
    };
}

static QJsonObject versionRecordToJson(const FileVersionRecord& v)
{
    return QJsonObject{
        {QStringLiteral("version_id"),     v.versionId},
        {QStringLiteral("file_id"),        v.fileId},
        {QStringLiteral("version_number"), v.versionNumber},
        {QStringLiteral("version_label"),  v.versionLabel},
        {QStringLiteral("uploader_id"),    v.uploaderId},
        {QStringLiteral("uploader_name"),  v.uploaderName},
        {QStringLiteral("uploaded_at_ms"), v.uploadedAtMs},
        {QStringLiteral("file_size"),      v.fileSize},
        {QStringLiteral("storage_path"),   v.storagePath},
        {QStringLiteral("change_note"),    v.changeNote}
    };
}

void FileServiceHttpServer::setupRoutes()
{
    // GET /api/v1/ping — no auth
    m_server.route(QStringLiteral("/api/v1/ping"), QHttpServerRequest::Method::Get,
        [](const QHttpServerRequest&) -> QHttpServerResponse {
            return QHttpServerResponse(QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")}});
        });

    // GET /api/v1/files?workspaceId=... — auth required
    m_server.route(QStringLiteral("/api/v1/files"), QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt = authenticateFileRequest(request, "GET /api/v1/files");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const QString workspaceId = QUrlQuery(request.url()).queryItemValue(
                QStringLiteral("workspaceId"));
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            if (!m_auth->canAccessWorkspace(*clientOpt, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto files = m_db->listFilesByWorkspace(workspaceId);
            QJsonArray arr;
            for (const auto& f : files)
                arr.append(fileRecordToJson(f));
            return QHttpServerResponse(arr);
        });

    // PUT /api/v1/files/:fileName — auth required, upsert by workspace + name
    m_server.route(QStringLiteral("/api/v1/files/<arg>"), QHttpServerRequest::Method::Put,
        [this](const QString& fileNameParam, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt = authenticateFileRequest(request, "PUT /api/v1/files/<fileName>");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const QString workspaceId = QString::fromUtf8(request.value("X-Workspace-Id"));
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            if (!m_auth->canAccessWorkspace(*clientOpt, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QString fileName = request.value("X-File-Name").isEmpty()
                ? fileNameParam
                : QString::fromUtf8(request.value("X-File-Name"));
            const QString changeNote   = QString::fromUtf8(request.value("X-Change-Note"));
            const QString uploaderName = QString::fromUtf8(request.value("X-Uploader-Name"));
            const QString effectiveClientId = [&]() {
                const QString h = QString::fromUtf8(request.value("X-Client-Id"));
                return h.isEmpty() ? clientOpt->clientId : h;
            }();
            const QByteArray fileData  = request.body();
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

            // Upsert: check if file already exists in this workspace
            const auto existingFile = m_db->findFileByName(workspaceId, fileName);
            QString fileId;
            if (existingFile.has_value()) {
                fileId = existingFile->fileId;
            } else {
                fileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            }

            // ── STEP 1: Save bytes to disk first ──────────────────────────────────
            const QString versionId   = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const auto storagePathOpt = m_storage->saveFile(fileId, versionId, fileData);
            if (!storagePathOpt.has_value()) {
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
            }
            const QString storagePath = *storagePathOpt;

            // ── STEP 2: Begin DB transaction ──────────────────────────────────────
            const auto rollbackAndCleanup = [&]() -> QHttpServerResponse {
                m_db->rollback();
                m_storage->deleteFile(storagePath);
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
            };

            if (!m_db->beginTransaction())
                return rollbackAndCleanup();

            // ── STEP 3: Insert file record (new file only) ───────────────────────
            if (!existingFile.has_value()) {
                FileRecord newFile;
                newFile.fileId         = fileId;
                newFile.workspaceId    = workspaceId;
                newFile.fileName       = fileName;
                newFile.uploadedById   = effectiveClientId;
                newFile.uploadedByName = uploaderName.isEmpty() ? effectiveClientId : uploaderName;
                newFile.createdAtMs    = nowMs;
                newFile.updatedAtMs    = nowMs;
                if (!m_db->insertFile(newFile))
                    return rollbackAndCleanup();
            }

            // ── STEP 4: Insert version record ─────────────────────────────────────
            const int versionNum = m_db->nextVersionNumber(fileId);
            FileVersionRecord ver;
            ver.versionId     = versionId;
            ver.fileId        = fileId;
            ver.versionNumber = versionNum;
            ver.versionLabel  = QStringLiteral("v%1").arg(versionNum);
            ver.uploaderId    = effectiveClientId;
            ver.uploaderName  = uploaderName.isEmpty() ? effectiveClientId : uploaderName;
            ver.uploadedAtMs  = nowMs;
            ver.fileSize      = fileData.size();
            ver.storagePath   = storagePath;
            ver.changeNote    = changeNote;
            if (!m_db->insertVersion(ver))
                return rollbackAndCleanup();

            // ── STEP 5: Update current_version (check return value) ───────────────
            if (!m_db->updateFileCurrentVersion(fileId, versionId, nowMs))
                return rollbackAndCleanup();

            // ── STEP 6: Commit ────────────────────────────────────────────────────
            if (!m_db->commit())
                return rollbackAndCleanup();

            // ── Return 201 ────────────────────────────────────────────────────────
            QJsonObject resp;
            resp[QStringLiteral("file_id")]     = fileId;
            resp[QStringLiteral("version_id")]  = versionId;
            resp[QStringLiteral("version")]     = ver.versionLabel;
            resp[QStringLiteral("is_new_file")] = !existingFile.has_value();
            return QHttpServerResponse(QJsonDocument(resp).toJson(),
                                       QHttpServerResponse::StatusCode::Created);
        });

    // GET /api/v1/files/:fileId — auth required, single file info
    m_server.route(QStringLiteral("/api/v1/files/<arg>"), QHttpServerRequest::Method::Get,
        [this](const QString& fileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt = authenticateFileRequest(request, "GET /api/v1/files/<fileId>");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            if (!m_auth->canAccessWorkspace(*clientOpt, fileOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            return QHttpServerResponse(fileRecordToJson(*fileOpt));
        });

    // GET /api/v1/files/:fileId/download — auth required (Authorization header or ?token= query)
    m_server.route(QStringLiteral("/api/v1/files/<arg>/download"), QHttpServerRequest::Method::Get,
        [this](const QString& fileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            // Support two auth modes:
            // 1. Authorization header (normal client auth)
            // 2. ?token= query parameter (WOPI access_token for ONLYOFFICE)
            const QUrlQuery query(request.query());
            const QString wopiToken = query.queryItemValue(QStringLiteral("token"));
            bool authorized = false;

            if (!wopiToken.isEmpty()) {
                const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
                const auto tokenOpt = m_db->validateWopiToken(wopiToken, nowMs);
                if (tokenOpt && tokenOpt->fileId == fileId)
                    authorized = true;
            }
            if (!authorized) {
                const auto clientOpt =
                    authenticateFileRequest(request, "GET /api/v1/files/<fileId>/download");
                if (!clientOpt)
                    return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
                const auto fileOpt2 = m_db->findFileById(fileId);
                if (fileOpt2 && !m_auth->canAccessWorkspace(*clientOpt, fileOpt2->workspaceId))
                    return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
            }

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            const auto verOpt = m_db->findCurrentVersion(fileId);
            if (!verOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            const auto bytesOpt = m_storage->readFile(verOpt->storagePath);
            if (!bytesOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

            QHttpServerResponse resp(QByteArrayLiteral("application/octet-stream"), *bytesOpt);
            resp.addHeader("Content-Disposition",
                QStringLiteral("attachment; filename=\"%1\"").arg(fileOpt->fileName).toLatin1());
            return resp;
        });

    // GET /api/v1/files/:fileId/versions — auth required
    m_server.route(QStringLiteral("/api/v1/files/<arg>/versions"), QHttpServerRequest::Method::Get,
        [this](const QString& fileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "GET /api/v1/files/<fileId>/versions");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            if (!m_auth->canAccessWorkspace(*clientOpt, fileOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto versions = m_db->listVersionsByFile(fileId);
            QJsonArray arr;
            for (const auto& v : versions)
                arr.append(versionRecordToJson(v));
            return QHttpServerResponse(arr);
        });

    // POST /api/v1/files/:fileId/versions — auth required, upload new version
    m_server.route(QStringLiteral("/api/v1/files/<arg>/versions"), QHttpServerRequest::Method::Post,
        [this](const QString& fileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "POST /api/v1/files/<fileId>/versions");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            if (!m_auth->canAccessWorkspace(*clientOpt, fileOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QString changeNote   = QString::fromUtf8(request.value("X-Change-Note"));
            const QString uploaderName = [&]() -> QString {
                const QString h = QString::fromUtf8(request.value("X-Uploader-Name"));
                return h.isEmpty() ? clientOpt->clientId : h;
            }();
            const QString effectiveClientId = [&]() {
                const QString h = QString::fromUtf8(request.value("X-Client-Id"));
                return h.isEmpty() ? clientOpt->clientId : h;
            }();

            const QByteArray body = request.body();
            const qint64 nowMs    = QDateTime::currentMSecsSinceEpoch();

            const int nextVersionNum = m_db->nextVersionNumber(fileId);

            const QString versionId = QUuid::createUuid().toString(QUuid::WithoutBraces);

            const auto storagePathOpt = m_storage->saveFile(fileId, versionId, body);
            if (!storagePathOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
            const QString storagePath = *storagePathOpt;

            const auto rollbackAndCleanup = [&]() -> QHttpServerResponse {
                m_db->rollback();
                m_storage->deleteFile(storagePath);
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
            };

            if (!m_db->beginTransaction())
                return rollbackAndCleanup();

            FileVersionRecord ver;
            ver.versionId     = versionId;
            ver.fileId        = fileId;
            ver.versionNumber = nextVersionNum;
            ver.versionLabel  = QStringLiteral("v") + QString::number(nextVersionNum);
            ver.uploaderId    = effectiveClientId;
            ver.uploaderName  = uploaderName;
            ver.uploadedAtMs  = nowMs;
            ver.fileSize      = static_cast<qint64>(body.size());
            ver.storagePath   = storagePath;
            ver.changeNote    = changeNote;

            if (!m_db->insertVersion(ver))
                return rollbackAndCleanup();

            if (!m_db->updateFileCurrentVersion(fileId, versionId, nowMs))
                return rollbackAndCleanup();

            if (!m_db->commit())
                return rollbackAndCleanup();

            return QHttpServerResponse(
                QJsonObject{{QStringLiteral("version_id"), versionId}},
                QHttpServerResponse::StatusCode::Created);
        });

    // GET /api/v1/files/:fileId/versions/:versionId/download — auth required
    m_server.route(QStringLiteral("/api/v1/files/<arg>/versions/<arg>/download"),
        QHttpServerRequest::Method::Get,
        [this](const QString& fileId, const QString& versionId,
               const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt = authenticateFileRequest(
                request, "GET /api/v1/files/<fileId>/versions/<versionId>/download");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            if (!m_auth->canAccessWorkspace(*clientOpt, fileOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto verOpt = m_db->findVersionById(versionId);
            if (!verOpt || verOpt->fileId != fileId)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            const auto bytesOpt = m_storage->readFile(verOpt->storagePath);
            if (!bytesOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

            QHttpServerResponse resp(QByteArrayLiteral("application/octet-stream"), *bytesOpt);
            resp.addHeader("Content-Disposition",
                QStringLiteral("attachment; filename=\"%1\"").arg(fileOpt->fileName).toLatin1());
            return resp;
        });

    // ── Chat Files（群文件中转）────────────────────────────────────────

    // POST /api/v1/chat-files — 群文件上传（body 即为文件内容）
    m_server.route(QStringLiteral("/api/v1/chat-files"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "POST /api/v1/chat-files");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const QString workspaceId = QString::fromUtf8(request.value("X-Workspace-Id"));
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);
            if (!m_auth->canAccessWorkspace(*clientOpt, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QString fileName = QString::fromUtf8(request.value("X-File-Name"));
            const QString fileHash = QString::fromUtf8(request.value("X-File-Hash"));
            const QString uploaderName = QString::fromUtf8(request.value("X-Uploader-Name"));
            const QString effectiveClientId = [&]() {
                const QString h = QString::fromUtf8(request.value("X-Client-Id"));
                return h.isEmpty() ? clientOpt->clientId : h;
            }();
            const QByteArray body = request.body();
            if (body.isEmpty() || fileName.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            const QString chatFileId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

            const auto storagePathOpt = m_storage->saveFile(chatFileId, fileName, body);
            if (!storagePathOpt) {
                qWarning() << "[chat-files] saveFile FAILED chatFileId=" << chatFileId
                           << "fileName=" << fileName << "bodySize=" << body.size();
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
            }
            qInfo() << "[chat-files] saveFile OK chatFileId=" << chatFileId
                    << "storagePath=" << *storagePathOpt;

            ChatFileRecord record;
            record.chatFileId   = chatFileId;
            record.workspaceId  = workspaceId;
            record.fileName     = fileName;
            record.fileHash     = fileHash;
            record.uploaderId   = effectiveClientId;
            record.uploaderName = uploaderName.isEmpty() ? effectiveClientId : uploaderName;
            record.fileSize     = body.size();
            record.createdAtMs  = nowMs;
            record.storagePath  = *storagePathOpt;

            if (!m_db->insertChatFile(record)) {
                qWarning() << "[chat-files] insertChatFile FAILED chatFileId=" << chatFileId
                           << "workspaceId=" << workspaceId;
                m_storage->deleteFile(*storagePathOpt);
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);
            }
            qInfo() << "[chat-files] upload complete chatFileId=" << chatFileId
                    << "fileName=" << fileName << "fileSize=" << body.size();

            QJsonObject resp;
            resp[QStringLiteral("file_id")]   = chatFileId;
            resp[QStringLiteral("file_name")] = fileName;
            resp[QStringLiteral("file_size")] = body.size();
            resp[QStringLiteral("file_hash")] = fileHash;
            return QHttpServerResponse(QJsonDocument(resp).toJson(),
                                       QHttpServerResponse::StatusCode::Created);
        });

    // GET /api/v1/chat-files/:chatFileId — 下载（支持 Range 断点续传）
    m_server.route(QStringLiteral("/api/v1/chat-files/<arg>"), QHttpServerRequest::Method::Get,
        [this](const QString& chatFileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "GET /api/v1/chat-files/<chatFileId>");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto recordOpt = m_db->findChatFileById(chatFileId);
            if (!recordOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
            if (!m_auth->canAccessWorkspace(*clientOpt, recordOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            // 解析 Range 请求头
            const QString rangeHeader = QString::fromLatin1(request.value("Range"));
            qint64 offset = 0;
            qint64 length = -1;
            bool isRangeRequest = false;
            if (rangeHeader.startsWith(QStringLiteral("bytes="))) {
                isRangeRequest = true;
                const QString rangeSpec = rangeHeader.mid(6);
                const auto parts = rangeSpec.split(QLatin1Char('-'));
                if (parts.size() == 2) {
                    offset = parts[0].toLongLong();
                    if (!parts[1].isEmpty())
                        length = parts[1].toLongLong() - offset + 1;
                }
            }

            // 追踪活跃下载
            {
                QMutexLocker lock(&m_activeDownloadsMutex);
                m_activeDownloads.insert(chatFileId);
            }

            const auto rangeResult = m_storage->readFileRange(recordOpt->storagePath, offset, length);

            {
                QMutexLocker lock(&m_activeDownloadsMutex);
                m_activeDownloads.remove(chatFileId);
            }

            if (!rangeResult) {
                // 物理文件不存在（可能已被清理）
                return QHttpServerResponse(
                    QJsonDocument(QJsonObject{
                        {QStringLiteral("error"), QStringLiteral("file_expired")},
                        {QStringLiteral("message"), QStringLiteral("文件已过期，无法下载")}
                    }).toJson(QJsonDocument::Compact),
                    QHttpServerResponse::StatusCode::NotFound);
            }

            auto statusCode = isRangeRequest
                ? QHttpServerResponse::StatusCode::PartialContent
                : QHttpServerResponse::StatusCode::Ok;

            QHttpServerResponse resp(QByteArrayLiteral("application/octet-stream"),
                                     rangeResult->data, statusCode);
            resp.addHeader("Content-Disposition",
                QStringLiteral("attachment; filename=\"%1\"").arg(recordOpt->fileName).toLatin1());
            resp.addHeader("Accept-Ranges", "bytes");
            resp.addHeader("Content-Length",
                QByteArray::number(rangeResult->data.size()));
            if (isRangeRequest) {
                resp.addHeader("Content-Range",
                    QStringLiteral("bytes %1-%2/%3")
                        .arg(rangeResult->rangeStart)
                        .arg(rangeResult->rangeEnd)
                        .arg(rangeResult->totalSize).toLatin1());
            }
            return resp;
        });

    // GET /api/v1/chat-files/:chatFileId/meta — 元数据
    m_server.route(QStringLiteral("/api/v1/chat-files/<arg>/meta"), QHttpServerRequest::Method::Get,
        [this](const QString& chatFileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "GET /api/v1/chat-files/<chatFileId>/meta");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto recordOpt = m_db->findChatFileById(chatFileId);
            if (!recordOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
            if (!m_auth->canAccessWorkspace(*clientOpt, recordOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            QJsonObject resp;
            resp[QStringLiteral("file_id")]       = recordOpt->chatFileId;
            resp[QStringLiteral("file_name")]     = recordOpt->fileName;
            resp[QStringLiteral("file_size")]     = recordOpt->fileSize;
            resp[QStringLiteral("file_hash")]     = recordOpt->fileHash;
            resp[QStringLiteral("uploader_name")] = recordOpt->uploaderName;
            resp[QStringLiteral("created_at_ms")] = recordOpt->createdAtMs;
            return QHttpServerResponse(resp);
        });

    // ── Folders CRUD ──────────────────────────────────────────────────────

    // POST /api/v1/folders — create folder (admin only)
    m_server.route(QStringLiteral("/api/v1/folders"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "POST /api/v1/folders");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto body = QJsonDocument::fromJson(request.body()).object();
            const QString workspaceId = body[QStringLiteral("workspaceId")].toString().trimmed();
            const QString folderName  = body[QStringLiteral("folderName")].toString().trimmed();

            if (workspaceId.isEmpty() || folderName.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);
            if (!m_auth->canAccessWorkspace(*clientOpt, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);
            if (folderName.size() > 50 || folderName.contains(QLatin1Char('/'))
                || folderName.contains(QLatin1Char('\\'))
                || folderName.contains(QStringLiteral(".."))
                || folderName != folderName.trimmed())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            const QString folderId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            if (!m_db->insertFolder(folderId, workspaceId, folderName, clientOpt->clientId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Conflict);

            QJsonObject resp;
            resp[QStringLiteral("folder_id")]   = folderId;
            resp[QStringLiteral("folder_name")] = folderName;
            return QHttpServerResponse(QJsonDocument(resp).toJson(),
                                       QHttpServerResponse::StatusCode::Created);
        });

    // GET /api/v1/folders?workspaceId=... — list folders
    m_server.route(QStringLiteral("/api/v1/folders"), QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "GET /api/v1/folders");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const QString workspaceId = QUrlQuery(request.url()).queryItemValue(
                QStringLiteral("workspaceId"));
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);
            if (!m_auth->canAccessWorkspace(*clientOpt, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto folders = m_db->listFolders(workspaceId);
            QJsonArray arr;
            for (const auto& f : folders) {
                QJsonObject obj;
                obj[QStringLiteral("folder_id")]    = f.folderId;
                obj[QStringLiteral("workspace_id")] = f.workspaceId;
                obj[QStringLiteral("folder_name")]  = f.folderName;
                obj[QStringLiteral("created_by_id")] = f.createdById;
                obj[QStringLiteral("created_at_ms")] = f.createdAtMs;
                arr.append(obj);
            }
            return QHttpServerResponse(arr);
        });

    // DELETE /api/v1/folders/<folderId> — delete folder (admin only), moves files to root
    m_server.route(QStringLiteral("/api/v1/folders/<arg>"), QHttpServerRequest::Method::Delete,
        [this](const QString& folderId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "DELETE /api/v1/folders/<folderId>");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (clientOpt->role != QStringLiteral("admin"))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (!m_db->deleteFolder(folderId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            return QHttpServerResponse(QHttpServerResponse::StatusCode::NoContent);
        });

    // DELETE /api/v1/files/<fileId> — delete file (uploader or admin)
    m_server.route(QStringLiteral("/api/v1/files/<arg>"), QHttpServerRequest::Method::Delete,
        [this](const QString& fileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "DELETE /api/v1/files/<fileId>");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
            if (!m_auth->canAccessWorkspace(*clientOpt, fileOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (clientOpt->clientId != fileOpt->uploadedById
                && clientOpt->role != QStringLiteral("admin"))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto paths = m_db->versionStoragePaths(fileId);
            if (!m_db->deleteFileAndVersions(fileId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

            for (const auto& path : paths)
                m_storage->deleteFile(path);

            return QHttpServerResponse(QHttpServerResponse::StatusCode::NoContent);
        });

    // PUT /api/v1/files/<fileId>/folder — move file to folder
    m_server.route(QStringLiteral("/api/v1/files/<arg>/folder"), QHttpServerRequest::Method::Put,
        [this](const QString& fileId, const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "PUT /api/v1/files/<fileId>/folder");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
            if (!m_auth->canAccessWorkspace(*clientOpt, fileOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto body = QJsonDocument::fromJson(request.body()).object();
            const QString folderId = body[QStringLiteral("folderId")].toString();

            if (!m_db->updateFileFolderId(fileId, folderId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

            return QHttpServerResponse(QHttpServerResponse::StatusCode::Ok);
        });

    // ── WOPI Tokens ───────────────────────────────────────────────────────

    // POST /api/v1/wopi-tokens/renew — 续期 WOPI token
    // NOTE: must be registered before the shorter /api/v1/wopi-tokens route
    m_server.route(QStringLiteral("/api/v1/wopi-tokens/renew"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto body = QJsonDocument::fromJson(request.body()).object();
            const QString accessToken = body[QStringLiteral("access_token")].toString().trimmed();
            if (accessToken.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const auto tokenOpt = m_db->validateWopiToken(accessToken, nowMs);
            if (!tokenOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const qint64 newExpiresAtMs = nowMs + 3600 * 1000;
            if (!m_db->renewWopiToken(accessToken, newExpiresAtMs))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

            QJsonObject resp;
            resp[QStringLiteral("expires_in")] = 3600;
            return QHttpServerResponse(resp);
        });

    // POST /api/v1/wopi-tokens — 申请 WOPI token
    m_server.route(QStringLiteral("/api/v1/wopi-tokens"), QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto clientOpt =
                authenticateFileRequest(request, "POST /api/v1/wopi-tokens");
            if (!clientOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);

            const auto body = QJsonDocument::fromJson(request.body()).object();
            const QString fileId = body[QStringLiteral("fileId")].toString().trimmed();
            if (fileId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
            if (!m_auth->canAccessWorkspace(*clientOpt, fileOpt->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QString accessToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const QString displayName = body[QStringLiteral("displayName")].toString();

            WopiTokenRecord record;
            record.token       = accessToken;
            record.fileId      = fileId;
            record.clientId    = clientOpt->clientId;
            record.displayName = displayName.isEmpty() ? clientOpt->clientId : displayName;
            record.role        = clientOpt->role;
            record.createdAtMs = nowMs;
            record.expiresAtMs = nowMs + 3600 * 1000;

            if (!m_db->insertWopiToken(record))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::InternalServerError);

            QJsonObject resp;
            resp[QStringLiteral("access_token")] = accessToken;
            resp[QStringLiteral("expires_in")]   = 3600;
            return QHttpServerResponse(resp);
        });

    // ── WOPI endpoints (used by ONLYOFFICE Document Server) ─────────

    // GET /wopi/files/<fileId> — CheckFileInfo
    m_server.route(QStringLiteral("/wopi/files/<arg>"), QHttpServerRequest::Method::Get,
        [this](const QString& fileId, const QHttpServerRequest& request) {
            const QUrlQuery query(request.query());
            const QString accessToken = query.queryItemValue(QStringLiteral("access_token"));
            auto result = m_wopiHandler->checkFileInfo(fileId, accessToken);
            return QHttpServerResponse(result.json,
                static_cast<QHttpServerResponse::StatusCode>(result.statusCode));
        });

    // GET /wopi/files/<fileId>/contents — GetFile
    m_server.route(QStringLiteral("/wopi/files/<arg>/contents"), QHttpServerRequest::Method::Get,
        [this](const QString& fileId, const QHttpServerRequest& request) {
            const QUrlQuery query(request.query());
            const QString accessToken = query.queryItemValue(QStringLiteral("access_token"));
            auto result = m_wopiHandler->getFile(fileId, accessToken);
            if (result.statusCode != 200) {
                return QHttpServerResponse(
                    static_cast<QHttpServerResponse::StatusCode>(result.statusCode));
            }
            return QHttpServerResponse(result.contentType.toUtf8(), result.content);
        });

    // POST /wopi/files/<fileId>/contents — PutFile
    m_server.route(QStringLiteral("/wopi/files/<arg>/contents"), QHttpServerRequest::Method::Post,
        [this](const QString& fileId, const QHttpServerRequest& request) {
            const QUrlQuery query(request.query());
            const QString accessToken = query.queryItemValue(QStringLiteral("access_token"));
            const QString lockId = QString::fromUtf8(
                request.value(QByteArrayLiteral("X-WOPI-Lock")));
            int status = m_wopiHandler->putFile(fileId, accessToken, lockId, request.body());
            if (status == 200) {
                QJsonObject empty;
                return QHttpServerResponse(empty);
            }
            return QHttpServerResponse(
                static_cast<QHttpServerResponse::StatusCode>(status));
        });

    // POST /wopi/files/<fileId> — Lock/Unlock/RefreshLock
    m_server.route(QStringLiteral("/wopi/files/<arg>"), QHttpServerRequest::Method::Post,
        [this](const QString& fileId, const QHttpServerRequest& request) {
            const QUrlQuery query(request.query());
            const QString accessToken = query.queryItemValue(QStringLiteral("access_token"));
            const QString wopiOverride = QString::fromUtf8(
                request.value(QByteArrayLiteral("X-WOPI-Override")));
            const QString lockId = QString::fromUtf8(
                request.value(QByteArrayLiteral("X-WOPI-Lock")));
            auto result = m_wopiHandler->handleLock(fileId, accessToken, wopiOverride, lockId);
            auto resp = QHttpServerResponse(
                static_cast<QHttpServerResponse::StatusCode>(result.statusCode));
            if (!result.existingLockId.isEmpty()) {
                resp.addHeader(QByteArrayLiteral("X-WOPI-Lock"), result.existingLockId.toUtf8());
            }
            return resp;
        });

    // POST /api/v1/onlyoffice/callback/<fileId>?access_token=... — ONLYOFFICE save callback
    m_server.route(QStringLiteral("/api/v1/onlyoffice/callback/<arg>"), QHttpServerRequest::Method::Post,
        [this](const QString& fileId, const QHttpServerRequest& request) {
            const QUrlQuery query(request.query());
            const QString accessToken = query.queryItemValue(QStringLiteral("access_token"));
            const auto body = QJsonDocument::fromJson(request.body()).object();
            auto result = m_callbackHandler->handleCallback(fileId, accessToken, body);
            return QHttpServerResponse(result);
        });

    // ── Editor page ─────────────────────────────────────────────────
    // GET /editor/<fileId>?access_token=...
    // Returns an HTML page that loads the ONLYOFFICE Document Server editor.
    m_server.route(QStringLiteral("/editor/<arg>"), QHttpServerRequest::Method::Get,
        [this](const QString& fileId, const QHttpServerRequest& request) {
            const QUrlQuery query(request.query());
            const QString accessToken = query.queryItemValue(QStringLiteral("access_token"));
            if (accessToken.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const auto tokenOpt = m_db->validateWopiToken(accessToken, nowMs);
            if (!tokenOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (tokenOpt->fileId != fileId)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto fileOpt = m_db->findFileById(fileId);
            if (!fileOpt)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            const QString ext = QFileInfo(fileOpt->fileName).suffix().toLower();
            QString fileType = ext;
            QString documentType;
            if (ext == QStringLiteral("xlsx") || ext == QStringLiteral("xls") ||
                ext == QStringLiteral("xlsm") || ext == QStringLiteral("xlsb") ||
                ext == QStringLiteral("csv") || ext == QStringLiteral("ods")) {
                documentType = QStringLiteral("cell");
            } else if (ext == QStringLiteral("pptx") || ext == QStringLiteral("ppt") ||
                       ext == QStringLiteral("pptm") || ext == QStringLiteral("odp")) {
                documentType = QStringLiteral("slide");
            } else {
                documentType = QStringLiteral("word");
            }

            // Extract the hostname from the request so that URLs in the
            // editor HTML resolve to the actual server, not localhost.
            const QString hostHeader = QString::fromUtf8(
                request.value(QByteArrayLiteral("Host")));

            const QString html = buildEditorHtml(
                fileId, fileOpt->fileName, fileType, documentType,
                fileOpt->currentVersion, accessToken,
                tokenOpt->clientId, tokenOpt->displayName, hostHeader);

            return QHttpServerResponse(QByteArrayLiteral("text/html"), html.toUtf8());
        });
}

QString FileServiceHttpServer::buildEditorHtml(
    const QString& fileId, const QString& fileName,
    const QString& fileType, const QString& documentType,
    const QString& version, const QString& token,
    const QString& userId, const QString& userName,
    const QString& clientHost) const
{
    // Derive client-reachable host from the request Host header.
    // clientHost is e.g. "192.0.2.100:8765" or "myserver:8765".
    // Extract just the hostname part (strip port).
    QString hostname = clientHost;
    const int colonIdx = hostname.lastIndexOf(QLatin1Char(':'));
    if (colonIdx > 0)
        hostname = hostname.left(colonIdx);

    // Build client-reachable base URLs using the real hostname.
    // FileService URL: same hostname, same port as externalUrl
    QUrl extBase(m_externalUrl);
    extBase.setHost(hostname);
    const QString clientExternalUrl = extBase.toString(QUrl::StripTrailingSlash);

    // ONLYOFFICE URL: same hostname, port from m_onlyOfficeUrl (default 80)
    QUrl ooBase(m_onlyOfficeUrl);
    ooBase.setHost(hostname);
    const QString clientOnlyOfficeUrl = ooBase.toString(QUrl::StripTrailingSlash);

    const QString fileUrl = clientExternalUrl
        + QStringLiteral("/api/v1/files/") + fileId
        + QStringLiteral("/download?token=") + token;

    const QString callbackUrl = clientExternalUrl
        + QStringLiteral("/api/v1/onlyoffice/callback/") + fileId
        + QStringLiteral("?access_token=") + token;

    const QString docKey = fileId + QStringLiteral("_") + version;

    // Build the config as JSON so we can sign it with JWT
    QJsonObject permissions;
    permissions[QStringLiteral("edit")] = true;
    permissions[QStringLiteral("download")] = true;
    permissions[QStringLiteral("print")] = true;

    QJsonObject document;
    document[QStringLiteral("fileType")] = fileType;
    document[QStringLiteral("key")] = docKey;
    document[QStringLiteral("title")] = fileName;
    document[QStringLiteral("url")] = fileUrl;
    document[QStringLiteral("permissions")] = permissions;

    QJsonObject user;
    user[QStringLiteral("id")] = userId;
    user[QStringLiteral("name")] = userName;

    QJsonObject customization;
    customization[QStringLiteral("autosave")] = true;
    customization[QStringLiteral("forcesave")] = true;

    QJsonObject editorConfig;
    editorConfig[QStringLiteral("callbackUrl")] = callbackUrl;
    editorConfig[QStringLiteral("lang")] = QStringLiteral("zh-CN");
    editorConfig[QStringLiteral("user")] = user;
    editorConfig[QStringLiteral("customization")] = customization;

    QJsonObject config;
    config[QStringLiteral("document")] = document;
    config[QStringLiteral("documentType")] = documentType;
    config[QStringLiteral("editorConfig")] = editorConfig;
    config[QStringLiteral("type")] = QStringLiteral("desktop");
    config[QStringLiteral("height")] = QStringLiteral("100%");
    config[QStringLiteral("width")] = QStringLiteral("100%");

    // Sign with JWT if secret is configured
    if (!m_jwtSecret.isEmpty()) {
        const QString jwt = QString::fromLatin1(generateJwt(config, m_jwtSecret));
        config[QStringLiteral("token")] = jwt;
    }

    const QString configJson = QString::fromUtf8(
        QJsonDocument(config).toJson(QJsonDocument::Compact));

    return QStringLiteral(R"html(<!DOCTYPE html>
<html><head>
<meta charset="utf-8">
<title>)html") + fileName.toHtmlEscaped() + QStringLiteral(R"html(</title>
<style>html,body{margin:0;padding:0;height:100%;overflow:hidden}
#editor{width:100%;height:100%}
#err{display:none;color:#c62828;background:#ffebee;padding:16px;font-size:14px;white-space:pre-wrap}</style>
</head><body>
<div id="err"></div>
<div id="editor"></div>
<script src=")html") + clientOnlyOfficeUrl + QStringLiteral(R"html(/web-apps/apps/api/documents/api.js"></script>
<script>
try {
  var editor = new DocsAPI.DocEditor("editor", )html") + configJson + QStringLiteral(R"html();
} catch(e) {
  var el = document.getElementById("err");
  el.style.display = "block";
  el.textContent = "编辑器初始化失败: " + e.message + "\n\n" + e.stack;
}
</script></body></html>)html");
}

QByteArray FileServiceHttpServer::generateJwt(const QJsonObject& payload, const QByteArray& secret)
{
    // JWT header: {"alg":"HS256","typ":"JWT"}
    const QByteArray header = QByteArrayLiteral("{\"alg\":\"HS256\",\"typ\":\"JWT\"}");
    const QByteArray headerB64 = header.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
    const QByteArray payloadB64 = QJsonDocument(payload).toJson(QJsonDocument::Compact)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    const QByteArray signingInput = headerB64 + '.' + payloadB64;
    const QByteArray signature = QMessageAuthenticationCode::hash(
        signingInput, secret, QCryptographicHash::Sha256)
        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);

    return signingInput + '.' + signature;
}

void FileServiceHttpServer::setChatFileCleanupConfig(int ttlDays, int quotaMb)
{
    m_chatFileTtlDays = ttlDays;
    m_chatFileQuotaMb = quotaMb;
}

void FileServiceHttpServer::startChatFileCleanup()
{
    // 快照活跃下载集合
    QSet<QString> activeSnapshot;
    {
        QMutexLocker lock(&m_activeDownloadsMutex);
        activeSnapshot = m_activeDownloads;
    }

    const int ttlDays = m_chatFileTtlDays;
    const qint64 quotaBytes = static_cast<qint64>(m_chatFileQuotaMb) * 1024 * 1024;
    const QString dbPath = m_db->dbPath();
    const QString storageRootPath = m_storage->storageRoot();

    (void)QtConcurrent::run([dbPath, storageRootPath, ttlDays, quotaBytes, activeSnapshot]() {
        // 工作线程：独立 SQLite 连接
        const QString connName = QStringLiteral("chat-cleanup-") +
            QUuid::createUuid().toString(QUuid::WithoutBraces);
        {
            FileServiceDatabase cleanupDb(dbPath, connName);
            if (!cleanupDb.open()) {
                qWarning() << "[chat-cleanup] Failed to open DB connection";
                return;
            }
            FileStorageManager cleanupStorage(storageRootPath);

            int deletedCount = 0;
            qint64 freedBytes = 0;

            // 阶段1: 按时间过期删除
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 cutoffMs = nowMs - static_cast<qint64>(ttlDays) * 86400000LL;
            auto expired = cleanupDb.getExpiredChatFiles(cutoffMs);
            for (const auto& rec : expired) {
                if (activeSnapshot.contains(rec.chatFileId))
                    continue;
                cleanupStorage.deleteFile(rec.storagePath);
                cleanupDb.deleteChatFileById(rec.chatFileId);
                freedBytes += rec.fileSize;
                ++deletedCount;
            }

            // 阶段2: 按配额删除（如果超额）
            qint64 totalSize = cleanupDb.getChatFilesTotalSize();
            if (totalSize > quotaBytes) {
                auto oldest = cleanupDb.getOldestChatFiles(500);
                for (const auto& rec : oldest) {
                    if (totalSize <= quotaBytes)
                        break;
                    if (activeSnapshot.contains(rec.chatFileId))
                        continue;
                    cleanupStorage.deleteFile(rec.storagePath);
                    cleanupDb.deleteChatFileById(rec.chatFileId);
                    freedBytes += rec.fileSize;
                    totalSize -= rec.fileSize;
                    ++deletedCount;
                }
            }

            if (deletedCount > 0) {
                qInfo() << "[chat-cleanup] Removed" << deletedCount
                        << "chat files, freed" << (freedBytes / 1024 / 1024) << "MB";
            }
        }
        QSqlDatabase::removeDatabase(connName);
    });
}
