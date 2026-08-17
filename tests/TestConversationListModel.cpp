// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增111行/修改0行/删除0行; 总行数111行
// @AI-LastModified: 2026-04-16 21:46:08

#include <QtTest/QTest>
#include <QSignalSpy>
#include "ui/ConversationListModel.h"
#include "domain/ConversationSummary.h"

class TestConversationListModel : public QObject {
    Q_OBJECT

private slots:
    void setItems_emitsModelReset()
    {
        ConversationListModel model;
        QSignalSpy spy(&model, &QAbstractItemModel::modelReset);
        QVector<ConversationSummary> items;
        ConversationSummary s;
        s.conversationId = L"conv-1";
        s.title = L"Alice";
        s.lastMessagePreview = L"hello";
        s.lastMessageAtMs = 1000;
        items.push_back(s);
        model.setItems(items);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.rowCount(), 1);
    }

    void setItemsAndUnread_emitsSingleModelReset()
    {
        ConversationListModel model;
        QSignalSpy spy(&model, &QAbstractItemModel::modelReset);
        QVector<ConversationSummary> items;
        ConversationSummary s;
        s.conversationId = L"conv-1";
        s.title = L"Alice";
        s.lastMessagePreview = L"hello";
        s.lastMessageAtMs = 1000;
        items.push_back(s);
        QSet<QString> unread;
        unread.insert(QStringLiteral("conv-1"));
        model.setItemsAndUnread(items, unread);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(model.rowCount(), 1);
        QModelIndex idx = model.index(0);
        QCOMPARE(idx.data(ConversationListModel::HasUnreadRole).toBool(), true);
    }

    void setItemsAndUnread_noUnread_showsFalse()
    {
        ConversationListModel model;
        QVector<ConversationSummary> items;
        ConversationSummary s;
        s.conversationId = L"conv-1";
        s.title = L"Bob";
        s.lastMessagePreview = L"hi";
        s.lastMessageAtMs = 2000;
        items.push_back(s);
        model.setItemsAndUnread(items, {});
        QModelIndex idx = model.index(0);
        QCOMPARE(idx.data(ConversationListModel::HasUnreadRole).toBool(), false);
    }

    void emptyItems_returnsZeroRows()
    {
        ConversationListModel model;
        model.setItems({});
        QCOMPARE(model.rowCount(), 0);
    }

    void filter_unread_showsOnlyUnreadItems()
    {
        ConversationListModel model;
        QVector<ConversationSummary> items;
        ConversationSummary s1;
        s1.conversationId = L"conv-1";
        s1.title = L"Alice";
        s1.lastMessagePreview = L"hello";
        s1.lastMessageAtMs = 1000;
        ConversationSummary s2;
        s2.conversationId = L"conv-2";
        s2.title = L"Bob";
        s2.lastMessagePreview = L"hi";
        s2.lastMessageAtMs = 2000;
        items.push_back(s1);
        items.push_back(s2);
        QSet<QString> unread;
        unread.insert(QStringLiteral("conv-1"));
        model.setItemsAndUnread(items, unread);
        QCOMPARE(model.rowCount(), 2);
        model.setFilter(1);
        QCOMPARE(model.rowCount(), 1);
    }

    void totalUnreadCount_ignoresActiveFilter()
    {
        ConversationListModel model;
        QVector<ConversationSummary> items;

        ConversationSummary groupConv;
        groupConv.conversationId = L"group-1";
        groupConv.title = L"项目群";
        groupConv.lastMessagePreview = L"群消息";
        groupConv.lastMessageAtMs = 1000;

        ConversationSummary directConv;
        directConv.conversationId = L"alice|self";
        directConv.title = L"Alice";
        directConv.lastMessagePreview = L"直聊消息";
        directConv.lastMessageAtMs = 2000;

        items.push_back(groupConv);
        items.push_back(directConv);

        model.setItemsAndUnread(items, QSet<QString>{QStringLiteral("alice|self")});
        QCOMPARE(model.totalUnreadCount(), 1);
        QCOMPARE(model.totalGroupUnreadCount(), 0);

        model.setFilter(static_cast<int>(ConversationListModel::Filter::Group));
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.totalUnreadCount(), 1);
        QCOMPARE(model.totalGroupUnreadCount(), 0);
    }
};

QTEST_MAIN(TestConversationListModel)
#include "TestConversationListModel.moc"
