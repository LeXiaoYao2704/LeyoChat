#include <QtTest>

#include <QFile>

#include "app/MessagePresentationHelpers.h"

class TestMessagePresentationHelpers : public QObject {
    Q_OBJECT

private slots:
    void filePreviewText_trimsNameAndFallsBack();
    void humanReadableBytes_formatsUnits();
    void fileTransferProgressText_clampsCompletedBytes();
    void fileTransferProgressText_doesNotRoundIncompleteTransferToHundred();
    void chunkBytesForTask_handlesLastChunkAndInvalidInput();
    void completedBytesForTask_sumsCompletedChunks();
    void displayBytesForTransferState_capsCompletingBeforeFullSize();
    void transferTaskRetryable_matchesUserActionableStates();
    void messageBubbleWidgetUsesDisplaySafeTransferProgress();
    void localFilePathForTransferTask_selectsDirectionAwarePath();
    void outgoingFileTransferInOpenConversation_doesNotBecomeRead();
    void incomingFileTransfer_becomesReadOnlyWhenVisible();
    void groupEnvelopePreviewText_prefersJsonText();
};

void TestMessagePresentationHelpers::filePreviewText_trimsNameAndFallsBack()
{
    QCOMPARE(filePreviewText(QStringLiteral("  report.pdf  ")), QStringLiteral("[File] report.pdf"));
    QCOMPARE(filePreviewText(QStringLiteral("   ")), QStringLiteral("[File]"));
}

void TestMessagePresentationHelpers::humanReadableBytes_formatsUnits()
{
    QCOMPARE(humanReadableBytes(512), QStringLiteral("512 B"));
    QCOMPARE(humanReadableBytes(1536), QStringLiteral("1.5 KB"));
    QCOMPARE(humanReadableBytes(2 * 1024 * 1024), QStringLiteral("2.0 MB"));
}

void TestMessagePresentationHelpers::fileTransferProgressText_clampsCompletedBytes()
{
    QCOMPARE(fileTransferProgressText(150, 100), QStringLiteral("100%, 100 B / 100 B"));
    QCOMPARE(fileTransferProgressText(-10, 100), QStringLiteral("0%, 0 B / 100 B"));
    QVERIFY(fileTransferProgressText(10, 0).isEmpty());
}

void TestMessagePresentationHelpers::fileTransferProgressText_doesNotRoundIncompleteTransferToHundred()
{
    QCOMPARE(fileTransferProgressText(999, 1000), QStringLiteral("99%, 999 B / 1000 B"));
}

void TestMessagePresentationHelpers::chunkBytesForTask_handlesLastChunkAndInvalidInput()
{
    FileTransferTask task;
    task.fileSize = 250;
    task.chunkSize = 100;

    QCOMPARE(chunkBytesForTask(task, 0), 100);
    QCOMPARE(chunkBytesForTask(task, 2), 50);
    QCOMPARE(chunkBytesForTask(task, 3), 0);
    QCOMPARE(chunkBytesForTask(task, -1), 0);
}

void TestMessagePresentationHelpers::completedBytesForTask_sumsCompletedChunks()
{
    FileTransferTask task;
    task.fileSize = 250;
    task.chunkSize = 100;

    QCOMPARE(completedBytesForTask(task, {0, 2}), 150);
}

void TestMessagePresentationHelpers::displayBytesForTransferState_capsCompletingBeforeFullSize()
{
    FileTransferTask task;
    task.fileSize = 1000;
    task.bytesCompleted = 1000;

    QCOMPARE(displayBytesForTransferState(task, FileTransferState::Transferring), 1000);
    QCOMPARE(displayBytesForTransferState(task, FileTransferState::Completing), 999);
    QCOMPARE(displayBytesForTransferState(task, FileTransferState::Completed), 1000);
}

void TestMessagePresentationHelpers::transferTaskRetryable_matchesUserActionableStates()
{
    FileTransferTask task;
    task.state = FileTransferState::Failed;
    QVERIFY(transferTaskRetryable(task));

    task.state = FileTransferState::Interrupted;
    QVERIFY(transferTaskRetryable(task));

    task.state = FileTransferState::Transferring;
    QVERIFY(!transferTaskRetryable(task));

    task.state = FileTransferState::Completing;
    QVERIFY(!transferTaskRetryable(task));

    task.state = FileTransferState::Completed;
    QVERIFY(!transferTaskRetryable(task));
}

void TestMessagePresentationHelpers::messageBubbleWidgetUsesDisplaySafeTransferProgress()
{
    QFile source(QStringLiteral(LEYOCHAT_MESSAGE_BUBBLE_WIDGET_SOURCE_PATH));
    QVERIFY2(source.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(QStringLiteral("failed to open %1").arg(source.fileName())));

    const QString text = QString::fromUtf8(source.readAll());
    QVERIFY2(text.contains(QStringLiteral("transferDisplayBytesForState")),
             "message bubble transfer cards must cap Completing progress before 100%");
    QVERIFY2(text.contains(QStringLiteral("transferDisplayPercent")),
             "message bubble transfer cards must floor incomplete percentages instead of rounding to 100%");
}

void TestMessagePresentationHelpers::localFilePathForTransferTask_selectsDirectionAwarePath()
{
    FileTransferTask task;
    task.sourcePath = L"C:/sender/file.txt";
    task.targetPath = L"C:/receiver/file.txt";
    task.tempPath = L"C:/receiver/file.part";

    task.direction = FileTransferDirection::Outgoing;
    QCOMPARE(localFilePathForTransferTask(task), QStringLiteral("C:/sender/file.txt"));

    task.direction = FileTransferDirection::Incoming;
    task.state = FileTransferState::Transferring;
    QCOMPARE(localFilePathForTransferTask(task), QStringLiteral("C:/receiver/file.part"));

    task.state = FileTransferState::Completed;
    QCOMPARE(localFilePathForTransferTask(task), QStringLiteral("C:/receiver/file.txt"));
}

void TestMessagePresentationHelpers::outgoingFileTransferInOpenConversation_doesNotBecomeRead()
{
    QCOMPARE(effectiveFileTransferDeliveryState(FileTransferDirection::Outgoing,
                                                true,
                                                MessageDeliveryState::Pending),
             MessageDeliveryState::Pending);
    QCOMPARE(effectiveFileTransferDeliveryState(FileTransferDirection::Outgoing,
                                                true,
                                                MessageDeliveryState::Sent),
             MessageDeliveryState::Sent);
    QCOMPARE(effectiveFileTransferDeliveryState(FileTransferDirection::Outgoing,
                                                true,
                                                MessageDeliveryState::Failed),
             MessageDeliveryState::Failed);
}

void TestMessagePresentationHelpers::incomingFileTransfer_becomesReadOnlyWhenVisible()
{
    QCOMPARE(effectiveFileTransferDeliveryState(FileTransferDirection::Incoming,
                                                true,
                                                MessageDeliveryState::Received),
             MessageDeliveryState::Read);
    QCOMPARE(effectiveFileTransferDeliveryState(FileTransferDirection::Incoming,
                                                false,
                                                MessageDeliveryState::Received),
             MessageDeliveryState::Received);
    QCOMPARE(effectiveFileTransferDeliveryState(FileTransferDirection::Incoming,
                                                true,
                                                MessageDeliveryState::Failed),
             MessageDeliveryState::Failed);
}

void TestMessagePresentationHelpers::groupEnvelopePreviewText_prefersJsonText()
{
    MessageEnvelope envelope;
    envelope.type = MessageType::GroupMessage;
    envelope.body = R"({"text":"team update","attachment_name":"report.pdf"})";

    QCOMPARE(groupEnvelopePreviewText(envelope), QStringLiteral("team update"));
}

QTEST_MAIN(TestMessagePresentationHelpers)
#include "TestMessagePresentationHelpers.moc"
