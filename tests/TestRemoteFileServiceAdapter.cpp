// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增287行/修改0行/删除0行; 总行数287行
// @AI-LastModified: 2026-04-13 20:50:11

#include <QtTest>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QTemporaryDir>
#include <QUrl>

#include "integrations/RemoteFileServiceAdapter.h"
#include "integrations/RemoteFileServiceContracts.h"
#include "integrations/RemoteFileServiceSettings.h"

namespace {

// Fake transport for testing — returns pre-canned documents keyed by URL or "PUT/POST url"
class FakeRemoteFileServiceTransport final : public IRemoteFileServiceTransport {
public:
    QHash<QString, QJsonDocument> jsonByUrl;
    QHash<QString, QByteArray>    bytesByUrl;
    mutable int putBytesCalls = 0;
    mutable int putDeviceCalls = 0;
    mutable int postBytesCalls = 0;
    mutable int postDeviceCalls = 0;
    mutable QByteArray lastUploadedPayload;

    std::optional<QJsonDocument> getJson(const QUrl& url,
                                          const RemoteFileServiceConnectionSettings&,
                                          QString* errorMessage) const override
    {
        const auto it = jsonByUrl.constFind(url.toString());
        if (it == jsonByUrl.cend()) {
            if (errorMessage) *errorMessage = QStringLiteral("no fake json for: ") + url.toString();
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QByteArray> getBytes(const QUrl& url,
                                        const RemoteFileServiceConnectionSettings&,
                                        QString* errorMessage) const override
    {
        const auto it = bytesByUrl.constFind(url.toString());
        if (it == bytesByUrl.cend()) {
            if (errorMessage) *errorMessage = QStringLiteral("no fake bytes for: ") + url.toString();
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QJsonDocument> putBytes(const QUrl& url,
                                           const QByteArray&,
                                           const QMap<QByteArray, QByteArray>&,
                                           const RemoteFileServiceConnectionSettings&,
                                           QString* errorMessage) const override
    {
        ++putBytesCalls;
        const QString key = QStringLiteral("PUT ") + url.toString();
        const auto it = jsonByUrl.constFind(key);
        if (it == jsonByUrl.cend()) {
            if (errorMessage) *errorMessage = QStringLiteral("no fake put-json for: ") + url.toString();
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QJsonDocument> putDevice(const QUrl& url,
                                            QIODevice* device,
                                            const QMap<QByteArray, QByteArray>&,
                                            const RemoteFileServiceConnectionSettings&,
                                            QString* errorMessage) const override
    {
        ++putDeviceCalls;
        lastUploadedPayload = device ? device->readAll() : QByteArray();
        const QString key = QStringLiteral("PUT ") + url.toString();
        const auto it = jsonByUrl.constFind(key);
        if (it == jsonByUrl.cend()) {
            if (errorMessage) *errorMessage = QStringLiteral("no fake put-json for: ") + url.toString();
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QJsonDocument> postBytes(const QUrl& url,
                                            const QByteArray&,
                                            const QMap<QByteArray, QByteArray>&,
                                            const RemoteFileServiceConnectionSettings&,
                                            QString* errorMessage) const override
    {
        ++postBytesCalls;
        const QString key = QStringLiteral("POST ") + url.toString();
        const auto it = jsonByUrl.constFind(key);
        if (it == jsonByUrl.cend()) {
            if (errorMessage) *errorMessage = QStringLiteral("no fake post-json for: ") + url.toString();
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QJsonDocument> postDevice(const QUrl& url,
                                             QIODevice* device,
                                             const QMap<QByteArray, QByteArray>&,
                                             const RemoteFileServiceConnectionSettings&,
                                             QString* errorMessage) const override
    {
        ++postDeviceCalls;
        lastUploadedPayload = device ? device->readAll() : QByteArray();
        const QString key = QStringLiteral("POST ") + url.toString();
        const auto it = jsonByUrl.constFind(key);
        if (it == jsonByUrl.cend()) {
            if (errorMessage) *errorMessage = QStringLiteral("no fake post-json for: ") + url.toString();
            return std::nullopt;
        }
        return it.value();
    }
};

RemoteFileServiceConnectionSettings configuredSettings()
{
    RemoteFileServiceConnectionSettings s;
    s.enabled             = true;
    s.baseUrl             = QStringLiteral("http://files.localhost:8765");
    s.bearerToken         = QStringLiteral("test-token-123");
    s.defaultWorkspaceId  = QStringLiteral("ws-default");
    return s;
}

}  // namespace

class TestRemoteFileServiceAdapter : public QObject {
    Q_OBJECT

private slots:
    void listFiles_parsesFileArray();
    void listFiles_returnsEmptyOnTransportFailure();
    void getVersionHistory_parsesVersionArray();
    void uploadFile_returnsFileIdAndVersionId();
    void uploadFile_failsWithoutCredentials();
    void downloadByUrl_writesContentToDisk();
    void testConnection_succeedsOnPingOk();
    void testConnection_failsOnTransportError();
};

void TestRemoteFileServiceAdapter::listFiles_parsesFileArray()
{
    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();
    transport->jsonByUrl.insert(
        QStringLiteral("http://files.localhost:8765/api/v1/files?workspaceId=ws-dev"),
        QJsonDocument(QJsonArray{
            QJsonObject{
                {QStringLiteral("file_id"),          QStringLiteral("fid-001")},
                {QStringLiteral("workspace_id"),     QStringLiteral("ws-dev")},
                {QStringLiteral("file_name"),         QStringLiteral("report.pdf")},
                {QStringLiteral("current_version"),   QStringLiteral("vid-001")},
                {QStringLiteral("uploaded_by_id"),    QStringLiteral("user-a")},
                {QStringLiteral("uploaded_by_name"),  QStringLiteral("张明")},
                {QStringLiteral("created_at_ms"),     1000},
                {QStringLiteral("updated_at_ms"),     2000},
            }
        }));

    RemoteFileServiceAdapter adapter(configuredSettings(), transport);
    QString err;
    const QVector<RemoteFileInfo> files = adapter.listFiles(QStringLiteral("ws-dev"), &err);

    QVERIFY(err.isEmpty());
    QCOMPARE(files.size(), 1);
    QCOMPARE(files[0].fileId,        QStringLiteral("fid-001"));
    QCOMPARE(files[0].workspaceId,   QStringLiteral("ws-dev"));
    QCOMPARE(files[0].fileName,      QStringLiteral("report.pdf"));
    QCOMPARE(files[0].uploadedByName, QStringLiteral("张明"));
    QCOMPARE(files[0].createdAtMs,   qint64(1000));
    QCOMPARE(files[0].updatedAtMs,   qint64(2000));
}

void TestRemoteFileServiceAdapter::listFiles_returnsEmptyOnTransportFailure()
{
    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();
    // No entries registered → transport will return nullopt

    RemoteFileServiceAdapter adapter(configuredSettings(), transport);
    QString err;
    const QVector<RemoteFileInfo> files = adapter.listFiles(QStringLiteral("ws-dev"), &err);

    QVERIFY(files.isEmpty());
    QVERIFY(!err.isEmpty());
}

void TestRemoteFileServiceAdapter::getVersionHistory_parsesVersionArray()
{
    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();
    transport->jsonByUrl.insert(
        QStringLiteral("http://files.localhost:8765/api/v1/files/fid-001/versions"),
        QJsonDocument(QJsonArray{
            QJsonObject{
                {QStringLiteral("version_id"),      QStringLiteral("vid-001")},
                {QStringLiteral("file_id"),          QStringLiteral("fid-001")},
                {QStringLiteral("version_number"),   1},
                {QStringLiteral("version_label"),    QStringLiteral("v1")},
                {QStringLiteral("uploader_id"),      QStringLiteral("user-a")},
                {QStringLiteral("uploader_name"),    QStringLiteral("张明")},
                {QStringLiteral("uploaded_at_ms"),   3000},
                {QStringLiteral("file_size_bytes"),  4096},
                {QStringLiteral("change_note"),      QStringLiteral("初始版本")},
            }
        }));

    RemoteFileServiceAdapter adapter(configuredSettings(), transport);
    QString err;
    const QVector<RemoteFileVersion> versions =
        adapter.getVersionHistory(QStringLiteral("fid-001"), &err);

    QVERIFY(err.isEmpty());
    QCOMPARE(versions.size(), 1);
    QCOMPARE(versions[0].versionId,    QStringLiteral("vid-001"));
    QCOMPARE(versions[0].versionLabel, QStringLiteral("v1"));
    QCOMPARE(versions[0].versionNumber, 1);
    QCOMPARE(versions[0].uploaderName, QStringLiteral("张明"));
    QCOMPARE(versions[0].uploadedAtMs, qint64(3000));
    QCOMPARE(versions[0].fileSizeBytes, qint64(4096));
    QCOMPARE(versions[0].changeNote,   QStringLiteral("初始版本"));
}

void TestRemoteFileServiceAdapter::uploadFile_returnsFileIdAndVersionId()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    // Write a small temp file to upload
    const QString localPath = dir.filePath(QStringLiteral("upload.txt"));
    {
        QFile f(localPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello world");
    }

    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();
    transport->jsonByUrl.insert(
        QStringLiteral("PUT http://files.localhost:8765/api/v1/files/upload.txt"),
        QJsonDocument(QJsonObject{
            {QStringLiteral("file_id"),    QStringLiteral("fid-new")},
            {QStringLiteral("version_id"), QStringLiteral("vid-new")},
        }));

    RemoteFileServiceAdapter adapter(configuredSettings(), transport);
    QString err;
    const auto result = adapter.uploadFile(
        QStringLiteral("ws-dev"), localPath, QStringLiteral("初始上传"),
        QStringLiteral("测试用户"), QString(), &err);

    QVERIFY(err.isEmpty());
    QVERIFY(result.has_value());
    QCOMPARE(result->fileId,    QStringLiteral("fid-new"));
    QCOMPARE(result->versionId, QStringLiteral("vid-new"));
    QCOMPARE(transport->putBytesCalls, 0);
    QCOMPARE(transport->putDeviceCalls, 1);
    QCOMPARE(transport->lastUploadedPayload, QByteArray("hello world"));
}

void TestRemoteFileServiceAdapter::uploadFile_failsWithoutCredentials()
{
    RemoteFileServiceConnectionSettings empty;
    empty.enabled = false;
    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();

    RemoteFileServiceAdapter adapter(empty, transport);
    QString err;
    const auto result = adapter.uploadFile(
        QStringLiteral("ws-dev"), QStringLiteral("/tmp/file.txt"),
        QString{}, QString{}, QString(), &err);

    QVERIFY(!result.has_value());
    QVERIFY(!err.isEmpty());
}

void TestRemoteFileServiceAdapter::downloadByUrl_writesContentToDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QByteArray content = "PDF content goes here";
    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();
    transport->bytesByUrl.insert(
        QStringLiteral("http://files.localhost:8765/api/v1/files/fid-001/download"),
        content);

    RemoteFileServiceAdapter adapter(configuredSettings(), transport);
    QString err;
    const auto savedPath = adapter.downloadByUrl(
        QStringLiteral("http://files.localhost:8765/api/v1/files/fid-001/download"),
        QStringLiteral("result.pdf"),
        dir.path(),
        &err);

    QVERIFY(err.isEmpty());
    QVERIFY(savedPath.has_value());

    QFile savedFile(*savedPath);
    QVERIFY(savedFile.open(QIODevice::ReadOnly));
    QCOMPARE(savedFile.readAll(), content);
}

void TestRemoteFileServiceAdapter::testConnection_succeedsOnPingOk()
{
    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();
    transport->jsonByUrl.insert(
        QStringLiteral("http://files.localhost:8765/api/v1/ping"),
        QJsonDocument(QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")}}));

    RemoteFileServiceAdapter adapter(configuredSettings(), transport);
    QString err;
    QVERIFY(adapter.testConnection(&err));
    QVERIFY(err.isEmpty());
}

void TestRemoteFileServiceAdapter::testConnection_failsOnTransportError()
{
    auto transport = std::make_shared<FakeRemoteFileServiceTransport>();
    // No ping endpoint registered → transport returns nullopt

    RemoteFileServiceAdapter adapter(configuredSettings(), transport);
    QString err;
    QVERIFY(!adapter.testConnection(&err));
    QVERIFY(!err.isEmpty());
}

QTEST_MAIN(TestRemoteFileServiceAdapter)
#include "TestRemoteFileServiceAdapter.moc"
