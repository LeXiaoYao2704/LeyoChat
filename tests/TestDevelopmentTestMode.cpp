#include <QCoreApplication>
#include <QDir>
#include <QSignalBlocker>
#include <QtTest>

#include "app/AppSettings.h"
#include "app/TestModeContext.h"
#include "diagnostics/Diagnostics.h"

class TestDevelopmentTestMode : public QObject {
    Q_OBJECT

private slots:
    void hiddenArgsAbsent_keepsNormalMode();
    void hiddenArgsPresent_enableDevelopmentTestMode();
    void applyToApplication_setsProfileScopedIdentity();
    void diagnosticsPaths_useDevelopmentRootsWhenEnabled();
};

void TestDevelopmentTestMode::hiddenArgsAbsent_keepsNormalMode()
{
    const TestModeContext context = TestModeContext::fromArguments({});
    QVERIFY(!context.enabled);
    QVERIFY(context.profile.isEmpty());
    QVERIFY(context.appDataRoot().isEmpty());
    QVERIFY(context.appLocalDataRoot().isEmpty());
}

void TestDevelopmentTestMode::hiddenArgsPresent_enableDevelopmentTestMode()
{
    const QString sandboxRoot = QDir::fromNativeSeparators(QStringLiteral("C:/temp/leyochat-devtest"));
    const TestModeContext context = TestModeContext::fromArguments(
        {
            QStringLiteral("--dev-test-profile"), QStringLiteral("client-a"),
            QStringLiteral("--dev-test-data-root"), sandboxRoot,
            QStringLiteral("--dev-test-port"), QStringLiteral("45454"),
            QStringLiteral("--dev-test-client-id"), QStringLiteral("dev-client-a"),
            QStringLiteral("--dev-test-display-name"), QStringLiteral("开发A"),
        });

    QVERIFY(context.enabled);
    QCOMPARE(context.profile, QStringLiteral("client-a"));
    QCOMPARE(context.listenPort, static_cast<quint16>(45454));
    QCOMPARE(context.clientId, QStringLiteral("dev-client-a"));
    QCOMPARE(context.displayName, QStringLiteral("开发A"));
    QVERIFY(context.appDataRoot().endsWith(QStringLiteral("/client-a/appdata")));
    QVERIFY(context.appLocalDataRoot().endsWith(QStringLiteral("/client-a/local")));
    QCOMPARE(context.settingsOrganizationName(), QStringLiteral("LeyoChatDevTest"));
    QCOMPARE(context.settingsApplicationName(), QStringLiteral("LeyoChat-client-a"));
    QCOMPARE(context.singleInstanceKey(), QStringLiteral("LeyoChat_SingleInstance_dev-client-a"));
}

void TestDevelopmentTestMode::applyToApplication_setsProfileScopedIdentity()
{
    const TestModeContext context = TestModeContext::fromArguments(
        {
            QStringLiteral("--dev-test-profile"), QStringLiteral("client-b"),
            QStringLiteral("--dev-test-data-root"), QStringLiteral("D:/sandbox"),
            QStringLiteral("--dev-test-port"), QStringLiteral("45455"),
            QStringLiteral("--dev-test-client-id"), QStringLiteral("dev-client-b"),
            QStringLiteral("--dev-test-display-name"), QStringLiteral("开发B"),
        });

    context.applyToApplication(*qApp);

    QCOMPARE(qApp->organizationName(), QStringLiteral("LeyoChatDevTest"));
    QCOMPARE(qApp->applicationName(), QStringLiteral("LeyoChat-client-b"));
    QCOMPARE(AppSettings::organizationName(), QStringLiteral("LeyoChatDevTest"));
    QCOMPARE(AppSettings::applicationName(), QStringLiteral("LeyoChat-client-b"));
    QCOMPARE(AppSettings::windowTitle(QStringLiteral("LeyoChat")),
             QStringLiteral("LeyoChat [dev client-b]"));
}

void TestDevelopmentTestMode::diagnosticsPaths_useDevelopmentRootsWhenEnabled()
{
    const TestModeContext context = TestModeContext::fromArguments(
        {
            QStringLiteral("--dev-test-profile"), QStringLiteral("client-c"),
            QStringLiteral("--dev-test-data-root"), QStringLiteral("E:/leyochat-sandbox"),
            QStringLiteral("--dev-test-port"), QStringLiteral("45456"),
            QStringLiteral("--dev-test-client-id"), QStringLiteral("dev-client-c"),
            QStringLiteral("--dev-test-display-name"), QStringLiteral("开发C"),
        });

    context.applyToApplication(*qApp);

    const Diagnostics::BundleSourcePaths paths = Diagnostics::defaultSourcePaths();
    QCOMPARE(QDir::fromNativeSeparators(paths.appDataDir),
             QStringLiteral("E:/leyochat-sandbox/client-c/appdata"));
    QCOMPARE(QDir::fromNativeSeparators(paths.appLocalDataDir),
             QStringLiteral("E:/leyochat-sandbox/client-c/local"));
    QCOMPARE(QDir::fromNativeSeparators(paths.databasePath),
             QStringLiteral("E:/leyochat-sandbox/client-c/appdata/leyochat.db"));
    QCOMPARE(QDir::fromNativeSeparators(paths.logsDir),
             QStringLiteral("E:/leyochat-sandbox/client-c/local/logs"));
}

QTEST_GUILESS_MAIN(TestDevelopmentTestMode)

#include "TestDevelopmentTestMode.moc"
