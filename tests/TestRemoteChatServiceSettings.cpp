#include <QtTest/QTest>

#include <QSettings>
#include <QTemporaryDir>

#include "integrations/RemoteChatServiceSettings.h"

class TestRemoteChatServiceSettings : public QObject {
    Q_OBJECT

private slots:
    void defaultsKeepServiceTransportDisabled()
    {
        RemoteChatServiceSettings settings;

        QVERIFY(!settings.enabled);
        QCOMPARE(settings.mode, RemoteChatTransportMode::P2POnly);
        QVERIFY(settings.allowP2PFallback);
        QVERIFY(!settings.allowAutomaticPeerConnections);
        QVERIFY(settings.baseUrl.isEmpty());
        QVERIFY(settings.bearerToken.isEmpty());
        QCOMPARE(settings.workspaceId, QStringLiteral("default"));
        QVERIFY(!settings.hasCredentialConfiguration());
        QVERIFY(!settings.canUseMessageService());
    }

    void emptyStoreLoadsBuiltInDefaults()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QSettings store(dir.filePath(QStringLiteral("remote-chat.ini")),
                        QSettings::IniFormat);
        const RemoteChatServiceSettings loaded =
            RemoteChatServiceSettingsStore::load(&store);

        QVERIFY(!loaded.enabled);
        QVERIFY(loaded.baseUrl.isEmpty());
        QVERIFY(loaded.bearerToken.isEmpty());
        QCOMPARE(loaded.workspaceId, QStringLiteral("default"));
        QCOMPARE(loaded.mode, RemoteChatTransportMode::P2POnly);
        QVERIFY(loaded.allowP2PFallback);
        QVERIFY(!loaded.allowAutomaticPeerConnections);
        QVERIFY(!loaded.canUseMessageService());
    }

    void normalizesServerAddressForSettingsUi()
    {
        QCOMPARE(normalizeRemoteChatServiceBaseUrl(QStringLiteral("192.0.2.10")),
                 QStringLiteral("http://192.0.2.10:8765"));
        QCOMPARE(normalizeRemoteChatServiceBaseUrl(QStringLiteral(" 192.0.2.10:8766/ ")),
                 QStringLiteral("http://192.0.2.10:8766"));
        QCOMPARE(normalizeRemoteChatServiceBaseUrl(QStringLiteral("http://chat.local:9000/")),
                 QStringLiteral("http://chat.local:9000"));
        QVERIFY(normalizeRemoteChatServiceBaseUrl(QString()).isEmpty());
        QVERIFY(normalizeRemoteChatServiceBaseUrl(QStringLiteral("http://")).isEmpty());
    }

    void messageServiceReachabilityRequiresRecentHealthSuccess()
    {
        RemoteChatServiceSettings settings;
        const qint64 nowMs = 10'000;
        settings.enabled = true;
        settings.baseUrl = QStringLiteral("http://chat.local:8765");
        settings.bearerToken = QStringLiteral("token-123");
        settings.mode = RemoteChatTransportMode::ServerPreferred;

        QVERIFY(settings.canUseMessageService());
        QVERIFY(!settings.hasRecentSuccessfulHealth(nowMs));
        QVERIFY(!settings.shouldAttemptMessageService(nowMs));

        settings.lastHealthCheckAtMs = nowMs - 1000;
        settings.lastHealthSuccessAtMs = nowMs - 1000;
        QVERIFY(settings.hasRecentSuccessfulHealth(nowMs));
        QVERIFY(settings.shouldAttemptMessageService(nowMs));

        settings.lastHealthSuccessAtMs = nowMs - 180'000;
        QVERIFY(!settings.hasRecentSuccessfulHealth(nowMs));
        QVERIFY(!settings.shouldAttemptMessageService(nowMs));

        settings.lastHealthSuccessAtMs = nowMs - 1000;
        settings.enabled = false;
        QVERIFY(settings.hasRecentSuccessfulHealth(nowMs));
        QVERIFY(!settings.shouldAttemptMessageService(nowMs));
    }

    void messageServiceRequestsMayProbeBeforeFirstHealthCheck()
    {
        RemoteChatServiceSettings settings;
        const qint64 nowMs = 20'000;
        settings.enabled = true;
        settings.baseUrl = QStringLiteral("http://chat.local:8765");
        settings.bearerToken = QStringLiteral("token-123");
        settings.mode = RemoteChatTransportMode::ServerPreferred;

        QVERIFY(settings.canUseMessageService());
        QVERIFY(settings.shouldProbeMessageService(nowMs));
        QVERIFY(!settings.shouldAttemptMessageService(nowMs));

        settings.lastHealthCheckAtMs = nowMs - 1000;
        settings.lastHealthSuccessAtMs = 0;
        QVERIFY(!settings.shouldProbeMessageService(nowMs));

        settings.lastHealthSuccessAtMs = nowMs - 1000;
        QVERIFY(settings.shouldProbeMessageService(nowMs));

        settings.enabled = false;
        QVERIFY(!settings.shouldProbeMessageService(nowMs));
    }

    void saveAndLoadRoundTripsAllFields()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QSettings store(dir.filePath(QStringLiteral("remote-chat.ini")),
                        QSettings::IniFormat);
        RemoteChatServiceSettings original;
        original.enabled = true;
        original.baseUrl = QStringLiteral(" http://chat.local:8765/ ");
        original.bearerToken = QStringLiteral(" token-123 ");
        original.workspaceId = QStringLiteral(" ws-main ");
        original.mode = RemoteChatTransportMode::ServerPreferred;
        original.allowP2PFallback = false;
        original.allowAutomaticPeerConnections = true;
        original.lastHealthCheckAtMs = 1000;
        original.lastHealthSuccessAtMs = 900;
        original.lastErrorMessage = QStringLiteral(" stale error ");

        RemoteChatServiceSettingsStore::save(original, &store);
        const RemoteChatServiceSettings loaded =
            RemoteChatServiceSettingsStore::load(&store);

        QVERIFY(loaded.enabled);
        QCOMPARE(loaded.baseUrl, QStringLiteral("http://chat.local:8765"));
        QCOMPARE(loaded.bearerToken, QStringLiteral("token-123"));
        QCOMPARE(loaded.workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(loaded.mode, RemoteChatTransportMode::ServerPreferred);
        QVERIFY(!loaded.allowP2PFallback);
        QVERIFY(loaded.allowAutomaticPeerConnections);
        QCOMPARE(loaded.lastHealthCheckAtMs, qint64(1000));
        QCOMPARE(loaded.lastHealthSuccessAtMs, qint64(900));
        QCOMPARE(loaded.lastErrorMessage, QStringLiteral("stale error"));
    }

    void modeStringParsingFallsBackToP2POnly()
    {
        QCOMPARE(remoteChatTransportModeFromString(QStringLiteral("server_preferred")),
                 RemoteChatTransportMode::ServerPreferred);
        QCOMPARE(remoteChatTransportModeFromString(QStringLiteral("server_only")),
                 RemoteChatTransportMode::ServerOnly);
        QCOMPARE(remoteChatTransportModeFromString(QStringLiteral("p2p_only")),
                 RemoteChatTransportMode::P2POnly);
        QCOMPARE(remoteChatTransportModeFromString(QStringLiteral("unexpected")),
                 RemoteChatTransportMode::P2POnly);

        QCOMPARE(remoteChatTransportModeToString(RemoteChatTransportMode::P2POnly),
                 QStringLiteral("p2p_only"));
        QCOMPARE(remoteChatTransportModeToString(RemoteChatTransportMode::ServerPreferred),
                 QStringLiteral("server_preferred"));
        QCOMPARE(remoteChatTransportModeToString(RemoteChatTransportMode::ServerOnly),
                 QStringLiteral("server_only"));
    }

    void canUseMessageServiceRequiresCredentialsEnabledAndServerMode()
    {
        RemoteChatServiceSettings settings;
        settings.enabled = true;
        settings.baseUrl = QStringLiteral("http://chat.local:8765");
        settings.bearerToken = QStringLiteral("token-123");
        settings.workspaceId = QStringLiteral("ws-main");

        settings.mode = RemoteChatTransportMode::P2POnly;
        QVERIFY(!settings.canUseMessageService());

        settings.mode = RemoteChatTransportMode::ServerPreferred;
        QVERIFY(settings.canUseMessageService());

        settings.workspaceId.clear();
        QVERIFY(!settings.canUseMessageService());
    }
};

QTEST_MAIN(TestRemoteChatServiceSettings)
#include "TestRemoteChatServiceSettings.moc"
