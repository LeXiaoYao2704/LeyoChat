#include <QtTest/QTest>
#include <QEventLoop>
#include <QFuture>
#include <QSqlDatabase>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>

#include "domain/ChatMessage.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"
#include "storage/FileTransferRepository.h"

class TestAsyncFlushWorker : public QObject {
    Q_OBJECT

private slots:
    void secondConnection_readsConversationsFromExistingDb()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("bg-conv.db"));

        const QString primaryConn = QStringLiteral("bg-primary-conv");
        DatabaseManager mgr(dbPath, primaryConn);
        QVERIFY(mgr.open());
        ConversationRepository primaryRepo(primaryConn);
        QVERIFY(primaryRepo.upsertConversation(
            ConversationSummary{L"conv-bg-1", L"Alice", L"hi", 1000}));

        const QString bgConn = QStringLiteral("leyochat-bg-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        std::vector<ConversationSummary> summaries;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), bgConn);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            ConversationRepository bgRepo(bgConn);
            summaries = bgRepo.loadConversationSummaries();
            db.close();
        }
        QSqlDatabase::removeDatabase(bgConn);

        QCOMPARE(summaries.size(), std::size_t(1));
        QCOMPARE(summaries[0].conversationId, std::wstring(L"conv-bg-1"));
    }

    void secondConnection_readsFileTasksFromExistingDb()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("bg-tasks.db"));

        const QString primaryConn = QStringLiteral("bg-primary-tasks");
        DatabaseManager mgr(dbPath, primaryConn);
        QVERIFY(mgr.open());
        FileTransferRepository primaryFt(primaryConn);

        FileTransferTask task{};
        task.taskId = L"task-bg-1"; task.conversationId = L"conv-bg-1";
        task.peerClientId = L"peer-x"; task.fileName = L"test.txt";
        task.direction = FileTransferDirection::Outgoing;
        task.state = FileTransferState::Completed;
        task.fileSize = 1024; task.chunkSize = 512; task.chunkCount = 2;
        task.bytesCompleted = 1024; task.createdAtMs = 1000; task.updatedAtMs = 2000;
        QVERIFY(primaryFt.upsertTask(task));

        const QString bgConn = QStringLiteral("leyochat-bg-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        std::vector<FileTransferTask> tasks;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(
                QStringLiteral("QSQLITE"), bgConn);
            db.setDatabaseName(dbPath);
            QVERIFY(db.open());
            FileTransferRepository bgFt(bgConn);
            tasks = bgFt.loadAllTasks();
            db.close();
        }
        QSqlDatabase::removeDatabase(bgConn);

        QCOMPARE(tasks.size(), std::size_t(1));
        QCOMPARE(tasks[0].taskId, std::wstring(L"task-bg-1"));
    }

    void qtConcurrentThen_deliversResultOnCallerThread()
    {
        QEventLoop loop;
        bool thenRan = false;
        int resultValue = 0;
        const Qt::HANDLE callerThread = QThread::currentThreadId();
        Qt::HANDLE thenThread = nullptr;

        QtConcurrent::run([]() -> int { return 42; })
            .then(this, [&](int val) {
                resultValue = val;
                thenRan    = true;
                thenThread = QThread::currentThreadId();
                loop.quit();
            });

        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        QVERIFY(thenRan);
        QCOMPARE(resultValue, 42);
        QCOMPARE(thenThread, callerThread);
    }
};

QTEST_MAIN(TestAsyncFlushWorker)
#include "TestAsyncFlushWorker.moc"
