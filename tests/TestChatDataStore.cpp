#include <QtTest>
#include "store/ChatDataStore.h"

class TestChatDataStore : public QObject {
    Q_OBJECT
private slots:
    void testUpsertAndQueryConversation();
    void testConversationOrdering();
    void testSetConversationFlag();
    void testRemoveConversation();
    void testUpsertEmitsSignal();
    void testAppendMessage();
    void testUpdateDeliveryState();
    void testMessageLruEviction();
    void testRecallMessage();
    void testEditMessage();
    void testGroupMemberEntriesCache();
    void testGroupMemberEntriesCacheInvalidation();
    void testBulkLoadConversations();
    void testSetMessagesSkipsResetWhenUnchanged();
};

void TestChatDataStore::testUpsertAndQueryConversation() {
    ChatDataStore store;
    ConversationSummary conv;
    conv.conversationId = L"conv-1";
    conv.title = L"Alice";
    conv.lastMessagePreview = L"Hello";
    conv.lastMessageAtMs = 1000;

    store.upsertConversation(conv);

    auto result = store.conversation(QStringLiteral("conv-1"));
    QVERIFY(result.has_value());
    QCOMPARE(QString::fromStdWString(result->title), QStringLiteral("Alice"));
    QCOMPARE(result->lastMessageAtMs, qint64(1000));
}

void TestChatDataStore::testConversationOrdering() {
    ChatDataStore store;
    // 插入 3 个会话: conv-1(t=100), conv-2(t=300, pinned), conv-3(t=200)
    ConversationSummary c1; c1.conversationId = L"conv-1"; c1.lastMessageAtMs = 100;
    ConversationSummary c2; c2.conversationId = L"conv-2"; c2.lastMessageAtMs = 300; c2.isPinned = true;
    ConversationSummary c3; c3.conversationId = L"conv-3"; c3.lastMessageAtMs = 200;
    store.upsertConversation(c1);
    store.upsertConversation(c2);
    store.upsertConversation(c3);

    auto all = store.allConversations();
    // 预期排序: pinned 优先(conv-2), 然后按时间倒序(conv-3, conv-1)
    QCOMPARE(all.size(), 3);
    QCOMPARE(QString::fromStdWString(all[0].conversationId), QStringLiteral("conv-2"));
    QCOMPARE(QString::fromStdWString(all[1].conversationId), QStringLiteral("conv-3"));
    QCOMPARE(QString::fromStdWString(all[2].conversationId), QStringLiteral("conv-1"));
}

void TestChatDataStore::testSetConversationFlag() {
    ChatDataStore store;
    ConversationSummary conv;
    conv.conversationId = L"conv-1";
    store.upsertConversation(conv);

    store.setConversationFlag(QStringLiteral("conv-1"), ConversationFlag::Pinned, true);
    auto result = store.conversation(QStringLiteral("conv-1"));
    QVERIFY(result.has_value());
    QVERIFY(result->isPinned);

    store.setConversationFlag(QStringLiteral("conv-1"), ConversationFlag::Pinned, false);
    result = store.conversation(QStringLiteral("conv-1"));
    QVERIFY(!result->isPinned);
}

void TestChatDataStore::testRemoveConversation() {
    ChatDataStore store;
    ConversationSummary conv;
    conv.conversationId = L"conv-1";
    store.upsertConversation(conv);

    store.removeConversation(QStringLiteral("conv-1"));
    QVERIFY(!store.conversation(QStringLiteral("conv-1")).has_value());
    QCOMPARE(store.allConversations().size(), 0);
}

void TestChatDataStore::testUpsertEmitsSignal() {
    ChatDataStore store;
    QSignalSpy spy(&store, &ChatDataStore::conversationUpserted);

    ConversationSummary conv;
    conv.conversationId = L"conv-1";
    store.upsertConversation(conv);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("conv-1"));
}

void TestChatDataStore::testAppendMessage() {
    ChatDataStore store;
    QSignalSpy spy(&store, &ChatDataStore::messageAppended);

    ChatMessage msg;
    msg.messageId = L"msg-1";
    msg.conversationId = L"conv-1";
    msg.body = L"Hello";
    msg.createdAtMs = 1000;

    store.appendMessage(QStringLiteral("conv-1"), msg);

    QCOMPARE(store.messages(QStringLiteral("conv-1")).size(), size_t(1));
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(1).toInt(), 0);  // newIndex = 0
}

void TestChatDataStore::testUpdateDeliveryState() {
    ChatDataStore store;
    ChatMessage msg;
    msg.messageId = L"msg-1";
    msg.deliveryState = MessageDeliveryState::Pending;
    store.appendMessage(QStringLiteral("conv-1"), msg);

    QSignalSpy spy(&store, &ChatDataStore::messageUpdated);
    store.updateDeliveryState(QStringLiteral("conv-1"), QStringLiteral("msg-1"),
                              MessageDeliveryState::Sent);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(store.messages(QStringLiteral("conv-1"))[0].deliveryState,
             MessageDeliveryState::Sent);
}

void TestChatDataStore::testMessageLruEviction() {
    ChatDataStore store;
    // 添加 31 个会话的消息 (LRU 容量 = 30)
    for (int i = 0; i < 31; ++i) {
        ChatMessage msg;
        msg.messageId = QString("msg-%1").arg(i).toStdWString();
        store.appendMessage(QString("conv-%1").arg(i), msg);
    }
    // conv-0 应该被淘汰
    QVERIFY(!store.hasMessages(QStringLiteral("conv-0")));
    // conv-30 应该存在
    QVERIFY(store.hasMessages(QStringLiteral("conv-30")));
}

void TestChatDataStore::testRecallMessage() {
    ChatDataStore store;
    ChatMessage msg;
    msg.messageId = L"msg-1";
    msg.isRecalled = false;
    store.appendMessage(QStringLiteral("conv-1"), msg);

    store.recallMessage(QStringLiteral("conv-1"), QStringLiteral("msg-1"), 5000);

    const auto& msgs = store.messages(QStringLiteral("conv-1"));
    QVERIFY(msgs[0].isRecalled);
    QCOMPARE(msgs[0].recalledAtMs, qint64(5000));
}

void TestChatDataStore::testEditMessage() {
    ChatDataStore store;
    ChatMessage msg;
    msg.messageId = L"msg-1";
    msg.body = L"Original";
    store.appendMessage(QStringLiteral("conv-1"), msg);

    store.editMessage(QStringLiteral("conv-1"), QStringLiteral("msg-1"),
                      L"Edited", 6000, L"editor-1");

    const auto& msgs = store.messages(QStringLiteral("conv-1"));
    QCOMPARE(QString::fromStdWString(msgs[0].body), QStringLiteral("Edited"));
    QCOMPARE(msgs[0].editedAtMs, qint64(6000));
}

void TestChatDataStore::testGroupMemberEntriesCache() {
    ChatDataStore store;
    store.setLocalClientId(QStringLiteral("me"));
    store.setDisplayNameResolver([](const QString& id) -> QString {
        if (id == "alice") return "Alice";
        if (id == "me") return "我";
        return {};
    });
    store.setAvatarPathResolver([](const QString&) { return QString(); });
    store.setOnlineChecker([](const QString& id) { return id == "me"; });

    Group g;
    g.groupId = L"g1";
    g.ownerClientId = L"alice";
    store.upsertGroup(g);

    std::vector<GroupMember> members;
    GroupMember m1; m1.memberClientId = L"alice"; m1.isActive = true;
    GroupMember m2; m2.memberClientId = L"me"; m2.isActive = true;
    members.push_back(m1);
    members.push_back(m2);
    store.setGroupMembers(QStringLiteral("g1"), members);

    auto entries = store.groupMemberEntries(QStringLiteral("g1"));
    QCOMPARE(entries.size(), 2);
    // owner (alice) 排第一
    QCOMPARE(entries[0].displayName, QStringLiteral("Alice"));
    QVERIFY(entries[0].isOwner);
}

void TestChatDataStore::testGroupMemberEntriesCacheInvalidation() {
    ChatDataStore store;
    store.setLocalClientId(QStringLiteral("me"));
    store.setDisplayNameResolver([](const QString& id) -> QString {
        return id;
    });
    store.setAvatarPathResolver([](const QString&) { return QString(); });
    store.setOnlineChecker([](const QString&) { return false; });

    Group g; g.groupId = L"g1";
    store.upsertGroup(g);
    std::vector<GroupMember> members;
    GroupMember m1; m1.memberClientId = L"alice"; m1.isActive = true;
    members.push_back(m1);
    store.setGroupMembers(QStringLiteral("g1"), members);

    // 首次构建缓存
    store.groupMemberEntries(QStringLiteral("g1"));
    // 更改群成员 — 应失效缓存
    GroupMember m2; m2.memberClientId = L"bob"; m2.isActive = true;
    members.push_back(m2);
    store.setGroupMembers(QStringLiteral("g1"), members);

    auto entries = store.groupMemberEntries(QStringLiteral("g1"));
    QCOMPARE(entries.size(), 2);
}

void TestChatDataStore::testBulkLoadConversations() {
    ChatDataStore store;
    QSignalSpy listSpy(&store, &ChatDataStore::conversationListChanged);
    QSignalSpy unreadSpy(&store, &ChatDataStore::unreadSetChanged);

    QVector<ConversationSummary> items;
    ConversationSummary c1; c1.conversationId = L"conv-1"; c1.lastMessageAtMs = 100;
    ConversationSummary c2; c2.conversationId = L"conv-2"; c2.lastMessageAtMs = 200;
    items.push_back(c1);
    items.push_back(c2);

    QSet<QString> unread;
    unread.insert(QStringLiteral("conv-1"));

    store.bulkLoadConversations(items, unread);

    QCOMPARE(store.allConversations().size(), 2);
    QVERIFY(store.unreadConversationIds().contains(QStringLiteral("conv-1")));
    QCOMPARE(listSpy.count(), 1);
    QCOMPARE(unreadSpy.count(), 1);
}

void TestChatDataStore::testSetMessagesSkipsResetWhenUnchanged() {
    ChatDataStore store;
    QSignalSpy resetSpy(&store, &ChatDataStore::messagesReset);

    ChatMessage msg;
    msg.messageId = L"msg-1";
    msg.conversationId = L"conv-1";
    msg.body = L"hello";
    msg.createdAtMs = 1000;
    msg.deliveryState = MessageDeliveryState::Sent;
    msg.localFilePath = L"C:/tmp/a.png";
    msg.fileCardJson = L"{\"name\":\"a.png\"}";

    std::vector<ChatMessage> first{msg};
    store.setMessages(QStringLiteral("conv-1"), first);
    QCOMPARE(resetSpy.count(), 1);

    std::vector<ChatMessage> second{msg};
    store.setMessages(QStringLiteral("conv-1"), second);
    QCOMPARE(resetSpy.count(), 1);
}

QTEST_MAIN(TestChatDataStore)
#include "TestChatDataStore.moc"
