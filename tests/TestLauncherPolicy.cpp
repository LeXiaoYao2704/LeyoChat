#include <QtTest>

#include "launcher/LauncherPolicy.h"

class TestLauncherPolicy : public QObject
{
    Q_OBJECT

private slots:
    void cleanExitStopsLauncher()
    {
        LauncherPolicy policy;
        const LauncherDecision decision = policy.afterChildExit(0, 1000, 1000, false);

        QCOMPARE(decision.action, LauncherAction::Exit);
        QCOMPARE(decision.delayMs, 0);
        QCOMPARE(decision.recentCrashCount, 0);
    }

    void firstThreeCrashesUseBoundedBackoff()
    {
        LauncherPolicy policy;

        const auto first = policy.afterChildExit(0xC0000005u, 1000, 1000, false);
        const auto second = policy.afterChildExit(1, 1000, 2000, false);
        const auto third = policy.afterChildExit(2, 1000, 3000, false);

        QCOMPARE(first.action, LauncherAction::Restart);
        QCOMPARE(first.delayMs, 2000);
        QCOMPARE(first.recentCrashCount, 1);
        QCOMPARE(second.action, LauncherAction::Restart);
        QCOMPARE(second.delayMs, 10000);
        QCOMPARE(second.recentCrashCount, 2);
        QCOMPARE(third.action, LauncherAction::Restart);
        QCOMPARE(third.delayMs, 30000);
        QCOMPARE(third.recentCrashCount, 3);
    }

    void fourthCrashStopsRestartLoop()
    {
        LauncherPolicy policy;
        policy.afterChildExit(1, 1000, 1000, false);
        policy.afterChildExit(1, 1000, 2000, false);
        policy.afterChildExit(1, 1000, 3000, false);

        const auto fourth = policy.afterChildExit(1, 1000, 4000, false);

        QCOMPARE(fourth.action, LauncherAction::StopCrashLoop);
        QCOMPARE(fourth.delayMs, 0);
        QCOMPARE(fourth.recentCrashCount, 4);
    }

    void shutdownSuppressesRestart()
    {
        LauncherPolicy policy;
        const auto decision = policy.afterChildExit(1, 1000, 1000, true);

        QCOMPARE(decision.action, LauncherAction::Exit);
        QCOMPARE(decision.recentCrashCount, 0);
    }

    void cancelledWindowsShutdownClearsSuppression()
    {
        LauncherShutdownState state;

        state.onQueryEndSession();
        QVERIFY(state.shutdownRequested());
        state.onEndSession(false);
        QVERIFY(!state.shutdownRequested());
        state.onEndSession(true);
        QVERIFY(state.shutdownRequested());
    }

    void stableRuntimeResetsPreviousCrashHistory()
    {
        LauncherPolicy policy;
        policy.afterChildExit(1, 1000, 1000, false);
        policy.afterChildExit(1, 1000, 2000, false);

        const auto afterStableRun = policy.afterChildExit(
            1,
            LauncherPolicy::stableRuntimeMs(),
            400000,
            false);

        QCOMPARE(afterStableRun.action, LauncherAction::Restart);
        QCOMPARE(afterStableRun.delayMs, 2000);
        QCOMPARE(afterStableRun.recentCrashCount, 1);
    }

    void rollingWindowDiscardsOldCrashes()
    {
        LauncherPolicy policy;
        policy.afterChildExit(1, 1000, 1000, false);
        policy.afterChildExit(1, 1000, 2000, false);

        const auto decision = policy.afterChildExit(
            1,
            1000,
            2000 + LauncherPolicy::crashWindowMs() + 1,
            false);

        QCOMPARE(decision.action, LauncherAction::Restart);
        QCOMPARE(decision.delayMs, 2000);
        QCOMPARE(decision.recentCrashCount, 1);
    }
};

QTEST_GUILESS_MAIN(TestLauncherPolicy)
#include "TestLauncherPolicy.moc"
