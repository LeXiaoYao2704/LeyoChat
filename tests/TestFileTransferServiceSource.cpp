#include <QtTest/QTest>

#include <QFile>

class TestFileTransferServiceSource : public QObject {
    Q_OBJECT

private slots:
    void outgoingTasksUseControlChannelFriendlyChunkSize()
    {
        QFile source(QStringLiteral(LEYOCHAT_FILE_TRANSFER_SERVICE_SOURCE_PATH));
        QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
                 qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

        const QString text = QString::fromUtf8(source.readAll());
        QVERIFY2(text.contains(QStringLiteral("constexpr qint64 kDefaultChunkSize = 64 * 1024")),
                 "P2P file chunks should stay at 64KB to avoid blocking the control channel");

        const qsizetype begin =
            text.indexOf(QStringLiteral("bool FileTransferService::createOutgoingTask"));
        QVERIFY2(begin >= 0, "createOutgoingTask implementation must exist");
        const qsizetype end =
            text.indexOf(QStringLiteral("bool FileTransferService::acceptIncomingOffer"), begin);
        QVERIFY2(end > begin, "createOutgoingTask block should be bounded");

        const QString block = text.mid(begin, end - begin);
        QVERIFY2(block.contains(QStringLiteral("task.chunkSize = kDefaultChunkSize")),
                 "new outgoing tasks should use the centralized control-channel chunk size");
    }
};

QTEST_MAIN(TestFileTransferServiceSource)
#include "TestFileTransferServiceSource.moc"
