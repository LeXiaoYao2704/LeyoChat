#include "installer/InstallerLaunchPolicy.h"

#include <QtTest/QtTest>

class InstallerLaunchPolicyTests : public QObject
{
    Q_OBJECT

private slots:
    void clientInstallLaunchesWhenNoClientIsRunning()
    {
        const auto plan = LeyoChatInstaller::clientLaunchPlan(false, false, false);

        QCOMPARE(plan.terminateExistingLauncher, false);
        QCOMPARE(plan.terminateExistingClient, false);
        QCOMPARE(plan.launchLauncher, true);
    }

    void clientInstallReplacesExistingClientBeforeLaunch()
    {
        const auto plan = LeyoChatInstaller::clientLaunchPlan(false, true, true);

        QCOMPARE(plan.terminateExistingLauncher, true);
        QCOMPARE(plan.terminateExistingClient, true);
        QCOMPARE(plan.launchLauncher, true);
    }

    void serverInstallDoesNotLaunchClient()
    {
        const auto plan = LeyoChatInstaller::clientLaunchPlan(true, true, true);

        QCOMPARE(plan.terminateExistingLauncher, false);
        QCOMPARE(plan.terminateExistingClient, false);
        QCOMPARE(plan.launchLauncher, false);
    }

    void preInstallStopOnlyAppliesToClientInstaller()
    {
        QCOMPARE(LeyoChatInstaller::shouldStopClientProcessesBeforeInstall(false), true);
        QCOMPARE(LeyoChatInstaller::shouldStopClientProcessesBeforeInstall(true), false);
    }

    void stopOrderIsLauncherThenClient()
    {
        const auto order = LeyoChatInstaller::clientProcessStopOrder();

        QCOMPARE(order.at(0), LeyoChatInstaller::ClientProcessKind::Launcher);
        QCOMPARE(order.at(1), LeyoChatInstaller::ClientProcessKind::Client);
    }

    void clientInstallContinuesOnlyAfterBothProcessesStop()
    {
        QVERIFY(LeyoChatInstaller::canContinueAfterStoppingClientProcesses(
            true, false, false));
        QVERIFY(LeyoChatInstaller::canContinueAfterStoppingClientProcesses(
            false, true, true));
        QVERIFY(!LeyoChatInstaller::canContinueAfterStoppingClientProcesses(
            false, false, true));
        QVERIFY(!LeyoChatInstaller::canContinueAfterStoppingClientProcesses(
            false, true, false));
    }
};

QTEST_GUILESS_MAIN(InstallerLaunchPolicyTests)
#include "TestInstallerLaunchPolicy.moc"
