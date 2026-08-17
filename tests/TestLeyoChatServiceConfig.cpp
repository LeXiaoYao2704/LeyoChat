#include <QtTest>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>

#include "chatservice/ServiceConfig.h"

class TestLeyoChatServiceConfig : public QObject {
    Q_OBJECT

private slots:
    void loadConfigFile_parsesDeploymentSettings();
    void defaults_disableLegacyFileAccess();
    void validate_rejectsUnsafeStartupInputs();
    void validateStartupPaths_rejectsDatabasePathUnderNonDirectory();
    void workspaceScopeJson_exportsWildcardOrAllowlist();
};

namespace {
QString writeConfig(QTemporaryDir& dir, const QByteArray& body)
{
    const QString path = dir.filePath(QStringLiteral("leyochat-service.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return {};
    file.write(body);
    file.close();
    return path;
}

LeyoChatService::ServiceConfig validConfig(QTemporaryDir& dir)
{
    LeyoChatService::ServiceConfig config =
        LeyoChatService::defaultServiceConfig();
    config.listenPort = 9001;
    config.databasePath = dir.filePath(QStringLiteral("service.db"));
    config.storagePath = dir.filePath(QStringLiteral("files"));
    config.seedToken = QStringLiteral("deploy-token");
    config.workspaceIds = {QStringLiteral("ws-a"), QStringLiteral("ws-b")};
    config.allowWildcardWorkspaces = false;
    return config;
}
}

void TestLeyoChatServiceConfig::defaults_disableLegacyFileAccess()
{
    const auto config = LeyoChatService::defaultServiceConfig();
    QVERIFY(!config.legacyFileAccessEnabled);
}

void TestLeyoChatServiceConfig::loadConfigFile_parsesDeploymentSettings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("service.db"));
    const QString storagePath = dir.filePath(QStringLiteral("storage"));
    const QString configPath = writeConfig(
        dir,
        QJsonDocument(QJsonObject{
            {QStringLiteral("listen"), QJsonObject{{QStringLiteral("port"), 9876}}},
            {QStringLiteral("database"), QJsonObject{{QStringLiteral("path"), dbPath}}},
            {QStringLiteral("storage"), QJsonObject{{QStringLiteral("path"), storagePath}}},
            {QStringLiteral("auth"), QJsonObject{
                {QStringLiteral("token"), QStringLiteral("deploy-token")},
                {QStringLiteral("legacyFileAccess"), false},
                {QStringLiteral("workspaces"), QJsonArray{
                    QStringLiteral("ws-a"),
                    QStringLiteral("ws-b")
                }}
            }},
            {QStringLiteral("sessions"), QJsonObject{{QStringLiteral("ttlSeconds"), 180}}},
            {QStringLiteral("rateLimit"), QJsonObject{
                {QStringLiteral("maxRequestsPerWindow"), 42},
                {QStringLiteral("windowMs"), 3000}
            }},
            {QStringLiteral("onlyOffice"), QJsonObject{
                {QStringLiteral("url"), QStringLiteral("http://office.local")},
                {QStringLiteral("jwtSecret"), QStringLiteral("jwt-secret")}
            }},
            {QStringLiteral("externalUrl"), QStringLiteral("http://chat.local:9876")},
            {QStringLiteral("chatFiles"), QJsonObject{
                {QStringLiteral("ttlDays"), 9},
                {QStringLiteral("quotaMb"), 512}
            }}
        }).toJson(QJsonDocument::Compact));
    QVERIFY(!configPath.isEmpty());

    const auto result =
        LeyoChatService::loadServiceConfigFile(configPath);

    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.config.listenPort, 9876);
    QCOMPARE(result.config.databasePath, dbPath);
    QCOMPARE(result.config.storagePath, storagePath);
    QCOMPARE(result.config.seedToken, QStringLiteral("deploy-token"));
    QCOMPARE(result.config.workspaceIds,
             QStringList({QStringLiteral("ws-a"), QStringLiteral("ws-b")}));
    QCOMPARE(result.config.allowWildcardWorkspaces, false);
    QCOMPARE(result.config.legacyFileAccessEnabled, false);
    QCOMPARE(result.config.sessionTtlMs, qint64(180000));
    QCOMPARE(result.config.rateLimitMaxRequestsPerWindow, 42);
    QCOMPARE(result.config.rateLimitWindowMs, qint64(3000));
    QCOMPARE(result.config.onlyOfficeUrl, QStringLiteral("http://office.local"));
    QCOMPARE(result.config.externalUrl, QStringLiteral("http://chat.local:9876"));
    QCOMPARE(result.config.onlyOfficeJwtSecret, QStringLiteral("jwt-secret"));
    QCOMPARE(result.config.chatFileTtlDays, 9);
    QCOMPARE(result.config.chatFileQuotaMb, 512);
}

void TestLeyoChatServiceConfig::validate_rejectsUnsafeStartupInputs()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto config = validConfig(dir);
    config.listenPort = 0;
    auto result = LeyoChatService::validateServiceConfig(config);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("port"), Qt::CaseInsensitive));

    config = validConfig(dir);
    config.seedToken.clear();
    result = LeyoChatService::validateServiceConfig(config);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("token"), Qt::CaseInsensitive));

    config = validConfig(dir);
    config.workspaceIds.clear();
    result = LeyoChatService::validateServiceConfig(config);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("workspace"), Qt::CaseInsensitive));

    config = validConfig(dir);
    config.allowWildcardWorkspaces = true;
    result = LeyoChatService::validateServiceConfig(config);
    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("mutually"), Qt::CaseInsensitive));
}

void TestLeyoChatServiceConfig::validateStartupPaths_rejectsDatabasePathUnderNonDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QFile fileParent(dir.filePath(QStringLiteral("not-a-directory")));
    QVERIFY(fileParent.open(QIODevice::WriteOnly));
    fileParent.write("occupied");
    fileParent.close();

    auto config = validConfig(dir);
    config.databasePath =
        fileParent.fileName() + QStringLiteral("/service.db");

    const auto result =
        LeyoChatService::validateServiceStartupPaths(config);

    QVERIFY(!result.ok);
    QVERIFY(result.error.contains(QStringLiteral("database"), Qt::CaseInsensitive));
}

void TestLeyoChatServiceConfig::workspaceScopeJson_exportsWildcardOrAllowlist()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    auto config = validConfig(dir);
    QCOMPARE(LeyoChatService::workspaceScopeJson(config),
             QStringLiteral("[\"ws-a\",\"ws-b\"]"));

    config.workspaceIds.clear();
    config.allowWildcardWorkspaces = true;
    QCOMPARE(LeyoChatService::workspaceScopeJson(config), QStringLiteral("*"));
}

QTEST_MAIN(TestLeyoChatServiceConfig)
#include "TestLeyoChatServiceConfig.moc"
