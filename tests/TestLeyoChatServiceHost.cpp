#include <QtTest>

#include "chatservice/ServiceHostPolicy.h"

class TestLeyoChatServiceHost : public QObject {
    Q_OBJECT

private slots:
    void defaultPaths_useProgramDataLayout();
    void childCommandLine_quotesExecutableAndConfig();
    void restartDelay_hasBoundedBackoff();
    void crashWindow_stopsAfterTenCrashesInFiveMinutes();
    void logPaths_useStdoutAndStderrFilesUnderLogs();
    void logRetention_hasBoundedDefaults();
    void logRetention_rotatesLargeActiveLogAndPrunesOldArchives();
};

void TestLeyoChatServiceHost::defaultPaths_useProgramDataLayout()
{
    const auto paths = LeyoChatService::ServiceHostPolicy::defaultPaths(
        QStringLiteral("C:/Program Files/LeyoChat Server"),
        QStringLiteral("C:/ProgramData"));

    QCOMPARE(paths.serviceExePath,
             QStringLiteral("C:/Program Files/LeyoChat Server/LeyoChatService.exe"));
    QCOMPARE(paths.configPath,
             QStringLiteral("C:/ProgramData/LeyoChat/Service/leyochat-service.json"));
    QCOMPARE(paths.dataRoot,
             QStringLiteral("C:/ProgramData/LeyoChat/Service"));
    QCOMPARE(paths.fileStoragePath,
             QStringLiteral("C:/ProgramData/LeyoChat/Service/files"));
    QCOMPARE(paths.logDirectory,
             QStringLiteral("C:/ProgramData/LeyoChat/Service/logs"));
}

void TestLeyoChatServiceHost::childCommandLine_quotesExecutableAndConfig()
{
    const QString commandLine = LeyoChatService::ServiceHostPolicy::childCommandLine(
        QStringLiteral("C:/Program Files/LeyoChat Server/LeyoChatService.exe"),
        QStringLiteral("C:/ProgramData/LeyoChat/Service/leyochat-service.json"));

    QCOMPARE(commandLine,
             QStringLiteral("\"C:/Program Files/LeyoChat Server/LeyoChatService.exe\" "
                            "--config "
                            "\"C:/ProgramData/LeyoChat/Service/leyochat-service.json\""));

    const QString nativeCommandLine =
        LeyoChatService::ServiceHostPolicy::childCommandLine(
            QStringLiteral("C:\\Program Files\\LeyoChat Server\\LeyoChatService.exe"),
            QStringLiteral("C:\\ProgramData\\LeyoChat\\Service\\leyochat-service.json"));
    QCOMPARE(nativeCommandLine,
             QStringLiteral("\"C:\\Program Files\\LeyoChat Server\\LeyoChatService.exe\" "
                            "--config "
                            "\"C:\\ProgramData\\LeyoChat\\Service\\leyochat-service.json\""));
}

void TestLeyoChatServiceHost::restartDelay_hasBoundedBackoff()
{
    QCOMPARE(LeyoChatService::ServiceHostPolicy::restartDelayMs(0), 5000);
    QCOMPARE(LeyoChatService::ServiceHostPolicy::restartDelayMs(1), 30000);
    QCOMPARE(LeyoChatService::ServiceHostPolicy::restartDelayMs(2), 60000);
    QCOMPARE(LeyoChatService::ServiceHostPolicy::restartDelayMs(20), 60000);
}

void TestLeyoChatServiceHost::crashWindow_stopsAfterTenCrashesInFiveMinutes()
{
    const qint64 nowMs = 1'000'000;
    QList<qint64> nineRecentCrashes;
    for (int i = 0; i < 9; ++i)
        nineRecentCrashes.push_back(nowMs - i * 1000);
    QVERIFY(LeyoChatService::ServiceHostPolicy::shouldRestartAfterCrash(
        nineRecentCrashes,
        nowMs));

    QList<qint64> tenRecentCrashes = nineRecentCrashes;
    tenRecentCrashes.push_back(nowMs - 9000);
    QVERIFY(!LeyoChatService::ServiceHostPolicy::shouldRestartAfterCrash(
        tenRecentCrashes,
        nowMs));

    tenRecentCrashes.push_back(nowMs - 301000);
    QVERIFY(!LeyoChatService::ServiceHostPolicy::shouldRestartAfterCrash(
        tenRecentCrashes,
        nowMs));
}

void TestLeyoChatServiceHost::logPaths_useStdoutAndStderrFilesUnderLogs()
{
    const auto paths = LeyoChatService::ServiceHostPolicy::defaultPaths(
        QStringLiteral("C:/Program Files/LeyoChat Server"),
        QStringLiteral("C:/ProgramData"));

    QCOMPARE(paths.stdoutLogPath,
             QStringLiteral("C:/ProgramData/LeyoChat/Service/logs/LeyoChatService.stdout.log"));
    QCOMPARE(paths.stderrLogPath,
             QStringLiteral("C:/ProgramData/LeyoChat/Service/logs/LeyoChatService.stderr.log"));
}

void TestLeyoChatServiceHost::logRetention_hasBoundedDefaults()
{
    const auto retention =
        LeyoChatService::ServiceHostPolicy::defaultLogRetentionPolicy();

    QCOMPARE(retention.maxActiveLogBytes, 10 * 1024 * 1024);
    QCOMPARE(retention.maxArchiveAgeMs, qint64(14) * 24 * 60 * 60 * 1000);
    QCOMPARE(retention.maxArchiveFilesPerLog, 8);
}

void TestLeyoChatServiceHost::logRetention_rotatesLargeActiveLogAndPrunesOldArchives()
{
    const auto retention =
        LeyoChatService::ServiceHostPolicy::defaultLogRetentionPolicy();
    const qint64 nowMs = 1'000'000'000;

    QVERIFY(!LeyoChatService::ServiceHostPolicy::shouldRotateActiveLog(
        retention.maxActiveLogBytes,
        retention));
    QVERIFY(LeyoChatService::ServiceHostPolicy::shouldRotateActiveLog(
        retention.maxActiveLogBytes + 1,
        retention));

    QVERIFY(!LeyoChatService::ServiceHostPolicy::shouldPruneArchivedLog(
        nowMs - retention.maxArchiveAgeMs,
        nowMs,
        0,
        retention));
    QVERIFY(LeyoChatService::ServiceHostPolicy::shouldPruneArchivedLog(
        nowMs - retention.maxArchiveAgeMs - 1,
        nowMs,
        0,
        retention));
    QVERIFY(LeyoChatService::ServiceHostPolicy::shouldPruneArchivedLog(
        nowMs,
        nowMs,
        retention.maxArchiveFilesPerLog,
        retention));
}

QTEST_MAIN(TestLeyoChatServiceHost)
#include "TestLeyoChatServiceHost.moc"
