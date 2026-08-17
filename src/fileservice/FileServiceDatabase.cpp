#include "FileServiceDatabase.h"
#include <QDateTime>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

FileServiceDatabase::FileServiceDatabase(const QString& dbPath, const QString& connectionName)
    : m_connectionName(connectionName), m_dbPath(dbPath)
{
}

FileServiceDatabase::~FileServiceDatabase()
{
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool FileServiceDatabase::open()
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_dbPath);
    if (!db.open()) {
        qWarning() << "FileServiceDatabase: failed to open:" << db.lastError().text();
        return false;
    }
    // 启用 WAL 模式，避免读写互斥导致 "readonly database" 错误
    QSqlQuery walQuery(db);
    if (!walQuery.exec(QStringLiteral("PRAGMA journal_mode = WAL"))) {
        qWarning() << "FileServiceDatabase: failed to set WAL mode:" << walQuery.lastError().text();
    }
    runMigrations();
    return true;
}

void FileServiceDatabase::runMigrations() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);

    const auto exec = [&](const QString& sql) {
        if (!q.exec(sql))
            qWarning() << "Migration error:" << q.lastError().text() << "\nSQL:" << sql;
    };

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS files ("
        "    file_id TEXT PRIMARY KEY,"
        "    workspace_id TEXT NOT NULL,"
        "    file_name TEXT NOT NULL,"
        "    current_version TEXT NOT NULL DEFAULT '',"
        "    uploaded_by_id TEXT NOT NULL DEFAULT '',"
        "    uploaded_by_name TEXT NOT NULL DEFAULT '',"
        "    created_at_ms INTEGER NOT NULL DEFAULT 0,"
        "    updated_at_ms INTEGER NOT NULL DEFAULT 0"
        ")"
    ));

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS file_versions ("
        "    version_id TEXT PRIMARY KEY,"
        "    file_id TEXT NOT NULL,"
        "    version_number INTEGER NOT NULL DEFAULT 1,"
        "    version_label TEXT NOT NULL DEFAULT 'v1',"
        "    uploader_id TEXT NOT NULL DEFAULT '',"
        "    uploader_name TEXT NOT NULL DEFAULT '',"
        "    uploaded_at_ms INTEGER NOT NULL DEFAULT 0,"
        "    file_size INTEGER NOT NULL DEFAULT 0,"
        "    storage_path TEXT NOT NULL DEFAULT '',"
        "    change_note TEXT NOT NULL DEFAULT ''"
        ")"
    ));

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS service_tokens ("
        "    token TEXT PRIMARY KEY,"
        "    client_id TEXT NOT NULL DEFAULT '',"
        "    display_name TEXT NOT NULL DEFAULT '',"
        "    created_at_ms INTEGER NOT NULL DEFAULT 0"
        ")"
    ));

    exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_files_workspace ON files(workspace_id)"));
    exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_versions_file ON file_versions(file_id, version_number)"));
    exec(QStringLiteral("ALTER TABLE service_tokens ADD COLUMN allowed_workspaces TEXT NOT NULL DEFAULT '*'"));
    exec(QStringLiteral(
        "CREATE UNIQUE INDEX IF NOT EXISTS idx_files_workspace_name "
        "ON files(workspace_id, file_name)"));

    exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS chat_files ("
        "    chat_file_id   TEXT PRIMARY KEY,"
        "    workspace_id   TEXT NOT NULL,"
        "    file_name      TEXT NOT NULL,"
        "    file_hash      TEXT NOT NULL DEFAULT '',"
        "    uploader_id    TEXT NOT NULL,"
        "    uploader_name  TEXT NOT NULL DEFAULT '',"
        "    file_size      INTEGER NOT NULL DEFAULT 0,"
        "    created_at_ms  INTEGER NOT NULL,"
        "    storage_path   TEXT NOT NULL"
        ")"
    ));

    // 迁移版本控制
    {
        QSqlQuery pragmaQuery(db);
        pragmaQuery.exec(QStringLiteral("PRAGMA user_version"));
        int currentVersion = 0;
        if (pragmaQuery.next()) {
            currentVersion = pragmaQuery.value(0).toInt();
        }

        if (currentVersion < 1) {
            // v1: folders 表 + files.folder_id 列
            exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS folders ("
                "  folder_id TEXT PRIMARY KEY,"
                "  workspace_id TEXT NOT NULL,"
                "  folder_name TEXT NOT NULL,"
                "  created_by_id TEXT NOT NULL,"
                "  created_at_ms INTEGER NOT NULL,"
                "  UNIQUE(workspace_id, folder_name)"
                ")"));

            // ALTER TABLE 幂等检查
            QSqlQuery colCheck(db);
            colCheck.exec(QStringLiteral(
                "SELECT COUNT(*) FROM pragma_table_info('files') WHERE name='folder_id'"));
            if (colCheck.next() && colCheck.value(0).toInt() == 0) {
                exec(QStringLiteral(
                    "ALTER TABLE files ADD COLUMN folder_id TEXT NOT NULL DEFAULT ''"));
            }

            exec(QStringLiteral("PRAGMA user_version = 1"));
        }

        if (currentVersion < 2) {
            QSqlQuery colCheck(db);
            colCheck.exec(QStringLiteral(
                "SELECT COUNT(*) FROM pragma_table_info('service_tokens') WHERE name='role'"));
            if (colCheck.next() && colCheck.value(0).toInt() == 0) {
                exec(QStringLiteral(
                    "ALTER TABLE service_tokens ADD COLUMN role TEXT NOT NULL DEFAULT 'member'"));
            }
            exec(QStringLiteral("PRAGMA user_version = 2"));
        }

        if (currentVersion < 3) {
            exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS file_locks ("
                "  file_id TEXT PRIMARY KEY,"
                "  lock_id TEXT NOT NULL,"
                "  locked_by TEXT NOT NULL,"
                "  locked_at_ms INTEGER NOT NULL DEFAULT 0,"
                "  expires_at_ms INTEGER NOT NULL DEFAULT 0"
                ")"));

            exec(QStringLiteral(
                "CREATE TABLE IF NOT EXISTS wopi_tokens ("
                "  token TEXT PRIMARY KEY,"
                "  file_id TEXT NOT NULL,"
                "  client_id TEXT NOT NULL DEFAULT '',"
                "  display_name TEXT NOT NULL DEFAULT '',"
                "  role TEXT NOT NULL DEFAULT 'viewer',"
                "  created_at_ms INTEGER NOT NULL DEFAULT 0,"
                "  expires_at_ms INTEGER NOT NULL DEFAULT 0"
                ")"));

            exec(QStringLiteral("CREATE INDEX IF NOT EXISTS idx_wopi_tokens_file ON wopi_tokens(file_id)"));

            exec(QStringLiteral("PRAGMA user_version = 3"));
        }

        if (currentVersion < 4) {
            QSqlQuery colCheck(db);
            colCheck.exec(QStringLiteral(
                "SELECT COUNT(*) FROM pragma_table_info('service_tokens') WHERE name='scopes'"));
            if (colCheck.next() && colCheck.value(0).toInt() == 0) {
                exec(QStringLiteral(
                    "ALTER TABLE service_tokens ADD COLUMN scopes TEXT NOT NULL DEFAULT '*'"));
            }
            exec(QStringLiteral("PRAGMA user_version = 4"));
        }
    }
}

bool FileServiceDatabase::insertFile(const FileRecord& record) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO files (file_id, workspace_id, file_name, current_version, "
        "uploaded_by_id, uploaded_by_name, created_at_ms, updated_at_ms, folder_id) "
        "VALUES (:file_id, :workspace_id, :file_name, :current_version, "
        ":uploaded_by_id, :uploaded_by_name, :created_at_ms, :updated_at_ms, :folder_id)"
    ));
    auto s = [](const QString& v) { return v.isNull() ? QStringLiteral("") : v; };
    q.bindValue(QStringLiteral(":file_id"),          record.fileId);
    q.bindValue(QStringLiteral(":workspace_id"),     record.workspaceId);
    q.bindValue(QStringLiteral(":file_name"),        record.fileName);
    q.bindValue(QStringLiteral(":current_version"),  s(record.currentVersion));
    q.bindValue(QStringLiteral(":uploaded_by_id"),   s(record.uploadedById));
    q.bindValue(QStringLiteral(":uploaded_by_name"), s(record.uploadedByName));
    q.bindValue(QStringLiteral(":created_at_ms"),    record.createdAtMs);
    q.bindValue(QStringLiteral(":updated_at_ms"),    record.updatedAtMs);
    q.bindValue(QStringLiteral(":folder_id"),        s(record.folderId));
    if (!q.exec()) {
        qWarning() << "insertFile error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool FileServiceDatabase::updateFileCurrentVersion(const QString& fileId,
                                                    const QString& versionId,
                                                    qint64 updatedAtMs) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE files SET current_version = :version_id, updated_at_ms = :updated_at_ms "
        "WHERE file_id = :file_id"
    ));
    q.bindValue(QStringLiteral(":version_id"),   versionId);
    q.bindValue(QStringLiteral(":updated_at_ms"), updatedAtMs);
    q.bindValue(QStringLiteral(":file_id"),       fileId);
    if (!q.exec()) {
        qWarning() << "updateFileCurrentVersion error:" << q.lastError().text();
        return false;
    }
    return true;
}

std::optional<FileRecord> FileServiceDatabase::findFileById(const QString& fileId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT * FROM files WHERE file_id = :file_id"));
    q.bindValue(QStringLiteral(":file_id"), fileId);
    if (!q.exec() || !q.next())
        return std::nullopt;

    FileRecord r;
    r.fileId          = q.value(QStringLiteral("file_id")).toString();
    r.workspaceId     = q.value(QStringLiteral("workspace_id")).toString();
    r.fileName        = q.value(QStringLiteral("file_name")).toString();
    r.currentVersion  = q.value(QStringLiteral("current_version")).toString();
    r.uploadedById    = q.value(QStringLiteral("uploaded_by_id")).toString();
    r.uploadedByName  = q.value(QStringLiteral("uploaded_by_name")).toString();
    r.createdAtMs     = q.value(QStringLiteral("created_at_ms")).toLongLong();
    r.updatedAtMs     = q.value(QStringLiteral("updated_at_ms")).toLongLong();
    r.folderId        = q.value(QStringLiteral("folder_id")).toString();
    return r;
}

QVector<FileRecord> FileServiceDatabase::listFilesByWorkspace(const QString& workspaceId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT f.*, COALESCE(v.file_size, 0) AS latest_file_size "
        "FROM files f "
        "LEFT JOIN file_versions v ON v.version_id = f.current_version "
        "WHERE f.workspace_id = :workspace_id "
        "ORDER BY f.created_at_ms DESC"
    ));
    q.bindValue(QStringLiteral(":workspace_id"), workspaceId);

    QVector<FileRecord> results;
    if (!q.exec()) {
        qWarning() << "listFilesByWorkspace error:" << q.lastError().text();
        return results;
    }
    while (q.next()) {
        FileRecord r;
        r.fileId          = q.value(QStringLiteral("file_id")).toString();
        r.workspaceId     = q.value(QStringLiteral("workspace_id")).toString();
        r.fileName        = q.value(QStringLiteral("file_name")).toString();
        r.currentVersion  = q.value(QStringLiteral("current_version")).toString();
        r.uploadedById    = q.value(QStringLiteral("uploaded_by_id")).toString();
        r.uploadedByName  = q.value(QStringLiteral("uploaded_by_name")).toString();
        r.createdAtMs     = q.value(QStringLiteral("created_at_ms")).toLongLong();
        r.updatedAtMs     = q.value(QStringLiteral("updated_at_ms")).toLongLong();
        r.folderId        = q.value(QStringLiteral("folder_id")).toString();
        r.fileSize        = q.value(QStringLiteral("latest_file_size")).toLongLong();
        results.append(r);
    }
    return results;
}

bool FileServiceDatabase::insertVersion(const FileVersionRecord& record) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO file_versions (version_id, file_id, version_number, version_label, "
        "uploader_id, uploader_name, uploaded_at_ms, file_size, storage_path, change_note) "
        "VALUES (:version_id, :file_id, :version_number, :version_label, "
        ":uploader_id, :uploader_name, :uploaded_at_ms, :file_size, :storage_path, :change_note)"
    ));
    auto s = [](const QString& v) { return v.isNull() ? QStringLiteral("") : v; };
    q.bindValue(QStringLiteral(":version_id"),     record.versionId);
    q.bindValue(QStringLiteral(":file_id"),        record.fileId);
    q.bindValue(QStringLiteral(":version_number"), record.versionNumber);
    q.bindValue(QStringLiteral(":version_label"),  s(record.versionLabel));
    q.bindValue(QStringLiteral(":uploader_id"),    s(record.uploaderId));
    q.bindValue(QStringLiteral(":uploader_name"),  s(record.uploaderName));
    q.bindValue(QStringLiteral(":uploaded_at_ms"), record.uploadedAtMs);
    q.bindValue(QStringLiteral(":file_size"),      record.fileSize);
    q.bindValue(QStringLiteral(":storage_path"),   s(record.storagePath));
    q.bindValue(QStringLiteral(":change_note"),    s(record.changeNote));
    if (!q.exec()) {
        qWarning() << "insertVersion error:" << q.lastError().text();
        return false;
    }
    return true;
}

QVector<FileVersionRecord> FileServiceDatabase::listVersionsByFile(const QString& fileId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT * FROM file_versions WHERE file_id = :file_id ORDER BY version_number ASC"
    ));
    q.bindValue(QStringLiteral(":file_id"), fileId);

    QVector<FileVersionRecord> results;
    if (!q.exec()) {
        qWarning() << "listVersionsByFile error:" << q.lastError().text();
        return results;
    }
    while (q.next()) {
        FileVersionRecord r;
        r.versionId     = q.value(QStringLiteral("version_id")).toString();
        r.fileId        = q.value(QStringLiteral("file_id")).toString();
        r.versionNumber = q.value(QStringLiteral("version_number")).toInt();
        r.versionLabel  = q.value(QStringLiteral("version_label")).toString();
        r.uploaderId    = q.value(QStringLiteral("uploader_id")).toString();
        r.uploaderName  = q.value(QStringLiteral("uploader_name")).toString();
        r.uploadedAtMs  = q.value(QStringLiteral("uploaded_at_ms")).toLongLong();
        r.fileSize      = q.value(QStringLiteral("file_size")).toLongLong();
        r.storagePath   = q.value(QStringLiteral("storage_path")).toString();
        r.changeNote    = q.value(QStringLiteral("change_note")).toString();
        results.append(r);
    }
    return results;
}

std::optional<FileVersionRecord> FileServiceDatabase::findVersionById(const QString& versionId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT * FROM file_versions WHERE version_id = :version_id"));
    q.bindValue(QStringLiteral(":version_id"), versionId);
    if (!q.exec() || !q.next())
        return std::nullopt;

    FileVersionRecord r;
    r.versionId     = q.value(QStringLiteral("version_id")).toString();
    r.fileId        = q.value(QStringLiteral("file_id")).toString();
    r.versionNumber = q.value(QStringLiteral("version_number")).toInt();
    r.versionLabel  = q.value(QStringLiteral("version_label")).toString();
    r.uploaderId    = q.value(QStringLiteral("uploader_id")).toString();
    r.uploaderName  = q.value(QStringLiteral("uploader_name")).toString();
    r.uploadedAtMs  = q.value(QStringLiteral("uploaded_at_ms")).toLongLong();
    r.fileSize      = q.value(QStringLiteral("file_size")).toLongLong();
    r.storagePath   = q.value(QStringLiteral("storage_path")).toString();
    r.changeNote    = q.value(QStringLiteral("change_note")).toString();
    return r;
}

std::optional<FileVersionRecord> FileServiceDatabase::findCurrentVersion(const QString& fileId) const
{
    const auto fileOpt = findFileById(fileId);
    if (!fileOpt || fileOpt->currentVersion.isEmpty())
        return std::nullopt;
    return findVersionById(fileOpt->currentVersion);
}

bool FileServiceDatabase::insertToken(const ServiceToken& token) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO service_tokens (token, client_id, display_name, created_at_ms, allowed_workspaces, role, scopes) "
        "VALUES (:token, :client_id, :display_name, :created_at_ms, :allowed_workspaces, :role, :scopes)"
    ));
    q.bindValue(QStringLiteral(":token"),               token.token);
    q.bindValue(QStringLiteral(":client_id"),           token.clientId);
    q.bindValue(QStringLiteral(":display_name"),        token.displayName);
    q.bindValue(QStringLiteral(":created_at_ms"),       token.createdAtMs);
    q.bindValue(QStringLiteral(":allowed_workspaces"),  token.allowedWorkspaces);
    q.bindValue(QStringLiteral(":role"),                token.role.isEmpty() ? QStringLiteral("member") : token.role);
    q.bindValue(QStringLiteral(":scopes"),              token.scopes.trimmed().isEmpty() ? QStringLiteral("*") : token.scopes);
    if (!q.exec()) {
        qWarning() << "insertToken error:" << q.lastError().text();
        return false;
    }
    return true;
}

std::optional<ServiceToken> FileServiceDatabase::findToken(const QString& token) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT * FROM service_tokens WHERE token = :token"));
    q.bindValue(QStringLiteral(":token"), token);
    if (!q.exec() || !q.next())
        return std::nullopt;

    ServiceToken t;
    t.token             = q.value(QStringLiteral("token")).toString();
    t.clientId          = q.value(QStringLiteral("client_id")).toString();
    t.displayName       = q.value(QStringLiteral("display_name")).toString();
    t.createdAtMs       = q.value(QStringLiteral("created_at_ms")).toLongLong();
    t.allowedWorkspaces = q.value(QStringLiteral("allowed_workspaces")).toString();
    t.role              = q.value(QStringLiteral("role")).toString();
    t.scopes            = q.value(QStringLiteral("scopes")).toString();
    return t;
}

std::optional<FileRecord> FileServiceDatabase::findFileByName(
    const QString& workspaceId, const QString& fileName) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT file_id, workspace_id, file_name, current_version, "
        "uploaded_by_id, uploaded_by_name, created_at_ms, updated_at_ms, folder_id "
        "FROM files WHERE workspace_id = ? AND file_name = ?"));
    q.addBindValue(workspaceId);
    q.addBindValue(fileName);
    if (!q.exec() || !q.next()) return std::nullopt;
    FileRecord r;
    r.fileId          = q.value(0).toString();
    r.workspaceId     = q.value(1).toString();
    r.fileName        = q.value(2).toString();
    r.currentVersion  = q.value(3).toString();
    r.uploadedById    = q.value(4).toString();
    r.uploadedByName  = q.value(5).toString();
    r.createdAtMs     = q.value(6).toLongLong();
    r.updatedAtMs     = q.value(7).toLongLong();
    r.folderId        = q.value(8).toString();
    return r;
}

int FileServiceDatabase::nextVersionNumber(const QString& fileId) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName));
    q.prepare(QStringLiteral(
        "SELECT COALESCE(MAX(version_number), 0) + 1 FROM file_versions WHERE file_id = ?"));
    q.addBindValue(fileId);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return 1;
}

bool FileServiceDatabase::beginTransaction() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.transaction()) {
        qWarning() << "FileServiceDatabase: beginTransaction failed:" << db.lastError().text();
        return false;
    }
    return true;
}

bool FileServiceDatabase::commit() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.commit()) {
        qWarning() << "FileServiceDatabase: commit failed:" << db.lastError().text();
        return false;
    }
    return true;
}

bool FileServiceDatabase::rollback() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.rollback()) {
        qWarning() << "FileServiceDatabase: rollback failed:" << db.lastError().text();
        return false;
    }
    return true;
}

bool FileServiceDatabase::updateTokenScope(const QString& token,
                                            const QString& allowedWorkspaces) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE service_tokens SET allowed_workspaces = :scope WHERE token = :token"));
    q.bindValue(QStringLiteral(":scope"), allowedWorkspaces);
    q.bindValue(QStringLiteral(":token"), token);
    if (!q.exec()) {
        qWarning() << "FileServiceDatabase: updateTokenScope failed:" << q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() < 1) {
        qWarning() << "FileServiceDatabase: updateTokenScope found no matching token";
        return false;
    }
    return true;
}

bool FileServiceDatabase::updateTokenRole(const QString& token, const QString& role) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE service_tokens SET role = :role WHERE token = :token"));
    q.bindValue(QStringLiteral(":role"), role);
    q.bindValue(QStringLiteral(":token"), token);
    if (!q.exec()) {
        qWarning() << "FileServiceDatabase: updateTokenRole failed:" << q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() < 1) {
        qWarning() << "FileServiceDatabase: updateTokenRole found no matching token";
        return false;
    }
    return true;
}

bool FileServiceDatabase::updateTokenScopes(const QString& token, const QString& scopes) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE service_tokens SET scopes = :scopes WHERE token = :token"));
    q.bindValue(QStringLiteral(":scopes"), scopes);
    q.bindValue(QStringLiteral(":token"), token);
    if (!q.exec()) {
        qWarning() << "FileServiceDatabase: updateTokenScopes failed:" << q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() < 1) {
        qWarning() << "FileServiceDatabase: updateTokenScopes found no matching token";
        return false;
    }
    return true;
}

bool FileServiceDatabase::insertChatFile(const ChatFileRecord& record) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO chat_files (chat_file_id, workspace_id, file_name, file_hash, "
        "uploader_id, uploader_name, file_size, created_at_ms, storage_path) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(record.chatFileId);
    q.addBindValue(record.workspaceId);
    q.addBindValue(record.fileName);
    // Qt 的 addBindValue 对 null QString 绑定为 SQL NULL，
    // 而 file_hash 定义为 NOT NULL —— 用空字符串替代 null 以满足约束
    q.addBindValue(record.fileHash.isNull() ? QStringLiteral("") : record.fileHash);
    q.addBindValue(record.uploaderId);
    q.addBindValue(record.uploaderName.isNull() ? QStringLiteral("") : record.uploaderName);
    q.addBindValue(record.fileSize);
    q.addBindValue(record.createdAtMs);
    q.addBindValue(record.storagePath);
    if (!q.exec()) {
        qWarning() << "insertChatFile error:" << q.lastError().text();
        return false;
    }
    return true;
}

std::optional<ChatFileRecord> FileServiceDatabase::findChatFileById(const QString& chatFileId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT chat_file_id, workspace_id, file_name, file_hash, "
        "uploader_id, uploader_name, file_size, created_at_ms, storage_path "
        "FROM chat_files WHERE chat_file_id = ?"));
    q.addBindValue(chatFileId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    ChatFileRecord r;
    r.chatFileId   = q.value(0).toString();
    r.workspaceId  = q.value(1).toString();
    r.fileName     = q.value(2).toString();
    r.fileHash     = q.value(3).toString();
    r.uploaderId   = q.value(4).toString();
    r.uploaderName = q.value(5).toString();
    r.fileSize     = q.value(6).toLongLong();
    r.createdAtMs  = q.value(7).toLongLong();
    r.storagePath  = q.value(8).toString();
    return r;
}

std::vector<ChatFileRecord> FileServiceDatabase::getExpiredChatFiles(qint64 cutoffMs) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT chat_file_id, workspace_id, file_name, file_hash, "
        "uploader_id, uploader_name, file_size, created_at_ms, storage_path "
        "FROM chat_files WHERE created_at_ms <= ? ORDER BY created_at_ms ASC"));
    q.addBindValue(cutoffMs);
    std::vector<ChatFileRecord> results;
    if (!q.exec()) {
        qWarning() << "getExpiredChatFiles error:" << q.lastError().text();
        return results;
    }
    while (q.next()) {
        ChatFileRecord r;
        r.chatFileId   = q.value(0).toString();
        r.workspaceId  = q.value(1).toString();
        r.fileName     = q.value(2).toString();
        r.fileHash     = q.value(3).toString();
        r.uploaderId   = q.value(4).toString();
        r.uploaderName = q.value(5).toString();
        r.fileSize     = q.value(6).toLongLong();
        r.createdAtMs  = q.value(7).toLongLong();
        r.storagePath  = q.value(8).toString();
        results.push_back(std::move(r));
    }
    return results;
}

qint64 FileServiceDatabase::getChatFilesTotalSize() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("SELECT COALESCE(SUM(file_size), 0) FROM chat_files"));
    if (!q.exec() || !q.next()) {
        qWarning() << "getChatFilesTotalSize error:" << q.lastError().text();
        return 0;
    }
    return q.value(0).toLongLong();
}

std::vector<ChatFileRecord> FileServiceDatabase::getOldestChatFiles(int limit) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT chat_file_id, workspace_id, file_name, file_hash, "
        "uploader_id, uploader_name, file_size, created_at_ms, storage_path "
        "FROM chat_files ORDER BY created_at_ms ASC LIMIT ?"));
    q.addBindValue(limit);
    std::vector<ChatFileRecord> results;
    if (!q.exec()) {
        qWarning() << "getOldestChatFiles error:" << q.lastError().text();
        return results;
    }
    while (q.next()) {
        ChatFileRecord r;
        r.chatFileId   = q.value(0).toString();
        r.workspaceId  = q.value(1).toString();
        r.fileName     = q.value(2).toString();
        r.fileHash     = q.value(3).toString();
        r.uploaderId   = q.value(4).toString();
        r.uploaderName = q.value(5).toString();
        r.fileSize     = q.value(6).toLongLong();
        r.createdAtMs  = q.value(7).toLongLong();
        r.storagePath  = q.value(8).toString();
        results.push_back(std::move(r));
    }
    return results;
}

bool FileServiceDatabase::deleteChatFileById(const QString& chatFileId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM chat_files WHERE chat_file_id = ?"));
    q.addBindValue(chatFileId);
    if (!q.exec()) {
        qWarning() << "deleteChatFileById error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool FileServiceDatabase::insertFolder(const QString& folderId, const QString& workspaceId,
                                        const QString& folderName, const QString& createdById) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
    q.prepare(QStringLiteral(
        "INSERT INTO folders (folder_id, workspace_id, folder_name, created_by_id, created_at_ms)"
        " VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(folderId);
    q.addBindValue(workspaceId);
    q.addBindValue(folderName);
    q.addBindValue(createdById);
    q.addBindValue(QDateTime::currentMSecsSinceEpoch());
    if (!q.exec()) {
        qWarning() << "insertFolder error:" << q.lastError().text();
        return false;
    }
    return true;
}

std::vector<FolderRecord> FileServiceDatabase::listFolders(const QString& workspaceId) const
{
    std::vector<FolderRecord> result;
    QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
    q.prepare(QStringLiteral("SELECT folder_id, workspace_id, folder_name, created_by_id, created_at_ms"
                              " FROM folders WHERE workspace_id = ? ORDER BY folder_name"));
    q.addBindValue(workspaceId);
    if (!q.exec()) {
        qWarning() << "listFolders error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        FolderRecord r;
        r.folderId    = q.value(0).toString();
        r.workspaceId = q.value(1).toString();
        r.folderName  = q.value(2).toString();
        r.createdById = q.value(3).toString();
        r.createdAtMs = q.value(4).toLongLong();
        result.push_back(std::move(r));
    }
    return result;
}

bool FileServiceDatabase::deleteFolder(const QString& folderId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) return false;

    QSqlQuery clearQ(db);
    clearQ.prepare(QStringLiteral("UPDATE files SET folder_id = '' WHERE folder_id = ?"));
    clearQ.addBindValue(folderId);
    if (!clearQ.exec()) {
        qWarning() << "deleteFolder clearFiles error:" << clearQ.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery delQ(db);
    delQ.prepare(QStringLiteral("DELETE FROM folders WHERE folder_id = ?"));
    delQ.addBindValue(folderId);
    if (!delQ.exec()) {
        qWarning() << "deleteFolder delete error:" << delQ.lastError().text();
        db.rollback();
        return false;
    }

    return db.commit();
}

std::vector<QString> FileServiceDatabase::versionStoragePaths(const QString& fileId) const
{
    std::vector<QString> result;
    QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
    q.prepare(QStringLiteral("SELECT storage_path FROM file_versions WHERE file_id = ?"));
    q.addBindValue(fileId);
    if (!q.exec()) {
        qWarning() << "versionStoragePaths error:" << q.lastError().text();
        return result;
    }
    while (q.next()) {
        result.push_back(q.value(0).toString());
    }
    return result;
}

bool FileServiceDatabase::deleteFileAndVersions(const QString& fileId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) return false;

    QSqlQuery delVersions(db);
    delVersions.prepare(QStringLiteral("DELETE FROM file_versions WHERE file_id = ?"));
    delVersions.addBindValue(fileId);
    if (!delVersions.exec()) {
        qWarning() << "deleteFileAndVersions delVersions error:" << delVersions.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery delFile(db);
    delFile.prepare(QStringLiteral("DELETE FROM files WHERE file_id = ?"));
    delFile.addBindValue(fileId);
    if (!delFile.exec()) {
        qWarning() << "deleteFileAndVersions delFile error:" << delFile.lastError().text();
        db.rollback();
        return false;
    }

    return db.commit();
}

bool FileServiceDatabase::updateFileFolderId(const QString& fileId, const QString& folderId) const
{
    QSqlQuery q(QSqlDatabase::database(m_connectionName, false));
    q.prepare(QStringLiteral("UPDATE files SET folder_id = ? WHERE file_id = ?"));
    q.addBindValue(folderId);
    q.addBindValue(fileId);
    if (!q.exec()) {
        qWarning() << "updateFileFolderId error:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

// ── File locks (WOPI) ───────────────────────────────────────────────

bool FileServiceDatabase::insertFileLock(const FileLockRecord& record) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO file_locks (file_id, lock_id, locked_by, locked_at_ms, expires_at_ms) "
        "VALUES (?, ?, ?, ?, ?)"));
    q.addBindValue(record.fileId);
    q.addBindValue(record.lockId);
    q.addBindValue(record.lockedBy);
    q.addBindValue(record.lockedAtMs);
    q.addBindValue(record.expiresAtMs);
    if (!q.exec()) {
        qWarning() << "insertFileLock error:" << q.lastError().text();
        return false;
    }
    return true;
}

std::optional<FileLockRecord> FileServiceDatabase::findFileLock(const QString& fileId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT file_id, lock_id, locked_by, locked_at_ms, expires_at_ms "
        "FROM file_locks WHERE file_id = ?"));
    q.addBindValue(fileId);
    if (!q.exec() || !q.next())
        return std::nullopt;
    FileLockRecord r;
    r.fileId      = q.value(0).toString();
    r.lockId      = q.value(1).toString();
    r.lockedBy    = q.value(2).toString();
    r.lockedAtMs  = q.value(3).toLongLong();
    r.expiresAtMs = q.value(4).toLongLong();
    return r;
}

bool FileServiceDatabase::deleteFileLock(const QString& fileId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM file_locks WHERE file_id = ?"));
    q.addBindValue(fileId);
    if (!q.exec()) {
        qWarning() << "deleteFileLock error:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

// ── WOPI tokens ─────────────────────────────────────────────────────

bool FileServiceDatabase::insertWopiToken(const WopiTokenRecord& record) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO wopi_tokens (token, file_id, client_id, display_name, role, "
        "created_at_ms, expires_at_ms) VALUES (?, ?, ?, ?, ?, ?, ?)"));
    q.addBindValue(record.token);
    q.addBindValue(record.fileId);
    q.addBindValue(record.clientId);
    q.addBindValue(record.displayName);
    q.addBindValue(record.role.isEmpty() ? QStringLiteral("viewer") : record.role);
    q.addBindValue(record.createdAtMs);
    q.addBindValue(record.expiresAtMs);
    if (!q.exec()) {
        qWarning() << "insertWopiToken error:" << q.lastError().text();
        return false;
    }
    return true;
}

std::optional<WopiTokenRecord> FileServiceDatabase::validateWopiToken(const QString& token, qint64 nowMs) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT token, file_id, client_id, display_name, role, created_at_ms, expires_at_ms "
        "FROM wopi_tokens WHERE token = ? AND expires_at_ms > ?"));
    q.addBindValue(token);
    q.addBindValue(nowMs);
    if (!q.exec() || !q.next())
        return std::nullopt;
    WopiTokenRecord r;
    r.token       = q.value(0).toString();
    r.fileId      = q.value(1).toString();
    r.clientId    = q.value(2).toString();
    r.displayName = q.value(3).toString();
    r.role        = q.value(4).toString();
    r.createdAtMs = q.value(5).toLongLong();
    r.expiresAtMs = q.value(6).toLongLong();
    return r;
}

bool FileServiceDatabase::renewWopiToken(const QString& token, qint64 newExpiresAtMs) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "UPDATE wopi_tokens SET expires_at_ms = ? WHERE token = ?"));
    q.addBindValue(newExpiresAtMs);
    q.addBindValue(token);
    if (!q.exec()) {
        qWarning() << "renewWopiToken error:" << q.lastError().text();
        return false;
    }
    return q.numRowsAffected() > 0;
}

int FileServiceDatabase::deleteExpiredFileLocks(qint64 nowMs) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM file_locks WHERE expires_at_ms <= ?"));
    q.addBindValue(nowMs);
    if (!q.exec()) {
        qWarning() << "deleteExpiredFileLocks error:" << q.lastError().text();
        return 0;
    }
    return q.numRowsAffected();
}

int FileServiceDatabase::deleteExpiredWopiTokens(qint64 nowMs) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery q(db);
    q.prepare(QStringLiteral("DELETE FROM wopi_tokens WHERE expires_at_ms <= ?"));
    q.addBindValue(nowMs);
    if (!q.exec()) {
        qWarning() << "deleteExpiredWopiTokens error:" << q.lastError().text();
        return 0;
    }
    return q.numRowsAffected();
}
