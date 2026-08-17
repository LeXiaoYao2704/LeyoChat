#include <QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QUuid>
#include <QDateTime>

#include "fileservice/OnlyOfficeCallbackHandler.h"
#include "fileservice/FileServiceDatabase.h"
#include "fileservice/FileStorageManager.h"

namespace {

QString uniqueConn()
{
    return QStringLiteral("test-callback-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// 可控的 download mock
class TestableCallbackHandler : public OnlyOfficeCallbackHandler
{
public:
    using OnlyOfficeCallbackHandler::OnlyOfficeCallbackHandler;

    QByteArray nextDownloadContent;

protected:
    QByteArray downloadFromUrl(const QString& /*url*/) override
    {
        return nextDownloadContent;
    }
};

struct CallbackEnv {
    QTemporaryDir dir;
    QString conn;
    FileServiceDatabase* db = nullptr;
    FileStorageManager* storage = nullptr;
    TestableCallbackHandler* handler = nullptr;

    CallbackEnv() : conn(uniqueConn()) {}

    bool setup()
    {
        if (!dir.isValid()) return false;
        db = new FileServiceDatabase(dir.filePath(QStringLiteral("service.db")), conn);
        if (!db->open()) return false;
        storage = new FileStorageManager(dir.filePath(QStringLiteral("storage")));
        handler = new TestableCallbackHandler(db, storage);
        return true;
    }

    // 插入文件 + 版本 + WOPI token，返回 access_token
    QString seedFileAndToken(const QString& fileId,
                             const QString& fileName,
                             const QByteArray& payload)
    {
        const QString versionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        auto sp = storage->saveFile(fileId, versionId, payload);
        if (!sp.has_value()) return {};

        FileRecord file;
        file.fileId = fileId;
        file.workspaceId = QStringLiteral("ws-1");
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
        ver.storagePath = *sp;
        ver.changeNote = QStringLiteral("seed");
        if (!db->insertVersion(ver)) return {};

        const QString token = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        WopiTokenRecord tok;
        tok.token = token;
        tok.fileId = fileId;
        tok.clientId = QStringLiteral("client-alice");
        tok.displayName = QStringLiteral("Alice");
        tok.role = QStringLiteral("editor");
        tok.createdAtMs = now;
        tok.expiresAtMs = now + 3600 * 1000;
        if (!db->insertWopiToken(tok)) return {};

        return token;
    }

    ~CallbackEnv()
    {
        delete handler;
        delete storage;
        delete db;
    }
};

} // namespace

class TestOnlyOfficeCallbackHandler : public QObject
{
    Q_OBJECT
private slots:
    void test_status1_noAction();
    void test_status2_savesNewVersion();
    void test_status4_releasesLock();
    void test_status6_forceSave();
    void test_duplicateCallback_skipped();
    void test_invalidToken_rejected();
};

void TestOnlyOfficeCallbackHandler::test_status1_noAction()
{
    CallbackEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-001"), QStringLiteral("doc.docx"),
        QByteArrayLiteral("content"));
    QVERIFY(!token.isEmpty());

    QJsonObject body;
    body[QStringLiteral("status")] = 1;

    auto result = env.handler->handleCallback(QStringLiteral("f-001"), token, body);
    QCOMPARE(result[QStringLiteral("error")].toInt(), 0);

    // 不应创建新版本
    auto versions = env.db->listVersionsByFile(QStringLiteral("f-001"));
    QCOMPARE(versions.size(), 1);
}

void TestOnlyOfficeCallbackHandler::test_status2_savesNewVersion()
{
    CallbackEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-002"), QStringLiteral("sheet.xlsx"),
        QByteArrayLiteral("original"));
    QVERIFY(!token.isEmpty());

    // 获取当前版本 ID 以构造匹配的 key
    auto file = env.db->findFileById(QStringLiteral("f-002"));
    QVERIFY(file.has_value());
    const QString key = QStringLiteral("f-002_") + file->currentVersion;

    // 加锁（模拟编辑中状态）
    FileLockRecord lock;
    lock.fileId = QStringLiteral("f-002");
    lock.lockId = QStringLiteral("lock-edit");
    lock.lockedBy = QStringLiteral("client-alice");
    lock.lockedAtMs = QDateTime::currentMSecsSinceEpoch();
    lock.expiresAtMs = lock.lockedAtMs + 1800000;
    QVERIFY(env.db->insertFileLock(lock));

    // 设置 mock 下载内容
    const QByteArray newContent = QByteArrayLiteral("updated spreadsheet data");
    env.handler->nextDownloadContent = newContent;

    QJsonObject body;
    body[QStringLiteral("status")] = 2;
    body[QStringLiteral("key")] = key;
    body[QStringLiteral("url")] = QStringLiteral("http://onlyoffice/cache/file.xlsx");
    QJsonArray users;
    users.append(QStringLiteral("client-alice"));
    body[QStringLiteral("users")] = users;

    auto result = env.handler->handleCallback(QStringLiteral("f-002"), token, body);
    QCOMPARE(result[QStringLiteral("error")].toInt(), 0);

    // 验证新版本已创建
    auto versions = env.db->listVersionsByFile(QStringLiteral("f-002"));
    QCOMPARE(versions.size(), 2);
    QCOMPARE(versions.last().changeNote, QStringLiteral("协同编辑保存"));
    QCOMPARE(versions.last().uploaderId, QStringLiteral("client-alice"));

    // 验证文件内容
    auto saved = env.storage->readFile(versions.last().storagePath);
    QVERIFY(saved.has_value());
    QCOMPARE(*saved, newContent);

    // 验证锁已释放
    auto lockAfter = env.db->findFileLock(QStringLiteral("f-002"));
    QVERIFY(!lockAfter.has_value());
}

void TestOnlyOfficeCallbackHandler::test_status4_releasesLock()
{
    CallbackEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-003"), QStringLiteral("pres.pptx"),
        QByteArrayLiteral("content"));
    QVERIFY(!token.isEmpty());

    // 加锁
    FileLockRecord lock;
    lock.fileId = QStringLiteral("f-003");
    lock.lockId = QStringLiteral("lock-view");
    lock.lockedBy = QStringLiteral("client-alice");
    lock.lockedAtMs = QDateTime::currentMSecsSinceEpoch();
    lock.expiresAtMs = lock.lockedAtMs + 1800000;
    QVERIFY(env.db->insertFileLock(lock));

    QJsonObject body;
    body[QStringLiteral("status")] = 4;

    auto result = env.handler->handleCallback(QStringLiteral("f-003"), token, body);
    QCOMPARE(result[QStringLiteral("error")].toInt(), 0);

    // 锁应已释放
    auto lockAfter = env.db->findFileLock(QStringLiteral("f-003"));
    QVERIFY(!lockAfter.has_value());

    // 不应创建新版本
    auto versions = env.db->listVersionsByFile(QStringLiteral("f-003"));
    QCOMPARE(versions.size(), 1);
}

void TestOnlyOfficeCallbackHandler::test_status6_forceSave()
{
    CallbackEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-004"), QStringLiteral("auto.xlsx"),
        QByteArrayLiteral("original"));
    QVERIFY(!token.isEmpty());

    auto file = env.db->findFileById(QStringLiteral("f-004"));
    QVERIFY(file.has_value());
    const QString key = QStringLiteral("f-004_") + file->currentVersion;

    env.handler->nextDownloadContent = QByteArrayLiteral("autosaved content");

    QJsonObject body;
    body[QStringLiteral("status")] = 6;
    body[QStringLiteral("key")] = key;
    body[QStringLiteral("url")] = QStringLiteral("http://onlyoffice/cache/auto.xlsx");

    auto result = env.handler->handleCallback(QStringLiteral("f-004"), token, body);
    QCOMPARE(result[QStringLiteral("error")].toInt(), 0);

    auto versions = env.db->listVersionsByFile(QStringLiteral("f-004"));
    QCOMPARE(versions.size(), 2);
    QCOMPARE(versions.last().changeNote, QStringLiteral("自动保存"));
}

void TestOnlyOfficeCallbackHandler::test_duplicateCallback_skipped()
{
    CallbackEnv env;
    QVERIFY(env.setup());

    const QString token = env.seedFileAndToken(
        QStringLiteral("f-005"), QStringLiteral("dup.docx"),
        QByteArrayLiteral("original"));
    QVERIFY(!token.isEmpty());

    // key 使用不匹配的版本（模拟已过期的 callback）
    const QString staleKey = QStringLiteral("f-005_stale-version-id");

    env.handler->nextDownloadContent = QByteArrayLiteral("should not save");

    QJsonObject body;
    body[QStringLiteral("status")] = 2;
    body[QStringLiteral("key")] = staleKey;
    body[QStringLiteral("url")] = QStringLiteral("http://onlyoffice/cache/dup.docx");

    auto result = env.handler->handleCallback(QStringLiteral("f-005"), token, body);
    QCOMPARE(result[QStringLiteral("error")].toInt(), 0);

    // 不应创建新版本
    auto versions = env.db->listVersionsByFile(QStringLiteral("f-005"));
    QCOMPARE(versions.size(), 1);
}

void TestOnlyOfficeCallbackHandler::test_invalidToken_rejected()
{
    CallbackEnv env;
    QVERIFY(env.setup());

    QJsonObject body;
    body[QStringLiteral("status")] = 2;

    auto result = env.handler->handleCallback(
        QStringLiteral("f-999"), QStringLiteral("invalid-token"), body);
    QCOMPARE(result[QStringLiteral("error")].toInt(), 1);
}

QTEST_MAIN(TestOnlyOfficeCallbackHandler)
#include "TestOnlyOfficeCallbackHandler.moc"
