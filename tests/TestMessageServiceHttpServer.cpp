#include <QtTest>

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTcpServer>
#include <QTemporaryDir>
#include <QUuid>

#include <algorithm>
#include <functional>
#include <memory>

#include "FileServiceAuth.h"
#include "FileServiceDatabase.h"
#include "FileServiceHttpServer.h"
#include "FileStorageManager.h"
#include "MessageEventBus.h"
#include "MessageServiceDatabase.h"
#include "MessageServiceHttpRoutes.h"
#include "MessageServiceOperations.h"
#include "MessageSessionRegistry.h"
#include "services/MessageRoutingCapabilities.h"
#include "integrations/ServerMessageClient.h"

namespace {
QString uniqueConn(const QString& prefix)
{
    return prefix + QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString jsonScope(std::initializer_list<const char*> workspaces)
{
    QJsonArray array;
    for (const char* workspace : workspaces)
        array.append(QString::fromLatin1(workspace));
    return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}

quint16 reserveFreePort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0))
        return 0;
    const quint16 port = probe.serverPort();
    probe.close();
    return port;
}

QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> getRequest(
    QNetworkAccessManager& nam,
    quint16 port,
    const QString& path,
    const QByteArray& token = {},
    const QByteArray& clientId = {})
{
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                                     .arg(port)
                                     .arg(path)));
    if (!token.isEmpty())
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token);
    if (!clientId.trimmed().isEmpty())
        request.setRawHeader("X-Client-Id", clientId.trimmed());
    return QScopedPointer<QNetworkReply, QScopedPointerDeleteLater>(nam.get(request));
}

QScopedPointer<QNetworkReply, QScopedPointerDeleteLater> postJson(
    QNetworkAccessManager& nam,
    quint16 port,
    const QString& path,
    const QJsonObject& body,
    const QByteArray& token,
    const QByteArray& clientId = {})
{
    QNetworkRequest request(QUrl(QStringLiteral("http://127.0.0.1:%1%2")
                                     .arg(port)
                                     .arg(path)));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + token);
    if (!clientId.trimmed().isEmpty())
        request.setRawHeader("X-Client-Id", clientId.trimmed());
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    return QScopedPointer<QNetworkReply, QScopedPointerDeleteLater>(
        nam.post(request, QJsonDocument(body).toJson(QJsonDocument::Compact)));
}

QJsonObject readObject(QNetworkReply* reply)
{
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    return document.object();
}

QJsonObject messageBody(const QString& clientMessageId,
                        QJsonArray recipients = QJsonArray{QStringLiteral("client-2")})
{
    QJsonObject body;
    body[QStringLiteral("clientMessageId")] = clientMessageId;
    body[QStringLiteral("conversationId")] = QStringLiteral("conv-1");
    body[QStringLiteral("workspaceId")] = QStringLiteral("ws-1");
    body[QStringLiteral("type")] = QStringLiteral("chat_text");
    body[QStringLiteral("body")] = QStringLiteral("hello %1").arg(clientMessageId);
    body[QStringLiteral("payload")] = QJsonObject{{QStringLiteral("format"),
                                                   QStringLiteral("plain")}};
    body[QStringLiteral("contentType")] = QStringLiteral("text/plain");
    body[QStringLiteral("recipientIds")] = recipients;
    return body;
}

struct ServerFixture {
    QTemporaryDir dir;
    QString fileConn = uniqueConn(QStringLiteral("test-message-http-file-"));
    QString messageConn = uniqueConn(QStringLiteral("test-message-http-message-"));
    std::unique_ptr<FileServiceDatabase> fileDb;
    std::unique_ptr<MessageServiceDatabase> messageDb;
    std::unique_ptr<FileStorageManager> storage;
    std::unique_ptr<FileServiceAuth> auth;
    std::unique_ptr<MessageEventBus> eventBus;
    std::unique_ptr<MessageServiceOperations> operations;
    std::unique_ptr<MessageSessionRegistry> sessions;
    std::unique_ptr<FileServiceHttpServer> server;
    std::function<QJsonObject()> healthProvider;
    quint16 port = 0;
    bool withEventBus = true;
    qint64 sessionTtlMs = 120000;

    bool start()
    {
        fileDb = std::make_unique<FileServiceDatabase>(
            dir.filePath(QStringLiteral("service.db")), fileConn);
        messageDb = std::make_unique<MessageServiceDatabase>(
            dir.filePath(QStringLiteral("service.db")), messageConn);
        storage = std::make_unique<FileStorageManager>(
            dir.filePath(QStringLiteral("storage")));
        auth = std::make_unique<FileServiceAuth>(fileDb.get());
        if (withEventBus) {
            eventBus = std::make_unique<MessageEventBus>();
        }
        operations = std::make_unique<MessageServiceOperations>();
        sessions = std::make_unique<MessageSessionRegistry>(sessionTtlMs);

        if (!dir.isValid() || !fileDb->open() || !messageDb->open())
            return false;
        if (!auth->seedOrUpdateTokenScope(QStringLiteral("tok-1"),
                                          QStringLiteral("client-1"),
                                          QStringLiteral("Client 1"),
                                          jsonScope({"ws-1"})))
            return false;
        if (!auth->seedOrUpdateTokenScope(QStringLiteral("tok-2"),
                                          QStringLiteral("client-2"),
                                          QStringLiteral("Client 2"),
                                          jsonScope({"ws-1"})))
            return false;
        if (!auth->seedOrUpdateTokenScope(QStringLiteral("tok-3"),
                                          QStringLiteral("client-3"),
                                          QStringLiteral("Client 3"),
                                          jsonScope({"ws-1"})))
            return false;
        if (!auth->seedOrUpdateTokenScope(QStringLiteral("tok-other"),
                                          QStringLiteral("client-other"),
                                          QStringLiteral("Other"),
                                          jsonScope({"ws-2"})))
            return false;
        if (!auth->seedOrUpdateTokenSecurity(QStringLiteral("tok-admin"),
                                             QStringLiteral("admin-1"),
                                             QStringLiteral("Admin"),
                                             QStringLiteral("*"),
                                             QStringLiteral("admin"),
                                             jsonScope({"admin:read",
                                                        "admin:write",
                                                        "audit:read",
                                                        "token:write",
                                                        "metrics:read"})))
            return false;

        server = std::make_unique<FileServiceHttpServer>(
            fileDb.get(), storage.get(), auth.get(), QString(), QString(), QString(),
            [this](QHttpServer& httpServer) {
                MessageServiceHttpRoutes::registerRoutes(httpServer,
                                                         auth.get(),
                                                         messageDb.get(),
                                                         eventBus.get(),
                                                         operations.get(),
                                                         sessions.get(),
                                                         healthProvider);
            });

        port = reserveFreePort();
        return port != 0 && server->listen(QHostAddress::LocalHost, port);
    }
};
}

class TestMessageServiceHttpServer : public QObject {
    Q_OBJECT

private slots:
    void messageRequestBodyHasHardSizeLimit()
    {
        constexpr qsizetype maxBodyBytes = 16 * 1024 * 1024;
        QVERIFY(MessageServiceHttpRoutes::isAcceptedMessageRequestBodySize(
            maxBodyBytes));
        QVERIFY(!MessageServiceHttpRoutes::isAcceptedMessageRequestBodySize(
            maxBodyBytes + 1));
    }

    void health_reportsReadySnapshot();
    void health_reportsServiceUnavailableWhenUnready();
    void capabilities_reportsReliableMessageSupport();
    void eventsStream_returnsSseHeartbeat();
    void postMessage_publishesMessageCreatedEvent();
    void eventsStream_excludesForeignConversationEvents();
    void eventsStream_survivesNewEventBusInstance();
    void sendMessage_failsWhenEventAppendFails();
    void postMessage_rateLimitReturnsTooManyRequests();
    void metrics_reportsCountersAndRecentAuditEntries();
    void eventsStream_registersDeviceSessionAndMetrics();
    void sessionHeartbeat_refreshesSession();
    void sessionHeartbeat_recordsCapabilityProfileForQuery();
    void sessionHeartbeat_publishesSessionOnlineEvent();
    void onlineSessions_returnsCurrentWorkspaceSessions();
    void eventsStream_publishesSessionOfflineForExpiredSession();
    void sessionHeartbeat_workspaceDeniedReturnsForbidden();
    void postMessage_persistsAndReturnsAck();
    void sharedAdminToken_usesRequestClientIdentityForMessageAndAck();
    void sharedAdminToken_thirdPartyCannotReadDirectConversation();
    void serverMessageClient_sharedTokenIdentityRoundTrip();
    void memberToken_rejectsDifferentRequestClientIdentity();
    void postMessage_sameClientMessageIdReturnsDuplicateAck();
    void listConversations_returnsOnlyClientMemberships();
    void listMessages_afterSeqReturnsOnlyMissingMessages();
    void listMessages_nonMemberReturnsForbidden();
    void deliveryAck_updatesRecipientDelivery();
    void deliveryAck_publishesMessageDeliveredEvent();
    void readAck_updatesRecipientReadState();
    void readAck_publishesMessageReadEvent();
    void postMessage_workspaceDeniedReturnsForbidden();
    void adminWorkspaceApi_createsListsAndAuditsWithoutMessageBody();
    void scopedToken_deniesOperationOutsideScope();
};

void TestMessageServiceHttpServer::health_reportsReadySnapshot()
{
    ServerFixture fixture;
    fixture.healthProvider = [] {
        return QJsonObject{
            {QStringLiteral("service"), QStringLiteral("LeyoChatService")},
            {QStringLiteral("ready"), true},
            {QStringLiteral("status"), QStringLiteral("ready")},
            {QStringLiteral("version"), QStringLiteral("test-version")},
            {QStringLiteral("serviceTimeUtc"), QStringLiteral("2026-06-12T01:02:03Z")},
            {QStringLiteral("processStartedAtUtc"), QStringLiteral("2026-06-12T00:00:00Z")},
            {QStringLiteral("database"), QJsonObject{
                {QStringLiteral("open"), true},
                {QStringLiteral("migrationComplete"), true}
            }}
        };
    };
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = getRequest(nam, fixture.port, QStringLiteral("/api/v1/health"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonObject object = readObject(reply.data());
    QCOMPARE(object[QStringLiteral("service")].toString(), QStringLiteral("LeyoChatService"));
    QCOMPARE(object[QStringLiteral("ready")].toBool(), true);
    QCOMPARE(object[QStringLiteral("status")].toString(), QStringLiteral("ready"));
    QCOMPARE(object[QStringLiteral("version")].toString(), QStringLiteral("test-version"));
    QVERIFY(!object[QStringLiteral("serviceTimeUtc")].toString().isEmpty());
    QVERIFY(!object[QStringLiteral("processStartedAtUtc")].toString().isEmpty());
    QCOMPARE(object[QStringLiteral("database")].toObject()
                 [QStringLiteral("migrationComplete")].toBool(),
             true);
}

void TestMessageServiceHttpServer::health_reportsServiceUnavailableWhenUnready()
{
    ServerFixture fixture;
    fixture.healthProvider = [] {
        return QJsonObject{
            {QStringLiteral("service"), QStringLiteral("LeyoChatService")},
            {QStringLiteral("ready"), false},
            {QStringLiteral("status"), QStringLiteral("unready")},
            {QStringLiteral("version"), QStringLiteral("test-version")},
            {QStringLiteral("serviceTimeUtc"), QStringLiteral("2026-06-12T01:02:03Z")},
            {QStringLiteral("processStartedAtUtc"), QStringLiteral("2026-06-12T00:00:00Z")},
            {QStringLiteral("database"), QJsonObject{
                {QStringLiteral("open"), false},
                {QStringLiteral("migrationComplete"), false}
            }}
        };
    };
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = getRequest(nam, fixture.port, QStringLiteral("/api/v1/health"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 503);
    const QJsonObject object = readObject(reply.data());
    QCOMPARE(object[QStringLiteral("ready")].toBool(), false);
    QCOMPARE(object[QStringLiteral("status")].toString(), QStringLiteral("unready"));
    QCOMPARE(object[QStringLiteral("database")].toObject()
                 [QStringLiteral("open")].toBool(),
             false);
}

void TestMessageServiceHttpServer::capabilities_reportsReliableMessageSupport()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = getRequest(nam, fixture.port, QStringLiteral("/api/v1/capabilities"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonObject object = readObject(reply.data());
    QCOMPARE(object[QStringLiteral("file_service")].toBool(), true);
    QCOMPARE(object[QStringLiteral("message_service")].toBool(), true);
    QCOMPARE(object[QStringLiteral("reliable_message")].toBool(), true);
    QCOMPARE(object[QStringLiteral("p2p_fallback_supported")].toBool(), true);
}

void TestMessageServiceHttpServer::postMessage_rateLimitReturnsTooManyRequests()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());
    fixture.operations->setRateLimit(1, 60000);

    QNetworkAccessManager nam;
    auto firstReply = postJson(nam,
                               fixture.port,
                               QStringLiteral("/api/v1/messages"),
                               messageBody(QStringLiteral("local-rate-1")),
                               QByteArrayLiteral("tok-1"));
    QSignalSpy firstFinished(firstReply.data(), &QNetworkReply::finished);
    QVERIFY(firstFinished.wait(5000));
    QCOMPARE(firstReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto secondReply = postJson(nam,
                                fixture.port,
                                QStringLiteral("/api/v1/messages"),
                                messageBody(QStringLiteral("local-rate-2")),
                                QByteArrayLiteral("tok-1"));
    QSignalSpy secondFinished(secondReply.data(), &QNetworkReply::finished);
    QVERIFY(secondFinished.wait(5000));
    QCOMPARE(secondReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 429);

    auto metricsReply = getRequest(nam,
                                   fixture.port,
                                   QStringLiteral("/api/v1/metrics"),
                                   QByteArrayLiteral("tok-1"));
    QSignalSpy metricsFinished(metricsReply.data(), &QNetworkReply::finished);
    QVERIFY(metricsFinished.wait(5000));
    QCOMPARE(metricsReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonObject metrics = readObject(metricsReply.data());
    const QJsonObject counters = metrics[QStringLiteral("counters")].toObject();
    QCOMPARE(counters[QStringLiteral("rejected")].toObject()
                 [QStringLiteral("post_message")].toInteger(),
             qint64(1));
}

void TestMessageServiceHttpServer::metrics_reportsCountersAndRecentAuditEntries()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam,
                              fixture.port,
                              QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-metrics-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=0&limit=10"),
                                  QByteArrayLiteral("tok-2"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));
    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto metricsReply = getRequest(nam,
                                   fixture.port,
                                   QStringLiteral("/api/v1/metrics"),
                                   QByteArrayLiteral("tok-1"));
    QSignalSpy metricsFinished(metricsReply.data(), &QNetworkReply::finished);
    QVERIFY(metricsFinished.wait(5000));
    QCOMPARE(metricsReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QJsonObject metrics = readObject(metricsReply.data());
    QCOMPARE(metrics[QStringLiteral("ok")].toBool(), true);
    const QJsonObject accepted =
        metrics[QStringLiteral("counters")].toObject()
            [QStringLiteral("accepted")].toObject();
    QCOMPARE(accepted[QStringLiteral("post_message")].toInteger(), qint64(1));
    QCOMPARE(accepted[QStringLiteral("events_stream")].toInteger(), qint64(1));
    QVERIFY(metrics[QStringLiteral("audit")].toObject()
                [QStringLiteral("recent")].toArray().size() >= 2);
}

void TestMessageServiceHttpServer::eventsStream_registersDeviceSessionAndMetrics()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&deviceId=pc-a"
                                                 "&afterEventId=0&limit=10"),
                                  QByteArrayLiteral("tok-1"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));
    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto metricsReply = getRequest(nam,
                                   fixture.port,
                                   QStringLiteral("/api/v1/metrics"),
                                   QByteArrayLiteral("tok-1"));
    QSignalSpy metricsFinished(metricsReply.data(), &QNetworkReply::finished);
    QVERIFY(metricsFinished.wait(5000));
    QCOMPARE(metricsReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QJsonObject sessions =
        readObject(metricsReply.data())[QStringLiteral("sessions")].toObject();
    QCOMPARE(sessions[QStringLiteral("onlineSessions")].toInt(), 1);
    QCOMPARE(sessions[QStringLiteral("onlineClients")].toInt(), 1);
    QCOMPARE(sessions[QStringLiteral("onlineDevices")].toInt(), 1);
}

void TestMessageServiceHttpServer::sessionHeartbeat_refreshesSession()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QJsonObject body{
        {QStringLiteral("workspaceId"), QStringLiteral("ws-1")},
        {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
        {QStringLiteral("lastEventId"), 12}
    };

    QNetworkAccessManager nam;
    auto reply = postJson(nam,
                          fixture.port,
                          QStringLiteral("/api/v1/sessions/heartbeat"),
                          body,
                          QByteArrayLiteral("tok-1"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QJsonObject response = readObject(reply.data());
    QCOMPARE(response[QStringLiteral("ok")].toBool(), true);
    QCOMPARE(response[QStringLiteral("clientId")].toString(), QStringLiteral("client-1"));
    QCOMPARE(response[QStringLiteral("deviceId")].toString(), QStringLiteral("pc-a"));
    QCOMPARE(response[QStringLiteral("workspaceId")].toString(), QStringLiteral("ws-1"));
    QCOMPARE(response[QStringLiteral("lastEventId")].toInteger(), qint64(12));
    QVERIFY(!response[QStringLiteral("sessionId")].toString().isEmpty());
}

void TestMessageServiceHttpServer::sessionHeartbeat_recordsCapabilityProfileForQuery()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto heartbeatReply = postJson(
        nam,
        fixture.port,
        QStringLiteral("/api/v1/sessions/heartbeat"),
        QJsonObject{
            {QStringLiteral("workspaceId"), QStringLiteral("ws-1")},
            {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
            {QStringLiteral("lastEventId"), 12},
            {QStringLiteral("appVersion"), QStringLiteral("0.2.0")},
            {QStringLiteral("capabilities"),
             QJsonArray{MessageRoutingCapabilities::serverReceiveV1()}}
        },
        QByteArrayLiteral("tok-1"));
    QSignalSpy heartbeatFinished(heartbeatReply.data(), &QNetworkReply::finished);
    QVERIFY(heartbeatFinished.wait(5000));
    QCOMPARE(heartbeatReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
             200);

    const QJsonObject heartbeat = readObject(heartbeatReply.data());
    QCOMPARE(heartbeat[QStringLiteral("appVersion")].toString(),
             QStringLiteral("0.2.0"));
    QVERIFY(heartbeat[QStringLiteral("capabilities")].toArray().contains(
        MessageRoutingCapabilities::serverReceiveV1()));

    auto queryReply = postJson(
        nam,
        fixture.port,
        QStringLiteral("/api/v1/clients/capabilities/query"),
        QJsonObject{
            {QStringLiteral("workspaceId"), QStringLiteral("ws-1")},
            {QStringLiteral("requiredCapability"),
             MessageRoutingCapabilities::serverReceiveV1()},
            {QStringLiteral("clientIds"),
             QJsonArray{QStringLiteral("client-1"), QStringLiteral("legacy")}}
        },
        QByteArrayLiteral("tok-1"));
    QSignalSpy queryFinished(queryReply.data(), &QNetworkReply::finished);
    QVERIFY(queryFinished.wait(5000));
    QCOMPARE(queryReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
             200);

    const QJsonObject response = readObject(queryReply.data());
    QCOMPARE(response[QStringLiteral("ok")].toBool(), true);
    const QJsonArray profiles = response[QStringLiteral("profiles")].toArray();
    QCOMPARE(profiles.size(), 2);

    const QJsonObject clientOne = profiles.at(0).toObject();
    QCOMPARE(clientOne[QStringLiteral("clientId")].toString(),
             QStringLiteral("client-1"));
    QCOMPARE(clientOne[QStringLiteral("appVersion")].toString(),
             QStringLiteral("0.2.0"));
    QCOMPARE(clientOne[QStringLiteral("supportsRequiredCapability")].toBool(),
             true);

    const QJsonObject legacy = profiles.at(1).toObject();
    QCOMPARE(legacy[QStringLiteral("clientId")].toString(),
             QStringLiteral("legacy"));
    QVERIFY(legacy[QStringLiteral("capabilities")].toArray().isEmpty());
    QCOMPARE(legacy[QStringLiteral("supportsRequiredCapability")].toBool(),
             false);
}

void TestMessageServiceHttpServer::sessionHeartbeat_publishesSessionOnlineEvent()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto heartbeatReply = postJson(nam,
                                   fixture.port,
                                   QStringLiteral("/api/v1/sessions/heartbeat"),
                                   QJsonObject{
                                       {QStringLiteral("workspaceId"), QStringLiteral("ws-1")},
                                       {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
                                       {QStringLiteral("lastEventId"), 12}
                                   },
                                   QByteArrayLiteral("tok-1"));
    QSignalSpy heartbeatFinished(heartbeatReply.data(), &QNetworkReply::finished);
    QVERIFY(heartbeatFinished.wait(5000));
    QCOMPARE(heartbeatReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QString sessionId =
        readObject(heartbeatReply.data())[QStringLiteral("sessionId")].toString();
    QVERIFY(!sessionId.isEmpty());

    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=0&limit=10"),
                                  QByteArrayLiteral("tok-2"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));
    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QByteArray body = streamReply->readAll();
    QVERIFY(body.contains(QByteArrayLiteral("event: session.online")));
    QVERIFY(body.contains(sessionId.toUtf8()));
    QVERIFY(body.contains(QByteArrayLiteral("\"clientId\":\"client-1\"")));
    QVERIFY(body.contains(QByteArrayLiteral("\"deviceId\":\"pc-a\"")));
    QVERIFY(body.contains(QByteArrayLiteral("\"lastEventId\":12")));
}

void TestMessageServiceHttpServer::onlineSessions_returnsCurrentWorkspaceSessions()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto heartbeatReply = postJson(nam,
                                   fixture.port,
                                   QStringLiteral("/api/v1/sessions/heartbeat"),
                                   QJsonObject{
                                       {QStringLiteral("workspaceId"), QStringLiteral("ws-1")},
                                       {QStringLiteral("deviceId"), QStringLiteral("pc-b")},
                                       {QStringLiteral("lastEventId"), 18},
                                       {QStringLiteral("appVersion"), QStringLiteral("0.3.3")},
                                       {QStringLiteral("capabilities"),
                                        QJsonArray{MessageRoutingCapabilities::serverReceiveV1()}}
                                   },
                                   QByteArrayLiteral("tok-2"));
    QSignalSpy heartbeatFinished(heartbeatReply.data(), &QNetworkReply::finished);
    QVERIFY(heartbeatFinished.wait(5000));
    QCOMPARE(heartbeatReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QString sessionId =
        readObject(heartbeatReply.data())[QStringLiteral("sessionId")].toString();
    QVERIFY(!sessionId.isEmpty());

    auto onlineReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/sessions/online"
                                                 "?workspaceId=ws-1"),
                                  QByteArrayLiteral("tok-1"));
    QSignalSpy onlineFinished(onlineReply.data(), &QNetworkReply::finished);
    QVERIFY(onlineFinished.wait(5000));
    QCOMPARE(onlineReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QJsonObject object = readObject(onlineReply.data());
    QCOMPARE(object[QStringLiteral("ok")].toBool(), true);
    QCOMPARE(object[QStringLiteral("workspaceId")].toString(), QStringLiteral("ws-1"));
    const QJsonArray sessions = object[QStringLiteral("sessions")].toArray();
    QCOMPARE(sessions.size(), 1);
    const QJsonObject session = sessions.at(0).toObject();
    QCOMPARE(session[QStringLiteral("sessionId")].toString(), sessionId);
    QCOMPARE(session[QStringLiteral("clientId")].toString(), QStringLiteral("client-2"));
    QCOMPARE(session[QStringLiteral("deviceId")].toString(), QStringLiteral("pc-b"));
    QCOMPARE(session[QStringLiteral("workspaceId")].toString(), QStringLiteral("ws-1"));
    QCOMPARE(session[QStringLiteral("lastEventId")].toInteger(), qint64(18));
    QCOMPARE(session[QStringLiteral("appVersion")].toString(), QStringLiteral("0.3.3"));
    QVERIFY(session[QStringLiteral("capabilities")].toArray().contains(
        MessageRoutingCapabilities::serverReceiveV1()));
}

void TestMessageServiceHttpServer::eventsStream_publishesSessionOfflineForExpiredSession()
{
    ServerFixture fixture;
    fixture.sessionTtlMs = 5;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto heartbeatReply = postJson(nam,
                                   fixture.port,
                                   QStringLiteral("/api/v1/sessions/heartbeat"),
                                   QJsonObject{
                                       {QStringLiteral("workspaceId"), QStringLiteral("ws-1")},
                                       {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
                                       {QStringLiteral("lastEventId"), 1}
                                   },
                                   QByteArrayLiteral("tok-1"));
    QSignalSpy heartbeatFinished(heartbeatReply.data(), &QNetworkReply::finished);
    QVERIFY(heartbeatFinished.wait(5000));
    QCOMPARE(heartbeatReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QString sessionId =
        readObject(heartbeatReply.data())[QStringLiteral("sessionId")].toString();
    QVERIFY(!sessionId.isEmpty());

    QTest::qWait(30);

    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=1&limit=10"),
                                  QByteArrayLiteral("tok-2"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));
    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QByteArray body = streamReply->readAll();
    QVERIFY(body.contains(QByteArrayLiteral("event: session.offline")));
    QVERIFY(body.contains(sessionId.toUtf8()));
    QVERIFY(body.contains(QByteArrayLiteral("\"clientId\":\"client-1\"")));
    QVERIFY(body.contains(QByteArrayLiteral("\"deviceId\":\"pc-a\"")));
}

void TestMessageServiceHttpServer::sessionHeartbeat_workspaceDeniedReturnsForbidden()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = postJson(nam,
                          fixture.port,
                          QStringLiteral("/api/v1/sessions/heartbeat"),
                          QJsonObject{
                              {QStringLiteral("workspaceId"), QStringLiteral("ws-1")},
                              {QStringLiteral("deviceId"), QStringLiteral("pc-a")},
                              {QStringLiteral("lastEventId"), 0}
                          },
                          QByteArrayLiteral("tok-other"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);
}

void TestMessageServiceHttpServer::eventsStream_returnsSseHeartbeat()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = getRequest(nam,
                            fixture.port,
                            QStringLiteral("/api/v1/events/stream"
                                           "?workspaceId=ws-1&afterEventId=0&limit=10"),
                            QByteArrayLiteral("tok-1"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    QCOMPARE(reply->header(QNetworkRequest::ContentTypeHeader).toString(),
             QStringLiteral("text/event-stream"));
    const QByteArray body = reply->readAll();
    QVERIFY(body.contains(QByteArrayLiteral(": leyochat heartbeat")));
    QVERIFY(body.contains(QByteArrayLiteral("retry: 5000")));
}

void TestMessageServiceHttpServer::postMessage_publishesMessageCreatedEvent()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam,
                              fixture.port,
                              QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();
    QVERIFY(!serverMessageId.isEmpty());

    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=0&limit=10"),
                                  QByteArrayLiteral("tok-2"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));

    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QByteArray body = streamReply->readAll();
    QVERIFY(body.contains(QByteArrayLiteral("event: message.created")));
    QVERIFY(body.contains(serverMessageId.toUtf8()));
    QVERIFY(body.contains(QByteArrayLiteral("\"conversationId\":\"conv-1\"")));
    QVERIFY(body.contains(QByteArrayLiteral("\"serverSeq\":1")));
}

void TestMessageServiceHttpServer::eventsStream_excludesForeignConversationEvents()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    const StoreMessageResult visible =
        fixture.messageDb->storeMessage(StoreMessageRequest{
            QStringLiteral("visible-event"),
            QStringLiteral("conv-visible"),
            QStringLiteral("ws-1"),
            QStringLiteral("client-1"),
            QStringLiteral("chat_text"),
            QStringLiteral("visible"),
            QStringLiteral("{}"),
            QString(),
            QStringLiteral("text/plain"),
            QString(),
            QStringList{QStringLiteral("client-2")},
            1000});
    QVERIFY2(visible.ok, qPrintable(visible.error));

    const StoreMessageResult foreign =
        fixture.messageDb->storeMessage(StoreMessageRequest{
            QStringLiteral("foreign-event"),
            QStringLiteral("conv-foreign"),
            QStringLiteral("ws-1"),
            QStringLiteral("client-3"),
            QStringLiteral("chat_text"),
            QStringLiteral("foreign"),
            QStringLiteral("{}"),
            QString(),
            QStringLiteral("text/plain"),
            QString(),
            QStringList{QStringLiteral("client-1")},
            1001});
    QVERIFY2(foreign.ok, qPrintable(foreign.error));

    QNetworkAccessManager nam;
    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=0&limit=10"),
                                  QByteArrayLiteral("tok-2"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));
    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QByteArray body = streamReply->readAll();
    QVERIFY(body.contains(visible.message.serverMessageId.toUtf8()));
    QVERIFY(body.contains(QByteArrayLiteral("\"conversationId\":\"conv-visible\"")));
    QVERIFY(!body.contains(foreign.message.serverMessageId.toUtf8()));
    QVERIFY(!body.contains(QByteArrayLiteral("\"conversationId\":\"conv-foreign\"")));
}

void TestMessageServiceHttpServer::eventsStream_survivesNewEventBusInstance()
{
    ServerFixture fixture;
    fixture.withEventBus = false;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam,
                              fixture.port,
                              QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-durable-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();
    QVERIFY(!serverMessageId.isEmpty());

    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=0&limit=10"),
                                  QByteArrayLiteral("tok-2"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));

    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QByteArray body = streamReply->readAll();
    QVERIFY(body.contains(QByteArrayLiteral("event: message.created")));
    QVERIFY(body.contains(serverMessageId.toUtf8()));
    QVERIFY(body.contains(QByteArrayLiteral("id: 1")));
}

void TestMessageServiceHttpServer::sendMessage_failsWhenEventAppendFails()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QSqlQuery dropEvents(QSqlDatabase::database(fixture.messageConn));
    QVERIFY(dropEvents.exec(QStringLiteral("DROP TABLE message_events")));

    QNetworkAccessManager nam;
    auto reply = postJson(nam,
                          fixture.port,
                          QStringLiteral("/api/v1/messages"),
                          messageBody(QStringLiteral("local-event-fail")),
                          QByteArrayLiteral("tok-1"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 500);
    QVERIFY(fixture.messageDb
                ->listMessagesAfterSeq(QStringLiteral("conv-1"), 0, 10)
                .isEmpty());
}

void TestMessageServiceHttpServer::postMessage_persistsAndReturnsAck()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                          messageBody(QStringLiteral("local-1")), QByteArrayLiteral("tok-1"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonObject object = readObject(reply.data());
    QCOMPARE(object[QStringLiteral("ok")].toBool(), true);
    QCOMPARE(object[QStringLiteral("duplicate")].toBool(), false);
    QCOMPARE(object[QStringLiteral("serverSeq")].toInteger(), qint64(1));
    const QString serverMessageId = object[QStringLiteral("serverMessageId")].toString();
    QVERIFY(!serverMessageId.isEmpty());
    QVERIFY(fixture.messageDb->findMessageByServerId(serverMessageId).has_value());
}

void TestMessageServiceHttpServer::postMessage_sameClientMessageIdReturnsDuplicateAck()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto firstReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                               messageBody(QStringLiteral("local-1")),
                               QByteArrayLiteral("tok-1"));
    QSignalSpy firstFinished(firstReply.data(), &QNetworkReply::finished);
    QVERIFY(firstFinished.wait(5000));
    QCOMPARE(firstReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonObject first = readObject(firstReply.data());

    QJsonObject retry = messageBody(QStringLiteral("local-1"));
    retry[QStringLiteral("body")] = QStringLiteral("retry should not overwrite");
    auto retryReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                               retry, QByteArrayLiteral("tok-1"));
    QSignalSpy retryFinished(retryReply.data(), &QNetworkReply::finished);
    QVERIFY(retryFinished.wait(5000));

    QCOMPARE(retryReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonObject duplicate = readObject(retryReply.data());
    QCOMPARE(duplicate[QStringLiteral("duplicate")].toBool(), true);
    QCOMPARE(duplicate[QStringLiteral("serverMessageId")].toString(),
             first[QStringLiteral("serverMessageId")].toString());
    QCOMPARE(fixture.messageDb->listMessagesAfterSeq(QStringLiteral("conv-1"), 0, 10).size(), 1);
}

void TestMessageServiceHttpServer::listConversations_returnsOnlyClientMemberships()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto memberReply = getRequest(nam, fixture.port,
                                  QStringLiteral("/api/v1/conversations"
                                                 "?workspaceId=ws-1&limit=100"),
                                  QByteArrayLiteral("tok-2"));
    QSignalSpy memberFinished(memberReply.data(), &QNetworkReply::finished);
    QVERIFY(memberFinished.wait(5000));
    QCOMPARE(memberReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonArray memberConversations =
        readObject(memberReply.data())[QStringLiteral("conversations")].toArray();
    QCOMPARE(memberConversations.size(), 1);
    QCOMPARE(memberConversations.at(0).toObject()
                 [QStringLiteral("conversationId")].toString(),
             QStringLiteral("conv-1"));

    auto nonMemberReply = getRequest(nam, fixture.port,
                                     QStringLiteral("/api/v1/conversations"
                                                    "?workspaceId=ws-1&limit=100"),
                                     QByteArrayLiteral("tok-3"));
    QSignalSpy nonMemberFinished(nonMemberReply.data(), &QNetworkReply::finished);
    QVERIFY(nonMemberFinished.wait(5000));
    QCOMPARE(nonMemberReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    QCOMPARE(readObject(nonMemberReply.data())
                 [QStringLiteral("conversations")].toArray().size(),
             0);
}

void TestMessageServiceHttpServer::listMessages_afterSeqReturnsOnlyMissingMessages()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    for (const QString& id : {QStringLiteral("local-1"), QStringLiteral("local-2")}) {
        auto reply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                              messageBody(id), QByteArrayLiteral("tok-1"));
        QSignalSpy finished(reply.data(), &QNetworkReply::finished);
        QVERIFY(finished.wait(5000));
        QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    }

    auto reply = getRequest(nam, fixture.port,
                            QStringLiteral("/api/v1/conversations/conv-1/messages"
                                           "?workspaceId=ws-1&afterSeq=1&limit=100"),
                            QByteArrayLiteral("tok-2"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonObject object = readObject(reply.data());
    const QJsonArray messages = object[QStringLiteral("messages")].toArray();
    QCOMPARE(messages.size(), 1);
    QCOMPARE(messages.at(0).toObject()[QStringLiteral("clientMessageId")].toString(),
             QStringLiteral("local-2"));
    QCOMPARE(object[QStringLiteral("nextAfterSeq")].toInteger(), qint64(2));
}

void TestMessageServiceHttpServer::listMessages_nonMemberReturnsForbidden()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto reply = getRequest(nam, fixture.port,
                            QStringLiteral("/api/v1/conversations/conv-1/messages"
                                           "?workspaceId=ws-1&afterSeq=0&limit=100"),
                            QByteArrayLiteral("tok-3"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);
}

void TestMessageServiceHttpServer::deliveryAck_updatesRecipientDelivery()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();

    QJsonObject ack;
    ack[QStringLiteral("receivedSeq")] = 1;
    auto ackReply = postJson(nam, fixture.port,
                             QStringLiteral("/api/v1/messages/%1/delivery-ack")
                                 .arg(serverMessageId),
                             ack, QByteArrayLiteral("tok-2"));
    QSignalSpy ackFinished(ackReply.data(), &QNetworkReply::finished);
    QVERIFY(ackFinished.wait(5000));

    QCOMPARE(ackReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const auto deliveries = fixture.messageDb->listDeliveries(serverMessageId);
    QCOMPARE(deliveries.size(), 1);
    QCOMPARE(deliveries.front().state, QStringLiteral("delivered"));
}

void TestMessageServiceHttpServer::sharedAdminToken_usesRequestClientIdentityForMessageAndAck()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());
    QVERIFY(fixture.auth->seedOrUpdateTokenSecurity(
        QStringLiteral("tok-shared-admin"),
        QStringLiteral("admin"),
        QStringLiteral("Shared Admin"),
        QStringLiteral("*"),
        QStringLiteral("admin"),
        QStringLiteral("*")));

    QNetworkAccessManager nam;
    auto postReply = postJson(nam,
                              fixture.port,
                              QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-shared-1")),
                              QByteArrayLiteral("tok-shared-admin"),
                              QByteArrayLiteral("client-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();
    QVERIFY(!serverMessageId.isEmpty());
    const auto stored = fixture.messageDb->findMessageByServerId(serverMessageId);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->senderId, QStringLiteral("client-1"));

    auto ackReply = postJson(
        nam,
        fixture.port,
        QStringLiteral("/api/v1/messages/%1/delivery-ack").arg(serverMessageId),
        QJsonObject{{QStringLiteral("receivedSeq"), 1}},
        QByteArrayLiteral("tok-shared-admin"),
        QByteArrayLiteral("client-2"));
    QSignalSpy ackFinished(ackReply.data(), &QNetworkReply::finished);
    QVERIFY(ackFinished.wait(5000));
    QCOMPARE(ackReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const auto deliveries = fixture.messageDb->listDeliveries(serverMessageId);
    QCOMPARE(deliveries.size(), 1);
    QCOMPARE(deliveries.front().recipientId, QStringLiteral("client-2"));
    QCOMPARE(deliveries.front().state, QStringLiteral("delivered"));
}

void TestMessageServiceHttpServer::sharedAdminToken_thirdPartyCannotReadDirectConversation()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());
    QVERIFY(fixture.auth->seedOrUpdateTokenSecurity(
        QStringLiteral("tok-shared-admin-isolation"),
        QStringLiteral("admin"),
        QStringLiteral("Shared Admin"),
        QStringLiteral("*"),
        QStringLiteral("admin"),
        QStringLiteral("*")));

    QNetworkAccessManager nam;
    const QByteArray sharedToken("tok-shared-admin-isolation");
    auto postReply = postJson(nam,
                              fixture.port,
                              QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("private-a-to-b")),
                              sharedToken,
                              QByteArrayLiteral("client-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
             200);
    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();
    QVERIFY(!serverMessageId.isEmpty());

    auto recipientEvents = getRequest(
        nam,
        fixture.port,
        QStringLiteral("/api/v1/events/stream"
                       "?workspaceId=ws-1&afterEventId=0&limit=10"),
        sharedToken,
        QByteArrayLiteral("client-2"));
    QSignalSpy recipientEventsFinished(recipientEvents.data(),
                                       &QNetworkReply::finished);
    QVERIFY(recipientEventsFinished.wait(5000));
    QCOMPARE(recipientEvents->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
             200);
    QVERIFY(recipientEvents->readAll().contains(serverMessageId.toUtf8()));

    auto thirdPartyEvents = getRequest(
        nam,
        fixture.port,
        QStringLiteral("/api/v1/events/stream"
                       "?workspaceId=ws-1&afterEventId=0&limit=10"),
        sharedToken,
        QByteArrayLiteral("client-3"));
    QSignalSpy thirdPartyEventsFinished(thirdPartyEvents.data(),
                                        &QNetworkReply::finished);
    QVERIFY(thirdPartyEventsFinished.wait(5000));
    QCOMPARE(thirdPartyEvents->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
             200);
    const QByteArray thirdPartyEventBody = thirdPartyEvents->readAll();
    QVERIFY(!thirdPartyEventBody.contains(serverMessageId.toUtf8()));
    QVERIFY(!thirdPartyEventBody.contains(QByteArrayLiteral("\"conversationId\":\"conv-1\"")));

    auto thirdPartyMessages = getRequest(
        nam,
        fixture.port,
        QStringLiteral("/api/v1/conversations/conv-1/messages"
                       "?workspaceId=ws-1&afterSeq=0&limit=100"),
        sharedToken,
        QByteArrayLiteral("client-3"));
    QSignalSpy thirdPartyMessagesFinished(thirdPartyMessages.data(),
                                          &QNetworkReply::finished);
    QVERIFY(thirdPartyMessagesFinished.wait(5000));
    QCOMPARE(thirdPartyMessages->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
             403);
}

void TestMessageServiceHttpServer::memberToken_rejectsDifferentRequestClientIdentity()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = postJson(nam,
                          fixture.port,
                          QStringLiteral("/api/v1/messages"),
                          messageBody(QStringLiteral("local-impersonation")),
                          QByteArrayLiteral("tok-1"),
                          QByteArrayLiteral("client-2"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));
    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 401);
}

void TestMessageServiceHttpServer::serverMessageClient_sharedTokenIdentityRoundTrip()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());
    QVERIFY(fixture.auth->seedOrUpdateTokenSecurity(
        QStringLiteral("tok-network-shared"),
        QStringLiteral("admin"),
        QStringLiteral("Shared Admin"),
        QStringLiteral("*"),
        QStringLiteral("admin"),
        QStringLiteral("*")));

    RemoteChatServiceSettings settings;
    settings.enabled = true;
    settings.baseUrl = QStringLiteral("http://127.0.0.1:%1").arg(fixture.port);
    settings.bearerToken = QStringLiteral("tok-network-shared");
    settings.workspaceId = QStringLiteral("ws-1");
    settings.mode = RemoteChatTransportMode::ServerPreferred;

    ServerMessageDraft draft;
    draft.clientMessageId = QStringLiteral("network-local-1");
    draft.conversationId = QStringLiteral("network-conversation");
    draft.workspaceId = QStringLiteral("ws-1");
    draft.type = QStringLiteral("chat_text");
    draft.body = QStringLiteral("hello over real HTTP");
    draft.recipientIds = {QStringLiteral("client-2")};

    ServerMessageClient sender(settings, QStringLiteral("client-1"));
    QString senderHeartbeatError;
    const auto senderSession = sender.sendSessionHeartbeat(
        QStringLiteral("ws-1"),
        QStringLiteral("device-client-1"),
        0,
        &senderHeartbeatError);
    QVERIFY2(senderSession.has_value(), qPrintable(senderHeartbeatError));
    QCOMPARE(senderSession->clientId, QStringLiteral("client-1"));

    QString sendError;
    const auto ack = sender.sendMessage(draft, &sendError);
    QVERIFY2(ack.has_value(), qPrintable(sendError));

    const auto stored = fixture.messageDb->findMessageByServerId(ack->serverMessageId);
    QVERIFY(stored.has_value());
    QCOMPARE(stored->senderId, QStringLiteral("client-1"));

    ServerMessageClient receiver(settings, QStringLiteral("client-2"));
    QString receiverHeartbeatError;
    const auto receiverSession = receiver.sendSessionHeartbeat(
        QStringLiteral("ws-1"),
        QStringLiteral("device-client-2"),
        0,
        &receiverHeartbeatError);
    QVERIFY2(receiverSession.has_value(), qPrintable(receiverHeartbeatError));
    QCOMPARE(receiverSession->clientId, QStringLiteral("client-2"));

    QString onlineSessionsError;
    const auto onlineSessions = receiver.listOnlineSessions(
        QStringLiteral("ws-1"), &onlineSessionsError);
    QVERIFY2(onlineSessions.has_value(), qPrintable(onlineSessionsError));
    QSet<QString> onlineClientIds;
    for (const auto& session : *onlineSessions) {
        onlineClientIds.insert(session.clientId);
    }
    QCOMPARE(onlineClientIds,
             QSet<QString>({QStringLiteral("client-1"),
                            QStringLiteral("client-2")}));

    QString ackError;
    QVERIFY2(receiver.acknowledgeDelivered(ack->serverMessageId, 1, &ackError),
             qPrintable(ackError));
    const auto deliveries = fixture.messageDb->listDeliveries(ack->serverMessageId);
    QCOMPARE(deliveries.size(), 1);
    QCOMPARE(deliveries.front().recipientId, QStringLiteral("client-2"));
    QCOMPARE(deliveries.front().state, QStringLiteral("delivered"));
}

void TestMessageServiceHttpServer::deliveryAck_publishesMessageDeliveredEvent()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();

    auto ackReply = postJson(nam,
                             fixture.port,
                             QStringLiteral("/api/v1/messages/%1/delivery-ack")
                                 .arg(serverMessageId),
                             QJsonObject{{QStringLiteral("receivedSeq"), 1}},
                             QByteArrayLiteral("tok-2"));
    QSignalSpy ackFinished(ackReply.data(), &QNetworkReply::finished);
    QVERIFY(ackFinished.wait(5000));
    QCOMPARE(ackReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=1&limit=10"),
                                  QByteArrayLiteral("tok-1"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));
    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QByteArray body = streamReply->readAll();
    QVERIFY(body.contains(QByteArrayLiteral("event: message.delivered")));
    QVERIFY(body.contains(serverMessageId.toUtf8()));
    QVERIFY(body.contains(QByteArrayLiteral("\"recipientId\":\"client-2\"")));
    QVERIFY(body.contains(QByteArrayLiteral("\"receivedSeq\":1")));
}

void TestMessageServiceHttpServer::readAck_updatesRecipientReadState()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();

    QJsonObject ack;
    ack[QStringLiteral("readSeq")] = 1;
    auto ackReply = postJson(nam, fixture.port,
                             QStringLiteral("/api/v1/messages/%1/read-ack")
                                 .arg(serverMessageId),
                             ack, QByteArrayLiteral("tok-2"));
    QSignalSpy ackFinished(ackReply.data(), &QNetworkReply::finished);
    QVERIFY(ackFinished.wait(5000));

    QCOMPARE(ackReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const auto deliveries = fixture.messageDb->listDeliveries(serverMessageId);
    QCOMPARE(deliveries.size(), 1);
    QCOMPARE(deliveries.front().state, QStringLiteral("read"));
}

void TestMessageServiceHttpServer::readAck_publishesMessageReadEvent()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto postReply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-1")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy postFinished(postReply.data(), &QNetworkReply::finished);
    QVERIFY(postFinished.wait(5000));
    QCOMPARE(postReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QString serverMessageId =
        readObject(postReply.data())[QStringLiteral("serverMessageId")].toString();

    auto ackReply = postJson(nam,
                             fixture.port,
                             QStringLiteral("/api/v1/messages/%1/read-ack")
                                 .arg(serverMessageId),
                             QJsonObject{{QStringLiteral("readSeq"), 1}},
                             QByteArrayLiteral("tok-2"));
    QSignalSpy ackFinished(ackReply.data(), &QNetworkReply::finished);
    QVERIFY(ackFinished.wait(5000));
    QCOMPARE(ackReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto streamReply = getRequest(nam,
                                  fixture.port,
                                  QStringLiteral("/api/v1/events/stream"
                                                 "?workspaceId=ws-1&afterEventId=1&limit=10"),
                                  QByteArrayLiteral("tok-1"));
    QSignalSpy streamFinished(streamReply.data(), &QNetworkReply::finished);
    QVERIFY(streamFinished.wait(5000));
    QCOMPARE(streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    const QByteArray body = streamReply->readAll();
    QVERIFY(body.contains(QByteArrayLiteral("event: message.read")));
    QVERIFY(body.contains(serverMessageId.toUtf8()));
    QVERIFY(body.contains(QByteArrayLiteral("\"recipientId\":\"client-2\"")));
    QVERIFY(body.contains(QByteArrayLiteral("\"readSeq\":1")));
}

void TestMessageServiceHttpServer::postMessage_workspaceDeniedReturnsForbidden()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto reply = postJson(nam, fixture.port, QStringLiteral("/api/v1/messages"),
                          messageBody(QStringLiteral("local-1")),
                          QByteArrayLiteral("tok-other"));
    QSignalSpy finished(reply.data(), &QNetworkReply::finished);
    QVERIFY(finished.wait(5000));

    QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);
}

void TestMessageServiceHttpServer::adminWorkspaceApi_createsListsAndAuditsWithoutMessageBody()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());

    QNetworkAccessManager nam;
    auto deniedReply = postJson(nam,
                                fixture.port,
                                QStringLiteral("/api/v1/admin/workspaces"),
                                QJsonObject{{QStringLiteral("workspaceId"), QStringLiteral("ws-admin")},
                                            {QStringLiteral("displayName"), QStringLiteral("Admin Workspace")}},
                                QByteArrayLiteral("tok-1"));
    QSignalSpy deniedFinished(deniedReply.data(), &QNetworkReply::finished);
    QVERIFY(deniedFinished.wait(5000));
    QCOMPARE(deniedReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);

    auto createReply = postJson(nam,
                                fixture.port,
                                QStringLiteral("/api/v1/admin/workspaces"),
                                QJsonObject{{QStringLiteral("workspaceId"), QStringLiteral("ws-admin")},
                                            {QStringLiteral("displayName"), QStringLiteral("Admin Workspace")},
                                            {QStringLiteral("enabled"), true}},
                                QByteArrayLiteral("tok-admin"));
    QSignalSpy createFinished(createReply.data(), &QNetworkReply::finished);
    QVERIFY(createFinished.wait(5000));
    QCOMPARE(createReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto listReply = getRequest(nam,
                                fixture.port,
                                QStringLiteral("/api/v1/admin/workspaces"),
                                QByteArrayLiteral("tok-admin"));
    QSignalSpy listFinished(listReply.data(), &QNetworkReply::finished);
    QVERIFY(listFinished.wait(5000));
    QCOMPARE(listReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonArray workspaces =
        readObject(listReply.data())[QStringLiteral("workspaces")].toArray();
    QVERIFY(std::any_of(workspaces.begin(), workspaces.end(), [](const QJsonValue& value) {
        return value.toObject()[QStringLiteral("workspaceId")].toString()
            == QStringLiteral("ws-admin");
    }));

    auto auditReply = getRequest(nam,
                                 fixture.port,
                                 QStringLiteral("/api/v1/admin/audit?workspaceId=ws-admin&limit=10"),
                                 QByteArrayLiteral("tok-admin"));
    QSignalSpy auditFinished(auditReply.data(), &QNetworkReply::finished);
    QVERIFY(auditFinished.wait(5000));
    QCOMPARE(auditReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    const QJsonArray entries =
        readObject(auditReply.data())[QStringLiteral("audit")].toArray();
    QVERIFY(!entries.isEmpty());
    const QJsonObject first = entries.at(0).toObject();
    QCOMPARE(first[QStringLiteral("action")].toString(),
             QStringLiteral("workspace.upsert"));
    QVERIFY(!first[QStringLiteral("metadata")].toObject()
                 .contains(QStringLiteral("body")));
}

void TestMessageServiceHttpServer::scopedToken_deniesOperationOutsideScope()
{
    ServerFixture fixture;
    QVERIFY(fixture.start());
    QVERIFY(fixture.auth->seedOrUpdateTokenSecurity(
        QStringLiteral("tok-read-only"),
        QStringLiteral("client-2"),
        QStringLiteral("Client 2 Read Only"),
        jsonScope({"ws-1"}),
        QStringLiteral("member"),
        jsonScope({"message:read"})));

    QNetworkAccessManager nam;
    auto writeReply = postJson(nam,
                               fixture.port,
                               QStringLiteral("/api/v1/messages"),
                               messageBody(QStringLiteral("local-readonly-denied")),
                               QByteArrayLiteral("tok-read-only"));
    QSignalSpy writeFinished(writeReply.data(), &QNetworkReply::finished);
    QVERIFY(writeFinished.wait(5000));
    QCOMPARE(writeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 403);

    auto seedReply = postJson(nam,
                              fixture.port,
                              QStringLiteral("/api/v1/messages"),
                              messageBody(QStringLiteral("local-readonly-sync")),
                              QByteArrayLiteral("tok-1"));
    QSignalSpy seedFinished(seedReply.data(), &QNetworkReply::finished);
    QVERIFY(seedFinished.wait(5000));
    QCOMPARE(seedReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);

    auto readReply = getRequest(nam,
                                fixture.port,
                                QStringLiteral("/api/v1/conversations/conv-1/messages"
                                               "?workspaceId=ws-1&afterSeq=0&limit=10"),
                                QByteArrayLiteral("tok-read-only"));
    QSignalSpy readFinished(readReply.data(), &QNetworkReply::finished);
    QVERIFY(readFinished.wait(5000));
    QCOMPARE(readReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
    QVERIFY(!readObject(readReply.data())[QStringLiteral("messages")].toArray().isEmpty());
}

QTEST_MAIN(TestMessageServiceHttpServer)
#include "TestMessageServiceHttpServer.moc"
