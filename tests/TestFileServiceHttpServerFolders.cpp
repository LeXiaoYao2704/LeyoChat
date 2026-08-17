#include <QtTest>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUuid>

#include "fileservice/FileServiceAuth.h"
#include "fileservice/FileServiceDatabase.h"
#include "fileservice/FileServiceHttpServer.h"
#include "fileservice/FileStorageManager.h"

namespace {
QString uniqueConn()
{
    return QStringLiteral("test-http-folders-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString jsonScope(std::initializer_list<const char*> workspaces)
{
    QJsonArray array;
    for (const char* workspace : workspaces)
        array.append(QString::fromLatin1(workspace));
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

quint16 reserveFreePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}
} // namespace

class TestFileServiceHttpServerFolders : public QObject {
    Q_OBJECT
private slots:
    void createAndListFolders();
    void deleteFolder_movesFilesToRoot();
    void deleteFile_byUploader_succeeds();
    void deleteFile_byNonUploader_returns403();
    void deleteFile_byAdmin_succeeds();
    void moveFileToFolder();
    void fileListIncludesFolderId();
    void uploadFile_viaHttpPut_succeeds();
};

void TestFileServiceHttpServerFolders::createAndListFolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    // Admin token
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-admin"), QStringLiteral("admin-1"),
        QStringLiteral("Admin User"), jsonScope({"ws-1"})));
    QVERIFY(db.updateTokenRole(QStringLiteral("tok-admin"), QStringLiteral("admin")));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    QNetworkAccessManager nam;

    // POST /api/v1/folders — create folder
    {
        QJsonObject body;
        body[QStringLiteral("workspaceId")] = QStringLiteral("ws-1");
        body[QStringLiteral("folderName")] = QStringLiteral("Design Docs");

        QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/folders").arg(port)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-admin"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
            nam.post(req, QJsonDocument(body).toJson()));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 201);
        const auto respDoc = QJsonDocument::fromJson(reply->readAll());
        QVERIFY(respDoc.isObject());
        QVERIFY(!respDoc.object()[QStringLiteral("folder_id")].toString().isEmpty());
        QCOMPARE(respDoc.object()[QStringLiteral("folder_name")].toString(),
                 QStringLiteral("Design Docs"));
    }

    // GET /api/v1/folders?workspaceId=ws-1 — list folders
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/folders?workspaceId=ws-1").arg(port)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-admin"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(nam.get(req));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        const auto respDoc = QJsonDocument::fromJson(reply->readAll());
        QVERIFY(respDoc.isArray());
        QCOMPARE(respDoc.array().size(), 1);
        QCOMPARE(respDoc.array()[0].toObject()[QStringLiteral("folder_name")].toString(),
                 QStringLiteral("Design Docs"));
    }
}

void TestFileServiceHttpServerFolders::deleteFolder_movesFilesToRoot()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-admin"), QStringLiteral("admin-1"),
        QStringLiteral("Admin"), jsonScope({"ws-1"})));
    QVERIFY(db.updateTokenRole(QStringLiteral("tok-admin"), QStringLiteral("admin")));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    QNetworkAccessManager nam;

    // Create folder via DB directly
    const QString folderId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(db.insertFolder(folderId, QStringLiteral("ws-1"),
                            QStringLiteral("TempFolder"), QStringLiteral("admin-1")));

    // Create a file assigned to that folder
    FileRecord file;
    file.fileId = QStringLiteral("file-del-folder-1");
    file.workspaceId = QStringLiteral("ws-1");
    file.fileName = QStringLiteral("readme.txt");
    file.currentVersion = QStringLiteral("v1");
    file.uploadedById = QStringLiteral("admin-1");
    file.uploadedByName = QStringLiteral("Admin");
    file.createdAtMs = 1;
    file.updatedAtMs = 1;
    file.folderId = folderId;
    QVERIFY(db.insertFile(file));

    // DELETE /api/v1/folders/<folderId>
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/folders/%2").arg(port).arg(folderId)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-admin"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
            nam.deleteResource(req));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 204);
    }

    // Verify file's folder_id is now empty (moved to root)
    const auto updatedFile = db.findFileById(QStringLiteral("file-del-folder-1"));
    QVERIFY(updatedFile.has_value());
    QVERIFY(updatedFile->folderId.isEmpty());
}

void TestFileServiceHttpServerFolders::deleteFile_byUploader_succeeds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    // user-a token (non-admin uploader)
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-a"), QStringLiteral("user-a"),
        QStringLiteral("User A"), jsonScope({"ws-1"})));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    // Seed file via DB
    const QString fileId = QStringLiteral("file-del-1");
    const QString versionId = QStringLiteral("ver-del-1");
    const auto storagePath = storage.saveFile(fileId, versionId, QByteArrayLiteral("content"));
    QVERIFY(storagePath.has_value());

    FileRecord file;
    file.fileId = fileId;
    file.workspaceId = QStringLiteral("ws-1");
    file.fileName = QStringLiteral("test.txt");
    file.currentVersion = versionId;
    file.uploadedById = QStringLiteral("user-a");
    file.uploadedByName = QStringLiteral("User A");
    file.createdAtMs = 1;
    file.updatedAtMs = 1;
    QVERIFY(db.insertFile(file));

    FileVersionRecord ver;
    ver.versionId = versionId;
    ver.fileId = fileId;
    ver.versionNumber = 1;
    ver.versionLabel = QStringLiteral("v1");
    ver.uploaderId = QStringLiteral("user-a");
    ver.uploaderName = QStringLiteral("User A");
    ver.uploadedAtMs = 1;
    ver.fileSize = 7;
    ver.storagePath = *storagePath;
    ver.changeNote = QStringLiteral("initial");
    QVERIFY(db.insertVersion(ver));

    QNetworkAccessManager nam;

    // DELETE /api/v1/files/<fileId> — by uploader
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/files/%2").arg(port).arg(fileId)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-a"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
            nam.deleteResource(req));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 204);
    }

    // Verify file is gone from DB
    QVERIFY(!db.findFileById(fileId).has_value());
}

void TestFileServiceHttpServerFolders::deleteFile_byNonUploader_returns403()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    // user-a uploads, user-b tries to delete
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-b"), QStringLiteral("user-b"),
        QStringLiteral("User B"), jsonScope({"ws-1"})));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    // Seed file uploaded by user-a
    FileRecord file;
    file.fileId = QStringLiteral("file-403-1");
    file.workspaceId = QStringLiteral("ws-1");
    file.fileName = QStringLiteral("secret.txt");
    file.currentVersion = QStringLiteral("ver-403-1");
    file.uploadedById = QStringLiteral("user-a");
    file.uploadedByName = QStringLiteral("User A");
    file.createdAtMs = 1;
    file.updatedAtMs = 1;
    QVERIFY(db.insertFile(file));

    QNetworkAccessManager nam;

    // DELETE by user-b (non-uploader, non-admin)
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/files/file-403-1").arg(port)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-b"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
            nam.deleteResource(req));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);
    }

    // Verify file still exists
    QVERIFY(db.findFileById(QStringLiteral("file-403-1")).has_value());
}

void TestFileServiceHttpServerFolders::deleteFile_byAdmin_succeeds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    // Admin token for user-b
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-admin-b"), QStringLiteral("user-b"),
        QStringLiteral("Admin B"), jsonScope({"ws-1"})));
    QVERIFY(db.updateTokenRole(QStringLiteral("tok-admin-b"), QStringLiteral("admin")));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    // Seed file uploaded by user-a
    const QString fileId = QStringLiteral("file-admin-del-1");
    const QString versionId = QStringLiteral("ver-admin-del-1");
    const auto storagePath = storage.saveFile(fileId, versionId, QByteArrayLiteral("data"));
    QVERIFY(storagePath.has_value());

    FileRecord file;
    file.fileId = fileId;
    file.workspaceId = QStringLiteral("ws-1");
    file.fileName = QStringLiteral("report.txt");
    file.currentVersion = versionId;
    file.uploadedById = QStringLiteral("user-a");
    file.uploadedByName = QStringLiteral("User A");
    file.createdAtMs = 1;
    file.updatedAtMs = 1;
    QVERIFY(db.insertFile(file));

    FileVersionRecord ver;
    ver.versionId = versionId;
    ver.fileId = fileId;
    ver.versionNumber = 1;
    ver.versionLabel = QStringLiteral("v1");
    ver.uploaderId = QStringLiteral("user-a");
    ver.uploaderName = QStringLiteral("User A");
    ver.uploadedAtMs = 1;
    ver.fileSize = 4;
    ver.storagePath = *storagePath;
    ver.changeNote = QStringLiteral("init");
    QVERIFY(db.insertVersion(ver));

    QNetworkAccessManager nam;

    // DELETE by admin user-b
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/files/%2").arg(port).arg(fileId)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-admin-b"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
            nam.deleteResource(req));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 204);
    }

    QVERIFY(!db.findFileById(fileId).has_value());
}

void TestFileServiceHttpServerFolders::moveFileToFolder()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-1"), QStringLiteral("client-1"),
        QStringLiteral("Client"), jsonScope({"ws-1"})));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    // Seed file + folder
    const QString folderId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVERIFY(db.insertFolder(folderId, QStringLiteral("ws-1"),
                            QStringLiteral("Archive"), QStringLiteral("client-1")));

    FileRecord file;
    file.fileId = QStringLiteral("file-move-1");
    file.workspaceId = QStringLiteral("ws-1");
    file.fileName = QStringLiteral("notes.txt");
    file.currentVersion = QStringLiteral("v1");
    file.uploadedById = QStringLiteral("client-1");
    file.uploadedByName = QStringLiteral("Client");
    file.createdAtMs = 1;
    file.updatedAtMs = 1;
    QVERIFY(db.insertFile(file));

    QNetworkAccessManager nam;

    // PUT /api/v1/files/<fileId>/folder
    {
        QJsonObject body;
        body[QStringLiteral("folderId")] = folderId;

        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/files/file-move-1/folder").arg(port)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-1"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
            nam.put(req, QJsonDocument(body).toJson()));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    }

    // Verify in DB
    const auto updated = db.findFileById(QStringLiteral("file-move-1"));
    QVERIFY(updated.has_value());
    QCOMPARE(updated->folderId, folderId);
}

void TestFileServiceHttpServerFolders::fileListIncludesFolderId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-1"), QStringLiteral("client-1"),
        QStringLiteral("Client"), jsonScope({"ws-1"})));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    // Seed file with folder_id
    const QString folderId = QStringLiteral("folder-abc");
    QVERIFY(db.insertFolder(folderId, QStringLiteral("ws-1"),
                            QStringLiteral("Specs"), QStringLiteral("client-1")));

    FileRecord file;
    file.fileId = QStringLiteral("file-list-1");
    file.workspaceId = QStringLiteral("ws-1");
    file.fileName = QStringLiteral("spec.docx");
    file.currentVersion = QStringLiteral("v1");
    file.uploadedById = QStringLiteral("client-1");
    file.uploadedByName = QStringLiteral("Client");
    file.createdAtMs = 1;
    file.updatedAtMs = 1;
    file.folderId = folderId;
    QVERIFY(db.insertFile(file));

    QNetworkAccessManager nam;

    // GET /api/v1/files?workspaceId=ws-1
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/files?workspaceId=ws-1").arg(port)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-1"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(nam.get(req));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        const auto respDoc = QJsonDocument::fromJson(reply->readAll());
        QVERIFY(respDoc.isArray());
        QCOMPARE(respDoc.array().size(), 1);
        QCOMPARE(respDoc.array()[0].toObject()[QStringLiteral("folder_id")].toString(),
                 folderId);
    }
}

void TestFileServiceHttpServerFolders::uploadFile_viaHttpPut_succeeds()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    // Seed a member token
    QVERIFY(auth.seedOrUpdateTokenScope(
        QStringLiteral("tok-upload"), QStringLiteral("uploader-1"),
        QStringLiteral("Test Uploader"), jsonScope({"ws-1"})));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    QNetworkAccessManager nam;

    // PUT /api/v1/files/testfile.txt — same way GroupFileManagerDialog does it
    const QByteArray fileContent = QByteArrayLiteral("hello world test content");
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/files/testfile.txt").arg(port)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-upload"));
        req.setRawHeader("X-Workspace-Id", QByteArrayLiteral("ws-1"));
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/octet-stream"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
            nam.put(req, fileContent));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 201) {
            qWarning() << "Upload failed! Status:" << status
                       << "Error:" << reply->errorString()
                       << "Body:" << reply->readAll();
        }
        QCOMPARE(status, 201);
        const auto respDoc = QJsonDocument::fromJson(reply->readAll());
        QVERIFY(respDoc.isObject());
        QVERIFY(!respDoc.object()[QStringLiteral("file_id")].toString().isEmpty());
        QVERIFY(!respDoc.object()[QStringLiteral("version_id")].toString().isEmpty());
        QCOMPARE(respDoc.object()[QStringLiteral("is_new_file")].toBool(), true);
    }

    // Verify file appears in list
    {
        QNetworkRequest req(QUrl(
            QStringLiteral("http://127.0.0.1:%1/api/v1/files?workspaceId=ws-1").arg(port)));
        req.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-upload"));

        QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(nam.get(req));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        const auto respDoc = QJsonDocument::fromJson(reply->readAll());
        QVERIFY(respDoc.isArray());
        QCOMPARE(respDoc.array().size(), 1);
        QCOMPARE(respDoc.array()[0].toObject()[QStringLiteral("file_name")].toString(),
                 QStringLiteral("testfile.txt"));
    }
}

QTEST_MAIN(TestFileServiceHttpServerFolders)
#include "TestFileServiceHttpServerFolders.moc"
