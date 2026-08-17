#include "integrations/ServerMessageClient.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonValue>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <QUrlQuery>

#include "integrations/SyncNetworkReply.h"

#include <algorithm>

namespace {

void setError(QString* errorMessage, const QString& message)
{
    if (errorMessage) {
        *errorMessage = message;
    }
}

void clearError(QString* errorMessage)
{
    if (errorMessage) {
        errorMessage->clear();
    }
}

QString firstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
    }
    return {};
}

QString normalizedBaseUrl(const QString& baseUrl)
{
    QString base = baseUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    return base;
}

QString pathSegment(const QString& value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(
        value.trimmed(), QByteArray(), QByteArrayLiteral("/?#")));
}

bool isOkResponse(const QJsonObject& object, QString* errorMessage)
{
    if (object.contains(QStringLiteral("ok"))
        && !object.value(QStringLiteral("ok")).toBool()) {
        setError(errorMessage,
                 firstNonEmpty({object.value(QStringLiteral("error")).toString(),
                                QStringLiteral("message service request failed")}));
        return false;
    }
    return true;
}

bool requireOkTrue(const QJsonObject& object, QString* errorMessage)
{
    if (!object.value(QStringLiteral("ok")).toBool(false)) {
        setError(errorMessage,
                 firstNonEmpty({object.value(QStringLiteral("error")).toString(),
                                QStringLiteral("message service request failed")}));
        return false;
    }
    return true;
}

std::optional<QString> requiredString(const QJsonObject& object,
                                      const QString& field,
                                      QString* errorMessage)
{
    const QJsonValue value = object.value(field);
    if (!value.isString()) {
        setError(errorMessage, QStringLiteral("%1 is required").arg(field));
        return std::nullopt;
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        setError(errorMessage, QStringLiteral("%1 is required").arg(field));
        return std::nullopt;
    }
    return text;
}

std::optional<qint64> requiredInteger(const QJsonObject& object,
                                      const QString& field,
                                      QString* errorMessage)
{
    const QJsonValue value = object.value(field);
    if (!value.isDouble()) {
        setError(errorMessage, QStringLiteral("%1 is required").arg(field));
        return std::nullopt;
    }
    return value.toInteger();
}

std::optional<QJsonObject> requiredObjectDocument(
    const std::optional<QJsonDocument>& document,
    QString* errorMessage)
{
    if (!document.has_value()) {
        if (errorMessage && errorMessage->trimmed().isEmpty()) {
            *errorMessage = QStringLiteral("message service request failed");
        }
        return std::nullopt;
    }

    if (!document->isObject()) {
        setError(errorMessage,
                 QStringLiteral("message service returned invalid JSON object"));
        return std::nullopt;
    }
    return document->object();
}

std::optional<ServerMessageAck> parseAck(
    const std::optional<QJsonDocument>& document,
    QString* errorMessage)
{
    const auto object = requiredObjectDocument(document, errorMessage);
    if (!object || !requireOkTrue(*object, errorMessage)) {
        return std::nullopt;
    }

    const auto serverMessageId =
        requiredString(*object, QStringLiteral("serverMessageId"), errorMessage);
    if (!serverMessageId) {
        return std::nullopt;
    }

    const auto conversationId =
        requiredString(*object, QStringLiteral("conversationId"), errorMessage);
    if (!conversationId) {
        return std::nullopt;
    }

    const auto serverSeq =
        requiredInteger(*object, QStringLiteral("serverSeq"), errorMessage);
    if (!serverSeq) {
        return std::nullopt;
    }

    const auto createdAtMs =
        requiredInteger(*object, QStringLiteral("createdAtMs"), errorMessage);
    if (!createdAtMs) {
        return std::nullopt;
    }

    ServerMessageAck ack;
    ack.duplicate = object->value(QStringLiteral("duplicate")).toBool(false);
    ack.serverMessageId = *serverMessageId;
    ack.conversationId = *conversationId;
    ack.serverSeq = *serverSeq;
    ack.createdAtMs = *createdAtMs;
    return ack;
}

std::optional<ServerMessageRecord> parseRecord(const QJsonObject& object,
                                               QString* errorMessage)
{
    const auto serverMessageId =
        requiredString(object, QStringLiteral("serverMessageId"), errorMessage);
    if (!serverMessageId) {
        return std::nullopt;
    }
    const auto clientMessageId =
        requiredString(object, QStringLiteral("clientMessageId"), errorMessage);
    if (!clientMessageId) {
        return std::nullopt;
    }
    const auto conversationId =
        requiredString(object, QStringLiteral("conversationId"), errorMessage);
    if (!conversationId) {
        return std::nullopt;
    }
    const auto workspaceId =
        requiredString(object, QStringLiteral("workspaceId"), errorMessage);
    if (!workspaceId) {
        return std::nullopt;
    }
    const auto senderId =
        requiredString(object, QStringLiteral("senderId"), errorMessage);
    if (!senderId) {
        return std::nullopt;
    }
    const auto serverSeq =
        requiredInteger(object, QStringLiteral("serverSeq"), errorMessage);
    if (!serverSeq) {
        return std::nullopt;
    }
    const auto type = requiredString(object, QStringLiteral("type"), errorMessage);
    if (!type) {
        return std::nullopt;
    }
    const auto createdAtMs =
        requiredInteger(object, QStringLiteral("createdAtMs"), errorMessage);
    if (!createdAtMs) {
        return std::nullopt;
    }

    ServerMessageRecord record;
    record.serverMessageId = *serverMessageId;
    record.clientMessageId = *clientMessageId;
    record.conversationId = *conversationId;
    record.workspaceId = *workspaceId;
    record.senderId = *senderId;
    record.serverSeq = *serverSeq;
    record.type = *type;
    record.body = object.value(QStringLiteral("body")).toString();
    const QJsonValue payload = object.value(QStringLiteral("payload"));
    if (!payload.isUndefined()) {
        if (!payload.isObject()) {
            setError(errorMessage, QStringLiteral("payload must be an object"));
            return std::nullopt;
        }
        record.payload = payload.toObject();
    }
    record.fileId = object.value(QStringLiteral("fileId")).toString().trimmed();
    record.contentType =
        object.value(QStringLiteral("contentType")).toString().trimmed();
    record.replyToMessageId =
        object.value(QStringLiteral("replyToMessageId")).toString().trimmed();
    record.createdAtMs = *createdAtMs;
    return record;
}

std::optional<ServerMessagePage> parsePage(
    const std::optional<QJsonDocument>& document,
    QString* errorMessage)
{
    const auto object = requiredObjectDocument(document, errorMessage);
    if (!object || !isOkResponse(*object, errorMessage)) {
        return std::nullopt;
    }

    const QJsonValue messagesValue = object->value(QStringLiteral("messages"));
    if (!messagesValue.isArray()) {
        setError(errorMessage, QStringLiteral("messages is required"));
        return std::nullopt;
    }

    const auto nextAfterSeq =
        requiredInteger(*object, QStringLiteral("nextAfterSeq"), errorMessage);
    if (!nextAfterSeq) {
        return std::nullopt;
    }

    ServerMessagePage page;
    page.nextAfterSeq = *nextAfterSeq;
    const QJsonArray messages = messagesValue.toArray();
    for (const QJsonValue& value : messages) {
        if (!value.isObject()) {
            setError(errorMessage, QStringLiteral("messages must contain objects"));
            return std::nullopt;
        }
        auto record = parseRecord(value.toObject(), errorMessage);
        if (!record) {
            return std::nullopt;
        }
        page.messages.push_back(*record);
    }
    return page;
}

std::optional<ServerConversationRecord> parseConversationRecord(
    const QJsonObject& object,
    QString* errorMessage)
{
    const auto conversationId =
        requiredString(object, QStringLiteral("conversationId"), errorMessage);
    if (!conversationId) {
        return std::nullopt;
    }
    const auto latestServerSeq =
        requiredInteger(object, QStringLiteral("latestServerSeq"), errorMessage);
    if (!latestServerSeq) {
        return std::nullopt;
    }
    const auto updatedAtMs =
        requiredInteger(object, QStringLiteral("updatedAtMs"), errorMessage);
    if (!updatedAtMs) {
        return std::nullopt;
    }

    ServerConversationRecord record;
    record.conversationId = *conversationId;
    record.workspaceId =
        object.value(QStringLiteral("workspaceId")).toString().trimmed();
    record.latestServerSeq = *latestServerSeq;
    record.updatedAtMs = *updatedAtMs;
    return record;
}

std::optional<QVector<ServerConversationRecord>> parseConversationList(
    const std::optional<QJsonDocument>& document,
    QString* errorMessage)
{
    const auto object = requiredObjectDocument(document, errorMessage);
    if (!object || !isOkResponse(*object, errorMessage)) {
        return std::nullopt;
    }

    const QJsonValue conversationsValue =
        object->value(QStringLiteral("conversations"));
    if (!conversationsValue.isArray()) {
        setError(errorMessage, QStringLiteral("conversations is required"));
        return std::nullopt;
    }

    QVector<ServerConversationRecord> conversations;
    for (const QJsonValue& value : conversationsValue.toArray()) {
        if (!value.isObject()) {
            setError(errorMessage,
                     QStringLiteral("conversations must contain objects"));
            return std::nullopt;
        }
        auto record = parseConversationRecord(value.toObject(), errorMessage);
        if (!record) {
            return std::nullopt;
        }
        conversations.push_back(*record);
    }
    return conversations;
}

std::optional<ServerMessageEvent> parseSseEvent(qint64 eventId,
                                                const QString& type,
                                                const QByteArray& data,
                                                QString* errorMessage)
{
    if (eventId <= 0 || type.trimmed().isEmpty() || data.trimmed().isEmpty()) {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(errorMessage, QStringLiteral("message event stream returned invalid JSON"));
        return std::nullopt;
    }

    const QJsonObject object = document.object();
    ServerMessageEvent event;
    event.eventId = eventId;
    event.type = type.trimmed();
    event.workspaceId = object.value(QStringLiteral("workspaceId")).toString().trimmed();
    event.conversationId =
        object.value(QStringLiteral("conversationId")).toString().trimmed();
    event.data = object;
    return event;
}

std::optional<ServerMessageEventPage> parseSsePage(const QByteArray& body,
                                                   QString* errorMessage)
{
    ServerMessageEventPage page;
    qint64 currentId = 0;
    QString currentType;
    QByteArray currentData;

    const auto flushEvent = [&]() -> bool {
        if (currentId <= 0 && currentType.trimmed().isEmpty()
            && currentData.trimmed().isEmpty()) {
            return true;
        }

        auto event = parseSseEvent(currentId, currentType, currentData, errorMessage);
        if (!event) {
            return false;
        }
        page.nextAfterEventId = std::max(page.nextAfterEventId, event->eventId);
        page.events.push_back(*event);
        currentId = 0;
        currentType.clear();
        currentData.clear();
        return true;
    };

    const QList<QByteArray> lines = body.split('\n');
    for (QByteArray rawLine : lines) {
        if (rawLine.endsWith('\r')) {
            rawLine.chop(1);
        }
        if (rawLine.isEmpty()) {
            if (!flushEvent()) {
                return std::nullopt;
            }
            continue;
        }
        if (rawLine.startsWith(':')) {
            continue;
        }

        const int colon = rawLine.indexOf(':');
        const QByteArray field = colon >= 0 ? rawLine.left(colon) : rawLine;
        QByteArray value = colon >= 0 ? rawLine.mid(colon + 1) : QByteArray{};
        if (value.startsWith(' ')) {
            value.remove(0, 1);
        }

        if (field == QByteArrayLiteral("id")) {
            bool ok = false;
            const qint64 parsed = value.toLongLong(&ok);
            currentId = ok ? parsed : 0;
        } else if (field == QByteArrayLiteral("event")) {
            currentType = QString::fromUtf8(value).trimmed();
        } else if (field == QByteArrayLiteral("data")) {
            if (!currentData.isEmpty()) {
                currentData.append('\n');
            }
            currentData.append(value);
        }
    }

    if (!flushEvent()) {
        return std::nullopt;
    }
    return page;
}

std::optional<ServerMessageSessionAck> parseSessionAck(
    const std::optional<QJsonDocument>& document,
    QString* errorMessage)
{
    const auto object = requiredObjectDocument(document, errorMessage);
    if (!object || !requireOkTrue(*object, errorMessage)) {
        return std::nullopt;
    }

    const auto sessionId =
        requiredString(*object, QStringLiteral("sessionId"), errorMessage);
    const auto clientId =
        requiredString(*object, QStringLiteral("clientId"), errorMessage);
    const auto deviceId =
        requiredString(*object, QStringLiteral("deviceId"), errorMessage);
    const auto workspaceId =
        requiredString(*object, QStringLiteral("workspaceId"), errorMessage);
    const auto lastEventId =
        requiredInteger(*object, QStringLiteral("lastEventId"), errorMessage);
    if (!sessionId || !clientId || !deviceId || !workspaceId || !lastEventId) {
        return std::nullopt;
    }

    ServerMessageSessionAck ack;
    ack.ok = true;
    ack.sessionId = *sessionId;
    ack.clientId = *clientId;
    ack.deviceId = *deviceId;
    ack.workspaceId = *workspaceId;
    ack.lastEventId = *lastEventId;
    return ack;
}

QStringList stringArray(const QJsonValue& value)
{
    if (!value.isArray()) {
        return {};
    }

    QStringList result;
    QSet<QString> seen;
    const QJsonArray array = value.toArray();
    for (const QJsonValue& item : array) {
        const QString text = item.toString().trimmed();
        const QString key = text.toLower();
        if (text.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        result.push_back(text);
    }
    return result;
}

QJsonArray stringArrayJson(const QStringList& values)
{
    QJsonArray array;
    QSet<QString> seen;
    for (const QString& value : values) {
        const QString text = value.trimmed();
        const QString key = text.toLower();
        if (text.isEmpty() || seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        array.append(text);
    }
    return array;
}

std::optional<ServerMessageSessionSnapshot> parseSessionSnapshot(
    const QJsonObject& object,
    const QString& fallbackWorkspaceId,
    QString* errorMessage)
{
    const auto sessionId =
        requiredString(object, QStringLiteral("sessionId"), errorMessage);
    const auto clientId =
        requiredString(object, QStringLiteral("clientId"), errorMessage);
    const auto deviceId =
        requiredString(object, QStringLiteral("deviceId"), errorMessage);
    const QString workspaceId =
        firstNonEmpty({object.value(QStringLiteral("workspaceId")).toString(),
                       fallbackWorkspaceId});
    if (!sessionId || !clientId || !deviceId || workspaceId.isEmpty()) {
        if (workspaceId.isEmpty()) {
            setError(errorMessage, QStringLiteral("workspaceId is required"));
        }
        return std::nullopt;
    }

    ServerMessageSessionSnapshot session;
    session.sessionId = *sessionId;
    session.clientId = *clientId;
    session.deviceId = *deviceId;
    session.workspaceId = workspaceId;
    session.connectedAtMs =
        object.value(QStringLiteral("connectedAtMs")).toInteger(0);
    session.lastSeenAtMs =
        object.value(QStringLiteral("lastSeenAtMs")).toInteger(0);
    session.lastEventId =
        object.value(QStringLiteral("lastEventId")).toInteger(0);
    session.appVersion =
        object.value(QStringLiteral("appVersion")).toString().trimmed();
    session.capabilities =
        stringArray(object.value(QStringLiteral("capabilities")));
    return session;
}

std::optional<QVector<ServerMessageSessionSnapshot>> parseOnlineSessions(
    const std::optional<QJsonDocument>& document,
    QString* errorMessage)
{
    const auto object = requiredObjectDocument(document, errorMessage);
    if (!object || !requireOkTrue(*object, errorMessage)) {
        return std::nullopt;
    }

    const auto workspaceId =
        requiredString(*object, QStringLiteral("workspaceId"), errorMessage);
    if (!workspaceId) {
        return std::nullopt;
    }

    const QJsonValue sessionsValue = object->value(QStringLiteral("sessions"));
    if (!sessionsValue.isArray()) {
        setError(errorMessage, QStringLiteral("sessions is required"));
        return std::nullopt;
    }

    QVector<ServerMessageSessionSnapshot> sessions;
    const QJsonArray array = sessionsValue.toArray();
    sessions.reserve(array.size());
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            setError(errorMessage,
                     QStringLiteral("sessions must contain objects"));
            return std::nullopt;
        }
        auto session =
            parseSessionSnapshot(value.toObject(), *workspaceId, errorMessage);
        if (!session) {
            return std::nullopt;
        }
        sessions.push_back(*session);
    }
    return sessions;
}

std::optional<ServerClientCapabilityProfile> parseCapabilityProfile(
    const QJsonObject& object,
    const QString& fallbackWorkspaceId,
    QString* errorMessage)
{
    const auto clientId =
        requiredString(object, QStringLiteral("clientId"), errorMessage);
    if (!clientId) {
        return std::nullopt;
    }

    ServerClientCapabilityProfile profile;
    profile.workspaceId =
        firstNonEmpty({object.value(QStringLiteral("workspaceId")).toString(),
                       fallbackWorkspaceId});
    profile.clientId = *clientId;
    profile.appVersion =
        object.value(QStringLiteral("appVersion")).toString().trimmed();
    profile.capabilities =
        stringArray(object.value(QStringLiteral("capabilities")));
    profile.updatedAtMs =
        object.value(QStringLiteral("updatedAtMs")).toInteger(0);
    return profile;
}

std::optional<ServerClientCapabilityQueryResult> parseCapabilityQueryResult(
    const std::optional<QJsonDocument>& document,
    QString* errorMessage)
{
    const auto object = requiredObjectDocument(document, errorMessage);
    if (!object || !requireOkTrue(*object, errorMessage)) {
        return std::nullopt;
    }

    const auto workspaceId =
        requiredString(*object, QStringLiteral("workspaceId"), errorMessage);
    if (!workspaceId) {
        return std::nullopt;
    }

    const QJsonValue profilesValue = object->value(QStringLiteral("profiles"));
    if (!profilesValue.isArray()) {
        setError(errorMessage, QStringLiteral("profiles is required"));
        return std::nullopt;
    }

    ServerClientCapabilityQueryResult result;
    result.workspaceId = *workspaceId;
    const QJsonArray profiles = profilesValue.toArray();
    for (const QJsonValue& value : profiles) {
        if (!value.isObject()) {
            setError(errorMessage,
                     QStringLiteral("profiles must contain objects"));
            return std::nullopt;
        }
        auto profile =
            parseCapabilityProfile(value.toObject(), result.workspaceId, errorMessage);
        if (!profile) {
            return std::nullopt;
        }
        result.profiles.push_back(*profile);
    }
    return result;
}

QJsonArray recipientArray(const QVector<QString>& recipientIds)
{
    QJsonArray array;
    for (const QString& recipientId : recipientIds) {
        const QString trimmed = recipientId.trimmed();
        if (!trimmed.isEmpty()) {
            array.append(trimmed);
        }
    }
    return array;
}

int boundedLimit(int limit)
{
    if (limit < 1) {
        return 1;
    }
    if (limit > 500) {
        return 500;
    }
    return limit;
}

std::optional<QByteArray> waitForByteReply(QNetworkReply* rawReply,
                                           int timeoutMs,
                                           QString* errorMessage,
                                           int* httpStatus = nullptr)
{
    if (httpStatus) {
        *httpStatus = 0;
    }
    QPointer<QNetworkReply> reply(rawReply);
    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeoutTimer.start(timeoutMs);
    loop.exec();

    if (!reply) {
        setError(errorMessage,
                 QStringLiteral("message service request object was released"));
        return std::nullopt;
    }

    if (timeoutTimer.isActive()) {
        timeoutTimer.stop();
    } else {
        reply->abort();
        setError(errorMessage, QStringLiteral("message service request timed out"));
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    const QByteArray body = reply->readAll();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (httpStatus) {
        *httpStatus = statusCode;
    }
    if (reply->error() != QNetworkReply::NoError) {
        const QString statusPrefix = statusCode > 0
            ? QStringLiteral("HTTP %1: ").arg(statusCode)
            : QString();
        setError(errorMessage,
                 statusPrefix + firstNonEmpty({QString::fromUtf8(body),
                                reply->errorString(),
                                QStringLiteral("message service request failed")}));
        deleteSynchronousNetworkReply(reply);
        return std::nullopt;
    }

    deleteSynchronousNetworkReply(reply);
    return body;
}

std::optional<QJsonDocument> waitForJsonReply(QNetworkReply* rawReply,
                                              int timeoutMs,
                                              QString* errorMessage,
                                              int* httpStatus = nullptr)
{
    const auto body = waitForByteReply(rawReply,
                                       timeoutMs,
                                       errorMessage,
                                       httpStatus);
    if (!body) {
        return std::nullopt;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(*body, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        setError(errorMessage,
                 QStringLiteral("message service returned invalid JSON"));
        return std::nullopt;
    }

    return document;
}

void applyMessageServiceRequestHeaders(
    QNetworkRequest* request,
    const RemoteChatServiceSettings& settings)
{
    if (!request) {
        return;
    }
    request->setRawHeader(
        "Authorization",
        QStringLiteral("Bearer %1").arg(settings.bearerToken.trimmed()).toUtf8());
    const QString clientId = settings.clientId.trimmed();
    if (!clientId.isEmpty()) {
        request->setRawHeader("X-Client-Id", clientId.toUtf8());
    }
}

}  // namespace

bool ServerClientCapabilityProfile::supports(const QString& capability) const
{
    return capabilities.contains(capability.trimmed(), Qt::CaseInsensitive);
}

std::optional<QJsonDocument> NetworkServerMessageTransport::getJson(
    const QUrl& url,
    const RemoteChatServiceSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    applyMessageServiceRequestHeaders(&request, settings);

    return waitForJsonReply(manager.get(request), 15000, errorMessage);
}

std::optional<QByteArray> NetworkServerMessageTransport::getBytes(
    const QUrl& url,
    const RemoteChatServiceSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Accept", QByteArrayLiteral("text/event-stream"));
    applyMessageServiceRequestHeaders(&request, settings);

    return waitForByteReply(manager.get(request), 15000, errorMessage);
}

std::optional<QJsonDocument> NetworkServerMessageTransport::postJson(
    const QUrl& url,
    const QJsonObject& body,
    const RemoteChatServiceSettings& settings,
    QString* errorMessage) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    applyMessageServiceRequestHeaders(&request, settings);

    const QByteArray payload =
        QJsonDocument(body).toJson(QJsonDocument::Compact);
    return waitForJsonReply(manager.post(request, payload), 15000, errorMessage);
}

ServerMessageTransportResponse NetworkServerMessageTransport::postJsonWithStatus(
    const QUrl& url,
    const QJsonObject& body,
    const RemoteChatServiceSettings& settings) const
{
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    applyMessageServiceRequestHeaders(&request, settings);

    const QByteArray payload =
        QJsonDocument(body).toJson(QJsonDocument::Compact);
    ServerMessageTransportResponse response;
    response.document = waitForJsonReply(manager.post(request, payload),
                                         15000,
                                         &response.errorMessage,
                                         &response.httpStatus);
    return response;
}

ServerMessageClient::ServerMessageClient(
    RemoteChatServiceSettings settings,
    std::shared_ptr<IServerMessageTransport> transport)
    : m_settings(std::move(settings))
    , m_transport(std::move(transport))
{
}

ServerMessageClient::ServerMessageClient(
    RemoteChatServiceSettings settings,
    QString clientId,
    std::shared_ptr<IServerMessageTransport> transport)
    : ServerMessageClient([&]() {
          settings.clientId = clientId.trimmed();
          return settings;
      }(),
      std::move(transport))
{
}

std::optional<ServerMessageAck> ServerMessageClient::sendMessage(
    const ServerMessageDraft& draft,
    QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return std::nullopt;
    }

    const QString workspaceId = firstNonEmpty({draft.workspaceId,
                                               m_settings.workspaceId});
    if (draft.clientMessageId.trimmed().isEmpty()
        || draft.conversationId.trimmed().isEmpty()
        || workspaceId.trimmed().isEmpty()
        || draft.type.trimmed().isEmpty()) {
        setError(errorMessage,
                 QStringLiteral("clientMessageId, conversationId, workspaceId, "
                                "and type are required"));
        return std::nullopt;
    }

    const QJsonArray recipients = recipientArray(draft.recipientIds);
    if (recipients.isEmpty()) {
        setError(errorMessage, QStringLiteral("recipientIds must not be empty"));
        return std::nullopt;
    }

    QJsonObject body;
    body[QStringLiteral("clientMessageId")] = draft.clientMessageId.trimmed();
    body[QStringLiteral("conversationId")] = draft.conversationId.trimmed();
    body[QStringLiteral("workspaceId")] = workspaceId.trimmed();
    body[QStringLiteral("type")] = draft.type.trimmed();
    body[QStringLiteral("body")] = draft.body;
    body[QStringLiteral("payload")] = draft.payload;
    body[QStringLiteral("fileId")] = draft.fileId.trimmed();
    body[QStringLiteral("contentType")] = draft.contentType.trimmed();
    body[QStringLiteral("replyToMessageId")] = draft.replyToMessageId.trimmed();
    body[QStringLiteral("recipientIds")] = recipients;

    const auto document =
        m_transport->postJson(messagesUrl(), body, m_settings, errorMessage);
    auto ack = parseAck(document, errorMessage);
    if (ack) {
        clearError(errorMessage);
    }
    return ack;
}

std::optional<ServerMessagePage> ServerMessageClient::listMessages(
    const QString& conversationId,
    qint64 afterSeq,
    int limit,
    QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return std::nullopt;
    }
    if (conversationId.trimmed().isEmpty()) {
        setError(errorMessage, QStringLiteral("conversationId is required"));
        return std::nullopt;
    }

    const auto document =
        m_transport->getJson(conversationMessagesUrl(conversationId,
                                                     afterSeq,
                                                     boundedLimit(limit)),
                             m_settings,
                             errorMessage);
    auto page = parsePage(document, errorMessage);
    if (page) {
        clearError(errorMessage);
    }
    return page;
}

std::optional<QVector<ServerConversationRecord>>
ServerMessageClient::listConversations(const QString& workspaceId,
                                       int limit,
                                       QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return std::nullopt;
    }

    const QString effectiveWorkspaceId =
        firstNonEmpty({workspaceId, m_settings.workspaceId});
    if (effectiveWorkspaceId.isEmpty()) {
        setError(errorMessage, QStringLiteral("workspaceId is required"));
        return std::nullopt;
    }

    const auto document =
        m_transport->getJson(conversationsUrl(effectiveWorkspaceId,
                                              boundedLimit(limit)),
                             m_settings,
                             errorMessage);
    auto conversations = parseConversationList(document, errorMessage);
    if (conversations) {
        clearError(errorMessage);
    }
    return conversations;
}

std::optional<ServerMessageEventPage> ServerMessageClient::listEvents(
    const QString& workspaceId,
    const QString& deviceId,
    qint64 afterEventId,
    int limit,
    QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return std::nullopt;
    }

    const QString effectiveWorkspaceId =
        firstNonEmpty({workspaceId, m_settings.workspaceId});
    if (effectiveWorkspaceId.isEmpty()) {
        setError(errorMessage, QStringLiteral("workspaceId is required"));
        return std::nullopt;
    }

    const auto body = m_transport->getBytes(
        eventsStreamUrl(effectiveWorkspaceId, deviceId, afterEventId, limit),
        m_settings,
        errorMessage);
    if (!body) {
        if (errorMessage && errorMessage->trimmed().isEmpty()) {
            *errorMessage = QStringLiteral("message event stream request failed");
        }
        return std::nullopt;
    }

    auto page = parseSsePage(*body, errorMessage);
    if (page) {
        clearError(errorMessage);
    }
    return page;
}

std::optional<ServerMessageSessionAck> ServerMessageClient::sendSessionHeartbeat(
    const QString& workspaceId,
    const QString& deviceId,
    qint64 lastEventId,
    QString* errorMessage) const
{
    return sendSessionHeartbeat(workspaceId,
                                deviceId,
                                lastEventId,
                                QString(),
                                QStringList(),
                                errorMessage);
}

std::optional<ServerMessageSessionAck> ServerMessageClient::sendSessionHeartbeat(
    const QString& workspaceId,
    const QString& deviceId,
    qint64 lastEventId,
    const QString& appVersion,
    const QStringList& capabilities,
    QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return std::nullopt;
    }

    const QString effectiveWorkspaceId =
        firstNonEmpty({workspaceId, m_settings.workspaceId});
    if (effectiveWorkspaceId.isEmpty()) {
        setError(errorMessage, QStringLiteral("workspaceId is required"));
        return std::nullopt;
    }

    QJsonObject body;
    body[QStringLiteral("workspaceId")] = effectiveWorkspaceId;
    body[QStringLiteral("deviceId")] = deviceId.trimmed();
    body[QStringLiteral("lastEventId")] = qMax<qint64>(0, lastEventId);
    const QString normalizedAppVersion = appVersion.trimmed();
    if (!normalizedAppVersion.isEmpty()) {
        body[QStringLiteral("appVersion")] = normalizedAppVersion;
    }
    const QJsonArray capabilityArray = stringArrayJson(capabilities);
    if (!capabilityArray.isEmpty()) {
        body[QStringLiteral("capabilities")] = capabilityArray;
    }

    const auto document = m_transport->postJson(sessionHeartbeatUrl(),
                                                body,
                                                m_settings,
                                                errorMessage);
    auto ack = parseSessionAck(document, errorMessage);
    if (ack) {
        clearError(errorMessage);
    }
    return ack;
}

std::optional<QVector<ServerMessageSessionSnapshot>>
ServerMessageClient::listOnlineSessions(const QString& workspaceId,
                                        QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return std::nullopt;
    }

    const QString effectiveWorkspaceId =
        firstNonEmpty({workspaceId, m_settings.workspaceId});
    if (effectiveWorkspaceId.isEmpty()) {
        setError(errorMessage, QStringLiteral("workspaceId is required"));
        return std::nullopt;
    }

    const auto document = m_transport->getJson(
        onlineSessionsUrl(effectiveWorkspaceId),
        m_settings,
        errorMessage);
    auto sessions = parseOnlineSessions(document, errorMessage);
    if (sessions) {
        clearError(errorMessage);
    }
    return sessions;
}

std::optional<ServerClientCapabilityQueryResult>
ServerMessageClient::queryClientCapabilities(
    const QString& workspaceId,
    const QStringList& clientIds,
    const QString& requiredCapability,
    QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return std::nullopt;
    }

    const QString effectiveWorkspaceId =
        firstNonEmpty({workspaceId, m_settings.workspaceId});
    if (effectiveWorkspaceId.isEmpty()) {
        setError(errorMessage, QStringLiteral("workspaceId is required"));
        return std::nullopt;
    }

    const QJsonArray clientIdArray = stringArrayJson(clientIds);
    if (clientIdArray.isEmpty()) {
        setError(errorMessage, QStringLiteral("clientIds must not be empty"));
        return std::nullopt;
    }

    QJsonObject body;
    body[QStringLiteral("workspaceId")] = effectiveWorkspaceId;
    body[QStringLiteral("requiredCapability")] = requiredCapability.trimmed();
    body[QStringLiteral("clientIds")] = clientIdArray;

    const auto document = m_transport->postJson(clientCapabilitiesQueryUrl(),
                                                body,
                                                m_settings,
                                                errorMessage);
    auto result = parseCapabilityQueryResult(document, errorMessage);
    if (result) {
        clearError(errorMessage);
    }
    return result;
}

bool ServerMessageClient::checkHealth(QString* errorMessage) const
{
    if (!validateReady(errorMessage)) {
        return false;
    }

    const auto document =
        m_transport->getJson(healthUrl(), m_settings, errorMessage);
    const auto object = requiredObjectDocument(document, errorMessage);
    if (!object || !isOkResponse(*object, errorMessage)) {
        return false;
    }

    if (object->contains(QStringLiteral("ready"))
        && !object->value(QStringLiteral("ready")).toBool(false)) {
        setError(errorMessage,
                 firstNonEmpty({object->value(QStringLiteral("status")).toString(),
                                object->value(QStringLiteral("error")).toString(),
                                QStringLiteral("message service is not ready")}));
        return false;
    }

    clearError(errorMessage);
    return true;
}

bool ServerMessageClient::acknowledgeDelivered(const QString& serverMessageId,
                                               qint64 receivedSeq,
                                               QString* errorMessage) const
{
    const ServerAckAttemptResult result =
        acknowledgeDeliveredResult(serverMessageId, receivedSeq);
    setError(errorMessage, result.errorMessage);
    return result.outcome == ServerAckOutcome::Acknowledged;
}

bool ServerMessageClient::acknowledgeRead(const QString& serverMessageId,
                                          qint64 readSeq,
                                          QString* errorMessage) const
{
    const ServerAckAttemptResult result =
        acknowledgeReadResult(serverMessageId, readSeq);
    setError(errorMessage, result.errorMessage);
    return result.outcome == ServerAckOutcome::Acknowledged;
}

ServerAckAttemptResult ServerMessageClient::acknowledgeDeliveredResult(
    const QString& serverMessageId,
    qint64 receivedSeq) const
{
    ServerAckAttemptResult result;
    QString validationError;
    if (!validateReady(&validationError)) {
        result.errorMessage = validationError;
        return result;
    }
    if (serverMessageId.trimmed().isEmpty() || receivedSeq < 0) {
        result.errorMessage =
            QStringLiteral("serverMessageId and receivedSeq are required");
        return result;
    }

    const ServerMessageTransportResponse response = m_transport->postJsonWithStatus(
        deliveryAckUrl(serverMessageId),
        QJsonObject{{QStringLiteral("receivedSeq"), receivedSeq}},
        m_settings);
    result.httpStatus = response.httpStatus;
    result.errorMessage = response.errorMessage;
    if (response.httpStatus == 404) {
        result.outcome = ServerAckOutcome::MessageNotFound;
        return result;
    }

    QString parseError = response.errorMessage;
    const auto object = requiredObjectDocument(response.document, &parseError);
    if (!object || !requireOkTrue(*object, &parseError)) {
        result.errorMessage = parseError;
        return result;
    }
    result.outcome = ServerAckOutcome::Acknowledged;
    result.errorMessage.clear();
    return result;
}

ServerAckAttemptResult ServerMessageClient::acknowledgeReadResult(
    const QString& serverMessageId,
    qint64 readSeq) const
{
    ServerAckAttemptResult result;
    QString validationError;
    if (!validateReady(&validationError)) {
        result.errorMessage = validationError;
        return result;
    }
    if (serverMessageId.trimmed().isEmpty() || readSeq < 0) {
        result.errorMessage =
            QStringLiteral("serverMessageId and readSeq are required");
        return result;
    }

    const ServerMessageTransportResponse response = m_transport->postJsonWithStatus(
        readAckUrl(serverMessageId),
        QJsonObject{{QStringLiteral("readSeq"), readSeq}},
        m_settings);
    result.httpStatus = response.httpStatus;
    result.errorMessage = response.errorMessage;
    if (response.httpStatus == 404) {
        result.outcome = ServerAckOutcome::MessageNotFound;
        return result;
    }

    QString parseError = response.errorMessage;
    const auto object = requiredObjectDocument(response.document, &parseError);
    if (!object || !requireOkTrue(*object, &parseError)) {
        result.errorMessage = parseError;
        return result;
    }
    result.outcome = ServerAckOutcome::Acknowledged;
    result.errorMessage.clear();
    return result;
}

QUrl ServerMessageClient::healthUrl() const
{
    return QUrl(normalizedBaseUrl(m_settings.baseUrl)
                + QStringLiteral("/api/v1/health"));
}

QUrl ServerMessageClient::messagesUrl() const
{
    return QUrl(normalizedBaseUrl(m_settings.baseUrl)
                + QStringLiteral("/api/v1/messages"));
}

QUrl ServerMessageClient::conversationsUrl(const QString& workspaceId,
                                           int limit) const
{
    QUrl url(normalizedBaseUrl(m_settings.baseUrl)
             + QStringLiteral("/api/v1/conversations"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("workspaceId"), workspaceId.trimmed());
    query.addQueryItem(QStringLiteral("limit"),
                       QString::number(boundedLimit(limit)));
    url.setQuery(query);
    return url;
}

QUrl ServerMessageClient::conversationMessagesUrl(const QString& conversationId,
                                                  qint64 afterSeq,
                                                  int limit) const
{
    QUrl url(normalizedBaseUrl(m_settings.baseUrl)
             + QStringLiteral("/api/v1/conversations/")
             + pathSegment(conversationId)
             + QStringLiteral("/messages"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("workspaceId"),
                       m_settings.workspaceId.trimmed());
    query.addQueryItem(QStringLiteral("afterSeq"),
                       QString::number(qMax<qint64>(0, afterSeq)));
    query.addQueryItem(QStringLiteral("limit"),
                       QString::number(boundedLimit(limit)));
    url.setQuery(query);
    return url;
}

QUrl ServerMessageClient::eventsStreamUrl(const QString& workspaceId,
                                          const QString& deviceId,
                                          qint64 afterEventId,
                                          int limit) const
{
    QUrl url(normalizedBaseUrl(m_settings.baseUrl)
             + QStringLiteral("/api/v1/events/stream"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("workspaceId"), workspaceId.trimmed());
    const QString trimmedDeviceId = deviceId.trimmed();
    if (!trimmedDeviceId.isEmpty()) {
        query.addQueryItem(QStringLiteral("deviceId"), trimmedDeviceId);
    }
    query.addQueryItem(QStringLiteral("afterEventId"),
                       QString::number(qMax<qint64>(0, afterEventId)));
    query.addQueryItem(QStringLiteral("limit"),
                       QString::number(boundedLimit(limit)));
    url.setQuery(query);
    return url;
}

QUrl ServerMessageClient::sessionHeartbeatUrl() const
{
    return QUrl(normalizedBaseUrl(m_settings.baseUrl)
                + QStringLiteral("/api/v1/sessions/heartbeat"));
}

QUrl ServerMessageClient::onlineSessionsUrl(const QString& workspaceId) const
{
    QUrl url(normalizedBaseUrl(m_settings.baseUrl)
             + QStringLiteral("/api/v1/sessions/online"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("workspaceId"), workspaceId.trimmed());
    url.setQuery(query);
    return url;
}

QUrl ServerMessageClient::clientCapabilitiesQueryUrl() const
{
    return QUrl(normalizedBaseUrl(m_settings.baseUrl)
                + QStringLiteral("/api/v1/clients/capabilities/query"));
}

QUrl ServerMessageClient::deliveryAckUrl(const QString& serverMessageId) const
{
    return QUrl(normalizedBaseUrl(m_settings.baseUrl)
                + QStringLiteral("/api/v1/messages/")
                + pathSegment(serverMessageId)
                + QStringLiteral("/delivery-ack"));
}

QUrl ServerMessageClient::readAckUrl(const QString& serverMessageId) const
{
    return QUrl(normalizedBaseUrl(m_settings.baseUrl)
                + QStringLiteral("/api/v1/messages/")
                + pathSegment(serverMessageId)
                + QStringLiteral("/read-ack"));
}

bool ServerMessageClient::validateReady(QString* errorMessage) const
{
    if (!m_settings.canUseMessageService()) {
        setError(errorMessage,
                 QStringLiteral("remote chat service is not configured"));
        return false;
    }
    if (!m_transport) {
        setError(errorMessage,
                 QStringLiteral("message service transport is not configured"));
        return false;
    }
    return true;
}
