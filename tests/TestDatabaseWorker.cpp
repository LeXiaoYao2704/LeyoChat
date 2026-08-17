#include <QtTest>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QThread>
#include <QSignalSpy>
#include <QUuid>

#include "store/DatabaseWorker.h"
#include "storage/DatabaseManager.h"
#include "storage/ConversationRepository.h"
#include "domain/ChatMessage.h"
#include "domain/ConversationSummary.h"

class TestDatabaseWorker : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void testLoadAllReturnsData();
    void testPersistMessageAndReload();
    void testPersistConversation();
    void testLoadMessagesForConversation();
    void testFlushReadReceiptsEnqueuesRemoteReadAck();
    void cleanupTestCase();

private:
    QString m_dbPath;
    QThread* m_thread = nullptr;
    DatabaseWorker* m_worker = nullptr;
};

void TestDatabaseWorker::initTestCase() {
    m_dbPath = QDir::tempPath() + "/test_db_worker_" +
               QUuid::createUuid().toString(QUuid::WithoutBraces) + ".db";

    const QString initConn = QStringLiteral("test-init");
    {
        DatabaseManager mgr(m_dbPath, initConn);
        QVERIFY(mgr.open());
    }
    if (QSqlDatabase::contains(initConn)) {
        QSqlDatabase::database(initConn, false).close();
        QSqlDatabase::removeDatabase(initConn);
    }

    m_thread = new QThread(this);
    m_worker = new DatabaseWorker(m_dbPath);
    m_worker->moveToThread(m_thread);
    m_thread->start();
}

void TestDatabaseWorker::testLoadAllReturnsData() {
    // Seed a conversation directly into the DB
    {
        const QString connName = QStringLiteral("test-seed");
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            db.setDatabaseName(m_dbPath);
            QVERIFY(db.open());
            ConversationRepository repo(connName);
            ConversationSummary cs;
            cs.conversationId = L"conv-1";
            cs.title = L"Test";
            cs.lastMessageAtMs = 1000;
            repo.upsertConversation(cs);
        }
        QSqlDatabase::removeDatabase(connName);
    }

    QSignalSpy spy(m_worker, &DatabaseWorker::allDataLoaded);
    QMetaObject::invokeMethod(m_worker, "loadAll", Qt::QueuedConnection);
    QVERIFY(spy.wait(5000));
    QCOMPARE(spy.count(), 1);

    auto conversations = spy.at(0).at(0).value<QVector<ConversationSummary>>();
    QVERIFY(!conversations.isEmpty());
}

void TestDatabaseWorker::testPersistMessageAndReload() {
    ChatMessage msg;
    msg.messageId = L"msg-1";
    msg.conversationId = L"conv-1";
    msg.senderId = L"user-1";
    msg.body = L"Hello World";
    msg.createdAtMs = QDateTime::currentMSecsSinceEpoch();
    msg.deliveryState = MessageDeliveryState::Pending;

    QMetaObject::invokeMethod(m_worker, [this, msg]() {
        m_worker->persistMessage(msg);
    }, Qt::QueuedConnection);

    // Wait for the async persist to complete
    QThread::msleep(500);
    QCoreApplication::processEvents();

    // Now load messages and verify round-trip
    QSignalSpy spy(m_worker, &DatabaseWorker::messagesLoaded);
    QMetaObject::invokeMethod(m_worker, [this]() {
        m_worker->loadMessagesForConversation(QStringLiteral("conv-1"));
    }, Qt::QueuedConnection);
    QVERIFY(spy.wait(5000));
    auto messages = spy.at(0).at(1).value<std::vector<ChatMessage>>();
    QVERIFY(!messages.empty());
    bool found = false;
    for (const auto& m : messages) {
        if (m.messageId == L"msg-1" && m.body == L"Hello World") {
            found = true;
            break;
        }
    }
    QVERIFY(found);
}

void TestDatabaseWorker::testPersistConversation() {
    ConversationSummary cs;
    cs.conversationId = L"conv-new";
    cs.title = L"New Conv";
    cs.lastMessageAtMs = 2000;

    QMetaObject::invokeMethod(m_worker, [this, cs]() {
        m_worker->persistConversation(cs);
    }, Qt::QueuedConnection);

    QThread::msleep(500);
    QCoreApplication::processEvents();

    // Reload all and verify
    QSignalSpy spy(m_worker, &DatabaseWorker::allDataLoaded);
    QMetaObject::invokeMethod(m_worker, "loadAll", Qt::QueuedConnection);
    QVERIFY(spy.wait(5000));

    auto conversations = spy.at(0).at(0).value<QVector<ConversationSummary>>();
    bool found = false;
    for (const auto& c : conversations) {
        if (c.conversationId == L"conv-new") { found = true; break; }
    }
    QVERIFY(found);
}

void TestDatabaseWorker::testLoadMessagesForConversation() {
    QSignalSpy spy(m_worker, &DatabaseWorker::messagesLoaded);
    QMetaObject::invokeMethod(m_worker, [this]() {
        m_worker->loadMessagesForConversation(QStringLiteral("nonexistent"));
    }, Qt::QueuedConnection);
    QVERIFY(spy.wait(5000));
    auto messages = spy.at(0).at(1).value<std::vector<ChatMessage>>();
    QVERIFY(messages.empty());
}

void TestDatabaseWorker::testFlushReadReceiptsEnqueuesRemoteReadAck() {
    QTemporaryDir dir; QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("worker-read-ack.db"));
    {
        const QString conn = QStringLiteral("worker-read-ack-seed");
        DatabaseManager mgr(dbPath, conn);
        QVERIFY(mgr.open());
        ConversationRepository repo(conn);

        QVERIFY(repo.upsertConversation(
            ConversationSummary{L"conv-service", L"Service", L"hello", 1000}));
        ChatMessage message;
        message.messageId = L"local-msg-1";
        message.conversationId = L"conv-service";
        message.senderId = L"peer-a";
        message.body = L"hello";
        message.createdAtMs = 1000;
        message.deliveryState = MessageDeliveryState::Received;
        QVERIFY(repo.appendMessage(message));
        QVERIFY(repo.saveRemoteMessageIdMapping(QStringLiteral("srv-1"),
                                                QStringLiteral("local-msg-1")));
        QVERIFY(repo.saveRemoteChatCursor(QStringLiteral("conv-service"), 42));
    }
    if (QSqlDatabase::contains(QStringLiteral("worker-read-ack-seed"))) {
        QSqlDatabase::database(QStringLiteral("worker-read-ack-seed"), false).close();
        QSqlDatabase::removeDatabase(QStringLiteral("worker-read-ack-seed"));
    }

    DatabaseWorker worker(dbPath);
    QSignalSpy spy(&worker, &DatabaseWorker::readReceiptsFlushed);
    worker.flushReadReceipts(QStringLiteral("conv-service"),
                             QStringLiteral("local-a"),
                             false);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(2).toBool(), true);

    const QString verifyConn = QStringLiteral("worker-read-ack-verify");
    QSqlDatabase verifyDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                      verifyConn);
    verifyDb.setDatabaseName(dbPath);
    QVERIFY(verifyDb.open());
    ConversationRepository repo(verifyConn);
    const auto pending = repo.loadPendingRemoteReadAcks(10);
    QCOMPARE(static_cast<int>(pending.size()), 1);
    QCOMPARE(pending.at(0).serverMessageId, QStringLiteral("srv-1"));
    QCOMPARE(pending.at(0).conversationId, QStringLiteral("conv-service"));
    QCOMPARE(pending.at(0).readSeq, qint64(42));

    ChatMessage stored;
    QVERIFY(repo.findMessageById(QStringLiteral("local-msg-1"), &stored));
    QCOMPARE(stored.deliveryState, MessageDeliveryState::Read);
    verifyDb.close();
    QSqlDatabase::removeDatabase(verifyConn);
}

void TestDatabaseWorker::cleanupTestCase() {
    m_thread->quit();
    m_thread->wait();
    delete m_worker;
    QFile::remove(m_dbPath);
}

QTEST_MAIN(TestDatabaseWorker)
#include "TestDatabaseWorker.moc"
