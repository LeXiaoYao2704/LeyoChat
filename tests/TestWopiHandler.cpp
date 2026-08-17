#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QUuid>
#include <QDateTime>

#include "fileservice/WopiHandler.h"
#include "fileservice/FileServiceDatabase.h"
#include "fileservice/FileStorageManager.h"

namespace {

QString uniqueConn()
{
    return QStringLiteral("test-wopi-handler-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

struct HandlerEnv {
    QTemporaryDir dir;
    QString conn;
    FileServiceDatabase* db = nullptr;
    FileStorageManager* storage = nullptr;
    WopiHandler* handler = nullptr;

    HandlerEnv() : conn(uniqueConn()) {}

    bool setup()
    {
        if (!dir.isValid()) return false;
        db = new FileServiceDatabase(dir.filePath(QStringLiteral("service.db")), conn);
        if (!db->open()) return false;
        storage = new FileStorageManager(dir.filePath(QStringLiteral("storage")));
        handler = new WopiHandler(db, storage);
        return true;
    }

    // 插入文件 + 版本 + WOPI token，返回 access_token
    QString seedFileAndToken(const QString& fileId,
                             const QString& workspaceId,
                             const QString& fileName,
                             const QByteArray& payload,
                             const QString& clientId = QStringLiteral("client-alice"),
                             const QString& displayName = QStringLiteral("Alice"))
    {
        const QString versionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto storagePath = storage->saveFile(fileId, versionId, payload);
        if (!storagePath.has_value()) return {};

        FileRecord file;
        file.fileId = fileId;
        file.workspaceId = workspaceId;
        file.fileName = fileName;
        file.currentVersion = versionId;
        file.uploadedById = QStringLiteral("seed-user");
        file.uploadedByName = QStringLiteral("Seed User");
        file.createdAtMs = 1;
        file.updatedAtMs = 1;
        file.fileSize = payload.size();
        if (!db->insertFile(file)) return {};

        FileVersionRecord ver;
        ver.versionId = versionId;
        ver.fileId = fileId;
        ver.versionNumber = 1;
        ver.versionLabel = QStringLiteral("v1");
        ver.uploaderId = QStringLiteral("seed-user");
        ver.uploaderName = QStringLiteral("Seed User");
        ver.uploadedAtMs = 1;
        ver.fileSize = payload.size();
        ver.storagePath = *storagePath;
        ver.changeNote = QStringLiteral("seed");
        if (!db->insertVersion(ver)) return {};

        const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        WopiTokenRecord tok;
        tok.token = token;
        tok.fileId = fileId;
        tok.clientId = clientId;
        tok.displayName = displayName;
        tok.role = QStringLiteral("editor");
        tok.createdAtMs = now;
        tok.expiresAtMs = now + 3600 * 1000;
        if (!db->insertWopiToken(tok)) return {};

        return token;
    }

    ~HandlerEnv()
    {
        delete handler;
        delete storage;
        delete db;
    }
};

} // namespace

class TestWopiHandler : public QObject
{
    Q_OBJECT
private slots:
    // CheckFileInfo
    void test_checkFileInfo_returnsCorrectMetadata();
    void test_checkFileInfo_invalidToken();
    void test_checkFileInfo_fileNotFound();

    // GetFile
    void test_getFile_returnsBinaryContent();
    void test_getFile_invalidToken();

    // Lock
    void test_lock_success();
    void test_lock_conflict_returns409();
    void test_lock_sameLockId_refreshes();
    void test_refreshLock_success();
    void test_unlock_success();

    // PutFile
    void test_putFile_savesNewVersion();
    void test_putFile_lockMismatch();
};

// --- CheckFileInfo ---

void TestWopiHandler::test_checkFileInfo_returnsCorrectMetadata()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QByteArray payload = QByteArrayLiteral("spreadsheet data");
    const QString token = env.seedFileAndToken(
        QStringLiteral("f-001"), QStringLiteral("ws-1"),
        QStringLiteral("budget.xlsx"), payload);

    auto result = env.handler->checkFileInfo(QStringLiteral("f-001"), token);
    QCOMPARE(result.statusCode, 200);
    QCOMPARE(result.json[QStringLiteral("BaseFileName")].toString(), QStringLiteral("budget.xlsx"));
    QCOMPARE(result.json[QStringLiteral("Size")].toInteger(), static_cast<qint64>(payload.size()));
    QCOMPARE(result.json[QStringLiteral("UserId")].toString(), QStringLiteral("client-alice"));
    QCOMPARE(result.json[QStringLiteral("UserFriendlyName")].toString(), QStringLiteral("Alice"));
    QCOMPARE(result.json[QStringLiteral("UserCanWrite")].toBool(), true);
    QCOMPARE(result.json[QStringLiteral("SupportsLocks")].toBool(), true);
    QCOMPARE(result.json[QStringLiteral("SupportsUpdate")].toBool(), true);
}

void TestWopiHandler::test_checkFileInfo_invalidToken()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    auto result = env.handler->checkFileInfo(QStringLiteral("f-001"), QStringLiteral("bad-token"));
    QCOMPARE(result.statusCode, 401);
}

void TestWopiHandler::test_checkFileInfo_fileNotFound()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    // 创建 token 后删除文件（token 绑定的 fileId 在 DB 中不存在）
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    WopiTokenRecord tok;
    tok.token = QStringLiteral("orphan-token");
    tok.fileId = QStringLiteral("nonexistent-file");
    tok.clientId = QStringLiteral("client-alice");
    tok.displayName = QStringLiteral("Alice");
    tok.role = QStringLiteral("editor");
    tok.createdAtMs = now;
    tok.expiresAtMs = now + 3600 * 1000;
    QVERIFY(env.db->insertWopiToken(tok));

    auto result = env.handler->checkFileInfo(QStringLiteral("nonexistent-file"),
                                              QStringLiteral("orphan-token"));
    QCOMPARE(result.statusCode, 404);
}

// --- GetFile ---

void TestWopiHandler::test_getFile_returnsBinaryContent()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QByteArray payload = QByteArrayLiteral("\x50\x4B\x03\x04 fake xlsx binary");
    const QString token = env.seedFileAndToken(
        QStringLiteral("f-002"), QStringLiteral("ws-1"),
        QStringLiteral("report.xlsx"), payload);

    auto result = env.handler->getFile(QStringLiteral("f-002"), token);
    QCOMPARE(result.statusCode, 200);
    QCOMPARE(result.content, payload);
    QCOMPARE(result.contentType, QStringLiteral("application/octet-stream"));
}

void TestWopiHandler::test_getFile_invalidToken()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    auto result = env.handler->getFile(QStringLiteral("f-002"), QStringLiteral("bad-token"));
    QCOMPARE(result.statusCode, 401);
}

// --- Lock ---

void TestWopiHandler::test_lock_success()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-003"), QStringLiteral("ws-1"),
        QStringLiteral("doc.docx"), QByteArrayLiteral("content"));

    auto result = env.handler->handleLock(QStringLiteral("f-003"), token,
                                           QStringLiteral("LOCK"), QStringLiteral("lock-aaa"));
    QCOMPARE(result.statusCode, 200);

    // DB 中应存在锁记录
    auto lock = env.db->findFileLock(QStringLiteral("f-003"));
    QVERIFY(lock.has_value());
    QCOMPARE(lock->lockId, QStringLiteral("lock-aaa"));
}

void TestWopiHandler::test_lock_conflict_returns409()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-004"), QStringLiteral("ws-1"),
        QStringLiteral("conflict.docx"), QByteArrayLiteral("content"));

    // 先锁定
    env.handler->handleLock(QStringLiteral("f-004"), token,
                             QStringLiteral("LOCK"), QStringLiteral("lock-aaa"));

    // 用不同 lockId 再次锁定
    auto result = env.handler->handleLock(QStringLiteral("f-004"), token,
                                           QStringLiteral("LOCK"), QStringLiteral("lock-bbb"));
    QCOMPARE(result.statusCode, 409);
    QCOMPARE(result.existingLockId, QStringLiteral("lock-aaa"));
}

void TestWopiHandler::test_lock_sameLockId_refreshes()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-005"), QStringLiteral("ws-1"),
        QStringLiteral("refresh.docx"), QByteArrayLiteral("content"));

    env.handler->handleLock(QStringLiteral("f-005"), token,
                             QStringLiteral("LOCK"), QStringLiteral("lock-aaa"));

    auto lockBefore = env.db->findFileLock(QStringLiteral("f-005"));
    QVERIFY(lockBefore.has_value());

    // 再次用相同 lockId → 应 200 且刷新过期时间
    auto result = env.handler->handleLock(QStringLiteral("f-005"), token,
                                           QStringLiteral("LOCK"), QStringLiteral("lock-aaa"));
    QCOMPARE(result.statusCode, 200);

    auto lockAfter = env.db->findFileLock(QStringLiteral("f-005"));
    QVERIFY(lockAfter.has_value());
    QVERIFY(lockAfter->expiresAtMs >= lockBefore->expiresAtMs);
}

void TestWopiHandler::test_refreshLock_success()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-006"), QStringLiteral("ws-1"),
        QStringLiteral("refresh2.docx"), QByteArrayLiteral("content"));

    env.handler->handleLock(QStringLiteral("f-006"), token,
                             QStringLiteral("LOCK"), QStringLiteral("lock-aaa"));

    auto result = env.handler->handleLock(QStringLiteral("f-006"), token,
                                           QStringLiteral("REFRESH_LOCK"), QStringLiteral("lock-aaa"));
    QCOMPARE(result.statusCode, 200);
}

void TestWopiHandler::test_unlock_success()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-007"), QStringLiteral("ws-1"),
        QStringLiteral("unlock.docx"), QByteArrayLiteral("content"));

    env.handler->handleLock(QStringLiteral("f-007"), token,
                             QStringLiteral("LOCK"), QStringLiteral("lock-aaa"));

    auto result = env.handler->handleLock(QStringLiteral("f-007"), token,
                                           QStringLiteral("UNLOCK"), QStringLiteral("lock-aaa"));
    QCOMPARE(result.statusCode, 200);

    // 锁应已删除
    auto lock = env.db->findFileLock(QStringLiteral("f-007"));
    QVERIFY(!lock.has_value());
}

// --- PutFile ---

void TestWopiHandler::test_putFile_savesNewVersion()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-008"), QStringLiteral("ws-1"),
        QStringLiteral("edit.xlsx"), QByteArrayLiteral("original"));

    // 先加锁
    env.handler->handleLock(QStringLiteral("f-008"), token,
                             QStringLiteral("LOCK"), QStringLiteral("lock-put"));

    const QByteArray newContent = QByteArrayLiteral("updated content via WOPI");
    int status = env.handler->putFile(QStringLiteral("f-008"), token,
                                       QStringLiteral("lock-put"), newContent);
    QCOMPARE(status, 200);

    // 验证新版本已写入
    auto versions = env.db->listVersionsByFile(QStringLiteral("f-008"));
    QCOMPARE(versions.size(), 2); // seed v1 + WOPI v2
    QCOMPARE(versions.last().changeNote, QStringLiteral("WOPI PutFile"));

    // 验证文件内容
    auto saved = env.storage->readFile(versions.last().storagePath);
    QVERIFY(saved.has_value());
    QCOMPARE(*saved, newContent);
}

void TestWopiHandler::test_putFile_lockMismatch()
{
    HandlerEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-009"), QStringLiteral("ws-1"),
        QStringLiteral("locked.xlsx"), QByteArrayLiteral("original"));

    env.handler->handleLock(QStringLiteral("f-009"), token,
                             QStringLiteral("LOCK"), QStringLiteral("lock-real"));

    int status = env.handler->putFile(QStringLiteral("f-009"), token,
                                       QStringLiteral("lock-wrong"),
                                       QByteArrayLiteral("should not save"));
    QCOMPARE(status, 409);
}

QTEST_MAIN(TestWopiHandler)
#include "TestWopiHandler.moc"
