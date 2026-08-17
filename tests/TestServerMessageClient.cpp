#include <QtTest/QTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QJsonObject>
#include <QUrlQuery>

#include "integrations/ServerMessageClient.h"
#include "services/MessageRoutingCapabilities.h"

namespace {

class FakeServerMessageTransport final : public IServerMessageTransport {
public:
    QHash<QString, QJsonDocument> jsonByRequest;
    QHash<QString, QByteArray> bytesByRequest;
    QHash<QString, int> httpStatusByRequest;
    mutable QVector<QString> getUrls;
    mutable QVector<QString> byteUrls;
    mutable QVector<QString> postUrls;
    mutable QVector<QJsonObject> postBodies;
    mutable QVector<RemoteChatServiceSettings> settingsSeen;

    std::optional<QJsonDocument> getJson(
        const QUrl& url,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const override
    {
        getUrls.push_back(url.toString());
        settingsSeen.push_back(settings);

        const auto it = jsonByRequest.constFind(url.toString());
        if (it == jsonByRequest.cend()) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("missing fake GET: %1").arg(url.toString());
            }
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QByteArray> getBytes(
        const QUrl& url,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const override
    {
        byteUrls.push_back(url.toString());
        settingsSeen.push_back(settings);

        const auto it = bytesByRequest.constFind(url.toString());
        if (it == bytesByRequest.cend()) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("missing fake byte GET: %1").arg(url.toString());
            }
            return std::nullopt;
        }
        return it.value();
    }

    std::optional<QJsonDocument> postJson(
        const QUrl& url,
        const QJsonObject& body,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const override
    {
        postUrls.push_back(url.toString());
        postBodies.push_back(body);
        settingsSeen.push_back(settings);

        const QString key = QStringLiteral("POST ") + url.toString();
        const auto it = jsonByRequest.constFind(key);
        if (it == jsonByRequest.cend()) {
            if (errorMessage) {
                *errorMessage =
                    QStringLiteral("missing fake POST: %1").arg(url.toString());
            }
            return std::nullopt;
        }
        return it.value();
    }

    ServerMessageTransportResponse postJsonWithStatus(
        const QUrl& url,
        const QJsonObject& body,
        const RemoteChatServiceSettings& settings) const override
    {
        ServerMessageTransportResponse response;
        response.document = postJson(url, body, settings, &response.errorMessage);
        const QString key = QStringLiteral("POST ") + url.toString();
        response.httpStatus = httpStatusByRequest.value(key, 0);
        if (response.httpStatus >= 400) {
            response.document.reset();
        }
        return response;
    }
};

RemoteChatServiceSettings configuredSettings()
{
    RemoteChatServiceSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral(" http://chat.local:8765/ ");
    settings.bearerToken = QStringLiteral("chat-token");
    settings.workspaceId = QStringLiteral("ws-main");
    settings.mode = RemoteChatTransportMode::ServerPreferred;
    return settings;
}

ServerMessageDraft textDraft()
{
    ServerMessageDraft draft;
    draft.clientMessageId = QStringLiteral("local-1");
    draft.conversationId = QStringLiteral("conv-1");
    draft.workspaceId = QStringLiteral("ws-main");
    draft.type = QStringLiteral("chat_text");
    draft.body = QStringLiteral("hello");
    draft.payload = QJsonObject{
        {QStringLiteral("html"), QStringLiteral("<b>hello</b>")}
    };
    draft.contentType = QStringLiteral("html");
    draft.replyToMessageId = QStringLiteral("reply-1");
    draft.recipientIds = {QStringLiteral("peer-a"), QStringLiteral("peer-b")};
    return draft;
}

QJsonDocument ackDocument(bool duplicate = false)
{
    return QJsonDocument(QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("duplicate"), duplicate},
        {QStringLiteral("serverMessageId"), QStringLiteral("srv-1")},
        {QStringLiteral("conversationId"), QStringLiteral("conv-1")},
        {QStringLiteral("serverSeq"), 42},
        {QStringLiteral("createdAtMs"), 1000}
    });
}

}  // namespace

class TestServerMessageClient : public QObject {
    Q_OBJECT

private slots:
    void sendMessage_postsExpectedUrlAndJsonBody()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/messages"),
            ackDocument());

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto ack = client.sendMessage(textDraft(), &error);

        QVERIFY(error.isEmpty());
        QVERIFY(ack.has_value());
        QCOMPARE(ack->serverMessageId, QStringLiteral("srv-1"));
        QCOMPARE(ack->conversationId, QStringLiteral("conv-1"));
        QCOMPARE(ack->serverSeq, qint64(42));
        QCOMPARE(ack->createdAtMs, qint64(1000));
        QVERIFY(!ack->duplicate);

        QCOMPARE(transport->postUrls.size(), 1);
        QCOMPARE(transport->postUrls.front(),
                 QStringLiteral("http://chat.local:8765/api/v1/messages"));
        QCOMPARE(transport->settingsSeen.front().bearerToken,
                 QStringLiteral("chat-token"));

        const QJsonObject body = transport->postBodies.front();
        QCOMPARE(body[QStringLiteral("clientMessageId")].toString(),
                 QStringLiteral("local-1"));
        QCOMPARE(body[QStringLiteral("conversationId")].toString(),
                 QStringLiteral("conv-1"));
        QCOMPARE(body[QStringLiteral("workspaceId")].toString(),
                 QStringLiteral("ws-main"));
        QCOMPARE(body[QStringLiteral("type")].toString(),
                 QStringLiteral("chat_text"));
        QCOMPARE(body[QStringLiteral("body")].toString(),
                 QStringLiteral("hello"));
        QCOMPARE(body[QStringLiteral("contentType")].toString(),
                 QStringLiteral("html"));
        QCOMPARE(body[QStringLiteral("replyToMessageId")].toString(),
                 QStringLiteral("reply-1"));
        QCOMPARE(body[QStringLiteral("payload")].toObject()
                     [QStringLiteral("html")].toString(),
                 QStringLiteral("<b>hello</b>"));

        const QJsonArray recipients =
            body[QStringLiteral("recipientIds")].toArray();
        QCOMPARE(recipients.size(), 2);
        QCOMPARE(recipients.at(0).toString(), QStringLiteral("peer-a"));
        QCOMPARE(recipients.at(1).toString(), QStringLiteral("peer-b"));
    }

    void sendMessage_forwardsRequestClientIdentityToTransport()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/messages"),
            ackDocument());
        RemoteChatServiceSettings settings = configuredSettings();
        settings.clientId = QStringLiteral("workstation-client-a");

        ServerMessageClient client(settings, transport);
        QString error;
        QVERIFY(client.sendMessage(textDraft(), &error).has_value());
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QCOMPARE(transport->settingsSeen.size(), 1);
        QCOMPARE(transport->settingsSeen.front().clientId,
                 QStringLiteral("workstation-client-a"));
    }

    void sendMessage_parsesDuplicateAck()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/messages"),
            ackDocument(true));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto ack = client.sendMessage(textDraft(), &error);

        QVERIFY(error.isEmpty());
        QVERIFY(ack.has_value());
        QVERIFY(ack->duplicate);
        QCOMPARE(ack->serverMessageId, QStringLiteral("srv-1"));
        QCOMPARE(ack->serverSeq, qint64(42));
    }

    void listConversations_buildsQueryAndParsesMemberships()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("http://chat.local:8765/api/v1/conversations?workspaceId=ws-main&limit=100"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("conversations"), QJsonArray{
                    QJsonObject{
                        {QStringLiteral("conversationId"), QStringLiteral("local-a|peer-new")},
                        {QStringLiteral("latestServerSeq"), 7},
                        {QStringLiteral("updatedAtMs"), 1200}
                    },
                    QJsonObject{
                        {QStringLiteral("conversationId"), QStringLiteral("group:ops")},
                        {QStringLiteral("latestServerSeq"), 12},
                        {QStringLiteral("updatedAtMs"), 1600}
                    }
                }}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto conversations =
            client.listConversations(QStringLiteral("ws-main"), 100, &error);

        QVERIFY(error.isEmpty());
        QVERIFY(conversations.has_value());
        QCOMPARE(conversations->size(), 2);
        QCOMPARE(conversations->at(0).conversationId,
                 QStringLiteral("local-a|peer-new"));
        QCOMPARE(conversations->at(0).latestServerSeq, qint64(7));
        QCOMPARE(conversations->at(1).conversationId, QStringLiteral("group:ops"));
        QCOMPARE(conversations->at(1).updatedAtMs, qint64(1600));
        QCOMPARE(transport->getUrls.size(), 1);
        QCOMPARE(transport->getUrls.front(),
                 QStringLiteral("http://chat.local:8765/api/v1/conversations?workspaceId=ws-main&limit=100"));
    }

    void listMessages_buildsQueryAndParsesPage()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("http://chat.local:8765/api/v1/conversations/"
                           "conv-1/messages?workspaceId=ws-main&afterSeq=7&limit=50"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("messages"), QJsonArray{
                    QJsonObject{
                        {QStringLiteral("serverMessageId"), QStringLiteral("srv-2")},
                        {QStringLiteral("clientMessageId"), QStringLiteral("local-2")},
                        {QStringLiteral("conversationId"), QStringLiteral("conv-1")},
                        {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
                        {QStringLiteral("senderId"), QStringLiteral("peer-a")},
                        {QStringLiteral("serverSeq"), 8},
                        {QStringLiteral("type"), QStringLiteral("chat_text")},
                        {QStringLiteral("body"), QStringLiteral("from service")},
                        {QStringLiteral("payload"), QJsonObject{
                            {QStringLiteral("html"), QStringLiteral("<i>from service</i>")}
                        }},
                        {QStringLiteral("fileId"), QStringLiteral("file-1")},
                        {QStringLiteral("contentType"), QStringLiteral("html")},
                        {QStringLiteral("replyToMessageId"), QStringLiteral("srv-1")},
                        {QStringLiteral("createdAtMs"), 2000}
                    }
                }},
                {QStringLiteral("nextAfterSeq"), 8}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto page =
            client.listMessages(QStringLiteral("conv-1"), 7, 50, &error);

        QVERIFY(error.isEmpty());
        QVERIFY(page.has_value());
        QCOMPARE(transport->getUrls.size(), 1);
        QCOMPARE(page->messages.size(), 1);
        QCOMPARE(page->nextAfterSeq, qint64(8));

        const ServerMessageRecord& record = page->messages.front();
        QCOMPARE(record.serverMessageId, QStringLiteral("srv-2"));
        QCOMPARE(record.clientMessageId, QStringLiteral("local-2"));
        QCOMPARE(record.conversationId, QStringLiteral("conv-1"));
        QCOMPARE(record.workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(record.senderId, QStringLiteral("peer-a"));
        QCOMPARE(record.serverSeq, qint64(8));
        QCOMPARE(record.body, QStringLiteral("from service"));
        QCOMPARE(record.payload[QStringLiteral("html")].toString(),
                 QStringLiteral("<i>from service</i>"));
        QCOMPARE(record.fileId, QStringLiteral("file-1"));
        QCOMPARE(record.contentType, QStringLiteral("html"));
        QCOMPARE(record.replyToMessageId, QStringLiteral("srv-1"));
        QCOMPARE(record.createdAtMs, qint64(2000));
    }

    void acknowledgeDelivered_postsReceivedSeq()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/messages/"
                           "srv-1/delivery-ack"),
            QJsonDocument(QJsonObject{{QStringLiteral("ok"), true}}));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        QVERIFY(client.acknowledgeDelivered(QStringLiteral("srv-1"), 42, &error));
        QVERIFY(error.isEmpty());

        QCOMPARE(transport->postUrls.front(),
                 QStringLiteral("http://chat.local:8765/api/v1/messages/"
                                "srv-1/delivery-ack"));
        QCOMPARE(transport->postBodies.front()
                     [QStringLiteral("receivedSeq")].toInteger(),
                 qint64(42));
    }

    void acknowledgeRead_postsReadSeq()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/messages/"
                           "srv-1/read-ack"),
            QJsonDocument(QJsonObject{{QStringLiteral("ok"), true}}));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        QVERIFY(client.acknowledgeRead(QStringLiteral("srv-1"), 43, &error));
        QVERIFY(error.isEmpty());

        QCOMPARE(transport->postUrls.front(),
                 QStringLiteral("http://chat.local:8765/api/v1/messages/"
                                "srv-1/read-ack"));
        QCOMPARE(transport->postBodies.front()
                     [QStringLiteral("readSeq")].toInteger(),
                 qint64(43));
    }

    void acknowledgeMissingMessageReturnsStructuredTerminalOutcome()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        const QString key =
            QStringLiteral("POST http://chat.local:8765/api/v1/messages/"
                           "missing/delivery-ack");
        transport->httpStatusByRequest.insert(key, 404);

        ServerMessageClient client(configuredSettings(), transport);
        const ServerAckAttemptResult result =
            client.acknowledgeDeliveredResult(QStringLiteral("missing"), 42);

        QCOMPARE(result.outcome, ServerAckOutcome::MessageNotFound);
        QCOMPARE(result.httpStatus, 404);
        QCOMPARE(transport->postUrls.size(), 1);
    }

    void listEvents_buildsSseQueryAndParsesEvents()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->bytesByRequest.insert(
            QStringLiteral("http://chat.local:8765/api/v1/events/stream"
                           "?workspaceId=ws-main&deviceId=pc-a"
                           "&afterEventId=9&limit=50"),
            QByteArrayLiteral(
                ": leyochat heartbeat\n"
                "retry: 5000\n\n"
                "id: 10\n"
                "event: message.created\n"
                "data: {\"eventId\":10,\"type\":\"message.created\","
                "\"workspaceId\":\"ws-main\",\"conversationId\":\"conv-1\","
                "\"serverMessageId\":\"srv-10\"}\n\n"));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto page = client.listEvents(QStringLiteral("ws-main"),
                                            QStringLiteral("pc-a"),
                                            9,
                                            50,
                                            &error);

        QVERIFY(error.isEmpty());
        QVERIFY(page.has_value());
        QCOMPARE(transport->byteUrls.size(), 1);
        QCOMPARE(page->events.size(), 1);
        QCOMPARE(page->nextAfterEventId, qint64(10));
        QCOMPARE(page->events.front().eventId, qint64(10));
        QCOMPARE(page->events.front().type, QStringLiteral("message.created"));
        QCOMPARE(page->events.front().workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(page->events.front().conversationId, QStringLiteral("conv-1"));
        QCOMPARE(page->events.front().data[QStringLiteral("serverMessageId")].toString(),
                 QStringLiteral("srv-10"));
    }

    void sessionHeartbeat_postsDeviceState()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/sessions/heartbeat"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("sessionId"), QStringLiteral("sess-1")},
                {QStringLiteral("clientId"), QStringLiteral("client-1")},
                {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
                {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
                {QStringLiteral("lastEventId"), 10}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto ack = client.sendSessionHeartbeat(QStringLiteral("ws-main"),
                                                     QStringLiteral("pc-a"),
                                                     10,
                                                     &error);

        QVERIFY(error.isEmpty());
        QVERIFY(ack.has_value());
        QVERIFY(ack->ok);
        QCOMPARE(ack->sessionId, QStringLiteral("sess-1"));
        QCOMPARE(ack->deviceId, QStringLiteral("pc-a"));
        QCOMPARE(ack->workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(ack->lastEventId, qint64(10));

        QCOMPARE(transport->postUrls.front(),
                 QStringLiteral("http://chat.local:8765/api/v1/sessions/heartbeat"));
        const QJsonObject body = transport->postBodies.front();
        QCOMPARE(body[QStringLiteral("workspaceId")].toString(),
                 QStringLiteral("ws-main"));
        QCOMPARE(body[QStringLiteral("deviceId")].toString(),
                 QStringLiteral("pc-a"));
        QCOMPARE(body[QStringLiteral("lastEventId")].toInteger(), qint64(10));
    }

    void sessionHeartbeat_postsCapabilities()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/sessions/heartbeat"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("sessionId"), QStringLiteral("sess-1")},
                {QStringLiteral("clientId"), QStringLiteral("client-1")},
                {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
                {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
                {QStringLiteral("lastEventId"), 10}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto ack = client.sendSessionHeartbeat(
            QStringLiteral("ws-main"),
            QStringLiteral("pc-a"),
            10,
            QStringLiteral("0.2.0"),
            QStringList{MessageRoutingCapabilities::serverReceiveV1()},
            &error);

        QVERIFY(error.isEmpty());
        QVERIFY(ack.has_value());
        const QJsonObject body = transport->postBodies.front();
        QCOMPARE(body[QStringLiteral("appVersion")].toString(),
                 QStringLiteral("0.2.0"));
        QVERIFY(body[QStringLiteral("capabilities")].toArray().contains(
            MessageRoutingCapabilities::serverReceiveV1()));
    }

    void listOnlineSessions_buildsQueryAndParsesSessions()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("http://chat.local:8765/api/v1/sessions/online"
                           "?workspaceId=ws-main"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
                {QStringLiteral("sessions"), QJsonArray{
                    QJsonObject{
                        {QStringLiteral("sessionId"), QStringLiteral("sess-1")},
                        {QStringLiteral("clientId"), QStringLiteral("peer-a")},
                        {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
                        {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
                        {QStringLiteral("connectedAtMs"), 1000},
                        {QStringLiteral("lastSeenAtMs"), 1200},
                        {QStringLiteral("lastEventId"), 7},
                        {QStringLiteral("appVersion"), QStringLiteral("0.3.3")},
                        {QStringLiteral("capabilities"),
                         QJsonArray{MessageRoutingCapabilities::serverReceiveV1()}}
                    }
                }}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto sessions =
            client.listOnlineSessions(QStringLiteral("ws-main"), &error);

        QVERIFY(error.isEmpty());
        QVERIFY(sessions.has_value());
        QCOMPARE(transport->getUrls.size(), 1);
        QCOMPARE(sessions->size(), 1);
        QCOMPARE(sessions->front().sessionId, QStringLiteral("sess-1"));
        QCOMPARE(sessions->front().clientId, QStringLiteral("peer-a"));
        QCOMPARE(sessions->front().deviceId, QStringLiteral("pc-a"));
        QCOMPARE(sessions->front().workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(sessions->front().connectedAtMs, qint64(1000));
        QCOMPARE(sessions->front().lastSeenAtMs, qint64(1200));
        QCOMPARE(sessions->front().lastEventId, qint64(7));
        QCOMPARE(sessions->front().appVersion, QStringLiteral("0.3.3"));
        QVERIFY(sessions->front().capabilities.contains(
            MessageRoutingCapabilities::serverReceiveV1()));
    }

    void queryClientCapabilities_postsClientIdsAndParsesProfiles()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/clients/capabilities/query"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("workspaceId"), QStringLiteral("ws-main")},
                {QStringLiteral("profiles"), QJsonArray{
                    QJsonObject{
                        {QStringLiteral("clientId"), QStringLiteral("client-a")},
                        {QStringLiteral("appVersion"), QStringLiteral("0.2.0")},
                        {QStringLiteral("capabilities"),
                         QJsonArray{MessageRoutingCapabilities::serverReceiveV1()}},
                        {QStringLiteral("updatedAtMs"), 1000}
                    },
                    QJsonObject{
                        {QStringLiteral("clientId"), QStringLiteral("legacy")},
                        {QStringLiteral("capabilities"), QJsonArray{}},
                        {QStringLiteral("updatedAtMs"), 0}
                    }
                }}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto result = client.queryClientCapabilities(
            QStringLiteral("ws-main"),
            QStringList{QStringLiteral("client-a"), QStringLiteral("legacy")},
            MessageRoutingCapabilities::serverReceiveV1(),
            &error);

        QVERIFY(error.isEmpty());
        QVERIFY(result.has_value());
        QCOMPARE(result->workspaceId, QStringLiteral("ws-main"));
        QCOMPARE(result->profiles.size(), 2);
        QCOMPARE(result->profiles.front().clientId, QStringLiteral("client-a"));
        QCOMPARE(result->profiles.front().appVersion, QStringLiteral("0.2.0"));
        QVERIFY(result->profiles.front().supports(
            MessageRoutingCapabilities::serverReceiveV1()));
        QCOMPARE(result->profiles.back().clientId, QStringLiteral("legacy"));
        QVERIFY(!result->profiles.back().supports(
            MessageRoutingCapabilities::serverReceiveV1()));

        const QJsonObject body = transport->postBodies.front();
        QCOMPARE(body[QStringLiteral("workspaceId")].toString(),
                 QStringLiteral("ws-main"));
        QCOMPARE(body[QStringLiteral("requiredCapability")].toString(),
                 MessageRoutingCapabilities::serverReceiveV1());
        QCOMPARE(body[QStringLiteral("clientIds")].toArray().size(), 2);
    }

    void checkHealth_getsHealthEndpointAndAcceptsReadyService()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("http://chat.local:8765/api/v1/health"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("ready"), true}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        QVERIFY(client.checkHealth(&error));
        QVERIFY(error.isEmpty());
        QCOMPARE(transport->getUrls.size(), 1);
        QCOMPARE(transport->getUrls.front(),
                 QStringLiteral("http://chat.local:8765/api/v1/health"));
    }

    void checkHealth_rejectsUnreadyService()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("http://chat.local:8765/api/v1/health"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("ready"), false},
                {QStringLiteral("status"), QStringLiteral("starting")}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        QVERIFY(!client.checkHealth(&error));
        QVERIFY(error.contains(QStringLiteral("starting")));
    }

    void acknowledgeDelivered_rejectsMalformedAckResponse()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/messages/"
                           "srv-1/delivery-ack"),
            QJsonDocument(QJsonObject{}));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        QVERIFY(!client.acknowledgeDelivered(QStringLiteral("srv-1"), 42, &error));
        QVERIFY(!error.isEmpty());
    }

    void malformedJsonReturnsError()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();
        transport->jsonByRequest.insert(
            QStringLiteral("POST http://chat.local:8765/api/v1/messages"),
            QJsonDocument(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("serverSeq"), 42}
            }));

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto ack = client.sendMessage(textDraft(), &error);

        QVERIFY(!ack.has_value());
        QVERIFY(!error.isEmpty());
    }

    void transportFailurePropagatesError()
    {
        auto transport = std::make_shared<FakeServerMessageTransport>();

        ServerMessageClient client(configuredSettings(), transport);
        QString error;
        const auto ack = client.sendMessage(textDraft(), &error);

        QVERIFY(!ack.has_value());
        QVERIFY(error.contains(QStringLiteral("missing fake POST")));
    }
};

QTEST_MAIN(TestServerMessageClient)
#include "TestServerMessageClient.moc"
