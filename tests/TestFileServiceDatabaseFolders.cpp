#include <QtTest>
#include <QTemporaryDir>
#include <QUuid>
#include "fileservice/FileServiceDatabase.h"

class TestFileServiceDatabaseFolders : public QObject {
    Q_OBJECT
    static QString uniqueConn() {
        return QStringLiteral("test-folder-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
private slots:
    void insertAndListFolders();
    void deleteFolderClearsFilesFolderId();
    void deleteFileAndVersions_removesAll();
    void updateFileFolderId_movesFile();
    void fileRecordIncludesFolderId();
    void folderNameUniquePerWorkspace();
    void tokenRoleDefaultsToMember();
    void tokenRoleUpdatedBySeedOrUpdate();
};

void TestFileServiceDatabaseFolders::insertAndListFolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    QVERIFY(db.insertFolder("f1", "ws-1", "Reports", "user-a"));
    QVERIFY(db.insertFolder("f2", "ws-1", "Contracts", "user-b"));
    QVERIFY(db.insertFolder("f3", "ws-2", "Other", "user-a"));

    const auto folders = db.listFolders("ws-1");
    QCOMPARE(folders.size(), std::size_t(2));
}

void TestFileServiceDatabaseFolders::deleteFolderClearsFilesFolderId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    QVERIFY(db.insertFolder("f1", "ws-1", "Reports", "user-a"));

    FileRecord rec;
    rec.fileId = "file1"; rec.workspaceId = "ws-1"; rec.fileName = "a.pdf";
    rec.currentVersion = "v1"; rec.uploadedById = "user-a"; rec.uploadedByName = "";
    rec.createdAtMs = 1000; rec.updatedAtMs = 1000; rec.folderId = "f1";
    QVERIFY(db.insertFile(rec));

    QVERIFY(db.deleteFolder("f1"));

    auto found = db.findFileById("file1");
    QVERIFY(found.has_value());
    QCOMPARE(found->folderId, QString());
}

void TestFileServiceDatabaseFolders::deleteFileAndVersions_removesAll()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    FileRecord rec;
    rec.fileId = "file1"; rec.workspaceId = "ws-1"; rec.fileName = "a.pdf";
    rec.currentVersion = "v2"; rec.uploadedById = "user-a"; rec.uploadedByName = "";
    rec.createdAtMs = 1000; rec.updatedAtMs = 1000;
    QVERIFY(db.insertFile(rec));

    FileVersionRecord v1;
    v1.versionId = "v1"; v1.fileId = "file1"; v1.versionNumber = 1;
    v1.storagePath = "file1/v1.bin"; v1.uploadedAtMs = 1000;
    v1.versionLabel = "v1"; v1.uploaderId = ""; v1.uploaderName = ""; v1.changeNote = "";
    QVERIFY(db.insertVersion(v1));

    FileVersionRecord v2;
    v2.versionId = "v2"; v2.fileId = "file1"; v2.versionNumber = 2;
    v2.storagePath = "file1/v2.bin"; v2.uploadedAtMs = 2000;
    v2.versionLabel = "v2"; v2.uploaderId = ""; v2.uploaderName = ""; v2.changeNote = "";
    QVERIFY(db.insertVersion(v2));

    auto paths = db.versionStoragePaths("file1");
    QCOMPARE(paths.size(), std::size_t(2));

    QVERIFY(db.deleteFileAndVersions("file1"));
    QVERIFY(!db.findFileById("file1").has_value());
    QCOMPARE(db.listVersionsByFile("file1").size(), qsizetype(0));
}

void TestFileServiceDatabaseFolders::updateFileFolderId_movesFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    QVERIFY(db.insertFolder("f1", "ws-1", "Reports", "user-a"));

    FileRecord rec;
    rec.fileId = "file1"; rec.workspaceId = "ws-1"; rec.fileName = "a.pdf";
    rec.currentVersion = "v1"; rec.uploadedById = "user-a"; rec.uploadedByName = "";
    rec.createdAtMs = 1000; rec.updatedAtMs = 1000;
    QVERIFY(db.insertFile(rec));

    QVERIFY(db.updateFileFolderId("file1", "f1"));
    auto found = db.findFileById("file1");
    QVERIFY(found.has_value());
    QCOMPARE(found->folderId, QStringLiteral("f1"));
}

void TestFileServiceDatabaseFolders::fileRecordIncludesFolderId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    FileRecord rec;
    rec.fileId = "file1"; rec.workspaceId = "ws-1"; rec.fileName = "a.pdf";
    rec.currentVersion = "v1"; rec.uploadedById = "user-a"; rec.uploadedByName = "";
    rec.createdAtMs = 1000; rec.updatedAtMs = 1000; rec.folderId = "folder-x";
    QVERIFY(db.insertFile(rec));

    const auto files = db.listFilesByWorkspace("ws-1");
    QCOMPARE(files.size(), qsizetype(1));
    QCOMPARE(files[0].folderId, QStringLiteral("folder-x"));
}

void TestFileServiceDatabaseFolders::folderNameUniquePerWorkspace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    QVERIFY(db.insertFolder("f1", "ws-1", "Reports", "user-a"));
    QVERIFY(!db.insertFolder("f2", "ws-1", "Reports", "user-b")); // 同 workspace 同名
    QVERIFY(db.insertFolder("f3", "ws-2", "Reports", "user-a")); // 不同 workspace 同名 OK
}

void TestFileServiceDatabaseFolders::tokenRoleDefaultsToMember()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    ServiceToken tok;
    tok.token = "tok-1";
    tok.clientId = "user-a";
    tok.displayName = "Alice";
    tok.allowedWorkspaces = "*";
    QVERIFY(db.insertToken(tok));
    auto token = db.findToken("tok-1");
    QVERIFY(token.has_value());
    QCOMPARE(token->role, QStringLiteral("member"));
}

void TestFileServiceDatabaseFolders::tokenRoleUpdatedBySeedOrUpdate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    FileServiceDatabase db(dir.filePath("test.db"), uniqueConn());
    QVERIFY(db.open());

    ServiceToken tok;
    tok.token = "tok-1";
    tok.clientId = "user-a";
    tok.displayName = "Alice";
    tok.allowedWorkspaces = "*";
    QVERIFY(db.insertToken(tok));
    QVERIFY(db.updateTokenRole("tok-1", "admin"));
    auto token = db.findToken("tok-1");
    QVERIFY(token.has_value());
    QCOMPARE(token->role, QStringLiteral("admin"));
}

QTEST_GUILESS_MAIN(TestFileServiceDatabaseFolders)
#include "TestFileServiceDatabaseFolders.moc"
