#include <QtTest>
#include <QTemporaryDir>

#include "integrations/RemoteFileServiceContracts.h"
#include "integrations/RemoteFileServiceSettings.h"
#include "integrations/SharedFileResourceContracts.h"

// Test that RemoteFileInfo and RemoteFileVersion hold their assigned values
// correctly after construction (plain struct semantics — no parsing logic here),
// and that SharedFileResourceContracts::makePayload produces the expected payload
// when given a SharedFileResource built from adapter-returned data.

class TestRemoteFileServiceContracts : public QObject {
    Q_OBJECT

private slots:
    void remoteFileInfo_roundTripsFields();
    void remoteFileVersion_roundTripsFields();
    void settings_credentialConfiguration_requiresBaseUrlAndToken();
    void sharedFilePayload_fromRemoteFileInfo_hasExpectedActions();
    void groupConfig_savesAndLoadsPerGroup();
    void groupConfig_twoGroupsAreIsolated();
};

void TestRemoteFileServiceContracts::remoteFileInfo_roundTripsFields()
{
    RemoteFileInfo info;
    info.fileId         = QStringLiteral("fid-001");
    info.workspaceId    = QStringLiteral("ws-dev");
    info.fileName       = QStringLiteral("spec.pdf");
    info.currentVersion = QStringLiteral("vid-001");
    info.uploadedById   = QStringLiteral("user-a");
    info.uploadedByName = QStringLiteral("张明");
    info.createdAtMs    = 1000;
    info.updatedAtMs    = 2000;

    QCOMPARE(info.fileId,         QStringLiteral("fid-001"));
    QCOMPARE(info.workspaceId,    QStringLiteral("ws-dev"));
    QCOMPARE(info.fileName,       QStringLiteral("spec.pdf"));
    QCOMPARE(info.currentVersion, QStringLiteral("vid-001"));
    QCOMPARE(info.uploadedByName, QStringLiteral("张明"));
    QCOMPARE(info.createdAtMs,    qint64(1000));
    QCOMPARE(info.updatedAtMs,    qint64(2000));
}

void TestRemoteFileServiceContracts::remoteFileVersion_roundTripsFields()
{
    RemoteFileVersion ver;
    ver.versionId     = QStringLiteral("vid-002");
    ver.fileId        = QStringLiteral("fid-001");
    ver.versionNumber = 2;
    ver.versionLabel  = QStringLiteral("v2");
    ver.uploaderId    = QStringLiteral("user-b");
    ver.uploaderName  = QStringLiteral("李磊");
    ver.uploadedAtMs  = 5000;
    ver.fileSizeBytes = 8192;
    ver.changeNote    = QStringLiteral("修复格式问题");

    QCOMPARE(ver.versionId,     QStringLiteral("vid-002"));
    QCOMPARE(ver.versionNumber, 2);
    QCOMPARE(ver.versionLabel,  QStringLiteral("v2"));
    QCOMPARE(ver.uploaderName,  QStringLiteral("李磊"));
    QCOMPARE(ver.uploadedAtMs,  qint64(5000));
    QCOMPARE(ver.fileSizeBytes, qint64(8192));
    QCOMPARE(ver.changeNote,    QStringLiteral("修复格式问题"));
}

void TestRemoteFileServiceContracts::settings_credentialConfiguration_requiresBaseUrlAndToken()
{
    RemoteFileServiceConnectionSettings settings;
    QVERIFY(!settings.hasCredentialConfiguration());

    settings.baseUrl     = QStringLiteral("http://files.example.com");
    settings.bearerToken = QStringLiteral("tkn-123");
    settings.enabled     = true;
    QVERIFY(settings.hasCredentialConfiguration());
}

void TestRemoteFileServiceContracts::sharedFilePayload_fromRemoteFileInfo_hasExpectedActions()
{
    // Verifies that a SharedFileResource built from adapter data produces a valid
    // shared_file payload with download and open actions populated from URLs.
    const QString baseUrl  = QStringLiteral("http://files.example.com");
    const QString fileId   = QStringLiteral("fid-001");
    const QString fileName = QStringLiteral("report.pdf");

    const SharedFileResource resource{
        QStringLiteral("svc-files"),
        QStringLiteral("ws-dev"),
        fileId,
        fileName,
        QStringLiteral("张明"),
        QStringLiteral("v1"),
        QStringLiteral("群共享文件"),
        baseUrl + QStringLiteral("/api/v1/files/") + fileId + QStringLiteral("/download"),
        baseUrl + QStringLiteral("/api/v1/files/") + fileId + QStringLiteral("/versions"),
        4096,
    };

    const ResourceRefPayload payload = SharedFileResourceContracts::makePayload(resource);
    QCOMPARE(payload.kind,       QStringLiteral("shared_file"));
    QCOMPARE(payload.resourceId, fileId);
    QCOMPARE(payload.title,      fileName);
    QCOMPARE(payload.actions.size(), 2);

    bool hasDownload = false;
    bool hasOpen     = false;
    for (const auto& action : payload.actions) {
        if (action.actionId == QStringLiteral("download")) {
            hasDownload = true;
            QVERIFY(!action.target.isEmpty());
        }
        if (action.actionId == QStringLiteral("open")) {
            hasOpen = true;
            QVERIFY(!action.target.isEmpty());
        }
    }
    QVERIFY(hasDownload);
    QVERIFY(hasOpen);
}

void TestRemoteFileServiceContracts::groupConfig_savesAndLoadsPerGroup()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings s(dir.filePath(QStringLiteral("t.ini")), QSettings::IniFormat);

    GroupFileServiceConfig cfg;
    cfg.groupId     = QStringLiteral("grp-001");
    cfg.enabled     = true;
    cfg.baseUrl     = QStringLiteral("http://192.0.2.1:8765");
    cfg.bearerToken = QStringLiteral("tok-abc");
    cfg.workspaceId = QStringLiteral("ws-x");
    GroupFileServiceSettingsStore::save(cfg, &s);

    const auto loaded = GroupFileServiceSettingsStore::load(QStringLiteral("grp-001"), &s);
    QCOMPARE(loaded.enabled,     true);
    QCOMPARE(loaded.baseUrl,     QStringLiteral("http://192.0.2.1:8765"));
    QCOMPARE(loaded.bearerToken, QStringLiteral("tok-abc"));
    QCOMPARE(loaded.workspaceId, QStringLiteral("ws-x"));
    QCOMPARE(loaded.groupId,     QStringLiteral("grp-001"));
}

void TestRemoteFileServiceContracts::groupConfig_twoGroupsAreIsolated()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings s(dir.filePath(QStringLiteral("t.ini")), QSettings::IniFormat);

    GroupFileServiceConfig a;
    a.groupId = QStringLiteral("grp-A"); a.enabled = true;
    a.baseUrl = QStringLiteral("http://a"); a.bearerToken = QStringLiteral("ta");
    a.workspaceId = QStringLiteral("ws-a");
    GroupFileServiceSettingsStore::save(a, &s);

    GroupFileServiceConfig b;
    b.groupId = QStringLiteral("grp-B"); b.enabled = false;
    b.baseUrl = QStringLiteral("http://b"); b.bearerToken = QStringLiteral("tb");
    b.workspaceId = QStringLiteral("ws-b");
    GroupFileServiceSettingsStore::save(b, &s);

    const auto la = GroupFileServiceSettingsStore::load(QStringLiteral("grp-A"), &s);
    const auto lb = GroupFileServiceSettingsStore::load(QStringLiteral("grp-B"), &s);
    QCOMPARE(la.baseUrl, QStringLiteral("http://a"));
    QCOMPARE(lb.baseUrl, QStringLiteral("http://b"));
    QCOMPARE(la.enabled, true);
    QCOMPARE(lb.enabled, false);
}

QTEST_MAIN(TestRemoteFileServiceContracts)
#include "TestRemoteFileServiceContracts.moc"
