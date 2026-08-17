// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增142行/修改2行/删除0行; 总行数87行
// @AI-LastModified: 2026-04-16 22:55:16

#include <QtTest>

#include "app/GroupFileServiceConfigResolver.h"
#include "app/GroupFileSendRouting.h"

class TestGroupFileSendRouting : public QObject {
    Q_OBJECT

private slots:
    void imagePrefersInlineAttachmentEvenWhenServiceIsBound()
    {
        GroupFileServiceConfig cfg;
        cfg.groupId = QStringLiteral("group-001");
        cfg.enabled = true;
        cfg.baseUrl = QStringLiteral("http://127.0.0.1:8765");
        cfg.bearerToken = QStringLiteral("token-123");
        cfg.workspaceId = QStringLiteral("ws-001");

        HybridRoutingDecision decision;
        decision.hasBoundService = true;
        decision.sharedFilesEnabled = true;

        const auto routing = decideGroupFileSendRoute(
            QStringLiteral("C:/temp/demo.png"), decision, cfg);

        QCOMPARE(routing.mode, GroupFileSendMode::InlineAttachmentImage);
        QVERIFY(routing.hasUsableFileServiceConfig);
    }

    void nonImageUsesFileServiceWhenConfigIsUsable()
    {
        GroupFileServiceConfig cfg;
        cfg.groupId = QStringLiteral("group-001");
        cfg.enabled = true;
        cfg.baseUrl = QStringLiteral("http://127.0.0.1:8765");
        cfg.bearerToken = QStringLiteral("token-123");
        cfg.workspaceId = QStringLiteral("ws-001");

        HybridRoutingDecision decision;
        decision.hasBoundService = true;
        decision.sharedFilesEnabled = true;

        const auto routing = decideGroupFileSendRoute(
            QStringLiteral("C:/temp/spec.docx"), decision, cfg);

        QCOMPARE(routing.mode, GroupFileSendMode::FileServiceUpload);
        QVERIFY(routing.hasUsableFileServiceConfig);
    }

    void nonImageFallsBackToP2PWhenConfigIsDisabled()
    {
        GroupFileServiceConfig cfg;
        cfg.groupId = QStringLiteral("group-001");
        cfg.enabled = false;
        cfg.baseUrl = QStringLiteral("http://127.0.0.1:8765");
        cfg.bearerToken = QStringLiteral("token-123");
        cfg.workspaceId = QStringLiteral("ws-001");

        HybridRoutingDecision decision;
        decision.hasBoundService = true;
        decision.sharedFilesEnabled = true;

        const auto routing = decideGroupFileSendRoute(
            QStringLiteral("C:/temp/spec.docx"), decision, cfg);

        QCOMPARE(routing.mode, GroupFileSendMode::P2PFileOffer);
        QVERIFY(!routing.hasUsableFileServiceConfig);
    }

    void nonImageUsesFileServiceEvenWithoutTokenAndWorkspaceId()
    {
        GroupFileServiceConfig cfg;
        cfg.groupId = QStringLiteral("group-001");
        cfg.enabled = true;
        cfg.baseUrl = QStringLiteral("http://127.0.0.1:8765");

        HybridRoutingDecision decision;
        decision.hasBoundService = true;
        decision.sharedFilesEnabled = true;

        const auto routing = decideGroupFileSendRoute(
            QStringLiteral("C:/temp/spec.docx"), decision, cfg);

        QCOMPARE(routing.mode, GroupFileSendMode::FileServiceUpload);
        QVERIFY(routing.hasUsableFileServiceConfig);
    }

    void nonImageUsesConfiguredFileServiceEvenWhenGroupHasNoExplicitBinding()
    {
        RemoteChatServiceSettings remoteSettings;
        remoteSettings.enabled = true;
        remoteSettings.baseUrl = QStringLiteral("chat.example.com");
        remoteSettings.bearerToken = QStringLiteral("token-123");
        remoteSettings.workspaceId = QStringLiteral("team-a");
        remoteSettings.mode = RemoteChatTransportMode::ServerPreferred;
        const GroupFileServiceConfig cfg =
            GroupFileServiceConfigResolver::makeDefaultConfig(
                QStringLiteral("group-001"),
                remoteSettings);

        HybridRoutingDecision decision;
        decision.hasBoundService = false;
        decision.sharedFilesEnabled = false;

        const auto routing = decideGroupFileSendRoute(
            QStringLiteral("C:/temp/spec.docx"), decision, cfg);

        QCOMPARE(cfg.baseUrl, QStringLiteral("http://chat.example.com:8765"));
        QCOMPARE(cfg.workspaceId, QStringLiteral("team-a"));
        QVERIFY(cfg.enabled);
        QCOMPARE(routing.mode, GroupFileSendMode::FileServiceUpload);
        QVERIFY(routing.hasUsableFileServiceConfig);
        QVERIFY(!routing.hasBoundService);
    }
};

QTEST_MAIN(TestGroupFileSendRouting)
#include "TestGroupFileSendRouting.moc"
