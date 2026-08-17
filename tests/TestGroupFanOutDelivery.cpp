#include <QtTest/QTest>

#include <QByteArray>
#include <QObject>
#include <QStringList>

#include "app/GroupFanOutDelivery.h"

namespace {

GroupFanOutPayload payloadFor(const QString& targetId,
                              const QByteArray& blob,
                              const QString& messageId = QString())
{
    return GroupFanOutPayload{targetId, blob, messageId};
}

}  // namespace

class TestGroupFanOutDelivery : public QObject {
    Q_OBJECT

private slots:
    void deliversAllPayloadsImmediatelyWhenUnderBatchSize()
    {
        const std::vector<GroupFanOutPayload> payloads{
            payloadFor(QStringLiteral("peer-a"), QByteArrayLiteral("a")),
            payloadFor(QStringLiteral("peer-b"), QByteArrayLiteral("b"))
        };
        QStringList deliveredTargets;
        QByteArray deliveredBytes;

        GroupFanOutDeliveryOptions options;
        options.batchSize = 20;
        deliverGroupFanOutPayloads(
            payloads,
            nullptr,
            [&](const GroupFanOutPayload& payload) {
                deliveredTargets.push_back(payload.targetId);
                deliveredBytes += payload.blob;
                return true;
            },
            options);

        QCOMPARE(deliveredTargets,
                 QStringList({QStringLiteral("peer-a"), QStringLiteral("peer-b")}));
        QCOMPARE(deliveredBytes, QByteArrayLiteral("ab"));
    }

    void deliversPayloadsAcrossQueuedBatches()
    {
        QObject context;
        const std::vector<GroupFanOutPayload> payloads{
            payloadFor(QStringLiteral("peer-a"), QByteArrayLiteral("a")),
            payloadFor(QStringLiteral("peer-b"), QByteArrayLiteral("b")),
            payloadFor(QStringLiteral("peer-c"), QByteArrayLiteral("c"))
        };
        QStringList deliveredTargets;

        GroupFanOutDeliveryOptions options;
        options.batchSize = 1;
        deliverGroupFanOutPayloads(
            payloads,
            &context,
            [&](const GroupFanOutPayload& payload) {
                deliveredTargets.push_back(payload.targetId);
                return true;
            },
            options);

        QTRY_COMPARE(deliveredTargets.size(), 3);
        QCOMPARE(deliveredTargets,
                 QStringList({QStringLiteral("peer-a"),
                              QStringLiteral("peer-b"),
                              QStringLiteral("peer-c")}));
    }

    void callsDeliveredCallbackOnlyAfterSuccessfulSend()
    {
        const std::vector<GroupFanOutPayload> payloads{
            payloadFor(QStringLiteral("peer-a"),
                       QByteArrayLiteral("a"),
                       QStringLiteral("msg-a")),
            payloadFor(QStringLiteral("peer-b"),
                       QByteArrayLiteral("b"),
                       QStringLiteral("msg-b")),
            payloadFor(QStringLiteral("peer-c"),
                       QByteArrayLiteral("c"),
                       QStringLiteral("msg-c"))
        };
        QStringList sendAttempts;
        QStringList cleanedMessages;

        GroupFanOutDeliveryOptions options;
        options.batchSize = 20;
        options.onDelivered = [&](const GroupFanOutPayload& payload) {
            cleanedMessages.push_back(payload.targetId + QStringLiteral(":")
                                      + payload.messageId);
        };

        deliverGroupFanOutPayloads(
            payloads,
            nullptr,
            [&](const GroupFanOutPayload& payload) {
                sendAttempts.push_back(payload.targetId);
                return payload.targetId != QStringLiteral("peer-b");
            },
            options);

        QCOMPARE(sendAttempts,
                 QStringList({QStringLiteral("peer-a"),
                              QStringLiteral("peer-b"),
                              QStringLiteral("peer-c")}));
        QCOMPARE(cleanedMessages,
                 QStringList({QStringLiteral("peer-a:msg-a"),
                              QStringLiteral("peer-c:msg-c")}));
    }

    void returnsAttemptDeliveredAndFailedCounts()
    {
        const std::vector<GroupFanOutPayload> payloads{
            payloadFor(QStringLiteral("peer-a"), QByteArrayLiteral("a")),
            payloadFor(QStringLiteral("peer-b"), QByteArrayLiteral("b")),
            payloadFor(QStringLiteral("peer-c"), QByteArrayLiteral("c"))
        };

        const GroupFanOutDeliveryResult result = deliverGroupFanOutPayloads(
            payloads,
            nullptr,
            [&](const GroupFanOutPayload& payload) {
                return payload.targetId != QStringLiteral("peer-b");
            });

        QCOMPARE(result.attemptedCount, 3);
        QCOMPARE(result.deliveredCount, 2);
        QCOMPARE(result.failedCount, 1);
        QVERIFY(result.anyDelivered());
        QVERIFY(!result.allDelivered());
    }

    void acceptsDeliveryOnlyWhenPolicyIsSatisfied()
    {
        GroupFanOutDeliveryResult partial;
        partial.attemptedCount = 3;
        partial.deliveredCount = 2;
        partial.failedCount = 1;

        QVERIFY(!partial.allDelivered());
        QVERIFY(!isGroupFanOutDeliveryAccepted(false, partial));
        QVERIFY(!isGroupFanOutDeliveryAccepted(true, partial));

        GroupFanOutDeliveryResult complete;
        complete.attemptedCount = 3;
        complete.deliveredCount = 3;
        complete.failedCount = 0;

        QVERIFY(complete.allDelivered());
        QVERIFY(isGroupFanOutDeliveryAccepted(false, complete));
        QVERIFY(isGroupFanOutDeliveryAccepted(true, complete));
    }

    void queuedOnlyIsAcceptedButNotFullyDelivered()
    {
        GroupFanOutDeliveryResult result;
        result.attemptedCount = 3;
        result.queuedCount = 3;

        QVERIFY(result.accepted());
        QVERIFY(!result.anyWritten());
        QVERIFY(!result.allWritten());
        QVERIFY(isGroupFanOutDeliveryAccepted(true, result));
        QVERIFY(!isGroupFanOutDeliveryAccepted(false, result));
    }

    void statusSenderSeparatesWrittenQueuedAndFailedCounts()
    {
        const std::vector<GroupFanOutPayload> payloads{
            payloadFor(QStringLiteral("peer-a"), QByteArrayLiteral("a")),
            payloadFor(QStringLiteral("peer-b"), QByteArrayLiteral("b")),
            payloadFor(QStringLiteral("peer-c"), QByteArrayLiteral("c"))
        };

        const GroupFanOutDeliveryResult result =
            deliverGroupFanOutPayloadsWithStatus(
                payloads,
                nullptr,
                [](const GroupFanOutPayload& payload) {
                    if (payload.targetId == QStringLiteral("peer-a")) {
                        return GroupFanOutPayloadDisposition::Written;
                    }
                    if (payload.targetId == QStringLiteral("peer-b")) {
                        return GroupFanOutPayloadDisposition::Queued;
                    }
                    return GroupFanOutPayloadDisposition::Failed;
                });

        QCOMPARE(result.attemptedCount, 3);
        QCOMPARE(result.writtenCount, 1);
        QCOMPARE(result.queuedCount, 1);
        QCOMPARE(result.failedCount, 1);
        QVERIFY(!result.accepted());
        QVERIFY(!result.allWritten());
    }
};

QTEST_MAIN(TestGroupFanOutDelivery)
#include "TestGroupFanOutDelivery.moc"
