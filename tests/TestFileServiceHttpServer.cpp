#include <QtTest>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUuid>

#include "FileServiceAuth.h"
#include "FileServiceDatabase.h"
#include "FileServiceHttpServer.h"
#include "FileStorageManager.h"

namespace {
QString uniqueConn()
{
    return QStringLiteral("test-http-")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString jsonScope(std::initializer_list<const char*> workspaces)
{
    QJsonArray array;
    for (const char* workspace : workspaces) {
        array.append(QString::fromLatin1(workspace));
    }
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

quint16 reserveFreePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

void seedExistingFile(FileServiceDatabase& db,
                      FileStorageManager& storage,
                      const QString& fileId,
                      const QString& workspaceId,
                      const QString& fileName,
                      const QString& versionId,
                      const QByteArray& payload)
{
    const auto storagePath = storage.saveFile(fileId, versionId, payload);
    QVERIFY(storagePath.has_value());

    FileRecord file;
    file.fileId = fileId;
    file.workspaceId = workspaceId;
    file.fileName = fileName;
    file.currentVersion = versionId;
    file.uploadedById = QStringLiteral("seed-user");
    file.uploadedByName = QStringLiteral("Seed User");
    file.createdAtMs = 1;
    file.updatedAtMs = 1;
    QVERIFY(db.insertFile(file));

    FileVersionRecord version;
    version.versionId = versionId;
    version.fileId = fileId;
    version.versionNumber = 1;
    version.versionLabel = QStringLiteral("v1");
    version.uploaderId = QStringLiteral("seed-user");
    version.uploaderName = QStringLiteral("Seed User");
    version.uploadedAtMs = 1;
    version.fileSize = payload.size();
    version.storagePath = *storagePath;
    version.changeNote = QStringLiteral("seed");
    QVERIFY(db.insertVersion(version));
}

QScopedPointer<QNetworkReply> postVersionRequest(
    QNetworkAccessManager& nam,
    quint16 port,
    const QString& fileId,
    const QByteArray& body)
{
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/files/%2/versions")
                 .arg(port)
                 .arg(fileId)));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-1"));
    request.setRawHeader("X-Uploader-Name", QByteArrayLiteral("Uploader"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/octet-stream"));
    return QScopedPointer<QNetworkReply>(nam.post(request, body));
}
}

class TestFileServiceHttpServer : public QObject {
    Q_OBJECT

private slots:
    void legacyFileAccess_allowsOldClientsWithoutValidBearer();
    void legacyFileAccess_canBeDisabled();
    void postVersion_rollbackDeletesBlobWhenInsertFails();
    void postVersion_rollbackDeletesBlobWhenCurrentVersionUpdateFails();
};

void TestFileServiceHttpServer::legacyFileAccess_allowsOldClientsWithoutValidBearer()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    seedExistingFile(db, storage,
                     QStringLiteral("legacy-file-1"),
                     QStringLiteral("legacy-group"),
                     QStringLiteral("legacy.docx"),
                     QStringLiteral("legacy-version-1"),
                     QByteArrayLiteral("legacy payload"));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    QNetworkAccessManager nam;

    const auto requestUrl = QUrl(
        QStringLiteral("http://127.0.0.1:%1/api/v1/files?workspaceId=legacy-group")
            .arg(port));

    {
        QNetworkRequest request(requestUrl);
        QScopedPointer<QNetworkReply> reply(nam.get(request));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
        const auto files = QJsonDocument::fromJson(reply->readAll()).array();
        QCOMPARE(files.size(), 1);
        QCOMPARE(files.at(0).toObject().value(QStringLiteral("file_name")).toString(),
                 QStringLiteral("legacy.docx"));
    }

    {
        QNetworkRequest request(requestUrl);
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer stale-old-client-token"));
        QScopedPointer<QNetworkReply> reply(nam.get(request));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));

        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    }
}

void TestFileServiceHttpServer::legacyFileAccess_canBeDisabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString conn = uniqueConn();
    FileServiceDatabase db(dir.filePath(QStringLiteral("service.db")), conn);
    QVERIFY(db.open());
    FileStorageManager storage(dir.filePath(QStringLiteral("storage")));
    FileServiceAuth auth(&db);

    FileServiceHttpServer server(&db, &storage, &auth);
    server.setLegacyFileAccessEnabled(false);

    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    QNetworkAccessManager nam;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/files?workspaceId=legacy-group")
                 .arg(port)));
    QScopedPointer<QNetworkReply> reply(nam.get(request));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
}

void TestFileServiceHttpServer::postVersion_rollbackDeletesBlobWhenInsertFails()
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
        QStringLiteral("Admin"), jsonScope({"ws-1"})));

    seedExistingFile(db, storage,
                     QStringLiteral("file-1"),
                     QStringLiteral("ws-1"),
                     QStringLiteral("spec.docx"),
                     QStringLiteral("ver-1"),
                     QByteArrayLiteral("seed"));

    QSqlQuery trigger(QSqlDatabase::database(conn));
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_version_insert BEFORE INSERT ON file_versions "
        "WHEN NEW.file_id = 'file-1' AND NEW.version_number = 2 "
        "BEGIN SELECT RAISE(FAIL, 'forced version insert failure'); END")));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    QNetworkAccessManager nam;
    auto reply = postVersionRequest(nam, port, QStringLiteral("file-1"),
                                    QByteArrayLiteral("new payload"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 500);

    const auto versions = db.listVersionsByFile(QStringLiteral("file-1"));
    QCOMPARE(versions.size(), 1);
    QCOMPARE(versions.front().versionId, QStringLiteral("ver-1"));
    QVERIFY(db.findFileById(QStringLiteral("file-1")).has_value());
    QCOMPARE(db.findFileById(QStringLiteral("file-1"))->currentVersion, QStringLiteral("ver-1"));

    const QDir fileDir(dir.filePath(QStringLiteral("storage/file-1")));
    const QStringList blobs = fileDir.entryList(QStringList() << QStringLiteral("*.bin"),
                                                QDir::Files);
    QCOMPARE(blobs.size(), 1);
}

void TestFileServiceHttpServer::postVersion_rollbackDeletesBlobWhenCurrentVersionUpdateFails()
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
        QStringLiteral("Admin"), jsonScope({"ws-1"})));

    seedExistingFile(db, storage,
                     QStringLiteral("file-2"),
                     QStringLiteral("ws-1"),
                     QStringLiteral("plan.docx"),
                     QStringLiteral("ver-1"),
                     QByteArrayLiteral("seed"));

    QSqlQuery trigger(QSqlDatabase::database(conn));
    QVERIFY(trigger.exec(QStringLiteral(
        "CREATE TRIGGER fail_current_version_update "
        "BEFORE UPDATE OF current_version ON files "
        "WHEN OLD.file_id = 'file-2' "
        "BEGIN SELECT RAISE(FAIL, 'forced current version update failure'); END")));

    FileServiceHttpServer server(&db, &storage, &auth);
    const quint16 port = reserveFreePort();
    QVERIFY(port != 0);
    QVERIFY(server.listen(QHostAddress::LocalHost, port));

    QNetworkAccessManager nam;
    auto reply = postVersionRequest(nam, port, QStringLiteral("file-2"),
                                    QByteArrayLiteral("new payload"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 500);

    const auto versions = db.listVersionsByFile(QStringLiteral("file-2"));
    QCOMPARE(versions.size(), 1);
    QCOMPARE(versions.front().versionId, QStringLiteral("ver-1"));
    QVERIFY(db.findFileById(QStringLiteral("file-2")).has_value());
    QCOMPARE(db.findFileById(QStringLiteral("file-2"))->currentVersion, QStringLiteral("ver-1"));

    const QDir fileDir(dir.filePath(QStringLiteral("storage/file-2")));
    const QStringList blobs = fileDir.entryList(QStringList() << QStringLiteral("*.bin"),
                                                QDir::Files);
    QCOMPARE(blobs.size(), 1);
}

QTEST_MAIN(TestFileServiceHttpServer)
#include "TestFileServiceHttpServer.moc"
