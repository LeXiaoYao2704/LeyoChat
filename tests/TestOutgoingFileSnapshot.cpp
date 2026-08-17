#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QTest>

#include "services/OutgoingFileSnapshot.h"

class TestOutgoingFileSnapshot : public QObject {
    Q_OBJECT

private slots:
    void appendedSourceUsesPreparedLogicalSize();
    void truncatedSourceFailsPreparedBatch();
};

void TestOutgoingFileSnapshot::appendedSourceUsesPreparedLogicalSize()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString sourcePath = dir.filePath(QStringLiteral("active.log"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("ABCDEFGHIJ"), qint64(10));
    source.close();

    const auto snapshot = prepareOutgoingFileSnapshot(sourcePath);
    QVERIFY(snapshot.has_value());
    QCOMPARE(snapshot->fileSize, qint64(10));

    QVERIFY(source.open(QIODevice::Append));
    QCOMPARE(source.write("KLMNOP"), qint64(6));
    source.close();

    const auto batch = prepareOutgoingFileChunkBatch(
        sourcePath, 4, 3, 0, {}, 3, snapshot->fileSize);
    QVERIFY(batch.ok);
    QCOMPARE(batch.chunks.size(), std::size_t(3));
    QCOMPARE(QByteArray::fromBase64(batch.chunks.back().encodedBody), QByteArray("IJ"));
}

void TestOutgoingFileSnapshot::truncatedSourceFailsPreparedBatch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString sourcePath = dir.filePath(QStringLiteral("active.log"));

    QFile source(sourcePath);
    QVERIFY(source.open(QIODevice::WriteOnly));
    QCOMPARE(source.write("ABCDEFGHIJ"), qint64(10));
    source.close();

    const auto snapshot = prepareOutgoingFileSnapshot(sourcePath);
    QVERIFY(snapshot.has_value());
    QVERIFY(source.open(QIODevice::ReadWrite));
    QVERIFY(source.resize(snapshot->fileSize - 1));
    source.close();

    const auto batch = prepareOutgoingFileChunkBatch(
        sourcePath, 4, 3, 0, {}, 3, snapshot->fileSize);
    QVERIFY(!batch.ok);
    QCOMPARE(batch.errorCode, QStringLiteral("source_changed"));
}

QTEST_MAIN(TestOutgoingFileSnapshot)
#include "TestOutgoingFileSnapshot.moc"
