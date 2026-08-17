#include <QtTest>

#include "recovery/ClientStartupOptions.h"

class TestClientStartupOptions : public QObject
{
    Q_OBJECT

private slots:
    void normalStartupDoesNotRequestRecovery()
    {
        const ClientStartupOptions options =
            ClientStartupOptions::fromArguments({QStringLiteral("LeyoChat.exe")});

        QVERIFY(!options.supervised);
        QVERIFY(!options.recoveredFromCrash);
        QVERIFY(options.recoverySessionId.isEmpty());
        QVERIFY(!options.validRecoveryRequest());
        QVERIFY(!options.suppressSplash());
        QVERIFY(options.shouldRegisterWindowsArr());
        QCOMPARE(options.stateSessionId(), QStringLiteral("windows-arr"));
    }

    void supervisedRecoveryRequiresMatchingArguments()
    {
        const ClientStartupOptions options = ClientStartupOptions::fromArguments({
            QStringLiteral("LeyoChat.exe"),
            QStringLiteral("--leyochat-supervised"),
            QStringLiteral("--recovered-from-crash"),
            QStringLiteral("--recovery-session=session-1"),
        });

        QVERIFY(options.supervised);
        QVERIFY(options.recoveredFromCrash);
        QCOMPARE(options.recoverySessionId, QStringLiteral("session-1"));
        QVERIFY(options.validRecoveryRequest());
        QVERIFY(options.suppressSplash());
        QVERIFY(!options.shouldRegisterWindowsArr());
        QCOMPARE(options.stateSessionId(), QStringLiteral("session-1"));
    }

    void missingSessionFallsBackToNormalPresentation()
    {
        const ClientStartupOptions options = ClientStartupOptions::fromArguments({
            QStringLiteral("LeyoChat.exe"),
            QStringLiteral("--leyochat-supervised"),
            QStringLiteral("--recovered-from-crash"),
        });

        QVERIFY(options.supervised);
        QVERIFY(options.recoveredFromCrash);
        QVERIFY(!options.validRecoveryRequest());
        QVERIFY(!options.suppressSplash());
        QVERIFY(!options.shouldRegisterWindowsArr());
        QVERIFY(options.stateSessionId().isEmpty());
    }

    void oversizedSessionIsRejected()
    {
        const ClientStartupOptions options = ClientStartupOptions::fromArguments({
            QStringLiteral("LeyoChat.exe"),
            QStringLiteral("--leyochat-supervised"),
            QStringLiteral("--recovered-from-crash"),
            QStringLiteral("--recovery-session=%1").arg(QString(129, QLatin1Char('x'))),
        });

        QVERIFY(!options.validRecoveryRequest());
        QVERIFY(!options.suppressSplash());
    }

    void unknownArgumentsAreIgnored()
    {
        const ClientStartupOptions options = ClientStartupOptions::fromArguments({
            QStringLiteral("LeyoChat.exe"),
            QStringLiteral("--unknown-flag"),
            QStringLiteral("--recovery-session=session-ignored"),
        });

        QVERIFY(!options.supervised);
        QVERIFY(!options.recoveredFromCrash);
        QCOMPARE(options.recoverySessionId, QStringLiteral("session-ignored"));
        QVERIFY(!options.validRecoveryRequest());
    }

    void directArrFallbackCarriesRecoveryArguments()
    {
        const ClientStartupOptions options =
            ClientStartupOptions::fromArguments({QStringLiteral("LeyoChat.exe")});

        QCOMPARE(options.windowsArrCommandLine(),
                 QStringLiteral("--recovered-from-crash --recovery-session=windows-arr"));
    }

    void directArrFallbackRecoveryIsValidWithoutSupervisorFlag()
    {
        const ClientStartupOptions options = ClientStartupOptions::fromArguments({
            QStringLiteral("LeyoChat.exe"),
            QStringLiteral("--recovered-from-crash"),
            QStringLiteral("--recovery-session=windows-arr"),
        });

        QVERIFY(!options.supervised);
        QVERIFY(options.validRecoveryRequest());
        QVERIFY(options.suppressSplash());
        QVERIFY(options.shouldRegisterWindowsArr());
        QCOMPARE(options.stateSessionId(), QStringLiteral("windows-arr"));
    }
};

QTEST_GUILESS_MAIN(TestClientStartupOptions)
#include "TestClientStartupOptions.moc"
