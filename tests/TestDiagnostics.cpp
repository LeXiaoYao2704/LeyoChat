// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增103行/修改0行/删除0行; 总行数325行
// @AI-LastModified: 2026-04-16 09:11:52

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

#include "architecture/RuntimeArchitectureSnapshot.h"
#include "diagnostics/Diagnostics.h"

class TestDiagnostics : public QObject {
    Q_OBJECT

private slots:
    void exportBundle_copiesDatabaseLogsAndManifest();
    void exportBundle_copiesOpenSqliteDatabase();
    void exportBundle_preservesDataFromWalDatabase();
    void exportBundle_includesRuntimeStageTwoSnapshotWhenPresent();
    void exportBundle_includesIntegrationSnapshotWhenPresent();
};

void TestDiagnostics::exportBundle_copiesDatabaseLogsAndManifest()
{
    QTemporaryDir sandbox;
    QVERIFY2(sandbox.isValid(), "temporary sandbox should be created");

    const QString appDataDir = sandbox.filePath(QStringLiteral("appdata"));
    const QString appLocalDir = sandbox.filePath(QStringLiteral("applocal"));
    const QString exportRoot = sandbox.filePath(QStringLiteral("exports"));
    QVERIFY(QDir().mkpath(appDataDir));
    QVERIFY(QDir().mkpath(appLocalDir));
    QVERIFY(QDir().mkpath(exportRoot));

    Diagnostics::BundleSourcePaths source;
    source.appDataDir = appDataDir;
    source.appLocalDataDir = appLocalDir;
    source.databasePath = QDir(appDataDir).filePath(QStringLiteral("leyochat.db"));
    source.logsDir = QDir(appLocalDir).filePath(QStringLiteral("logs"));
    source.crashDir = QDir(appLocalDir).filePath(QStringLiteral("crash"));
    source.screenshotsDir = QDir(appLocalDir).filePath(QStringLiteral("screenshots"));
    source.runtimeDir = QDir(appLocalDir).filePath(QStringLiteral("runtime"));

    QVERIFY(QDir().mkpath(source.logsDir));
    QVERIFY(QDir().mkpath(source.crashDir));
    QVERIFY(QDir().mkpath(source.screenshotsDir));

    {
        QFile database(source.databasePath);
        QVERIFY(database.open(QIODevice::WriteOnly));
        database.write("db");
    }
    {
        QFile logFile(QDir(source.logsDir).filePath(QStringLiteral("runtime.log")));
        QVERIFY(logFile.open(QIODevice::WriteOnly));
        logFile.write("log-entry");
    }
    {
        QFile crashFile(QDir(source.crashDir).filePath(QStringLiteral("latest-crash.txt")));
        QVERIFY(crashFile.open(QIODevice::WriteOnly));
        crashFile.write("crash-entry");
    }
    {
        QFile screenshotFile(QDir(source.screenshotsDir).filePath(QStringLiteral("shot.png")));
        QVERIFY(screenshotFile.open(QIODevice::WriteOnly));
        screenshotFile.write("image-data");
    }

    QString exportedDir;
    QString error;
    QVERIFY2(Diagnostics::exportBundle(source, exportRoot, &exportedDir, &error),
             qPrintable(error));
    QVERIFY2(!exportedDir.isEmpty(), "exported directory should be returned");

    const QDir bundleDir(exportedDir);
    QVERIFY(bundleDir.exists(QStringLiteral("leyochat.db")));
    QVERIFY(bundleDir.exists(QStringLiteral("logs/runtime.log")));
    QVERIFY(bundleDir.exists(QStringLiteral("crash/latest-crash.txt")));
    QVERIFY(bundleDir.exists(QStringLiteral("screenshots/shot.png")));
    QVERIFY(bundleDir.exists(QStringLiteral("environment.json")));

    QFile manifest(bundleDir.filePath(QStringLiteral("environment.json")));
    QVERIFY(manifest.open(QIODevice::ReadOnly));
    const auto document = QJsonDocument::fromJson(manifest.readAll());
    QVERIFY(document.isObject());
    const QJsonObject object = document.object();
    QCOMPARE(object.value(QStringLiteral("appDisplayName")).toString(),
             QStringLiteral("LeyoChat"));
    QCOMPARE(object.value(QStringLiteral("databasePresent")).toBool(), true);
}

void TestDiagnostics::exportBundle_copiesOpenSqliteDatabase()
{
    QTemporaryDir sandbox;
    QVERIFY2(sandbox.isValid(), "temporary sandbox should be created");

    const QString appDataDir = sandbox.filePath(QStringLiteral("appdata"));
    const QString appLocalDir = sandbox.filePath(QStringLiteral("applocal"));
    const QString exportRoot = sandbox.filePath(QStringLiteral("exports"));
    QVERIFY(QDir().mkpath(appDataDir));
    QVERIFY(QDir().mkpath(appLocalDir));
    QVERIFY(QDir().mkpath(exportRoot));

    Diagnostics::BundleSourcePaths source;
    source.appDataDir = appDataDir;
    source.appLocalDataDir = appLocalDir;
    source.databasePath = QDir(appDataDir).filePath(QStringLiteral("leyochat.db"));
    source.logsDir = QDir(appLocalDir).filePath(QStringLiteral("logs"));
    source.crashDir = QDir(appLocalDir).filePath(QStringLiteral("crash"));
    source.screenshotsDir = QDir(appLocalDir).filePath(QStringLiteral("screenshots"));
    source.runtimeDir = QDir(appLocalDir).filePath(QStringLiteral("runtime"));

    QVERIFY(QDir().mkpath(source.logsDir));
    QVERIFY(QDir().mkpath(source.crashDir));
    QVERIFY(QDir().mkpath(source.screenshotsDir));

    const QString connectionName = QStringLiteral("diagnostics-open-db-test");
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        database.setDatabaseName(source.databasePath);
        QVERIFY2(database.open(), qPrintable(database.lastError().text()));

        QSqlQuery query(database);
        QVERIFY2(query.exec(QStringLiteral("CREATE TABLE test_data (id INTEGER PRIMARY KEY, value TEXT)")),
                 qPrintable(query.lastError().text()));
        QVERIFY2(query.exec(QStringLiteral("INSERT INTO test_data(value) VALUES ('open-db')")),
                 qPrintable(query.lastError().text()));

        QString exportedDir;
        QString error;
        QVERIFY2(Diagnostics::exportBundle(source, exportRoot, &exportedDir, &error),
                 qPrintable(error));
        QVERIFY2(!exportedDir.isEmpty(), "exported directory should be returned");
        QVERIFY(QFile::exists(QDir(exportedDir).filePath(QStringLiteral("leyochat.db"))));

        database.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void TestDiagnostics::exportBundle_preservesDataFromWalDatabase()
{
    QTemporaryDir sandbox;
    QVERIFY2(sandbox.isValid(), "temporary sandbox should be created");

    const QString appDataDir = sandbox.filePath(QStringLiteral("appdata"));
    const QString appLocalDir = sandbox.filePath(QStringLiteral("applocal"));
    const QString exportRoot = sandbox.filePath(QStringLiteral("exports"));
    QVERIFY(QDir().mkpath(appDataDir));
    QVERIFY(QDir().mkpath(appLocalDir));
    QVERIFY(QDir().mkpath(exportRoot));

    Diagnostics::BundleSourcePaths source;
    source.appDataDir = appDataDir;
    source.appLocalDataDir = appLocalDir;
    source.databasePath = QDir(appDataDir).filePath(QStringLiteral("leyochat.db"));
    source.logsDir = QDir(appLocalDir).filePath(QStringLiteral("logs"));
    source.crashDir = QDir(appLocalDir).filePath(QStringLiteral("crash"));
    source.screenshotsDir = QDir(appLocalDir).filePath(QStringLiteral("screenshots"));
    source.runtimeDir = QDir(appLocalDir).filePath(QStringLiteral("runtime"));

    QVERIFY(QDir().mkpath(source.logsDir));
    QVERIFY(QDir().mkpath(source.crashDir));
    QVERIFY(QDir().mkpath(source.screenshotsDir));

    const QString sourceConnectionName = QStringLiteral("diagnostics-wal-source-db");
    QString exportedDir;
    {
        QSqlDatabase sourceDatabase = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), sourceConnectionName);
        sourceDatabase.setDatabaseName(source.databasePath);
        QVERIFY2(sourceDatabase.open(), qPrintable(sourceDatabase.lastError().text()));

        QSqlQuery sourceQuery(sourceDatabase);
        QVERIFY2(sourceQuery.exec(QStringLiteral("PRAGMA journal_mode=WAL")),
                 qPrintable(sourceQuery.lastError().text()));
        QVERIFY2(sourceQuery.exec(QStringLiteral("CREATE TABLE test_data (id INTEGER PRIMARY KEY, value TEXT)")),
                 qPrintable(sourceQuery.lastError().text()));
        QVERIFY2(sourceQuery.exec(QStringLiteral("INSERT INTO test_data(value) VALUES ('wal-row')")),
                 qPrintable(sourceQuery.lastError().text()));

        QString error;
        QVERIFY2(Diagnostics::exportBundle(source, exportRoot, &exportedDir, &error),
                 qPrintable(error));
        QVERIFY2(!exportedDir.isEmpty(), "exported directory should be returned");

        sourceDatabase.close();
    }
    QSqlDatabase::removeDatabase(sourceConnectionName);

    const QString exportedDbPath = QDir(exportedDir).filePath(QStringLiteral("leyochat.db"));
    QVERIFY(QFile::exists(exportedDbPath));

    const QString verifyConnectionName = QStringLiteral("diagnostics-wal-exported-db");
    {
        QSqlDatabase verifyDatabase = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), verifyConnectionName);
        verifyDatabase.setDatabaseName(exportedDbPath);
        QVERIFY2(verifyDatabase.open(), qPrintable(verifyDatabase.lastError().text()));

        QSqlQuery verifyQuery(verifyDatabase);
        QVERIFY2(verifyQuery.exec(QStringLiteral("SELECT value FROM test_data")),
                 qPrintable(verifyQuery.lastError().text()));
        QVERIFY2(verifyQuery.next(), "exported database should contain WAL-backed row");
        QCOMPARE(verifyQuery.value(0).toString(), QStringLiteral("wal-row"));

        verifyDatabase.close();
    }
    QSqlDatabase::removeDatabase(verifyConnectionName);
}

void TestDiagnostics::exportBundle_includesRuntimeStageTwoSnapshotWhenPresent()
{
    QTemporaryDir sandbox;
    QVERIFY2(sandbox.isValid(), "temporary sandbox should be created");

    const QString appDataDir = sandbox.filePath(QStringLiteral("appdata"));
    const QString appLocalDir = sandbox.filePath(QStringLiteral("applocal"));
    const QString exportRoot = sandbox.filePath(QStringLiteral("exports"));
    QVERIFY(QDir().mkpath(appDataDir));
    QVERIFY(QDir().mkpath(appLocalDir));
    QVERIFY(QDir().mkpath(exportRoot));

    Diagnostics::BundleSourcePaths source;
    source.appDataDir = appDataDir;
    source.appLocalDataDir = appLocalDir;
    source.databasePath = QDir(appDataDir).filePath(QStringLiteral("leyochat.db"));
    source.logsDir = QDir(appLocalDir).filePath(QStringLiteral("logs"));
    source.crashDir = QDir(appLocalDir).filePath(QStringLiteral("crash"));
    source.screenshotsDir = QDir(appLocalDir).filePath(QStringLiteral("screenshots"));
    source.runtimeDir = QDir(appLocalDir).filePath(QStringLiteral("runtime"));

    QVERIFY(QDir().mkpath(source.logsDir));
    QVERIFY(QDir().mkpath(source.crashDir));
    QVERIFY(QDir().mkpath(source.screenshotsDir));

    {
        QFile database(source.databasePath);
        QVERIFY(database.open(QIODevice::WriteOnly));
        database.write("db");
    }

    RuntimeArchitectureSnapshot snapshot;
    snapshot.discoveryResult.defaultServiceId = QStringLiteral("svc-001");
    snapshot.discoveryResult.multipleServicesDetected = false;
    snapshot.discoveryResult.services.push_back(ServiceDiscoverySnapshot{
        QStringLiteral("svc-001"),
        QStringLiteral("LeyoChat Service"),
        QStringLiteral("Demo Org"),
        QStringLiteral("lan"),
        1712510000000LL,
        {}
    });
    snapshot.serviceRegistry.push_back(ServiceRegistryEntry{
        QStringLiteral("svc-001"),
        QStringLiteral("LeyoChat Service"),
        QStringLiteral("Demo Org"),
        QStringLiteral("lan"),
        QStringLiteral("192.0.2.10"),
        static_cast<quint16>(8443),
        true,
        {}
    });
    snapshot.selection.groupId = QStringLiteral("group-001");
    snapshot.selection.serviceId = QStringLiteral("svc-001");
    snapshot.selection.bound = true;

    QVERIFY(Diagnostics::writeRuntimeArchitectureSnapshot(source, snapshot));

    QString exportedDir;
    QString error;
    QVERIFY2(Diagnostics::exportBundle(source, exportRoot, &exportedDir, &error),
             qPrintable(error));

    QFile runtimeFile(QDir(exportedDir).filePath(QStringLiteral("runtime/stage2-snapshot.json")));
    QVERIFY(runtimeFile.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(runtimeFile.readAll());
    QVERIFY(document.isObject());
    const QJsonObject object = document.object();
    QCOMPARE(object.value(QStringLiteral("defaultServiceId")).toString(), QStringLiteral("svc-001"));
    QCOMPARE(object.value(QStringLiteral("bound")).toBool(), true);
    QCOMPARE(object.value(QStringLiteral("groupId")).toString(), QStringLiteral("group-001"));
    const QJsonArray services = object.value(QStringLiteral("services")).toArray();
    QCOMPARE(services.size(), 1);
}

void TestDiagnostics::exportBundle_includesIntegrationSnapshotWhenPresent()
{
    QTemporaryDir sandbox;
    QVERIFY2(sandbox.isValid(), "temporary sandbox should be created");

    const QString appDataDir = sandbox.filePath(QStringLiteral("appdata"));
    const QString appLocalDir = sandbox.filePath(QStringLiteral("applocal"));
    const QString exportRoot = sandbox.filePath(QStringLiteral("exports"));
    QVERIFY(QDir().mkpath(appDataDir));
    QVERIFY(QDir().mkpath(appLocalDir));
    QVERIFY(QDir().mkpath(exportRoot));

    Diagnostics::BundleSourcePaths source;
    source.appDataDir = appDataDir;
    source.appLocalDataDir = appLocalDir;
    source.databasePath = QDir(appDataDir).filePath(QStringLiteral("leyochat.db"));
    source.logsDir = QDir(appLocalDir).filePath(QStringLiteral("logs"));
    source.crashDir = QDir(appLocalDir).filePath(QStringLiteral("crash"));
    source.screenshotsDir = QDir(appLocalDir).filePath(QStringLiteral("screenshots"));
    source.runtimeDir = QDir(appLocalDir).filePath(QStringLiteral("runtime"));

    QVERIFY(QDir().mkpath(source.logsDir));
    QVERIFY(QDir().mkpath(source.crashDir));
    QVERIFY(QDir().mkpath(source.screenshotsDir));

    {
        QFile database(source.databasePath);
        QVERIFY(database.open(QIODevice::WriteOnly));
        database.write("db");
    }

    AzureDevOpsConnectionSettings devOpsSettings;
    devOpsSettings.enabled = true;
    devOpsSettings.baseUrl = QStringLiteral("https://dev.azure.com");
    devOpsSettings.organization = QStringLiteral("org-a");
    devOpsSettings.project = QStringLiteral("Project-1");
    devOpsSettings.notificationsEnabled = true;
    devOpsSettings.lastPollAttemptAtMs = 111;
    devOpsSettings.lastPollSuccessAtMs = 222;
    devOpsSettings.lastPollErrorMessage = QStringLiteral("none");
    devOpsSettings.lastPollErrorCategory = QStringLiteral("unknown");
    devOpsSettings.consecutivePollFailures = 0;
    AzureDevOpsNotificationTarget notificationTarget;
    notificationTarget.organization = QStringLiteral("org-a");
    notificationTarget.project = QStringLiteral("Project-1");
    notificationTarget.enabled = true;
    notificationTarget.lastNotifiedBuildId = 77;
    notificationTarget.lastNotifiedPullRequestUpdatedAtMs = 88;
    notificationTarget.lastNotifiedAssignedWorkItemUpdatedAtMs = 99;
    notificationTarget.lastNotifiedBuildResult = QStringLiteral("succeeded");
    notificationTarget.lastPollAttemptAtMs = 444;
    notificationTarget.lastPollSuccessAtMs = 555;
    notificationTarget.lastPollErrorCategory = QStringLiteral("network");
    devOpsSettings.notificationTargets.push_back(notificationTarget);

    OutlookConnectionSettings outlookSettings;
    outlookSettings.enabled = true;
    outlookSettings.accountEmail = QStringLiteral("user@example.com");
    outlookSettings.notificationsEnabled = true;
    outlookSettings.lastPollAttemptAtMs = 333;
    outlookSettings.lastPollSuccessAtMs = 444;
    outlookSettings.lastPollErrorMessage = QStringLiteral("token refresh pending");
    outlookSettings.lastPollErrorCategory = QStringLiteral("auth");
    outlookSettings.consecutivePollFailures = 1;
    outlookSettings.recentMailIds = {QStringLiteral("mail-1")};
    outlookSettings.recentEventIds = {QStringLiteral("event-1|ck-1|active")};

    QVERIFY(Diagnostics::writeIntegrationSnapshot(source, devOpsSettings, outlookSettings));

    QString exportedDir;
    QString error;
    QVERIFY2(Diagnostics::exportBundle(source, exportRoot, &exportedDir, &error),
             qPrintable(error));

    QFile integrationFile(QDir(exportedDir).filePath(QStringLiteral("runtime/integration-snapshot.json")));
    QVERIFY(integrationFile.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(integrationFile.readAll());
    QVERIFY(document.isObject());
    const QJsonObject object = document.object();
    QCOMPARE(object.value(QStringLiteral("azureDevOps")).toObject().value(QStringLiteral("organization")).toString(),
             QStringLiteral("org-a"));
    QCOMPARE(object.value(QStringLiteral("azureDevOps")).toObject().value(QStringLiteral("lastPollErrorCategory")).toString(),
             QStringLiteral("unknown"));
    QCOMPARE(object.value(QStringLiteral("azureDevOps")).toObject().value(QStringLiteral("defaultNotificationTarget")).toString(),
             QStringLiteral("org-a / Project-1"));
    QCOMPARE(object.value(QStringLiteral("azureDevOps")).toObject().value(QStringLiteral("enabledNotificationTargetCount")).toInt(),
             1);
    QCOMPARE(object.value(QStringLiteral("azureDevOps")).toObject().value(QStringLiteral("enabledTargetSummaries")).toArray().size(),
             1);
    QCOMPARE(object.value(QStringLiteral("outlook")).toObject().value(QStringLiteral("accountEmail")).toString(),
             QStringLiteral("user@example.com"));
    QCOMPARE(object.value(QStringLiteral("outlook")).toObject().value(QStringLiteral("lastPollErrorCategory")).toString(),
             QStringLiteral("auth"));
    QCOMPARE(object.value(QStringLiteral("outlook")).toObject().value(QStringLiteral("authorizationState")).toString(),
             QStringLiteral("unconfigured"));
}

QTEST_MAIN(TestDiagnostics)

#include "TestDiagnostics.moc"
