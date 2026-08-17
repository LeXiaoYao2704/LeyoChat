#include <QtTest/QTest>

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QTemporaryDir>

#include <optional>

#include "domain/ConversationSummary.h"
#include "integrations/ServerMessageClient.h"
#include "services/RemoteMessageSyncCoordinator.h"
#include "storage/ConversationRepository.h"
#include "storage/DatabaseManager.h"

namespace {

class FakeServerMessageClient final : public IServerMessageClient {
public:
    struct ListCall {
        QString conversationId;
        qint64 afterSeq = -1;
        int limit = 0;
    };

    struct ReadAckCall {
        QString serverMessageId;
        qint64 readSeq = -1;
    };

    QHash<QString, ServerMessagePage> pagesByConversation;
    QSet<QString> failingConversations;
    mutable QVector<ListCall> listCalls;
    mutable QVector<ReadAckCall> readAckCalls;

    std::optional<ServerMessageAck> sendMessage(
        const ServerMessageDraft&,
        QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("send not used by coordinator tests");
        }
        return std::nullopt;
    }

    std::optional<ServerMessagePage> listMessages(
        const QString& conversationId,
        qint64 afterSeq,
        int limit,
        QString* errorMessage = nullptr) const override
    {
        listCalls.push_back({conversationId, afterSeq, limit});
        if (failingConversations.contains(conversationId)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("service unavailable for %1")
                                    .arg(conversationId);
            }
            return std::nullopt;
        }
        return pagesByConversation.value(conversationId);
    }

    bool acknowledgeDelivered(const QString&,
                              qint64,
                              QString* errorMessage = nullptr) const override
    {
        if (errorMessage) {
            *errorMessage = QStringLiteral("delivery ack not used by tests");
        }
        return false;
    }

    bool acknowledgeRead(const QString& serverMessageId,
                         qint64 readSeq,
                         QString* errorMessage = nullptr) const override
    {
        Q_UNUSED(errorMessage);
        readAckCalls.push_back({serverMessageId, readSeq});
        return true;
    }
};

ConversationSummary summary(const QString& conversationId)
{
    ConversationSummary item;
    item.conversationId = conversationId.toStdWString();
    item.title = conversationId.toStdWString();
    item.lastMessagePreview = L"";
    item.lastMessageAtMs = 100;
    return item;
}

ServerMessageRecord textRecord(const QString& conversationId,
                               const QString& clientMessageId,
                               const QString& senderId,
                               qint64 serverSeq)
{
    ServerMessageRecord record;
    record.serverMessageId = QStringLiteral("%1-srv-%2")
                                 .arg(conversationId)
                                 .arg(serverSeq);
    record.clientMessageId = clientMessageId;
    record.conversationId = conversationId;
    record.workspaceId = QStringLiteral("ws-main");
    record.senderId = senderId;
    record.serverSeq = serverSeq;
    record.type = QStringLiteral("chat_text");
    record.body = QStringLiteral("hello %1").arg(serverSeq);
    record.payload = QJsonObject{{QStringLiteral("source"),
                                  QStringLiteral("coordinator-test")}};
    record.contentType = QStringLiteral("html");
    record.createdAtMs = 1000 + serverSeq;
    return record;
}

}  // namespace

class TestRemoteMessageSyncCoordinator : public QObject {
    Q_OBJECT

private slots:
    void directConversationFilterKeepsOnlyLocalDirectChats()
    {
        std::vector<ConversationSummary> summaries;
        summaries.push_back(summary(QStringLiteral("local-a|peer-a")));
        summaries.push_back(summary(QStringLiteral("group:ops")));
        summaries.push_back(summary(QStringLiteral("peer-b|local-a")));
        summaries.push_back(summary(QStringLiteral("peer-x|peer-y")));

        const QStringList ids =
            RemoteMessageSyncCoordinator::directConversationIdsForLocalClient(
                QStringLiteral("local-a"), summaries);

        QCOMPARE(ids, QStringList({QStringLiteral("local-a|peer-a"),
                                   QStringLiteral("peer-b|local-a")}));
    }

    void serviceConversationFilterAppendsKnownGroupsAfterDirectChats()
    {
        std::vector<ConversationSummary> summaries;
        summaries.push_back(summary(QStringLiteral("local-a|peer-a")));
        summaries.push_back(summary(QStringLiteral("group:ops")));
        summaries.push_back(summary(QStringLiteral("peer-b|local-a")));
        summaries.push_back(summary(QStringLiteral("local-a|peer-a")));

        const QStringList ids =
            RemoteMessageSyncCoordinator::serviceConversationIdsForLocalClient(
                QStringLiteral("local-a"),
                summaries,
                QStringList{
                    QStringLiteral("group:ops"),
                    QStringLiteral("group:ops"),
                    QStringLiteral(""),
                    QStringLiteral("group:qa"),
                    QStringLiteral("peer-b|local-a")
                });

        QCOMPARE(ids, QStringList({QStringLiteral("local-a|peer-a"),
                                   QStringLiteral("peer-b|local-a"),
                                   QStringLiteral("group:ops"),
                                   QStringLiteral("group:qa")}));
    }

    void serviceConversationFilterAppendsServerDiscoveredConversations()
    {
        std::vector<ConversationSummary> summaries;
        summaries.push_back(summary(QStringLiteral("local-a|peer-a")));

        const QStringList ids =
            RemoteMessageSyncCoordinator::serviceConversationIdsForLocalClient(
                QStringLiteral("local-a"),
                summaries,
                QStringList{QStringLiteral("group:ops")},
                QStringList{
                    QStringLiteral("local-a|peer-service-new"),
                    QStringLiteral("group:ops"),
                    QStringLiteral(""),
                    QStringLiteral("group:server-new")
                });

        QCOMPARE(ids, QStringList({QStringLiteral("local-a|peer-a"),
                                   QStringLiteral("group:ops"),
                                   QStringLiteral("local-a|peer-service-new"),
                                   QStringLiteral("group:server-new")}));
    }

    void syncsAllDirectConversationsAndAggregatesCounts()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("remote-message-sync-coordinator");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;

        ServerMessagePage conv1;
        conv1.messages.push_back(textRecord(QStringLiteral("local-a|peer-a"),
                                            QStringLiteral("msg-a"),
                                            QStringLiteral("peer-a"),
                                            1));
        conv1.nextAfterSeq = 1;
        client.pagesByConversation.insert(QStringLiteral("local-a|peer-a"),
                                          conv1);

        ServerMessagePage conv2;
        conv2.messages.push_back(textRecord(QStringLiteral("local-a|peer-b"),
                                            QStringLiteral("msg-b"),
                                            QStringLiteral("peer-b"),
                                            3));
        conv2.nextAfterSeq = 3;
        client.pagesByConversation.insert(QStringLiteral("local-a|peer-b"),
                                          conv2);

        RemoteMessageSyncCoordinator coordinator(
            QStringLiteral("local-a"), &repository, &client, 50);
        const RemoteMessageSyncRunResult result =
            coordinator.syncDirectConversations({
                QStringLiteral("local-a|peer-a"),
                QStringLiteral("local-a|peer-b")
            });

        QVERIFY(result.success);
        QCOMPARE(result.attemptedCount, 2);
        QCOMPARE(result.succeededCount, 2);
        QCOMPARE(result.failedCount, 0);
        QCOMPARE(result.storedCount, 2);
        QCOMPARE(result.skippedDuplicateCount, 0);
        QCOMPARE(result.newIncomingConversationIds,
                 QStringList({QStringLiteral("local-a|peer-a"),
                              QStringLiteral("local-a|peer-b")}));
        QCOMPARE(result.newIncomingNotifications.size(), 2);
        QCOMPARE(result.newIncomingNotifications.at(0).conversationId,
                 QStringLiteral("local-a|peer-a"));
        QCOMPARE(result.newIncomingNotifications.at(1).conversationId,
                 QStringLiteral("local-a|peer-b"));
        QCOMPARE(result.failedConversationIds.size(), 0);
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("local-a|peer-a")),
                 qint64(1));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("local-a|peer-b")),
                 qint64(3));
        QCOMPARE(client.listCalls.size(), 2);
        QCOMPARE(client.listCalls.front().afterSeq, qint64(0));
        QCOMPARE(client.listCalls.front().limit, 50);
    }

    void failedConversationMakesRunFailWithoutStoppingSuccessfulOnes()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("remote-message-sync-partial-failure");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-failure.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;

        ServerMessagePage conv1;
        conv1.messages.push_back(textRecord(QStringLiteral("local-a|peer-a"),
                                            QStringLiteral("msg-a"),
                                            QStringLiteral("peer-a"),
                                            2));
        conv1.nextAfterSeq = 2;
        client.pagesByConversation.insert(QStringLiteral("local-a|peer-a"),
                                          conv1);
        client.failingConversations.insert(QStringLiteral("local-a|peer-b"));

        RemoteMessageSyncCoordinator coordinator(
            QStringLiteral("local-a"), &repository, &client);
        const RemoteMessageSyncRunResult result =
            coordinator.syncDirectConversations({
                QStringLiteral("local-a|peer-a"),
                QStringLiteral("local-a|peer-b")
            });

        QVERIFY(!result.success);
        QCOMPARE(result.attemptedCount, 2);
        QCOMPARE(result.succeededCount, 1);
        QCOMPARE(result.failedCount, 1);
        QCOMPARE(result.storedCount, 1);
        QCOMPARE(result.failedConversationIds,
                 QStringList({QStringLiteral("local-a|peer-b")}));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("local-a|peer-a")),
                 qint64(2));
        QCOMPARE(repository.loadRemoteChatCursor(QStringLiteral("local-a|peer-b")),
                 qint64(0));
    }

    void stopOnFirstFailureAvoidsLaterNetworkCalls()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("remote-message-sync-stop-on-failure");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-stop.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        FakeServerMessageClient client;
        client.failingConversations.insert(QStringLiteral("local-a|peer-a"));

        RemoteMessageSyncCoordinator coordinator(
            QStringLiteral("local-a"), &repository, &client, 100, true);
        const RemoteMessageSyncRunResult result =
            coordinator.syncDirectConversations({
                QStringLiteral("local-a|peer-a"),
                QStringLiteral("local-a|peer-b")
            });

        QVERIFY(!result.success);
        QCOMPARE(result.attemptedCount, 1);
        QCOMPARE(result.failedCount, 1);
        QCOMPARE(result.failedConversationIds,
                 QStringList({QStringLiteral("local-a|peer-a")}));
        QCOMPARE(client.listCalls.size(), 1);
        QCOMPARE(client.listCalls.front().conversationId,
                 QStringLiteral("local-a|peer-a"));
    }

    void successfulFallbackSyncFlushesPendingReadAcks()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString connectionName =
            QStringLiteral("remote-message-sync-read-ack-flush");
        DatabaseManager manager(dir.filePath(QStringLiteral("sync-read-ack.db")),
                                connectionName);
        QVERIFY(manager.open());

        ConversationRepository repository(connectionName);
        QVERIFY(repository.enqueuePendingRemoteReadAck(
            QStringLiteral("srv-read-1"),
            QStringLiteral("local-a|peer-a"),
            44));

        FakeServerMessageClient client;
        ServerMessagePage page;
        page.nextAfterSeq = 0;
        client.pagesByConversation.insert(QStringLiteral("local-a|peer-a"),
                                          page);

        RemoteMessageSyncCoordinator coordinator(
            QStringLiteral("local-a"), &repository, &client);
        const RemoteMessageSyncRunResult result =
            coordinator.syncDirectConversations({
                QStringLiteral("local-a|peer-a")
            });

        QVERIFY(result.success);
        QCOMPARE(result.pendingReadAcksAttempted, 1);
        QCOMPARE(result.pendingReadAcksAcknowledged, 1);
        QCOMPARE(client.readAckCalls.size(), 1);
        QCOMPARE(client.readAckCalls.front().serverMessageId,
                 QStringLiteral("srv-read-1"));
        QCOMPARE(client.readAckCalls.front().readSeq, qint64(44));
        QVERIFY(repository.loadPendingRemoteReadAcks().empty());
    }

    void retryBackoffIsExponentialAndCapped()
    {
        QCOMPARE(remoteMessageSyncRetryDelayMs(0), qint64(0));
        QCOMPARE(remoteMessageSyncRetryDelayMs(1), qint64(30000));
        QCOMPARE(remoteMessageSyncRetryDelayMs(2), qint64(60000));
        QCOMPARE(remoteMessageSyncRetryDelayMs(3), qint64(120000));
        QCOMPARE(remoteMessageSyncRetryDelayMs(9), qint64(300000));
        QCOMPARE(remoteMessageSyncRetryDelayMs(2, 1000, 5000), qint64(2000));
        QCOMPARE(remoteMessageSyncRetryDelayMs(10, 1000, 5000), qint64(5000));
    }
};

QTEST_MAIN(TestRemoteMessageSyncCoordinator)
#include "TestRemoteMessageSyncCoordinator.moc"
