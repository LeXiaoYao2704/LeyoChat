#include <QtTest>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QUuid>

#include "FileServiceDatabase.h"

class TestFileServiceDatabase : public QObject {
    Q_OBJECT

    static QString uniqueConn()
    {
        return QStringLiteral("test-db-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

private slots:
    void insertAndFindFileById();
    void findFileByName_returnsNulloptWhenMissing();
    void findFileByName_returnsRecordAfterInsert();
    void insertVersion_andListVersionsByFile();
    void nextVersionNumber_startsAtOneAndIncrements();
    void updateFileCurrentVersion_updatesRecord();
    void insertAndFindToken();
    void findToken_returnsNulloptForUnknownToken();
    void listFilesByWorkspace_returnsOnlyMatchingWorkspace();
    void transaction_rollback_undoesInsert();
    void transaction_commit_persistsInsert();
    void test_createAndQueryFileLock();
    void test_fileLockConflict();
    void test_createAndValidateWopiToken();
    void test_wopiTokenExpiration();
    void test_wopiTokenRenewal();
    void test_deleteExpiredFileLocks();
    void test_deleteExpiredWopiTokens();
    void test_getExpiredChatFiles_returnsOnlyExpired();
    void test_getChatFilesTotalSize_sumsCorrectly();
    void test_getOldestChatFiles_orderedByCreatedAt();
    void test_deleteChatFileById_removesRecord();
};

void TestFileServiceDatabase::insertAndFindFileById()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f1.db")), uniqueConn());
    QVERIFY(db.open());

    FileRecord rec;
    rec.fileId         = QStringLiteral("fid-001");
    rec.workspaceId    = QStringLiteral("ws-dev");
    rec.fileName       = QStringLiteral("report.pdf");
    rec.currentVersion = QStringLiteral("vid-001");
    rec.uploadedById   = QStringLiteral("user-a");
    rec.uploadedByName = QStringLiteral("张明");
    rec.createdAtMs    = 1000;
    rec.updatedAtMs    = 2000;
    QVERIFY(db.insertFile(rec));

    const auto found = db.findFileById(QStringLiteral("fid-001"));
    QVERIFY(found.has_value());
    QCOMPARE(found->fileId,         QStringLiteral("fid-001"));
    QCOMPARE(found->fileName,       QStringLiteral("report.pdf"));
    QCOMPARE(found->workspaceId,    QStringLiteral("ws-dev"));
    QCOMPARE(found->uploadedByName, QStringLiteral("张明"));
    QCOMPARE(found->createdAtMs,    qint64(1000));
}

void TestFileServiceDatabase::findFileByName_returnsNulloptWhenMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f2.db")), uniqueConn());
    QVERIFY(db.open());

    const auto result = db.findFileByName(QStringLiteral("ws-dev"), QStringLiteral("nonexistent.pdf"));
    QVERIFY(!result.has_value());
}

void TestFileServiceDatabase::findFileByName_returnsRecordAfterInsert()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f3.db")), uniqueConn());
    QVERIFY(db.open());

    FileRecord rec;
    rec.fileId = QStringLiteral("fid-002"); rec.workspaceId = QStringLiteral("ws-prod");
    rec.fileName = QStringLiteral("notes.docx"); rec.currentVersion = QStringLiteral("vid-002");
    rec.uploadedById = QStringLiteral("u1"); rec.uploadedByName = QStringLiteral("");
    rec.createdAtMs = 100; rec.updatedAtMs = 200;
    QVERIFY(db.insertFile(rec));

    const auto found = db.findFileByName(QStringLiteral("ws-prod"), QStringLiteral("notes.docx"));
    QVERIFY(found.has_value());
    QCOMPARE(found->fileId, QStringLiteral("fid-002"));

    const auto notFound = db.findFileByName(QStringLiteral("ws-other"), QStringLiteral("notes.docx"));
    QVERIFY(!notFound.has_value());
}

void TestFileServiceDatabase::insertVersion_andListVersionsByFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f4.db")), uniqueConn());
    QVERIFY(db.open());

    FileRecord file;
    file.fileId = QStringLiteral("fid-x"); file.workspaceId = QStringLiteral("ws-dev");
    file.fileName = QStringLiteral("x.bin"); file.currentVersion = QStringLiteral("vid-x1");
    file.uploadedById = QStringLiteral("u"); file.uploadedByName = QStringLiteral("");
    file.createdAtMs = 1; file.updatedAtMs = 2;
    QVERIFY(db.insertFile(file));

    FileVersionRecord v1;
    v1.versionId = QStringLiteral("vid-x1"); v1.fileId = QStringLiteral("fid-x");
    v1.versionNumber = 1; v1.versionLabel = QStringLiteral("v1");
    v1.uploaderId = QStringLiteral("u"); v1.uploaderName = QStringLiteral("");
    v1.uploadedAtMs = 1000;
    v1.fileSize = 512; v1.storagePath = QStringLiteral("fid-x/vid-x1.bin");
    v1.changeNote = QStringLiteral("");
    QVERIFY(db.insertVersion(v1));

    FileVersionRecord v2;
    v2.versionId = QStringLiteral("vid-x2"); v2.fileId = QStringLiteral("fid-x");
    v2.versionNumber = 2; v2.versionLabel = QStringLiteral("v2");
    v2.uploaderId = QStringLiteral("u"); v2.uploaderName = QStringLiteral("");
    v2.uploadedAtMs = 2000;
    v2.fileSize = 768; v2.storagePath = QStringLiteral("fid-x/vid-x2.bin");
    v2.changeNote = QStringLiteral("更新了密钥");
    QVERIFY(db.insertVersion(v2));

    const QVector<FileVersionRecord> versions = db.listVersionsByFile(QStringLiteral("fid-x"));
    QCOMPARE(versions.size(), 2);
    bool hasV1 = false, hasV2 = false;
    for (const auto& v : versions) {
        if (v.versionId == QStringLiteral("vid-x1")) hasV1 = true;
        if (v.versionId == QStringLiteral("vid-x2")) {
            hasV2 = true;
            QCOMPARE(v.changeNote, QStringLiteral("更新了密钥"));
        }
    }
    QVERIFY(hasV1);
    QVERIFY(hasV2);
}

void TestFileServiceDatabase::nextVersionNumber_startsAtOneAndIncrements()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f5.db")), uniqueConn());
    QVERIFY(db.open());

    FileRecord file;
    file.fileId = QStringLiteral("fid-y"); file.workspaceId = QStringLiteral("ws");
    file.fileName = QStringLiteral("y.txt"); file.currentVersion = QStringLiteral("vid-y1");
    file.uploadedById = QStringLiteral("u"); file.uploadedByName = QStringLiteral("");
    file.createdAtMs = 1; file.updatedAtMs = 1;
    QVERIFY(db.insertFile(file));

    QCOMPARE(db.nextVersionNumber(QStringLiteral("fid-y")), 1);

    FileVersionRecord v1;
    v1.versionId = QStringLiteral("vid-y1"); v1.fileId = QStringLiteral("fid-y");
    v1.versionNumber = 1; v1.versionLabel = QStringLiteral("v1");
    v1.uploaderId = QStringLiteral("u"); v1.uploaderName = QStringLiteral("");
    v1.uploadedAtMs = 1000;
    v1.fileSize = 100; v1.storagePath = QStringLiteral("fid-y/vid-y1.bin");
    v1.changeNote = QStringLiteral("");
    QVERIFY(db.insertVersion(v1));

    QCOMPARE(db.nextVersionNumber(QStringLiteral("fid-y")), 2);
}

void TestFileServiceDatabase::updateFileCurrentVersion_updatesRecord()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f6.db")), uniqueConn());
    QVERIFY(db.open());

    FileRecord file;
    file.fileId = QStringLiteral("fid-z"); file.workspaceId = QStringLiteral("ws");
    file.fileName = QStringLiteral("z.txt"); file.currentVersion = QStringLiteral("vid-z1");
    file.uploadedById = QStringLiteral("u"); file.uploadedByName = QStringLiteral("");
    file.createdAtMs = 1; file.updatedAtMs = 1;
    QVERIFY(db.insertFile(file));

    QVERIFY(db.updateFileCurrentVersion(
        QStringLiteral("fid-z"), QStringLiteral("vid-z2"), 9999));

    const auto updated = db.findFileById(QStringLiteral("fid-z"));
    QVERIFY(updated.has_value());
    QCOMPARE(updated->currentVersion, QStringLiteral("vid-z2"));
    QCOMPARE(updated->updatedAtMs,    qint64(9999));
}

void TestFileServiceDatabase::insertAndFindToken()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f7.db")), uniqueConn());
    QVERIFY(db.open());

    ServiceToken tok;
    tok.token              = QStringLiteral("test-token-abc");
    tok.clientId           = QStringLiteral("client-1");
    tok.displayName        = QStringLiteral("管理员");
    tok.createdAtMs        = 1234;
    tok.allowedWorkspaces  = QStringLiteral("*");
    QVERIFY(db.insertToken(tok));

    const auto found = db.findToken(QStringLiteral("test-token-abc"));
    QVERIFY(found.has_value());
    QCOMPARE(found->clientId,          QStringLiteral("client-1"));
    QCOMPARE(found->displayName,       QStringLiteral("管理员"));
    QCOMPARE(found->allowedWorkspaces, QStringLiteral("*"));
}

void TestFileServiceDatabase::findToken_returnsNulloptForUnknownToken()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f8.db")), uniqueConn());
    QVERIFY(db.open());

    const auto result = db.findToken(QStringLiteral("no-such-token"));
    QVERIFY(!result.has_value());
}

void TestFileServiceDatabase::listFilesByWorkspace_returnsOnlyMatchingWorkspace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("f9.db")), uniqueConn());
    QVERIFY(db.open());

    auto addFile = [&](const QString& id, const QString& ws, const QString& name) {
        FileRecord r;
        r.fileId = id; r.workspaceId = ws; r.fileName = name;
        r.currentVersion = QStringLiteral("v1"); r.uploadedById = QStringLiteral("u");
        r.uploadedByName = QStringLiteral("");
        r.createdAtMs = 1; r.updatedAtMs = 1;
        db.insertFile(r);
    };
    addFile(QStringLiteral("fa"), QStringLiteral("ws-a"), QStringLiteral("a.txt"));
    addFile(QStringLiteral("fb"), QStringLiteral("ws-a"), QStringLiteral("b.txt"));
    addFile(QStringLiteral("fc"), QStringLiteral("ws-b"), QStringLiteral("c.txt"));

    const QVector<FileRecord> wsA = db.listFilesByWorkspace(QStringLiteral("ws-a"));
    QCOMPARE(wsA.size(), 2);

    const QVector<FileRecord> wsB = db.listFilesByWorkspace(QStringLiteral("ws-b"));
    QCOMPARE(wsB.size(), 1);
    QCOMPARE(wsB[0].fileId, QStringLiteral("fc"));
}

void TestFileServiceDatabase::transaction_rollback_undoesInsert()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("tx1.db")), uniqueConn());
    QVERIFY(db.open());

    QVERIFY(db.beginTransaction());

    FileRecord rec;
    rec.fileId = QStringLiteral("fid-tx-1"); rec.workspaceId = QStringLiteral("ws");
    rec.fileName = QStringLiteral("tx.bin"); rec.currentVersion = QStringLiteral("");
    rec.uploadedById = QStringLiteral("u"); rec.uploadedByName = QStringLiteral("");
    rec.createdAtMs = 1; rec.updatedAtMs = 1;
    QVERIFY(db.insertFile(rec));

    QVERIFY(db.rollback());

    // After rollback, the record must not exist
    const auto found = db.findFileById(QStringLiteral("fid-tx-1"));
    QVERIFY(!found.has_value());
}

void TestFileServiceDatabase::transaction_commit_persistsInsert()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("tx2.db")), uniqueConn());
    QVERIFY(db.open());

    QVERIFY(db.beginTransaction());

    FileRecord rec;
    rec.fileId = QStringLiteral("fid-tx-2"); rec.workspaceId = QStringLiteral("ws");
    rec.fileName = QStringLiteral("tx2.bin"); rec.currentVersion = QStringLiteral("");
    rec.uploadedById = QStringLiteral("u"); rec.uploadedByName = QStringLiteral("");
    rec.createdAtMs = 1; rec.updatedAtMs = 1;
    QVERIFY(db.insertFile(rec));

    QVERIFY(db.commit());

    // After commit, the record must exist
    const auto found = db.findFileById(QStringLiteral("fid-tx-2"));
    QVERIFY(found.has_value());
    QCOMPARE(found->fileName, QStringLiteral("tx2.bin"));
}

void TestFileServiceDatabase::test_createAndQueryFileLock()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("lock1.db")), uniqueConn());
    QVERIFY(db.open());

    FileLockRecord lock;
    lock.fileId      = QStringLiteral("fid-lock-1");
    lock.lockId      = QStringLiteral("lk-abc");
    lock.lockedBy    = QStringLiteral("user-a");
    lock.lockedAtMs  = 1000;
    lock.expiresAtMs = 9000;
    QVERIFY(db.insertFileLock(lock));

    const auto found = db.findFileLock(QStringLiteral("fid-lock-1"));
    QVERIFY(found.has_value());
    QCOMPARE(found->lockId,   QStringLiteral("lk-abc"));
    QCOMPARE(found->lockedBy, QStringLiteral("user-a"));
    QCOMPARE(found->expiresAtMs, qint64(9000));

    QVERIFY(db.deleteFileLock(QStringLiteral("fid-lock-1")));

    const auto gone = db.findFileLock(QStringLiteral("fid-lock-1"));
    QVERIFY(!gone.has_value());
}

void TestFileServiceDatabase::test_fileLockConflict()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("lock2.db")), uniqueConn());
    QVERIFY(db.open());

    FileLockRecord lock;
    lock.fileId      = QStringLiteral("fid-conflict");
    lock.lockId      = QStringLiteral("lk-1");
    lock.lockedBy    = QStringLiteral("user-a");
    lock.lockedAtMs  = 1000;
    lock.expiresAtMs = 9000;
    QVERIFY(db.insertFileLock(lock));

    // 同一文件重复加锁应失败（PRIMARY KEY 冲突）
    FileLockRecord lock2;
    lock2.fileId      = QStringLiteral("fid-conflict");
    lock2.lockId      = QStringLiteral("lk-2");
    lock2.lockedBy    = QStringLiteral("user-b");
    lock2.lockedAtMs  = 2000;
    lock2.expiresAtMs = 10000;
    QVERIFY(!db.insertFileLock(lock2));
}

void TestFileServiceDatabase::test_createAndValidateWopiToken()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("wopi1.db")), uniqueConn());
    QVERIFY(db.open());

    WopiTokenRecord tok;
    tok.token       = QStringLiteral("wopi-tok-001");
    tok.fileId      = QStringLiteral("fid-w1");
    tok.clientId    = QStringLiteral("client-1");
    tok.displayName = QStringLiteral("编辑者");
    tok.role        = QStringLiteral("editor");
    tok.createdAtMs = 1000;
    tok.expiresAtMs = 60000;
    QVERIFY(db.insertWopiToken(tok));

    // nowMs < expiresAtMs → 有效
    const auto valid = db.validateWopiToken(QStringLiteral("wopi-tok-001"), 5000);
    QVERIFY(valid.has_value());
    QCOMPARE(valid->fileId,      QStringLiteral("fid-w1"));
    QCOMPARE(valid->clientId,    QStringLiteral("client-1"));
    QCOMPARE(valid->displayName, QStringLiteral("编辑者"));
    QCOMPARE(valid->role,        QStringLiteral("editor"));
}

void TestFileServiceDatabase::test_wopiTokenExpiration()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("wopi2.db")), uniqueConn());
    QVERIFY(db.open());

    WopiTokenRecord tok;
    tok.token       = QStringLiteral("wopi-tok-exp");
    tok.fileId      = QStringLiteral("fid-w2");
    tok.clientId    = QStringLiteral("client-2");
    tok.displayName = QStringLiteral("查看者");
    tok.role        = QStringLiteral("viewer");
    tok.createdAtMs = 1000;
    tok.expiresAtMs = 5000;
    QVERIFY(db.insertWopiToken(tok));

    // nowMs >= expiresAtMs → 过期，返回 nullopt
    const auto expired = db.validateWopiToken(QStringLiteral("wopi-tok-exp"), 5000);
    QVERIFY(!expired.has_value());

    const auto alsoExpired = db.validateWopiToken(QStringLiteral("wopi-tok-exp"), 9999);
    QVERIFY(!alsoExpired.has_value());
}

void TestFileServiceDatabase::test_wopiTokenRenewal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("wopi3.db")), uniqueConn());
    QVERIFY(db.open());

    WopiTokenRecord tok;
    tok.token       = QStringLiteral("wopi-tok-renew");
    tok.fileId      = QStringLiteral("fid-w3");
    tok.clientId    = QStringLiteral("client-3");
    tok.displayName = QStringLiteral("用户");
    tok.role        = QStringLiteral("editor");
    tok.createdAtMs = 1000;
    tok.expiresAtMs = 5000;
    QVERIFY(db.insertWopiToken(tok));

    // 续期
    QVERIFY(db.renewWopiToken(QStringLiteral("wopi-tok-renew"), 99000));

    // 原本 5000 已过期的时间点现在应该有效
    const auto renewed = db.validateWopiToken(QStringLiteral("wopi-tok-renew"), 6000);
    QVERIFY(renewed.has_value());
    QCOMPARE(renewed->expiresAtMs, qint64(99000));
}

void TestFileServiceDatabase::test_deleteExpiredFileLocks()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("exp-locks.db")), uniqueConn());
    QVERIFY(db.open());

    FileLockRecord lock1;
    lock1.fileId = QStringLiteral("f-exp-1");
    lock1.lockId = QStringLiteral("lock-1");
    lock1.lockedBy = QStringLiteral("user-1");
    lock1.lockedAtMs = 1000;
    lock1.expiresAtMs = 5000; // expired
    QVERIFY(db.insertFileLock(lock1));

    FileLockRecord lock2;
    lock2.fileId = QStringLiteral("f-exp-2");
    lock2.lockId = QStringLiteral("lock-2");
    lock2.lockedBy = QStringLiteral("user-2");
    lock2.lockedAtMs = 1000;
    lock2.expiresAtMs = 99000; // still valid
    QVERIFY(db.insertFileLock(lock2));

    const int deleted = db.deleteExpiredFileLocks(10000);
    QCOMPARE(deleted, 1);

    // lock1 should be gone
    QVERIFY(!db.findFileLock(QStringLiteral("f-exp-1")).has_value());
    // lock2 should remain
    QVERIFY(db.findFileLock(QStringLiteral("f-exp-2")).has_value());
}

void TestFileServiceDatabase::test_deleteExpiredWopiTokens()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("exp-tokens.db")), uniqueConn());
    QVERIFY(db.open());

    WopiTokenRecord tok1;
    tok1.token = QStringLiteral("tok-expired");
    tok1.fileId = QStringLiteral("f1");
    tok1.clientId = QStringLiteral("c1");
    tok1.displayName = QStringLiteral("U1");
    tok1.role = QStringLiteral("editor");
    tok1.createdAtMs = 1000;
    tok1.expiresAtMs = 5000; // expired
    QVERIFY(db.insertWopiToken(tok1));

    WopiTokenRecord tok2;
    tok2.token = QStringLiteral("tok-valid");
    tok2.fileId = QStringLiteral("f2");
    tok2.clientId = QStringLiteral("c2");
    tok2.displayName = QStringLiteral("U2");
    tok2.role = QStringLiteral("editor");
    tok2.createdAtMs = 1000;
    tok2.expiresAtMs = 99000; // still valid
    QVERIFY(db.insertWopiToken(tok2));

    const int deleted = db.deleteExpiredWopiTokens(10000);
    QCOMPARE(deleted, 1);

    // expired token should be gone
    QVERIFY(!db.validateWopiToken(QStringLiteral("tok-expired"), 0).has_value());
    // valid token should remain
    QVERIFY(db.validateWopiToken(QStringLiteral("tok-valid"), 0).has_value());
}

void TestFileServiceDatabase::test_getExpiredChatFiles_returnsOnlyExpired()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("cleanup.db")), uniqueConn());
    QVERIFY(db.open());

    const qint64 now = 1000000000LL;
    const qint64 day = 86400000LL;

    ChatFileRecord old1;
    old1.chatFileId = QStringLiteral("cf-old1"); old1.workspaceId = QStringLiteral("ws-1");
    old1.fileName = QStringLiteral("old.txt"); old1.uploaderId = QStringLiteral("u1");
    old1.fileSize = 100; old1.createdAtMs = now - 10 * day;
    old1.storagePath = QStringLiteral("chat-files/cf-old1/old.txt");
    QVERIFY(db.insertChatFile(old1));

    ChatFileRecord old2;
    old2.chatFileId = QStringLiteral("cf-old2"); old2.workspaceId = QStringLiteral("ws-1");
    old2.fileName = QStringLiteral("border.txt"); old2.uploaderId = QStringLiteral("u1");
    old2.fileSize = 200; old2.createdAtMs = now - 7 * day;
    old2.storagePath = QStringLiteral("chat-files/cf-old2/border.txt");
    QVERIFY(db.insertChatFile(old2));

    ChatFileRecord fresh;
    fresh.chatFileId = QStringLiteral("cf-fresh"); fresh.workspaceId = QStringLiteral("ws-1");
    fresh.fileName = QStringLiteral("new.txt"); fresh.uploaderId = QStringLiteral("u1");
    fresh.fileSize = 300; fresh.createdAtMs = now - 3 * day;
    fresh.storagePath = QStringLiteral("chat-files/cf-fresh/new.txt");
    QVERIFY(db.insertChatFile(fresh));

    const qint64 cutoff = now - 7 * day;
    auto expired = db.getExpiredChatFiles(cutoff);
    QCOMPARE(static_cast<int>(expired.size()), 2);
    QCOMPARE(expired[0].chatFileId, QStringLiteral("cf-old1"));
}

void TestFileServiceDatabase::test_getChatFilesTotalSize_sumsCorrectly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("size.db")), uniqueConn());
    QVERIFY(db.open());

    QCOMPARE(db.getChatFilesTotalSize(), qint64(0));

    ChatFileRecord r1;
    r1.chatFileId = QStringLiteral("cf-1"); r1.workspaceId = QStringLiteral("ws-1");
    r1.fileName = QStringLiteral("a.bin"); r1.uploaderId = QStringLiteral("u1");
    r1.fileSize = 1024; r1.createdAtMs = 1000;
    r1.storagePath = QStringLiteral("chat-files/cf-1/a.bin");
    QVERIFY(db.insertChatFile(r1));

    ChatFileRecord r2;
    r2.chatFileId = QStringLiteral("cf-2"); r2.workspaceId = QStringLiteral("ws-1");
    r2.fileName = QStringLiteral("b.bin"); r2.uploaderId = QStringLiteral("u1");
    r2.fileSize = 2048; r2.createdAtMs = 2000;
    r2.storagePath = QStringLiteral("chat-files/cf-2/b.bin");
    QVERIFY(db.insertChatFile(r2));

    QCOMPARE(db.getChatFilesTotalSize(), qint64(3072));
}

void TestFileServiceDatabase::test_getOldestChatFiles_orderedByCreatedAt()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("oldest.db")), uniqueConn());
    QVERIFY(db.open());

    for (int i = 3; i >= 1; --i) {
        ChatFileRecord r;
        r.chatFileId = QStringLiteral("cf-%1").arg(i);
        r.workspaceId = QStringLiteral("ws-1");
        r.fileName = QStringLiteral("f%1.bin").arg(i);
        r.uploaderId = QStringLiteral("u1"); r.fileSize = 500;
        r.createdAtMs = i * 1000;
        r.storagePath = QStringLiteral("chat-files/cf-%1/f%1.bin").arg(i);
        QVERIFY(db.insertChatFile(r));
    }

    auto oldest = db.getOldestChatFiles(2);
    QCOMPARE(static_cast<int>(oldest.size()), 2);
    QCOMPARE(oldest[0].chatFileId, QStringLiteral("cf-1"));
    QCOMPARE(oldest[1].chatFileId, QStringLiteral("cf-2"));
}

void TestFileServiceDatabase::test_deleteChatFileById_removesRecord()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath(QStringLiteral("del.db")), uniqueConn());
    QVERIFY(db.open());

    ChatFileRecord r;
    r.chatFileId = QStringLiteral("cf-del"); r.workspaceId = QStringLiteral("ws-1");
    r.fileName = QStringLiteral("del.txt"); r.uploaderId = QStringLiteral("u1");
    r.fileSize = 100; r.createdAtMs = 1000;
    r.storagePath = QStringLiteral("chat-files/cf-del/del.txt");
    QVERIFY(db.insertChatFile(r));
    QVERIFY(db.findChatFileById(QStringLiteral("cf-del")).has_value());

    QVERIFY(db.deleteChatFileById(QStringLiteral("cf-del")));
    QVERIFY(!db.findChatFileById(QStringLiteral("cf-del")).has_value());

    // 删除不存在的记录也应返回 true（幂等）
    QVERIFY(db.deleteChatFileById(QStringLiteral("cf-nonexistent")));
}

QTEST_MAIN(TestFileServiceDatabase)
#include "TestFileServiceDatabase.moc"
