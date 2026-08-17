#include <QtTest>

#include "app/TransferSpeedTracker.h"

class TestTransferSpeedTracker : public QObject {
    Q_OBJECT

private slots:
    void firstSampleReturnsZero();
    void updatesSpeedAfterMinimumSamplingInterval();
    void ignoresUpdatesInsideMinimumSamplingInterval();
    void tracksTasksIndependently();
    void nonIncreasingBytesDoNotCreateInstantSpeed();
};

void TestTransferSpeedTracker::firstSampleReturnsZero()
{
    TransferSpeedTracker tracker;

    QCOMPARE(tracker.updateSpeed(QStringLiteral("task-a"), 128), 0);
}

void TestTransferSpeedTracker::updatesSpeedAfterMinimumSamplingInterval()
{
    TransferSpeedTracker tracker;
    QCOMPARE(tracker.updateSpeed(QStringLiteral("task-a"), 0), 0);

    QTest::qWait(230);

    const qint64 speed = tracker.updateSpeed(QStringLiteral("task-a"), 4096);
    QVERIFY(speed > 0);
}

void TestTransferSpeedTracker::ignoresUpdatesInsideMinimumSamplingInterval()
{
    TransferSpeedTracker tracker;
    QCOMPARE(tracker.updateSpeed(QStringLiteral("task-a"), 0), 0);

    QTest::qWait(230);
    const qint64 firstSpeed = tracker.updateSpeed(QStringLiteral("task-a"), 4096);
    QVERIFY(firstSpeed > 0);

    const qint64 immediateSpeed = tracker.updateSpeed(QStringLiteral("task-a"), 8192);
    QCOMPARE(immediateSpeed, firstSpeed);
}

void TestTransferSpeedTracker::tracksTasksIndependently()
{
    TransferSpeedTracker tracker;
    QCOMPARE(tracker.updateSpeed(QStringLiteral("task-a"), 0), 0);

    QTest::qWait(230);
    QVERIFY(tracker.updateSpeed(QStringLiteral("task-a"), 4096) > 0);

    QCOMPARE(tracker.updateSpeed(QStringLiteral("task-b"), 4096), 0);
}

void TestTransferSpeedTracker::nonIncreasingBytesDoNotCreateInstantSpeed()
{
    TransferSpeedTracker tracker;
    QCOMPARE(tracker.updateSpeed(QStringLiteral("task-a"), 4096), 0);

    QTest::qWait(230);

    QCOMPARE(tracker.updateSpeed(QStringLiteral("task-a"), 2048), 0);
}

QTEST_MAIN(TestTransferSpeedTracker)
#include "TestTransferSpeedTracker.moc"
