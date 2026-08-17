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
    return QStringLiteral("test-wopi-")
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
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
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

struct TestEnv {
    QTemporaryDir dir;
    QString conn;
    FileServiceDatabase* db;
    FileStorageManager* storage;
    FileServiceAuth* auth;
    FileServiceHttpServer* server;
    quint16 port;

    TestEnv()
        : conn(uniqueConn())
        , db(nullptr), storage(nullptr), auth(nullptr), server(nullptr), port(0)
    {}

    bool setup()
    {
        if (!dir.isValid()) return false;
        db = new FileServiceDatabase(dir.filePath(QStringLiteral("service.db")), conn);
        if (!db->open()) return false;
        storage = new FileStorageManager(dir.filePath(QStringLiteral("storage")));
        auth = new FileServiceAuth(db);
        if (!auth->seedOrUpdateTokenScope(
                QStringLiteral("tok-1"), QStringLiteral("client-alice"),
                QStringLiteral("Alice"), jsonScope({"ws-1"})))
            return false;
        server = new FileServiceHttpServer(db, storage, auth);
        port = reserveFreePort();
        if (port == 0) return false;
        return server->listen(QHostAddress::LocalHost, port);
    }

    ~TestEnv()
    {
        delete server;
        delete auth;
        delete storage;
        delete db;
    }
};
}

class TestWopiTokens : public QObject {
    Q_OBJECT

private slots:
    void test_createWopiToken_success();
    void test_createWopiToken_missingFileId();
    void test_createWopiToken_legacyAccessAllowsOldClients();
    void test_createWopiToken_unauthorizedWhenLegacyDisabled();
    void test_createWopiToken_forbiddenWorkspace();
    void test_renewWopiToken_success();
    void test_renewWopiToken_expired();
};

void TestWopiTokens::test_createWopiToken_success()
{
    TestEnv env;
    QVERIFY(env.setup());
    seedExistingFile(*env.db, *env.storage,
                     QStringLiteral("file-001"), QStringLiteral("ws-1"),
                     QStringLiteral("doc.docx"), QStringLiteral("ver-1"),
                     QByteArrayLiteral("hello"));

    QJsonObject body;
    body[QStringLiteral("fileId")] = QStringLiteral("file-001");
    body[QStringLiteral("clientId")] = QStringLiteral("client-alice");

    QNetworkAccessManager nam;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens").arg(env.port)));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-1"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
        nam.post(request, QJsonDocument(body).toJson()));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const auto respDoc = QJsonDocument::fromJson(reply->readAll());
    QVERIFY(respDoc.isObject());
    const auto respObj = respDoc.object();
    QVERIFY(!respObj[QStringLiteral("access_token")].toString().isEmpty());
    QCOMPARE(respObj[QStringLiteral("expires_in")].toInt(), 3600);
}

void TestWopiTokens::test_createWopiToken_missingFileId()
{
    TestEnv env;
    QVERIFY(env.setup());

    QJsonObject body;
    // no fileId

    QNetworkAccessManager nam;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens").arg(env.port)));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-1"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
        nam.post(request, QJsonDocument(body).toJson()));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 400);
}

void TestWopiTokens::test_createWopiToken_legacyAccessAllowsOldClients()
{
    TestEnv env;
    QVERIFY(env.setup());
    seedExistingFile(*env.db, *env.storage,
                     QStringLiteral("file-legacy"), QStringLiteral("ws-1"),
                     QStringLiteral("legacy.docx"), QStringLiteral("ver-1"),
                     QByteArrayLiteral("legacy"));

    QJsonObject body;
    body[QStringLiteral("fileId")] = QStringLiteral("file-legacy");
    body[QStringLiteral("displayName")] = QStringLiteral("Legacy Client");

    QNetworkAccessManager nam;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens").arg(env.port)));
    // no Authorization header
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
        nam.post(request, QJsonDocument(body).toJson()));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const auto respObj = QJsonDocument::fromJson(reply->readAll()).object();
    QVERIFY(!respObj[QStringLiteral("access_token")].toString().isEmpty());
    QCOMPARE(respObj[QStringLiteral("expires_in")].toInt(), 3600);
}

void TestWopiTokens::test_createWopiToken_unauthorizedWhenLegacyDisabled()
{
    TestEnv env;
    QVERIFY(env.setup());
    env.server->setLegacyFileAccessEnabled(false);

    QJsonObject body;
    body[QStringLiteral("fileId")] = QStringLiteral("file-001");

    QNetworkAccessManager nam;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens").arg(env.port)));
    // no Authorization header
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
        nam.post(request, QJsonDocument(body).toJson()));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
}

void TestWopiTokens::test_createWopiToken_forbiddenWorkspace()
{
    TestEnv env;
    QVERIFY(env.setup());

    // 文件在 ws-other，但 tok-1 只能访问 ws-1
    seedExistingFile(*env.db, *env.storage,
                     QStringLiteral("file-forbidden"), QStringLiteral("ws-other"),
                     QStringLiteral("secret.xlsx"), QStringLiteral("ver-1"),
                     QByteArrayLiteral("secret"));

    QJsonObject body;
    body[QStringLiteral("fileId")] = QStringLiteral("file-forbidden");

    QNetworkAccessManager nam;
    QNetworkRequest request(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens").arg(env.port)));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-1"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> reply(
        nam.post(request, QJsonDocument(body).toJson()));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);
}

void TestWopiTokens::test_renewWopiToken_success()
{
    TestEnv env;
    QVERIFY(env.setup());
    seedExistingFile(*env.db, *env.storage,
                     QStringLiteral("file-001"), QStringLiteral("ws-1"),
                     QStringLiteral("doc.docx"), QStringLiteral("ver-1"),
                     QByteArrayLiteral("hello"));

    // Step 1: create a token
    QJsonObject createBody;
    createBody[QStringLiteral("fileId")] = QStringLiteral("file-001");
    createBody[QStringLiteral("clientId")] = QStringLiteral("client-alice");

    QNetworkAccessManager nam;
    QNetworkRequest createReq(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens").arg(env.port)));
    createReq.setRawHeader("Authorization", QByteArrayLiteral("Bearer tok-1"));
    createReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> createReply(
        nam.post(createReq, QJsonDocument(createBody).toJson()));
    QSignalSpy createFinished(createReply.data(), &QNetworkReply::finished);
    QVERIFY(createFinished.wait(5000));
    QCOMPARE(createReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const auto createResp = QJsonDocument::fromJson(createReply->readAll()).object();
    const QString accessToken = createResp[QStringLiteral("access_token")].toString();
    QVERIFY(!accessToken.isEmpty());

    // Step 2: renew the token
    QJsonObject renewBody;
    renewBody[QStringLiteral("access_token")] = accessToken;

    QNetworkRequest renewReq(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens/renew").arg(env.port)));
    renewReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> renewReply(
        nam.post(renewReq, QJsonDocument(renewBody).toJson()));
    QSignalSpy renewFinished(renewReply.data(), &QNetworkReply::finished);
    QVERIFY(renewFinished.wait(5000));

    QCOMPARE(renewReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const auto renewResp = QJsonDocument::fromJson(renewReply->readAll()).object();
    QCOMPARE(renewResp[QStringLiteral("expires_in")].toInt(), 3600);
}

void TestWopiTokens::test_renewWopiToken_expired()
{
    TestEnv env;
    QVERIFY(env.setup());

    // Insert a token that is already expired directly into DB
    WopiTokenRecord record;
    record.token = QStringLiteral("expired-token-001");
    record.fileId = QStringLiteral("file-001");
    record.clientId = QStringLiteral("client-alice");
    record.displayName = QStringLiteral("Alice");
    record.role = QStringLiteral("editor");
    record.createdAtMs = 1000;
    record.expiresAtMs = 2000; // expired long ago
    QVERIFY(env.db->insertWopiToken(record));

    QJsonObject renewBody;
    renewBody[QStringLiteral("access_token")] = QStringLiteral("expired-token-001");

    QNetworkAccessManager nam;
    QNetworkRequest renewReq(
        QUrl(QStringLiteral("http://127.0.0.1:%1/api/v1/wopi-tokens/renew").arg(env.port)));
    renewReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> renewReply(
        nam.post(renewReq, QJsonDocument(renewBody).toJson()));
    QSignalSpy renewFinished(renewReply.data(), &QNetworkReply::finished);
    QVERIFY(renewFinished.wait(5000));

    QCOMPARE(renewReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
}

QTEST_MAIN(TestWopiTokens)
#include "TestWopiTokens.moc"
