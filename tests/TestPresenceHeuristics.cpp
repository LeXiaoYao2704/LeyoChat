#include <QtTest/QTest>

#include "app/PresenceHeuristics.h"

class TestPresenceHeuristics : public QObject {
    Q_OBJECT

private slots:
    void lockedWorkstation_reportsAway()
    {
        PresenceHeuristicInputs inputs;
        inputs.workstationLocked = true;
        inputs.idleMilliseconds = 2000;

        QCOMPARE(determineLocalPresence(inputs), PeerPresenceStatus::Away);
    }

    void idleOverThreshold_reportsAway()
    {
        PresenceHeuristicInputs inputs;
        inputs.workstationLocked = false;
        inputs.idleMilliseconds = 11 * 60 * 1000; // 11分钟，超过10分钟阈值

        QCOMPARE(determineLocalPresence(inputs), PeerPresenceStatus::Away);
    }

    void activeUser_reportsOnline()
    {
        PresenceHeuristicInputs inputs;
        inputs.workstationLocked = false;
        inputs.idleMilliseconds = 10 * 1000; // 10秒空闲

        QCOMPARE(determineLocalPresence(inputs), PeerPresenceStatus::Online);
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TestPresenceHeuristics tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "TestPresenceHeuristics.moc"
