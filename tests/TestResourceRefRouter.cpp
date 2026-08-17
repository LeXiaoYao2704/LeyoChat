#include <QtTest/QTest>

#include "domain/MessageEnvelope.h"
#include "domain/ResourceRefPayload.h"
#include "services/ResourceRefRouter.h"

class TestResourceRefRouter : public QObject {
    Q_OBJECT

private slots:
    void roundTripsStructuredPayload()
    {
        ResourceRefPayload payload;
        payload.serviceId = QStringLiteral("svc-001");
        payload.workspaceId = QStringLiteral("ws-001");
        payload.origin = QStringLiteral("service");
        payload.kind = QStringLiteral("shared_file");
        payload.resourceId = QStringLiteral("res-001");
        payload.title = QStringLiteral("Spec Board");
        payload.subtitle = QStringLiteral("设计规范");
        payload.status = QStringLiteral("ready");
        payload.snapshotVersion = QStringLiteral("v3");
        payload.updatedAtMs = 1712800000000LL;
        payload.actions.push_back(ResourceRefAction{
            QStringLiteral("open"),
            QStringLiteral("打开"),
            QStringLiteral("resource://res-001"),
            true});

        const auto decoded = ResourceRefRouter::parsePayload(ResourceRefRouter::serializePayload(payload));

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->serviceId, payload.serviceId);
        QCOMPARE(decoded->workspaceId, payload.workspaceId);
        QCOMPARE(decoded->origin, payload.origin);
        QCOMPARE(decoded->kind, payload.kind);
        QCOMPARE(decoded->resourceId, payload.resourceId);
        QCOMPARE(decoded->title, payload.title);
        QCOMPARE(decoded->subtitle, payload.subtitle);
        QCOMPARE(decoded->status, payload.status);
        QCOMPARE(decoded->snapshotVersion, payload.snapshotVersion);
        QCOMPARE(decoded->updatedAtMs, payload.updatedAtMs);
        QCOMPARE(decoded->actions.size(), payload.actions.size());
        QCOMPARE(decoded->actions.front().actionId, QStringLiteral("open"));
    }

    void prefersStructuredPayloadOverLegacyBody()
    {
        MessageEnvelope envelope;
        envelope.type = MessageType::ResourceReference;
        envelope.contentType = "resource_reference";
        envelope.messageSubtype = "resource_ref";
        envelope.payloadJson =
            R"({"service_id":"svc-001","workspace_id":"ws-001","origin":"service","kind":"shared_file","resource_id":"res-structured","title":"Structured","subtitle":"Modern"})";
        envelope.body =
            R"({"message_kind":"resource_reference","resource_id":"res-legacy","resource_kind":"shared_file","title":"Legacy","summary":"Old"})";
        envelope.resourceId = "res-envelope";
        envelope.resourceKind = "shared_file";

        const auto decoded = ResourceRefRouter::parseEnvelope(envelope);

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->resourceId, QStringLiteral("res-structured"));
        QCOMPARE(decoded->title, QStringLiteral("Structured"));
        QCOMPARE(decoded->subtitle, QStringLiteral("Modern"));
    }

    void fallsBackToLegacyEnvelopeFields()
    {
        MessageEnvelope envelope;
        envelope.type = MessageType::ResourceReference;
        envelope.contentType = "resource_reference";
        envelope.body =
            R"({"message_kind":"resource_reference","resource_id":"res-001","resource_kind":"shared_file","title":"Legacy Board","summary":"共享文件"})";
        envelope.resourceId = "res-001";
        envelope.resourceKind = "shared_file";
        envelope.resourceTitle = "Legacy Board";
        envelope.workspaceId = "ws-001";
        envelope.serviceId = "svc-001";

        const auto decoded = ResourceRefRouter::parseEnvelope(envelope);

        QVERIFY(decoded.has_value());
        QCOMPARE(decoded->serviceId, QStringLiteral("svc-001"));
        QCOMPARE(decoded->workspaceId, QStringLiteral("ws-001"));
        QCOMPARE(decoded->resourceId, QStringLiteral("res-001"));
        QCOMPARE(decoded->kind, QStringLiteral("shared_file"));
        QCOMPARE(decoded->title, QStringLiteral("Legacy Board"));
        QCOMPARE(decoded->subtitle, QStringLiteral("共享文件"));
        QCOMPARE(ResourceRefRouter::previewLabel(envelope), QStringLiteral("[共享文件] Legacy Board"));
    }

    void previewLabel_usesKindSpecificPrefixes()
    {
        MessageEnvelope envelope;
        envelope.type = MessageType::ResourceReference;
        envelope.contentType = "resource_reference";
        envelope.messageSubtype = "resource_ref";
        envelope.payloadJson =
            R"({"service_id":"svc-devops","workspace_id":"ws-001","origin":"service","kind":"devops_work_item","resource_id":"wi-2048","title":"修复 P2P 群文件","subtitle":"Bug · LeyoChat","status":"进行中"})";

        QCOMPARE(ResourceRefRouter::previewLabel(envelope), QStringLiteral("[DevOps 工作项] 修复 P2P 群文件"));
    }
};

QTEST_MAIN(TestResourceRefRouter)
#include "TestResourceRefRouter.moc"
