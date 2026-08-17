#pragma once
#include <QString>
#include <QVector>
#include <optional>
#include <vector>

struct FileRecord {
    QString fileId;
    QString workspaceId;
    QString fileName;
    QString currentVersion = QStringLiteral("");
    QString uploadedById;
    QString uploadedByName;
    qint64  createdAtMs = 0;
    qint64  updatedAtMs = 0;
    QString folderId = QStringLiteral("");  // 空字符串 = 根目录
    qint64  fileSize = 0;
};

struct FileVersionRecord {
    QString versionId;
    QString fileId;
    int     versionNumber = 0;
    QString versionLabel = QStringLiteral("");
    QString uploaderId;
    QString uploaderName = QStringLiteral("");
    qint64  uploadedAtMs = 0;
    qint64  fileSize     = 0;
    QString storagePath = QStringLiteral("");
    QString changeNote = QStringLiteral("");
};

struct ServiceToken {
    QString token;
    QString clientId;
    QString displayName;
    qint64  createdAtMs = 0;
    QString allowedWorkspaces = QStringLiteral("*"); // "*" = all, or JSON array e.g. ["ws-1","ws-2"]
    QString role;
    QString scopes = QStringLiteral("*"); // "*" = all, or JSON array e.g. ["message:read"]
};

struct ChatFileRecord {
    QString chatFileId;
    QString workspaceId;
    QString fileName;
    QString fileHash;
    QString uploaderId;
    QString uploaderName;
    qint64 fileSize = 0;
    qint64 createdAtMs = 0;
    QString storagePath;  // 相对路径
};

struct FolderRecord {
    QString folderId;
    QString workspaceId;
    QString folderName;
    QString createdById;
    qint64  createdAtMs = 0;
};

struct FileLockRecord {
    QString fileId;
    QString lockId;
    QString lockedBy;
    qint64  lockedAtMs  = 0;
    qint64  expiresAtMs = 0;
};

struct WopiTokenRecord {
    QString token;
    QString fileId;
    QString clientId;
    QString displayName;
    QString role;
    qint64  createdAtMs  = 0;
    qint64  expiresAtMs  = 0;
};

class FileServiceDatabase {
public:
    explicit FileServiceDatabase(const QString& dbPath,
                                 const QString& connectionName = QStringLiteral("leyo-file-service"));
    ~FileServiceDatabase();

    bool open();
    QString dbPath() const { return m_dbPath; }

    // Files
    bool insertFile(const FileRecord& record) const;
    bool updateFileCurrentVersion(const QString& fileId,
                                  const QString& versionId,
                                  qint64 updatedAtMs) const;
    std::optional<FileRecord> findFileById(const QString& fileId) const;
    std::optional<FileRecord> findFileByName(const QString& workspaceId,
                                             const QString& fileName) const;
    QVector<FileRecord> listFilesByWorkspace(const QString& workspaceId) const;

    // Versions
    bool insertVersion(const FileVersionRecord& record) const;
    QVector<FileVersionRecord> listVersionsByFile(const QString& fileId) const;
    std::optional<FileVersionRecord> findVersionById(const QString& versionId) const;
    std::optional<FileVersionRecord> findCurrentVersion(const QString& fileId) const;
    int nextVersionNumber(const QString& fileId) const;

    // 文件夹
    bool insertFolder(const QString& folderId, const QString& workspaceId,
                      const QString& folderName, const QString& createdById) const;
    std::vector<FolderRecord> listFolders(const QString& workspaceId) const;
    bool deleteFolder(const QString& folderId) const;

    // 文件删除
    std::vector<QString> versionStoragePaths(const QString& fileId) const;
    bool deleteFileAndVersions(const QString& fileId) const;

    // 文件夹归属
    bool updateFileFolderId(const QString& fileId, const QString& folderId) const;

    // Transaction control
    bool beginTransaction() const;
    bool commit() const;
    bool rollback() const;

    // Auth tokens
    bool insertToken(const ServiceToken& token) const;
    std::optional<ServiceToken> findToken(const QString& token) const;
    bool updateTokenScope(const QString& token, const QString& allowedWorkspaces) const;
    bool updateTokenRole(const QString& token, const QString& role) const;
    bool updateTokenScopes(const QString& token, const QString& scopes) const;

    // Chat files (群文件中转)
    bool insertChatFile(const ChatFileRecord& record) const;
    std::optional<ChatFileRecord> findChatFileById(const QString& chatFileId) const;

    // Chat files cleanup (聊天文件清理)
    std::vector<ChatFileRecord> getExpiredChatFiles(qint64 cutoffMs) const;
    qint64 getChatFilesTotalSize() const;
    std::vector<ChatFileRecord> getOldestChatFiles(int limit) const;
    bool deleteChatFileById(const QString& chatFileId) const;

    // File locks (WOPI 文件锁)
    bool insertFileLock(const FileLockRecord& record) const;
    std::optional<FileLockRecord> findFileLock(const QString& fileId) const;
    bool deleteFileLock(const QString& fileId) const;

    // WOPI tokens (短时效访问令牌)
    bool insertWopiToken(const WopiTokenRecord& record) const;
    std::optional<WopiTokenRecord> validateWopiToken(const QString& token, qint64 nowMs) const;
    bool renewWopiToken(const QString& token, qint64 newExpiresAtMs) const;

    // 过期清理
    int deleteExpiredFileLocks(qint64 nowMs) const;
    int deleteExpiredWopiTokens(qint64 nowMs) const;

private:
    void runMigrations() const;
    QString m_connectionName;
    QString m_dbPath;
};
