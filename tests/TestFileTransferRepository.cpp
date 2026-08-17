#include <QtTest/QTest>

#include <cstdlib>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>

#include "domain/FileTransferTask.h"
#include "storage/DatabaseManager.h"
#include "storage/FileTransferRepository.h"

class TestFileTransferRepository : public QObject {
    Q_OBJECT

private slots:
    void savesAndLoadsTask();
    void updatesChunkProgress();
    void loadsResumableTasks();
    void includesCompletingTasksInResumableSet();
    void remapsLegacyConversationIds();
    void deletesSingleTaskAndChunks();
    void deletesTasksByState();
};

void TestFileTransferRepository::savesAndLoadsTask() {
    const QString databasePath = QStringLiteral("file-transfer.db");
    QFile::remove(databasePath);
    QFile::remove(QStringLiteral("file-transfer.sqlite"));
    const QString connectionName = QStringLiteral("file-transfer-test-db");

    DatabaseManager manager(databasePath, connectionName);
    QVERIFY(manager.open());

    FileTransferRepository repository(connectionName);
    const FileTransferTask task{
        L"task-1",
        L"conv-1",
        L"peer-1",
        {},
        L"C:/send/a.bin",
        L"C:/recv/a.bin",
        L"C:/recv/.transfer/task-1.part",
        L"a.bin",
        L"hash-1",
        {},
        {},
        FileTransferDirection::Outgoing,
        FileTransferState::WaitingAccept,
        4096,
        1024,
        0,
        100,
        100,
        4,
        -1
    };

    QVERIFY(repository.upsertTask(task));

    FileTransferTask loaded;
    QVERIFY(repository.findTaskById(QStringLiteral("task-1"), &loaded));
    QCOMPARE(loaded.fileName, std::wstring(L"a.bin"));
    QCOMPARE(loaded.fileSize, 4096);
    QCOMPARE(loaded.chunkCount, 4);
    QCOMPARE(loaded.state, FileTransferState::WaitingAccept);
}

void TestFileTransferRepository::updatesChunkProgress() {
    const QString databasePath = QStringLiteral("file-transfer-chunks.db");
    QFile::remove(databasePath);
    QFile::remove(QStringLiteral("file-transfer-chunks.sqlite"));
    const QString connectionName = QStringLiteral("file-transfer-chunks-db");

    DatabaseManager manager(databasePath, connectionName);
    QVERIFY(manager.open());

    FileTransferRepository repository(connectionName);
    QVERIFY(repository.recordCompletedChunk(QStringLiteral("task-2"), 0, 1024, 100));
    QVERIFY(repository.recordCompletedChunk(QStringLiteral("task-2"), 2, 1024, 200));

    const auto completed = repository.loadCompletedChunkIndexes(QStringLiteral("task-2"));
    QCOMPARE(completed.size(), 2);
    QCOMPARE(completed.front(), 0);
    QCOMPARE(completed.back(), 2);
}

void TestFileTransferRepository::loadsResumableTasks() {
    const QString databasePath = QStringLiteral("file-transfer-resume.db");
    QFile::remove(databasePath);
    QFile::remove(QStringLiteral("file-transfer-resume.sqlite"));
    const QString connectionName = QStringLiteral("file-transfer-resume-db");

    DatabaseManager manager(databasePath, connectionName);
    QVERIFY(manager.open());

    FileTransferRepository repository(connectionName);

    const FileTransferTask resumable{
        L"task-resume",
        L"conv-1",
        L"peer-1",
        {},
        L"C:/send/a.bin",
        L"C:/recv/a.bin",
        L"C:/recv/.transfer/task-resume.part",
        L"a.bin",
        L"hash-1",
        {},
        {},
        FileTransferDirection::Incoming,
        FileTransferState::Interrupted,
        2048,
        512,
        1024,
        100,
        200,
        4,
        1
    };
    const FileTransferTask completed{
        L"task-complete",
        L"conv-2",
        L"peer-2",
        {},
        L"C:/send/b.bin",
        L"C:/recv/b.bin",
        L"C:/recv/.transfer/task-complete.part",
        L"b.bin",
        L"hash-2",
        {},
        {},
        FileTransferDirection::Outgoing,
        FileTransferState::Completed,
        1024,
        512,
        1024,
        100,
        300,
        2,
        1
    };

    QVERIFY(repository.upsertTask(resumable));
    QVERIFY(repository.upsertTask(completed));

    const auto tasks = repository.loadResumableTasks();
    QCOMPARE(tasks.size(), 1);
    QCOMPARE(tasks.front().taskId, std::wstring(L"task-resume"));
}

void TestFileTransferRepository::includesCompletingTasksInResumableSet() {
    const QString databasePath = QStringLiteral("file-transfer-completing.db");
    QFile::remove(databasePath);
    const QString connectionName = QStringLiteral("file-transfer-completing-db");

    DatabaseManager manager(databasePath, connectionName);
    QVERIFY(manager.open());

    FileTransferRepository repository(connectionName);

    const FileTransferTask completing{
        L"task-completing",
        L"conv-1",
        L"peer-1",
        {},
        L"C:/send/a.bin",
        {},
        {},
        L"a.bin",
        L"hash-1",
        {},
        {},
        FileTransferDirection::Outgoing,
        FileTransferState::Completing,
        2048,
        512,
        2048,
        100,
        200,
        4,
        3
    };

    QVERIFY(repository.upsertTask(completing));

    const auto tasks = repository.loadResumableTasks();
    QCOMPARE(tasks.size(), 1);
    QCOMPARE(tasks.front().taskId, std::wstring(L"task-completing"));
    QCOMPARE(tasks.front().state, FileTransferState::Completing);
}

void TestFileTransferRepository::remapsLegacyConversationIds() {
    const QString databasePath = QStringLiteral("file-transfer-remap.db");
    QFile::remove(databasePath);
    const QString connectionName = QStringLiteral("file-transfer-remap-db");

    DatabaseManager manager(databasePath, connectionName);
    QVERIFY(manager.open());

    FileTransferRepository repository(connectionName);
    QVERIFY(repository.upsertTask(FileTransferTask{
        L"task-remap",
        L"peer-legacy",
        L"peer-legacy",
        {},
        L"C:/send/a.bin",
        L"C:/recv/a.bin",
        L"C:/recv/.transfer/task-remap.part",
        L"a.bin",
        L"hash-a",
        {},
        {},
        FileTransferDirection::Incoming,
        FileTransferState::WaitingAccept,
        1024,
        512,
        0,
        10,
        20,
        2,
        -1
    }));

    QVERIFY(repository.remapConversationId(QStringLiteral("peer-legacy"),
                                           QStringLiteral("local-001|peer-legacy")));

    const auto tasks = repository.loadAllTasks();
    QCOMPARE(tasks.size(), 1u);
    QCOMPARE(tasks.front().conversationId, std::wstring(L"local-001|peer-legacy"));
}

void TestFileTransferRepository::deletesSingleTaskAndChunks() {
    const QString databasePath = QStringLiteral("file-transfer-delete.db");
    QFile::remove(databasePath);
    const QString connectionName = QStringLiteral("file-transfer-delete-db");

    DatabaseManager manager(databasePath, connectionName);
    QVERIFY(manager.open());

    FileTransferRepository repository(connectionName);
    const FileTransferTask task{
        L"task-delete",
        L"conv-1",
        L"peer-1",
        {},
        L"C:/send/a.bin",
        L"C:/recv/a.bin",
        L"C:/recv/.transfer/task-delete.part",
        L"a.bin",
        L"hash-1",
        {},
        {},
        FileTransferDirection::Incoming,
        FileTransferState::Completed,
        2048,
        512,
        2048,
        100,
        200,
        4,
        3
    };
    QVERIFY(repository.upsertTask(task));
    QVERIFY(repository.recordCompletedChunk(QStringLiteral("task-delete"), 0, 512, 100));
    QVERIFY(repository.deleteTask(QStringLiteral("task-delete")));

    FileTransferTask loaded;
    QVERIFY(!repository.findTaskById(QStringLiteral("task-delete"), &loaded));
    QVERIFY(repository.loadCompletedChunkIndexes(QStringLiteral("task-delete")).empty());
}

void TestFileTransferRepository::deletesTasksByState() {
    const QString databasePath = QStringLiteral("file-transfer-bulk-delete.db");
    QFile::remove(databasePath);
    const QString connectionName = QStringLiteral("file-transfer-bulk-delete-db");

    DatabaseManager manager(databasePath, connectionName);
    QVERIFY(manager.open());

    FileTransferRepository repository(connectionName);
    QVERIFY(repository.upsertTask(FileTransferTask{
        L"task-complete", L"conv-1", L"peer-1", {}, {}, {}, {}, L"a.bin", L"hash-a", {}, {},
        FileTransferDirection::Incoming, FileTransferState::Completed, 1024, 512, 1024, 10, 20, 2, 1
    }));
    QVERIFY(repository.upsertTask(FileTransferTask{
        L"task-failed", L"conv-2", L"peer-2", {}, {}, {}, {}, L"b.bin", L"hash-b", {}, {},
        FileTransferDirection::Outgoing, FileTransferState::Failed, 1024, 512, 512, 10, 20, 2, 0
    }));
    QVERIFY(repository.upsertTask(FileTransferTask{
        L"task-active", L"conv-3", L"peer-3", {}, {}, {}, {}, L"c.bin", L"hash-c", {}, {},
        FileTransferDirection::Outgoing, FileTransferState::Transferring, 1024, 512, 512, 10, 20, 2, 0
    }));

    QCOMPARE(repository.deleteTasksByStates({FileTransferState::Completed, FileTransferState::Failed}), 2);

    const auto remaining = repository.loadAllTasks();
    QCOMPARE(remaining.size(), 1u);
    QCOMPARE(remaining.front().taskId, std::wstring(L"task-active"));
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    TestFileTransferRepository testCase;
    const int result = QTest::qExec(&testCase, argc, argv);
    std::_Exit(result);
}

#include "TestFileTransferRepository.moc"
