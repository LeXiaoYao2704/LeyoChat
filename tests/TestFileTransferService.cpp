#include <QtTest/QTest>

#include <cstdlib>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include "services/FileTransferService.h"
#include "storage/DatabaseManager.h"
#include "storage/FileTransferRepository.h"

class TestFileTransferService : public QObject {
    Q_OBJECT

private slots:
    void allowsReadyHandshakeWhenControlChannelChunkModeIsEnabled()
    {
        QVERIFY(FileTransferService::shouldSendReadyEnvelope(false));
        QVERIFY(FileTransferService::shouldSendReadyEnvelope(true));
    }

    void createOutgoingTask_usesControlChannelFriendlyChunkSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath(QStringLiteral("file-transfer-service.db"));
        const QString connectionName = QStringLiteral("file-transfer-service-create");
        DatabaseManager manager(dbPath, connectionName);
        QVERIFY(manager.open());

        const QString sourcePath = dir.filePath(QStringLiteral("payload.bin"));
        QFile sourceFile(sourcePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QVERIFY(sourceFile.write(QByteArray(200000, 'A')) == 200000);
        sourceFile.close();

        FileTransferRepository repository(connectionName);
        FileTransferService service(&repository);

        FileTransferTask task;
        QVERIFY(service.createOutgoingTask(QStringLiteral("conv-1"),
                                           QStringLiteral("peer-1"),
                                           QString(),
                                           sourcePath,
                                           100,
                                           &task));

        QCOMPARE(task.state, FileTransferState::WaitingAccept);
        QVERIFY(task.chunkSize <= 64 * 1024);
        QVERIFY(task.chunkCount >= 4);
    }

    void acceptIncomingOffer_refreshesExistingTaskToReadyState()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath(QStringLiteral("file-transfer-refresh.db"));
        const QString connectionName = QStringLiteral("file-transfer-service-refresh");
        DatabaseManager manager(dbPath, connectionName);
        QVERIFY(manager.open());

        FileTransferRepository repository(connectionName);
        FileTransferService service(&repository);

        const QString downloadRoot = dir.filePath(QStringLiteral("downloads"));
        QDir().mkpath(downloadRoot);

        const FileTransferTask staleTask{
            L"task-1",
            L"conv-old",
            L"peer-old",
            {},
            {},
            downloadRoot.toStdWString() + L"/old.bin",
            downloadRoot.toStdWString() + L"/.transfer/task-1.part",
            L"old.bin",
            L"hash-old",
            L"transfer_failed",
            L"stale",
            FileTransferDirection::Incoming,
            FileTransferState::Failed,
            4096,
            1024,
            0,
            10,
            20,
            4,
            -1
        };
        QVERIFY(repository.upsertTask(staleTask));

        FileControlPayload payload;
        payload.type = FileControlType::Offer;
        payload.taskId = "task-1";
        payload.conversationId = "conv-new";
        payload.senderId = "peer-new";
        payload.fileName = "report.bin";
        payload.fileHash = "hash-new";
        payload.fileSize = 8192;
        payload.chunkSize = 2048;
        payload.chunkCount = 4;

        FileTransferTask acceptedTask;
        QVERIFY(service.acceptIncomingOffer(payload, downloadRoot, 200, &acceptedTask));

        QCOMPARE(acceptedTask.state, FileTransferState::ReadyToTransfer);
        QCOMPARE(QString::fromStdWString(acceptedTask.peerClientId), QStringLiteral("peer-new"));
        QCOMPARE(QString::fromStdWString(acceptedTask.conversationId), QStringLiteral("conv-new"));
        QCOMPARE(QString::fromStdWString(acceptedTask.fileName), QStringLiteral("report.bin"));
        QCOMPARE(acceptedTask.fileSize, 8192);
        QCOMPARE(acceptedTask.chunkSize, 2048);

        FileTransferTask reloadedTask;
        QVERIFY(repository.findTaskById(QStringLiteral("task-1"), &reloadedTask));
        QCOMPARE(reloadedTask.state, FileTransferState::ReadyToTransfer);
        QCOMPARE(QString::fromStdWString(reloadedTask.peerClientId), QStringLiteral("peer-new"));
        QCOMPARE(QString::fromStdWString(reloadedTask.errorCode), QString());
        QCOMPARE(reloadedTask.bytesCompleted, 0);
    }

    void acceptIncomingOffer_preservesProgressForDuplicateOffer()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath(QStringLiteral("file-transfer-duplicate-offer.db"));
        const QString connectionName = QStringLiteral("file-transfer-service-duplicate-offer");
        DatabaseManager manager(dbPath, connectionName);
        QVERIFY(manager.open());

        FileTransferRepository repository(connectionName);
        FileTransferService service(&repository);

        const QString downloadRoot = dir.filePath(QStringLiteral("downloads"));
        QDir().mkpath(downloadRoot);
        const QString targetPath = dir.filePath(QStringLiteral("downloads/report.bin"));
        const QString tempPath = dir.filePath(QStringLiteral("downloads/.transfer/task-dup.part"));
        QDir().mkpath(QFileInfo(tempPath).absolutePath());
        QFile partFile(tempPath);
        QVERIFY(partFile.open(QIODevice::WriteOnly));
        QVERIFY(partFile.write("progress") > 0);
        partFile.close();

        const FileTransferTask activeTask{
            L"task-dup",
            L"group-1",
            L"peer-a",
            L"group-1",
            {},
            targetPath.toStdWString(),
            tempPath.toStdWString(),
            L"report.bin",
            L"hash-1",
            {},
            {},
            FileTransferDirection::Incoming,
            FileTransferState::Transferring,
            4096,
            1024,
            2048,
            10,
            20,
            4,
            1
        };
        QVERIFY(repository.upsertTask(activeTask));
        QVERIFY(repository.recordCompletedChunk(QStringLiteral("task-dup"), 0, 1024, 100));
        QVERIFY(repository.recordCompletedChunk(QStringLiteral("task-dup"), 1, 1024, 100));

        FileControlPayload payload;
        payload.type = FileControlType::Offer;
        payload.taskId = "task-dup";
        payload.conversationId = "group-1";
        payload.groupId = "group-1";
        payload.senderId = "peer-a";
        payload.fileName = "report.bin";
        payload.fileHash = "hash-1";
        payload.fileSize = 4096;
        payload.chunkSize = 1024;
        payload.chunkCount = 4;

        FileTransferTask acceptedTask;
        QVERIFY(service.acceptIncomingOffer(payload, downloadRoot, 200, &acceptedTask));

        QCOMPARE(acceptedTask.state, FileTransferState::Transferring);
        QCOMPARE(acceptedTask.bytesCompleted, 2048);
        QCOMPARE(acceptedTask.lastChunkIndex, 1);
        QVERIFY(QFileInfo::exists(tempPath));
        const auto completedChunks = repository.loadCompletedChunkIndexes(QStringLiteral("task-dup"));
        QCOMPARE(completedChunks.size(), static_cast<std::size_t>(2));
        QCOMPARE(completedChunks[0], 0);
        QCOMPARE(completedChunks[1], 1);
    }

    void prepareOutgoingChunkBatch_skipsCompletedChunks_andEncodesPayload()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = dir.filePath(QStringLiteral("payload.bin"));
        QFile sourceFile(sourcePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QVERIFY(sourceFile.write("ABCDEFGHIJKL") == 12);
        sourceFile.close();

        const QSet<int> completedChunks{1};
        const auto batch = FileTransferService::prepareOutgoingChunkBatch(sourcePath,
                                                                          4,
                                                                          3,
                                                                          0,
                                                                          completedChunks,
                                                                          3);

        QVERIFY(batch.ok);
        QCOMPARE(batch.chunks.size(), static_cast<std::size_t>(2));
        QCOMPARE(batch.chunks[0].chunkIndex, 0);
        QCOMPARE(batch.chunks[0].encodedBody, QByteArray("ABCD").toBase64());
        QCOMPARE(batch.chunks[1].chunkIndex, 2);
        QCOMPARE(batch.chunks[1].encodedBody, QByteArray("IJKL").toBase64());
        QCOMPARE(batch.nextChunkIndex, 3);
        QVERIFY(batch.reachedEnd);
    }

    void appendedSourceUsesPreparedLogicalSize()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = dir.filePath(QStringLiteral("active.log"));
        QFile sourceFile(sourcePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QCOMPARE(sourceFile.write("ABCDEFGHIJ"), qint64(10));
        sourceFile.close();

        const auto snapshot = FileTransferService::prepareOutgoingFile(sourcePath);
        QVERIFY(snapshot.has_value());
        QCOMPARE(snapshot->fileSize, qint64(10));

        QVERIFY(sourceFile.open(QIODevice::Append));
        QCOMPARE(sourceFile.write("KLMNOP"), qint64(6));
        sourceFile.close();

        const auto batch = FileTransferService::prepareOutgoingChunkBatch(sourcePath,
                                                                          4,
                                                                          3,
                                                                          0,
                                                                          {},
                                                                          3,
                                                                          snapshot->fileSize);
        QVERIFY(batch.ok);
        QCOMPARE(batch.chunks.size(), static_cast<std::size_t>(3));
        QCOMPARE(QByteArray::fromBase64(batch.chunks.back().encodedBody), QByteArray("IJ"));
    }

    void truncatedSourceFailsPreparedBatch()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString sourcePath = dir.filePath(QStringLiteral("active.log"));
        QFile sourceFile(sourcePath);
        QVERIFY(sourceFile.open(QIODevice::WriteOnly));
        QCOMPARE(sourceFile.write("ABCDEFGHIJ"), qint64(10));
        sourceFile.close();

        const auto snapshot = FileTransferService::prepareOutgoingFile(sourcePath);
        QVERIFY(snapshot.has_value());
        QVERIFY(sourceFile.open(QIODevice::ReadWrite));
        QVERIFY(sourceFile.resize(snapshot->fileSize - 1));
        sourceFile.close();

        const int chunkCount = static_cast<int>((snapshot->fileSize + 3) / 4);
        const auto batch = FileTransferService::prepareOutgoingChunkBatch(sourcePath,
                                                                          4,
                                                                          chunkCount,
                                                                          0,
                                                                          {},
                                                                          20,
                                                                          snapshot->fileSize);
        QVERIFY(!batch.ok);
        QCOMPARE(batch.errorCode, QStringLiteral("source_changed"));
    }

    void applyRemoteReady_doesNotDowngradeTransferringTask()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath(QStringLiteral("file-transfer-remote-ready.db"));
        const QString connectionName = QStringLiteral("file-transfer-service-remote-ready");
        DatabaseManager manager(dbPath, connectionName);
        QVERIFY(manager.open());

        FileTransferRepository repository(connectionName);
        FileTransferService service(&repository);

        const FileTransferTask activeTask{
            L"task-ready",
            L"conv-1",
            L"peer-a",
            {},
            L"C:/tmp/source.bin",
            {},
            {},
            L"source.bin",
            L"hash-1",
            {},
            {},
            FileTransferDirection::Outgoing,
            FileTransferState::Transferring,
            4096,
            1024,
            2048,
            10,
            20,
            4,
            1
        };
        QVERIFY(repository.upsertTask(activeTask));
        QVERIFY(repository.recordCompletedChunk(QStringLiteral("task-ready"), 0, 1024, 100));
        QVERIFY(repository.recordCompletedChunk(QStringLiteral("task-ready"), 1, 1024, 100));

        FileControlPayload payload;
        payload.type = FileControlType::Accept;
        payload.taskId = "task-ready";
        payload.senderId = "peer-a";
        payload.completedChunks = {0};

        QVERIFY(service.applyRemoteReady(payload, 200));

        FileTransferTask reloadedTask;
        QVERIFY(repository.findTaskById(QStringLiteral("task-ready"), &reloadedTask));
        QCOMPARE(reloadedTask.state, FileTransferState::Transferring);
        QCOMPARE(reloadedTask.bytesCompleted, 2048);
        QCOMPARE(reloadedTask.lastChunkIndex, 1);
    }

    void applyRemoteProgress_doesNotDowngradeCompletingTask()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString dbPath = dir.filePath(QStringLiteral("file-transfer-remote-progress-completing.db"));
        const QString connectionName = QStringLiteral("file-transfer-service-remote-progress-completing");
        DatabaseManager manager(dbPath, connectionName);
        QVERIFY(manager.open());

        FileTransferRepository repository(connectionName);
        FileTransferService service(&repository);

        const FileTransferTask completingTask{
            L"task-progress",
            L"conv-1",
            L"peer-a",
            {},
            L"C:/tmp/source.bin",
            {},
            {},
            L"source.bin",
            L"hash-1",
            {},
            {},
            FileTransferDirection::Outgoing,
            FileTransferState::Completing,
            4096,
            1024,
            4096,
            10,
            20,
            4,
            3
        };
        QVERIFY(repository.upsertTask(completingTask));

        FileControlPayload payload;
        payload.type = FileControlType::Progress;
        payload.taskId = "task-progress";
        payload.senderId = "peer-a";
        payload.completedChunks = {0, 1, 2, 3};

        QVERIFY(service.applyRemoteProgress(payload, 200));

        FileTransferTask reloadedTask;
        QVERIFY(repository.findTaskById(QStringLiteral("task-progress"), &reloadedTask));
        QCOMPARE(reloadedTask.state, FileTransferState::Completing);
        QCOMPARE(reloadedTask.bytesCompleted, 4096);
        QCOMPARE(reloadedTask.lastChunkIndex, 3);
    }

};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TestFileTransferService testCase;
    const int result = QTest::qExec(&testCase, argc, argv);
    std::_Exit(result);
}

#include "TestFileTransferService.moc"
