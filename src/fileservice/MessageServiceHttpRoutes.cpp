#include "MessageServiceHttpRoutes.h"

#include "FileServiceAuth.h"
#include "MessageEventBus.h"
#include "MessageServiceDatabase.h"
#include "MessageServiceHttpContracts.h"
#include "MessageServiceOperations.h"
#include "MessageSessionRegistry.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHttpServer>
#include <QHttpServerRequest>
#include <QHttpServerResponse>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSet>
#include <QUrlQuery>

#include <algorithm>
#include <optional>
#include <utility>

Q_LOGGING_CATEGORY(lcMessageServiceAuth, "leyochat.service.auth")
Q_LOGGING_CATEGORY(lcMessageServiceEventStream, "leyochat.service.event_stream")
Q_LOGGING_CATEGORY(lcMessageServiceMessageStore, "leyochat.service.message_store")
Q_LOGGING_CATEGORY(lcMessageServiceMetrics, "leyochat.service.metrics")
Q_LOGGING_CATEGORY(lcMessageServiceRateLimit, "leyochat.service.rate_limit")
Q_LOGGING_CATEGORY(lcMessageServiceSessions, "leyochat.service.sessions")

namespace {
QHttpServerResponse jsonResponse(
    const QJsonObject& object,
    QHttpServerResponse::StatusCode status = QHttpServerResponse::StatusCode::Ok)
{
    return QHttpServerResponse(
        QJsonDocument(object).toJson(QJsonDocument::Compact), status);
}

QHttpServerResponse errorResponse(const QString& message,
                                  QHttpServerResponse::StatusCode status)
{
    return jsonResponse(QJsonObject{
                            {QStringLiteral("ok"), false},
                            {QStringLiteral("error"), message}
                        },
                        status);
}

QHttpServerResponse rateLimitedResponse(qint64 retryAfterMs)
{
    QHttpServerResponse response =
        jsonResponse(QJsonObject{
                         {QStringLiteral("ok"), false},
                         {QStringLiteral("error"), QStringLiteral("rate limit exceeded")},
                         {QStringLiteral("retryAfterMs"), retryAfterMs}
                     },
                     QHttpServerResponse::StatusCode::TooManyRequests);
    response.addHeader(QByteArrayLiteral("Retry-After"),
                       QByteArray::number((retryAfterMs + 999) / 1000));
    return response;
}

QString utcNowString()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}

QJsonObject defaultHealthSnapshot(MessageServiceDatabase* messages)
{
    const bool ready = messages != nullptr;
    const QString version = QCoreApplication::applicationVersion().isEmpty()
        ? QStringLiteral("unknown")
        : QCoreApplication::applicationVersion();
    return QJsonObject{
        {QStringLiteral("service"), QStringLiteral("LeyoChatService")},
        {QStringLiteral("ready"), ready},
        {QStringLiteral("status"), ready ? QStringLiteral("ready") : QStringLiteral("unready")},
        {QStringLiteral("version"), version},
        {QStringLiteral("serviceTimeUtc"), utcNowString()},
        {QStringLiteral("processStartedAtUtc"), QString()},
        {QStringLiteral("database"), QJsonObject{
            {QStringLiteral("open"), ready},
            {QStringLiteral("migrationComplete"), ready}
        }}
    };
}

QHttpServerResponse healthResponse(const QJsonObject& snapshot)
{
    const bool ready = snapshot.value(QStringLiteral("ready")).toBool(false);
    return jsonResponse(
        snapshot,
        ready ? QHttpServerResponse::StatusCode::Ok
              : QHttpServerResponse::StatusCode::ServiceUnavailable);
}

std::optional<qint64> rateLimitRetryAfterMs(
    MessageServiceOperations* operations,
    const QString& clientId,
    MessageServiceOperation operation)
{
    if (!operations)
        return std::nullopt;

    const MessageServiceRateLimitDecision decision =
        operations->accept(clientId, operation);
    if (decision.allowed)
        return std::nullopt;
    qCWarning(lcMessageServiceRateLimit)
        << "rate limit rejected"
        << "clientId=" << clientId
        << "operation=" << MessageServiceOperations::operationKey(operation)
        << "retryAfterMs=" << decision.retryAfterMs;
    return decision.retryAfterMs;
}

std::optional<AuthenticatedClient> authenticate(FileServiceAuth* auth,
                                                const QHttpServerRequest& request)
{
    if (!auth)
        return std::nullopt;
    const auto tokenClient =
        auth->validate(QString::fromLatin1(request.value("Authorization")));
    if (!tokenClient) {
        qCWarning(lcMessageServiceAuth)
            << "auth failure"
            << "path=" << request.url().path();
        return std::nullopt;
    }
    const QString requestedClientId =
        QString::fromUtf8(request.value("X-Client-Id")).trimmed();
    const auto resolved =
        auth->resolveRequestClient(*tokenClient, requestedClientId);
    if (!resolved) {
        qCWarning(lcMessageServiceAuth)
            << "client identity mismatch"
            << "tokenClientId=" << tokenClient->clientId
            << "requestedClientId=" << requestedClientId
            << "path=" << request.url().path();
    }
    return resolved;
}

bool hasScope(FileServiceAuth* auth,
              const AuthenticatedClient& client,
              const QString& scope)
{
    return auth && auth->canUseScope(client, scope);
}

bool hasAdminScope(FileServiceAuth* auth,
                   const AuthenticatedClient& client,
                   const QString& scope)
{
    return auth
        && auth->hasRole(client, QStringLiteral("admin"))
        && auth->canUseScope(client, scope);
}

std::optional<QJsonObject> requestJsonObject(const QHttpServerRequest& request,
                                             QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(request.body(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("request body must be a JSON object");
        return std::nullopt;
    }
    return document.object();
}

int boundedLimit(const QString& value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok)
        return 100;
    return std::max(1, std::min(parsed, 500));
}

qint64 afterSeqValue(const QString& value)
{
    bool ok = false;
    const qint64 parsed = value.toLongLong(&ok);
    return ok && parsed > 0 ? parsed : 0;
}

qint64 afterEventIdValue(const QString& value)
{
    bool ok = false;
    const qint64 parsed = value.toLongLong(&ok);
    return ok && parsed > 0 ? parsed : 0;
}

QJsonObject sessionToJson(const MessageSessionSnapshot& session)
{
    QJsonArray capabilities;
    for (const QString& capability : session.capabilities) {
        capabilities.append(capability);
    }

    return QJsonObject{
        {QStringLiteral("ok"), true},
        {QStringLiteral("sessionId"), session.sessionId},
        {QStringLiteral("clientId"), session.clientId},
        {QStringLiteral("deviceId"), session.deviceId},
        {QStringLiteral("workspaceId"), session.workspaceId},
        {QStringLiteral("connectedAtMs"), session.connectedAtMs},
        {QStringLiteral("lastSeenAtMs"), session.lastSeenAtMs},
        {QStringLiteral("lastEventId"), session.lastEventId},
        {QStringLiteral("appVersion"), session.appVersion},
        {QStringLiteral("capabilities"), capabilities}
    };
}

QStringList stringArrayFromJson(const QJsonValue& value, int maxItems = 200)
{
    if (!value.isArray()) {
        return {};
    }

    QStringList result;
    QSet<QString> seen;
    const QJsonArray array = value.toArray();
    for (const QJsonValue& item : array) {
        if (result.size() >= maxItems) {
            break;
        }
        const QString text = item.toString().trimmed();
        if (text.isEmpty() || seen.contains(text)) {
            continue;
        }
        seen.insert(text);
        result.push_back(text);
    }
    return result;
}

QJsonObject capabilityProfileToJson(
    const MessageClientCapabilityProfile& profile,
    const QString& requiredCapability)
{
    QJsonArray capabilities;
    for (const QString& capability : profile.capabilities) {
        capabilities.append(capability);
    }

    return QJsonObject{
        {QStringLiteral("workspaceId"), profile.workspaceId},
        {QStringLiteral("clientId"), profile.clientId},
        {QStringLiteral("appVersion"), profile.appVersion},
        {QStringLiteral("capabilities"), capabilities},
        {QStringLiteral("updatedAtMs"), profile.updatedAtMs},
        {QStringLiteral("supportsRequiredCapability"),
         !requiredCapability.trimmed().isEmpty()
             && profile.supports(requiredCapability)}
    };
}

QJsonObject workspaceToJson(const MessageWorkspaceRecord& workspace)
{
    return QJsonObject{
        {QStringLiteral("workspaceId"), workspace.workspaceId},
        {QStringLiteral("displayName"), workspace.displayName},
        {QStringLiteral("createdById"), workspace.createdById},
        {QStringLiteral("enabled"), workspace.enabled},
        {QStringLiteral("createdAtMs"), workspace.createdAtMs},
        {QStringLiteral("updatedAtMs"), workspace.updatedAtMs}
    };
}

QJsonObject auditToJson(const MessageAuditRecord& audit)
{
    QJsonObject metadata;
    const QJsonDocument metadataDoc =
        QJsonDocument::fromJson(audit.metadataJson.toUtf8());
    if (metadataDoc.isObject()) {
        metadata = metadataDoc.object();
    }
    return QJsonObject{
        {QStringLiteral("auditId"), audit.auditId},
        {QStringLiteral("workspaceId"), audit.workspaceId},
        {QStringLiteral("actorClientId"), audit.actorClientId},
        {QStringLiteral("action"), audit.action},
        {QStringLiteral("outcome"), audit.outcome},
        {QStringLiteral("metadata"), metadata},
        {QStringLiteral("createdAtMs"), audit.createdAtMs}
    };
}

QString compactStringArray(const QJsonValue& value, const QString& fallback)
{
    if (value.isArray()) {
        return QString::fromUtf8(
            QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    }
    const QString stringValue = value.toString().trimmed();
    return stringValue.isEmpty() ? fallback : stringValue;
}

bool appendSessionStatusEvent(MessageServiceDatabase* messages,
                              const QString& eventType,
                              const MessageSessionSnapshot& session)
{
    if (!messages)
        return true;
    return messages->appendSessionStatusEvent(session.workspaceId,
                                              eventType,
                                              session.sessionId,
                                              session.clientId,
                                              session.deviceId,
                                              session.connectedAtMs,
                                              session.lastSeenAtMs,
                                              session.lastEventId)
               .eventId > 0;
}

bool appendExpiredSessionEvents(MessageServiceDatabase* messages,
                                MessageSessionRegistry* sessions)
{
    if (!messages || !sessions)
        return true;

    const QVector<MessageSessionSnapshot> expired =
        sessions->takeExpiredSessions();
    for (const MessageSessionSnapshot& session : expired) {
        if (!appendSessionStatusEvent(messages,
                                      QStringLiteral("session.offline"),
                                      session)) {
            return false;
        }
    }
    return true;
}

qint64 maxEventId(const QVector<MessageServiceEvent>& events,
                  qint64 fallbackEventId)
{
    qint64 result = std::max<qint64>(0, fallbackEventId);
    for (const MessageServiceEvent& event : events) {
        result = std::max(result, event.eventId);
    }
    return result;
}

QJsonObject payloadObjectForEvent(const StoredMessageEvent& event)
{
    const QJsonDocument document =
        QJsonDocument::fromJson(event.payloadJson.toUtf8());
    if (document.isObject()) {
        return document.object();
    }
    return QJsonObject{
        {QStringLiteral("eventId"), event.eventId},
        {QStringLiteral("type"), event.eventType},
        {QStringLiteral("createdAtMs"), event.createdAtMs},
        {QStringLiteral("conversationId"), event.conversationId},
        {QStringLiteral("workspaceId"), event.workspaceId}
    };
}

QVector<MessageServiceEvent> toServiceEvents(
    const QVector<StoredMessageEvent>& storedEvents)
{
    QVector<MessageServiceEvent> events;
    events.reserve(storedEvents.size());
    for (const StoredMessageEvent& stored : storedEvents) {
        MessageServiceEvent event;
        event.eventId = stored.eventId;
        event.createdAtMs = stored.createdAtMs;
        event.type = stored.eventType;
        event.workspaceId = stored.workspaceId;
        event.conversationId = stored.conversationId;
        event.data = payloadObjectForEvent(stored);
        events.push_back(event);
    }
    return events;
}
}

namespace MessageServiceHttpRoutes {

bool isAcceptedMessageRequestBodySize(qsizetype bodySize)
{
    constexpr qsizetype kMaxMessageRequestBodyBytes = 16 * 1024 * 1024;
    return bodySize >= 0 && bodySize <= kMaxMessageRequestBodyBytes;
}

void registerRoutes(QHttpServer& server,
                    FileServiceAuth* auth,
                    MessageServiceDatabase* messages,
                    MessageEventBus* events,
                    MessageServiceOperations* operations,
                    MessageSessionRegistry* sessions,
                    std::function<QJsonObject()> healthProvider)
{
    server.route(QStringLiteral("/api/v1/health"), QHttpServerRequest::Method::Get,
        [messages, healthProvider = std::move(healthProvider)](
            const QHttpServerRequest&) -> QHttpServerResponse {
            if (healthProvider)
                return healthResponse(healthProvider());
            return healthResponse(defaultHealthSnapshot(messages));
        });

    server.route(QStringLiteral("/api/v1/capabilities"), QHttpServerRequest::Method::Get,
        [messages, operations, sessions](const QHttpServerRequest&) -> QHttpServerResponse {
            const bool messageServiceEnabled = messages != nullptr;
            return jsonResponse(QJsonObject{
                {QStringLiteral("file_service"), true},
                {QStringLiteral("message_service"), messageServiceEnabled},
                {QStringLiteral("reliable_message"), messageServiceEnabled},
                {QStringLiteral("message_events_stream"), messageServiceEnabled},
                {QStringLiteral("message_sessions"), sessions != nullptr},
                {QStringLiteral("message_metrics"), messageServiceEnabled},
                {QStringLiteral("message_rate_limit"), operations != nullptr},
                {QStringLiteral("p2p_fallback_supported"), true},
                {QStringLiteral("transport_modes"), QJsonArray{
                    QStringLiteral("P2POnly"),
                    QStringLiteral("ServerPreferred"),
                    QStringLiteral("ServerOnly")
                }}
            });
        });

    server.route(QStringLiteral("/api/v1/admin/workspaces"),
        QHttpServerRequest::Method::Post,
        [auth, messages](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasAdminScope(auth, *client, QStringLiteral("admin:write")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            QString error;
            const auto body = requestJsonObject(request, &error);
            if (!body)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            MessageWorkspaceRecord workspace;
            workspace.workspaceId =
                body->value(QStringLiteral("workspaceId")).toString().trimmed();
            workspace.displayName =
                body->value(QStringLiteral("displayName")).toString().trimmed();
            workspace.createdById = client->clientId;
            workspace.enabled = body->value(QStringLiteral("enabled")).toBool(true);
            if (workspace.workspaceId.isEmpty())
                return errorResponse(QStringLiteral("workspaceId is required"),
                                     QHttpServerResponse::StatusCode::BadRequest);

            if (!messages->upsertWorkspace(workspace))
                return errorResponse(QStringLiteral("failed to save workspace"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            messages->appendAuditEvent(
                workspace.workspaceId,
                client->clientId,
                QStringLiteral("workspace.upsert"),
                QStringLiteral("accepted"),
                QJsonObject{
                    {QStringLiteral("workspaceId"), workspace.workspaceId},
                    {QStringLiteral("enabled"), workspace.enabled}
                });
            return jsonResponse(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("workspace"), workspaceToJson(workspace)}
            });
        });

    server.route(QStringLiteral("/api/v1/admin/workspaces"),
        QHttpServerRequest::Method::Get,
        [auth, messages](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasAdminScope(auth, *client, QStringLiteral("admin:read")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            QJsonArray workspaces;
            for (const MessageWorkspaceRecord& workspace : messages->listWorkspaces()) {
                workspaces.append(workspaceToJson(workspace));
            }
            return jsonResponse(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("workspaces"), workspaces}
            });
        });

    server.route(QStringLiteral("/api/v1/admin/tokens"),
        QHttpServerRequest::Method::Post,
        [auth, messages](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasAdminScope(auth, *client, QStringLiteral("token:write")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            QString error;
            const auto body = requestJsonObject(request, &error);
            if (!body)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            const QString token = body->value(QStringLiteral("token")).toString().trimmed();
            const QString clientId =
                body->value(QStringLiteral("clientId")).toString().trimmed();
            const QString displayName =
                body->value(QStringLiteral("displayName")).toString().trimmed();
            const QString role =
                body->value(QStringLiteral("role")).toString(QStringLiteral("member")).trimmed();
            const QString allowedWorkspaces =
                compactStringArray(body->value(QStringLiteral("allowedWorkspaces")),
                                   QStringLiteral("*"));
            const QString scopes =
                compactStringArray(body->value(QStringLiteral("scopes")),
                                   QStringLiteral("*"));
            if (token.isEmpty() || clientId.isEmpty())
                return errorResponse(QStringLiteral("token and clientId are required"),
                                     QHttpServerResponse::StatusCode::BadRequest);

            if (!auth->seedOrUpdateTokenSecurity(token,
                                                 clientId,
                                                 displayName,
                                                 allowedWorkspaces,
                                                 role,
                                                 scopes)) {
                return errorResponse(QStringLiteral("failed to save token"),
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }

            messages->appendAuditEvent(
                QString(),
                client->clientId,
                QStringLiteral("token.upsert"),
                QStringLiteral("accepted"),
                QJsonObject{
                    {QStringLiteral("clientId"), clientId},
                    {QStringLiteral("role"), role}
                });
            return jsonResponse(QJsonObject{{QStringLiteral("ok"), true}});
        });

    server.route(QStringLiteral("/api/v1/admin/audit"),
        QHttpServerRequest::Method::Get,
        [auth, messages](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasAdminScope(auth, *client, QStringLiteral("audit:read")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QUrlQuery query(request.url());
            QJsonArray audits;
            for (const MessageAuditRecord& audit :
                 messages->listAuditEvents(
                     query.queryItemValue(QStringLiteral("workspaceId")),
                     boundedLimit(query.queryItemValue(QStringLiteral("limit"))))) {
                audits.append(auditToJson(audit));
            }
            return jsonResponse(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("audit"), audits}
            });
        });

    server.route(QStringLiteral("/api/v1/messages"), QHttpServerRequest::Method::Post,
        [auth, messages, events, operations, sessions](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("message:write")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (!isAcceptedMessageRequestBodySize(request.body().size())) {
                return errorResponse(
                    QStringLiteral("message request body exceeds 16 MB"),
                    static_cast<QHttpServerResponse::StatusCode>(413));
            }

            QString error;
            const auto body = requestJsonObject(request, &error);
            if (!body)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            const auto storeRequest =
                MessageServiceHttpContracts::parseStoreMessageRequest(
                    *body, client->clientId, &error);
            if (!storeRequest)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            if (!auth->canAccessWorkspace(*client, storeRequest->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (const auto retryAfterMs = rateLimitRetryAfterMs(
                    operations,
                    client->clientId,
                    MessageServiceOperation::PostMessage)) {
                return rateLimitedResponse(*retryAfterMs);
            }

            const StoreMessageResult result = messages->storeMessage(*storeRequest);
            if (!result.ok) {
                qCWarning(lcMessageServiceMessageStore)
                    << "message store failure"
                    << "conversationId=" << storeRequest->conversationId
                    << "workspaceId=" << storeRequest->workspaceId
                    << "error=" << result.error;
                return errorResponse(result.error,
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }
            if (events && !result.duplicate) {
                events->publishMessageCreated(result.message);
            }
            qCInfo(lcMessageServiceMessageStore)
                << "[message-store]"
                << "serverMessageId=" << result.message.serverMessageId
                << "clientMessageId=" << result.message.clientMessageId
                << "senderClientId=" << client->clientId
                << "conversationId=" << result.message.conversationId
                << "workspaceId=" << result.message.workspaceId
                << "type=" << result.message.type
                << "recipientCount=" << storeRequest->recipientIds.size()
                << "serverSeq=" << result.message.serverSeq
                << "duplicate=" << result.duplicate;

            return jsonResponse(
                MessageServiceHttpContracts::storeMessageResultToJson(result));
        });

    server.route(QStringLiteral("/api/v1/metrics"), QHttpServerRequest::Method::Get,
        [auth, messages, operations, events, sessions](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("metrics:read")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (!appendExpiredSessionEvents(messages, sessions)) {
                return errorResponse(QStringLiteral("failed to append session offline event"),
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }

            QJsonObject metrics =
                operations
                    ? operations->metricsJson()
                    : QJsonObject{{QStringLiteral("ok"), true}};
            if (sessions) {
                metrics[QStringLiteral("sessions")] = sessions->metricsJson();
            }
            if (events) {
                metrics[QStringLiteral("events")] = events->metricsJson();
            }
            qCInfo(lcMessageServiceMetrics)
                << "[metrics]"
                << "snapshot requested"
                << "clientId=" << client->clientId;
            return jsonResponse(metrics);
        });

    server.route(QStringLiteral("/api/v1/sessions/heartbeat"),
        QHttpServerRequest::Method::Post,
        [auth, messages, operations, sessions](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("session:write")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            QString error;
            const auto body = requestJsonObject(request, &error);
            if (!body)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            const QString workspaceId =
                body->value(QStringLiteral("workspaceId")).toString().trimmed();
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            if (!auth->canAccessWorkspace(*client, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (const auto retryAfterMs = rateLimitRetryAfterMs(
                    operations,
                    client->clientId,
                    MessageServiceOperation::SessionHeartbeat)) {
                return rateLimitedResponse(*retryAfterMs);
            }

            if (!sessions)
                return errorResponse(QStringLiteral("message session service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            if (!appendExpiredSessionEvents(messages, sessions)) {
                return errorResponse(QStringLiteral("failed to append session offline event"),
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }

            const QString deviceId =
                body->value(QStringLiteral("deviceId")).toString().trimmed();
            const qint64 lastEventId =
                body->value(QStringLiteral("lastEventId")).toInteger(0);
            const QString appVersion =
                body->value(QStringLiteral("appVersion")).toString().trimmed();
            const QStringList capabilities =
                stringArrayFromJson(body->value(QStringLiteral("capabilities")));
            const MessageSessionTouchResult session =
                sessions->touchWithStatus(client->clientId,
                                          deviceId,
                                          workspaceId,
                                          lastEventId,
                                          appVersion,
                                          capabilities);
            if (messages
                && !messages->upsertClientCapabilities(workspaceId,
                                                       client->clientId,
                                                       appVersion,
                                                       capabilities,
                                                       session.session.lastSeenAtMs)) {
                return errorResponse(QStringLiteral("failed to save client capabilities"),
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }
            if (session.created
                && !appendSessionStatusEvent(messages,
                                             QStringLiteral("session.online"),
                                             session.session)) {
                return errorResponse(QStringLiteral("failed to append session online event"),
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }
            qCInfo(lcMessageServiceSessions)
                << "[session-heartbeat]"
                << "clientId=" << client->clientId
                << "deviceId=" << deviceId
                << "workspaceId=" << workspaceId
                << "appVersion=" << appVersion
                << "capabilities=" << capabilities.join(QLatin1Char(','))
                << "lastEventId=" << lastEventId
                << "created=" << session.created;
            return jsonResponse(sessionToJson(session.session));
        });

    server.route(QStringLiteral("/api/v1/sessions/online"),
        QHttpServerRequest::Method::Get,
        [auth, messages, sessions](const QHttpServerRequest& request) -> QHttpServerResponse {
            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("events:read")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QUrlQuery query(request.url());
            const QString workspaceId =
                query.queryItemValue(QStringLiteral("workspaceId")).trimmed();
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            if (!auth->canAccessWorkspace(*client, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (!sessions)
                return errorResponse(QStringLiteral("message session service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            if (!appendExpiredSessionEvents(messages, sessions)) {
                return errorResponse(QStringLiteral("failed to append session offline event"),
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }

            QJsonArray sessionArray;
            const QVector<MessageSessionSnapshot> online =
                sessions->onlineSessions(workspaceId);
            for (const MessageSessionSnapshot& session : online) {
                sessionArray.append(sessionToJson(session));
            }

            qCInfo(lcMessageServiceSessions)
                << "[sessions-online]"
                << "clientId=" << client->clientId
                << "workspaceId=" << workspaceId
                << "sessions=" << sessionArray.size();

            return jsonResponse(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("workspaceId"), workspaceId},
                {QStringLiteral("sessions"), sessionArray}
            });
        });

    server.route(QStringLiteral("/api/v1/clients/capabilities/query"),
        QHttpServerRequest::Method::Post,
        [auth, messages, operations](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("message:write")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            QString error;
            const auto body = requestJsonObject(request, &error);
            if (!body)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            const QString workspaceId =
                body->value(QStringLiteral("workspaceId")).toString().trimmed();
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);
            if (!auth->canAccessWorkspace(*client, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            Q_UNUSED(operations);

            const QString requiredCapability =
                body->value(QStringLiteral("requiredCapability")).toString().trimmed();
            const QStringList clientIds =
                stringArrayFromJson(body->value(QStringLiteral("clientIds")), 200);
            if (clientIds.isEmpty()) {
                return errorResponse(QStringLiteral("clientIds is required"),
                                     QHttpServerResponse::StatusCode::BadRequest);
            }

            QJsonArray profiles;
            for (const MessageClientCapabilityProfile& profile :
                 messages->loadClientCapabilities(workspaceId, clientIds)) {
                profiles.append(
                    capabilityProfileToJson(profile, requiredCapability));
            }
            qCInfo(lcMessageServiceSessions)
                << "[capability-query]"
                << "clientId=" << client->clientId
                << "workspaceId=" << workspaceId
                << "requestedClientCount=" << clientIds.size()
                << "profileCount=" << profiles.size()
                << "requiredCapability=" << requiredCapability;

            return jsonResponse(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("workspaceId"), workspaceId},
                {QStringLiteral("requiredCapability"), requiredCapability},
                {QStringLiteral("profiles"), profiles}
            });
        });

    server.route(QStringLiteral("/api/v1/events/stream"),
        QHttpServerRequest::Method::Get,
        [auth, messages, events, operations, sessions](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("events:read")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QUrlQuery query(request.url());
            const QString workspaceId =
                query.queryItemValue(QStringLiteral("workspaceId")).trimmed();
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            if (!auth->canAccessWorkspace(*client, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (const auto retryAfterMs = rateLimitRetryAfterMs(
                    operations,
                    client->clientId,
                    MessageServiceOperation::EventsStream)) {
                return rateLimitedResponse(*retryAfterMs);
            }

            if (!appendExpiredSessionEvents(messages, sessions)) {
                return errorResponse(QStringLiteral("failed to append session offline event"),
                                     QHttpServerResponse::StatusCode::InternalServerError);
            }

            const qint64 afterEventId =
                afterEventIdValue(query.queryItemValue(QStringLiteral("afterEventId")));
            const int limit = boundedLimit(
                query.queryItemValue(QStringLiteral("limit")));
            const QVector<MessageServiceEvent> streamEvents =
                toServiceEvents(messages->listMessageEventsAfterForClient(
                    workspaceId, client->clientId, afterEventId, limit));
            if (events) {
                events->recordConsumed(streamEvents.size());
            }
            if (sessions) {
                const MessageSessionTouchResult session =
                    sessions->touchWithStatus(
                        client->clientId,
                        query.queryItemValue(QStringLiteral("deviceId")),
                        workspaceId,
                        maxEventId(streamEvents, afterEventId));
                if (session.created
                    && !appendSessionStatusEvent(messages,
                                                 QStringLiteral("session.online"),
                                                 session.session)) {
                    return errorResponse(QStringLiteral("failed to append session online event"),
                                         QHttpServerResponse::StatusCode::InternalServerError);
                }
            }
            qCInfo(lcMessageServiceEventStream)
                << "[event-stream]"
                << "snapshot"
                << "clientId=" << client->clientId
                << "workspaceId=" << workspaceId
                << "afterEventId=" << afterEventId
                << "limit=" << limit
                << "events=" << streamEvents.size();
            const QByteArray body = MessageEventBus::encodeSse(streamEvents);

            QHttpServerResponse response(QByteArrayLiteral("text/event-stream"),
                                         body);
            response.addHeader(QByteArrayLiteral("Cache-Control"),
                               QByteArrayLiteral("no-cache"));
            response.addHeader(QByteArrayLiteral("X-Accel-Buffering"),
                               QByteArrayLiteral("no"));
            return response;
        });

    server.route(QStringLiteral("/api/v1/conversations"),
        QHttpServerRequest::Method::Get,
        [auth, messages, operations](const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("message:read")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QUrlQuery query(request.url());
            const QString workspaceId =
                query.queryItemValue(QStringLiteral("workspaceId")).trimmed();
            if (workspaceId.isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            if (!auth->canAccessWorkspace(*client, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (const auto retryAfterMs = rateLimitRetryAfterMs(
                    operations,
                    client->clientId,
                    MessageServiceOperation::SyncMessages)) {
                return rateLimitedResponse(*retryAfterMs);
            }

            const int limit = boundedLimit(
                query.queryItemValue(QStringLiteral("limit")));
            const QVector<StoredConversation> conversations =
                messages->listConversationsForMember(workspaceId,
                                                     client->clientId,
                                                     limit);
            qCInfo(lcMessageServiceMessageStore)
                << "[conversation-sync]"
                << "clientId=" << client->clientId
                << "workspaceId=" << workspaceId
                << "limit=" << limit
                << "conversations=" << conversations.size();
            return jsonResponse(
                MessageServiceHttpContracts::conversationListToJson(
                    conversations));
        });

    server.route(QStringLiteral("/api/v1/conversations/<arg>/messages"),
        QHttpServerRequest::Method::Get,
        [auth, messages, operations](const QString& conversationId,
                                     const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("message:read")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const QUrlQuery query(request.url());
            const QString workspaceId =
                query.queryItemValue(QStringLiteral("workspaceId"));
            if (workspaceId.trimmed().isEmpty())
                return QHttpServerResponse(QHttpServerResponse::StatusCode::BadRequest);

            if (!auth->canAccessWorkspace(*client, workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (const auto retryAfterMs = rateLimitRetryAfterMs(
                    operations,
                    client->clientId,
                    MessageServiceOperation::SyncMessages)) {
                return rateLimitedResponse(*retryAfterMs);
            }

            if (!messages->isConversationMember(conversationId, client->clientId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const qint64 afterSeq =
                afterSeqValue(query.queryItemValue(QStringLiteral("afterSeq")));
            const int limit = boundedLimit(
                query.queryItemValue(QStringLiteral("limit")));
            const QVector<StoredMessage> page =
                messages->listMessagesAfterSeq(conversationId, afterSeq, limit);
            qCInfo(lcMessageServiceMessageStore)
                << "[message-sync]"
                << "clientId=" << client->clientId
                << "conversationId=" << conversationId
                << "workspaceId=" << workspaceId
                << "afterSeq=" << afterSeq
                << "limit=" << limit
                << "messages=" << page.size();
            return jsonResponse(
                MessageServiceHttpContracts::messageListToJson(page));
        });

    server.route(QStringLiteral("/api/v1/messages/<arg>/delivery-ack"),
        QHttpServerRequest::Method::Post,
        [auth, messages, operations](const QString& serverMessageId,
                                     const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("message:ack")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto message = messages->findMessageByServerId(serverMessageId);
            if (!message)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            if (!auth->canAccessWorkspace(*client, message->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (const auto retryAfterMs = rateLimitRetryAfterMs(
                    operations,
                    client->clientId,
                    MessageServiceOperation::DeliveryAck)) {
                return rateLimitedResponse(*retryAfterMs);
            }

            QString error;
            const auto body = requestJsonObject(request, &error);
            if (!body)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            const auto receivedSeq = MessageServiceHttpContracts::parseAckSeq(
                *body, QStringLiteral("receivedSeq"), &error);
            if (!receivedSeq)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            if (!messages->markDelivered(serverMessageId, client->clientId, *receivedSeq))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
            qCInfo(lcMessageServiceMessageStore)
                << "[delivery-ack]"
                << "serverMessageId=" << serverMessageId
                << "clientId=" << client->clientId
                << "conversationId=" << message->conversationId
                << "workspaceId=" << message->workspaceId
                << "receivedSeq=" << *receivedSeq
                << "serverSeq=" << message->serverSeq;

            return jsonResponse(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("serverMessageId"), serverMessageId},
                {QStringLiteral("state"), QStringLiteral("delivered")},
                {QStringLiteral("serverSeq"), message->serverSeq}
            });
        });

    server.route(QStringLiteral("/api/v1/messages/<arg>/read-ack"),
        QHttpServerRequest::Method::Post,
        [auth, messages, operations](const QString& serverMessageId,
                                     const QHttpServerRequest& request) -> QHttpServerResponse {
            if (!messages)
                return errorResponse(QStringLiteral("message service is disabled"),
                                     QHttpServerResponse::StatusCode::InternalServerError);

            const auto client = authenticate(auth, request);
            if (!client)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Unauthorized);
            if (!hasScope(auth, *client, QStringLiteral("message:ack")))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            const auto message = messages->findMessageByServerId(serverMessageId);
            if (!message)
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);

            if (!auth->canAccessWorkspace(*client, message->workspaceId))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::Forbidden);

            if (const auto retryAfterMs = rateLimitRetryAfterMs(
                    operations,
                    client->clientId,
                    MessageServiceOperation::ReadAck)) {
                return rateLimitedResponse(*retryAfterMs);
            }

            QString error;
            const auto body = requestJsonObject(request, &error);
            if (!body)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            const auto readSeq = MessageServiceHttpContracts::parseAckSeq(
                *body, QStringLiteral("readSeq"), &error);
            if (!readSeq)
                return errorResponse(error, QHttpServerResponse::StatusCode::BadRequest);

            if (!messages->markRead(serverMessageId, client->clientId, *readSeq))
                return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
            qCInfo(lcMessageServiceMessageStore)
                << "[read-ack]"
                << "serverMessageId=" << serverMessageId
                << "clientId=" << client->clientId
                << "conversationId=" << message->conversationId
                << "workspaceId=" << message->workspaceId
                << "readSeq=" << *readSeq
                << "serverSeq=" << message->serverSeq;

            return jsonResponse(QJsonObject{
                {QStringLiteral("ok"), true},
                {QStringLiteral("serverMessageId"), serverMessageId},
                {QStringLiteral("state"), QStringLiteral("read")},
                {QStringLiteral("serverSeq"), message->serverSeq}
            });
        });
}

}
