#include <QtTest/QTest>
#include <QTemporaryDir>

#include "storage/DatabaseManager.h"
#include "storage/ConversationRepository.h"

class TestConversationFlags : public QObject {
    Q_OBJECT

private slots:
    void flagsDefaultToFalseOnNewConversation() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("flags.db"));
        const QString conn = QStringLiteral("flags-default-test");
        DatabaseManager db(dbPath, conn);
        QVERIFY(db.open());

        ConversationRepository repo(conn);
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-1", L"Alice", L"hi", 1000}));

        const auto summaries = repo.loadConversationSummaries();
        QCOMPARE(summaries.size(), 1u);
        QVERIFY(!summaries[0].isPinned);
        QVERIFY(!summaries[0].isStarred);
        QVERIFY(!summaries[0].isMuted);
        QVERIFY(!summaries[0].isDone);
        QVERIFY(!summaries[0].isManuallyUnread);
    }

    void setFlagPersistsAcrossReload() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("flags2.db"));
        const QString conn = QStringLiteral("flags-persist-test");
        DatabaseManager db(dbPath, conn);
        QVERIFY(db.open());

        ConversationRepository repo(conn);
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-a", L"Bob", L"hello", 2000}));
        QVERIFY(repo.setConversationFlag(QStringLiteral("conv-a"), ConversationFlag::Pinned, true));
        QVERIFY(repo.setConversationFlag(QStringLiteral("conv-a"), ConversationFlag::Starred, true));

        const auto loaded = repo.loadConversationSummaries();
        QCOMPARE(loaded.size(), 1u);
        QVERIFY(loaded[0].isPinned);
        QVERIFY(loaded[0].isStarred);
        QVERIFY(!loaded[0].isDone);
    }

    void upsertReopensDoneConversationAndPreservesOtherFlags() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("flags3.db"));
        const QString conn = QStringLiteral("flags-preserve-test");
        DatabaseManager db(dbPath, conn);
        QVERIFY(db.open());

        ConversationRepository repo(conn);
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-b", L"Charlie", L"first", 1000}));
        QVERIFY(repo.setConversationFlag(QStringLiteral("conv-b"), ConversationFlag::Pinned, true));
        QVERIFY(repo.setConversationFlag(QStringLiteral("conv-b"), ConversationFlag::Starred, true));
        QVERIFY(repo.setConversationFlag(QStringLiteral("conv-b"), ConversationFlag::Done, true));

        // A new message reopens a closed conversation without losing user settings.
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-b", L"Charlie", L"second", 2000}));

        const auto loaded = repo.loadConversationSummaries();
        QCOMPARE(loaded.size(), 1u);
        QVERIFY(loaded[0].isPinned);
        QVERIFY(loaded[0].isStarred);
        QVERIFY(!loaded[0].isDone);
        QCOMPARE(loaded[0].lastMessagePreview, std::wstring(L"second"));
    }

    void pinnedConversationsAppearFirst() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("flags4.db"));
        const QString conn = QStringLiteral("flags-pin-order-test");
        DatabaseManager db(dbPath, conn);
        QVERIFY(db.open());

        ConversationRepository repo(conn);
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-old", L"Old", L"msg", 1000}));
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-new", L"New", L"msg", 9000}));
        // conv-old has older timestamp but is pinned
        QVERIFY(repo.setConversationFlag(QStringLiteral("conv-old"), ConversationFlag::Pinned, true));

        const auto loaded = repo.loadConversationSummaries();
        QCOMPARE(loaded.size(), 2u);
        QCOMPARE(loaded[0].conversationId, std::wstring(L"conv-old")); // pinned first
        QCOMPARE(loaded[1].conversationId, std::wstring(L"conv-new"));
    }

    void updateDeliveryStatePreservingRead_doesNotRegressReadMessage() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("flags5.db"));
        const QString conn = QStringLiteral("flags-preserve-read-test");
        DatabaseManager db(dbPath, conn);
        QVERIFY(db.open());

        ConversationRepository repo(conn);
        const ChatMessage message{
            L"msg-1",
            L"conv-read",
            L"peer-a",
            L"[File] demo.exe",
            3000,
            MessageDeliveryState::Read,
            L"demo.exe",
            {}
        };
        QVERIFY(repo.appendMessage(message));

        QVERIFY(repo.updateDeliveryStatePreservingRead(QStringLiteral("msg-1"),
                                                       MessageDeliveryState::Received));

        ChatMessage reloaded;
        QVERIFY(repo.findMessageById(QStringLiteral("msg-1"), &reloaded));
        QCOMPARE(reloaded.deliveryState, MessageDeliveryState::Read);
    }

    void emptyPreviewConversations_doNotSortAheadOfRealMessages() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString dbPath = dir.filePath(QStringLiteral("flags6.db"));
        const QString conn = QStringLiteral("flags-empty-preview-order-test");
        DatabaseManager db(dbPath, conn);
        QVERIFY(db.open());

        ConversationRepository repo(conn);
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-empty", L"Empty", L"", 999999}));
        QVERIFY(repo.upsertConversation(ConversationSummary{L"conv-real", L"Real", L"hello", 1000}));

        const auto loaded = repo.loadConversationSummaries();
        QCOMPARE(loaded.size(), 2u);
        QCOMPARE(loaded[0].conversationId, std::wstring(L"conv-real"));
        QCOMPARE(loaded[1].conversationId, std::wstring(L"conv-empty"));
    }
};

QTEST_MAIN(TestConversationFlags)
#include "TestConversationFlags.moc"
