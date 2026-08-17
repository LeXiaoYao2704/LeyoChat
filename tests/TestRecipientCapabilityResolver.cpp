#include <QtTest/QTest>

#include "services/MessageRoutingCapabilities.h"
#include "services/RecipientCapabilityResolver.h"

class TestRecipientCapabilityResolver : public QObject {
    Q_OBJECT

private slots:
    void unknownTreatsAsLegacy()
    {
        RecipientCapabilityResolver resolver;

        const RecipientCapabilityDecision result =
            resolver.resolveFromCacheOnly(QStringLiteral("peer-a"), 1000);

        QCOMPARE(result.status,
                 RecipientCapabilityStatus::UnknownTreatAsLegacy);
    }

    void freshLocalLegacyOverridesServerProfile()
    {
        RecipientCapabilityResolver resolver;
        resolver.rememberServerProfile(
            QStringLiteral("peer-a"),
            QStringList{MessageRoutingCapabilities::serverReceiveV1()},
            1000);
        resolver.rememberLocalObservation(QStringLiteral("peer-a"), {}, 1500);

        const RecipientCapabilityDecision result =
            resolver.resolveFromCacheOnly(QStringLiteral("peer-a"), 1600);

        QCOMPARE(result.status, RecipientCapabilityStatus::LegacyP2P);
        QCOMPARE(result.source, RecipientCapabilitySource::LocalObservation);
    }

    void serverProfileCanMarkReceiverCapable()
    {
        RecipientCapabilityResolver resolver;
        resolver.rememberServerProfile(
            QStringLiteral("peer-a"),
            QStringList{MessageRoutingCapabilities::serverReceiveV1()},
            1000);

        const RecipientCapabilityDecision result =
            resolver.resolveFromCacheOnly(QStringLiteral("peer-a"), 1100);

        QCOMPARE(result.status,
                 RecipientCapabilityStatus::ServerReceiveCapable);
        QCOMPARE(result.source, RecipientCapabilitySource::ServerProfile);
    }

    void staleNonCapableServerProfileShouldBeRefreshableBeforeTtlExpires()
    {
        RecipientCapabilityResolver resolver(120000, 300000);
        resolver.rememberServerProfile(QStringLiteral("peer-a"), {}, 1000);

        const RecipientCapabilityDecision fresh =
            resolver.resolveFromCacheOnly(QStringLiteral("peer-a"), 2500);
        QCOMPARE(fresh.status,
                 RecipientCapabilityStatus::UnknownTreatAsLegacy);
        QCOMPARE(fresh.source, RecipientCapabilitySource::ServerProfile);
        QVERIFY(!resolver.shouldRefreshServerProfile(fresh, 2500, 30000));

        const RecipientCapabilityDecision stale =
            resolver.resolveFromCacheOnly(QStringLiteral("peer-a"), 32000);
        QCOMPARE(stale.status,
                 RecipientCapabilityStatus::UnknownTreatAsLegacy);
        QCOMPARE(stale.source, RecipientCapabilitySource::ServerProfile);
        QVERIFY(resolver.shouldRefreshServerProfile(stale, 32000, 30000));
    }

    void localObservationAndCapableServerProfileDoNotNeedRefresh()
    {
        RecipientCapabilityResolver resolver;
        resolver.rememberLocalObservation(QStringLiteral("legacy-a"), {}, 1000);
        resolver.rememberServerProfile(
            QStringLiteral("new-a"),
            QStringList{MessageRoutingCapabilities::serverReceiveV1()},
            1000);

        const RecipientCapabilityDecision localLegacy =
            resolver.resolveFromCacheOnly(QStringLiteral("legacy-a"), 60000);
        QVERIFY(!resolver.shouldRefreshServerProfile(localLegacy, 60000, 30000));

        const RecipientCapabilityDecision serverCapable =
            resolver.resolveFromCacheOnly(QStringLiteral("new-a"), 60000);
        QVERIFY(!resolver.shouldRefreshServerProfile(serverCapable, 60000, 30000));
    }

    void serverQueryResultCachesMissingProfilesAsLegacyCandidates()
    {
        RecipientCapabilityResolver resolver;

        QHash<QString, QStringList> returnedCapabilities;
        returnedCapabilities.insert(
            QStringLiteral("new-a"),
            QStringList{MessageRoutingCapabilities::serverReceiveV1()});

        resolver.rememberServerQueryResult(
            QStringList{QStringLiteral("new-a"), QStringLiteral("legacy-a")},
            returnedCapabilities,
            1000);

        const RecipientCapabilityDecision newClient =
            resolver.resolveFromCacheOnly(QStringLiteral("new-a"), 1100);
        QCOMPARE(newClient.status,
                 RecipientCapabilityStatus::ServerReceiveCapable);

        const RecipientCapabilityDecision missingClient =
            resolver.resolveFromCacheOnly(QStringLiteral("legacy-a"), 1100);
        QCOMPARE(missingClient.status,
                 RecipientCapabilityStatus::UnknownTreatAsLegacy);
        QCOMPARE(missingClient.source, RecipientCapabilitySource::ServerProfile);
        QVERIFY(!resolver.shouldRefreshServerProfile(missingClient, 1100, 30000));
    }
};

QTEST_MAIN(TestRecipientCapabilityResolver)
#include "TestRecipientCapabilityResolver.moc"
