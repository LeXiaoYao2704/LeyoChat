#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include <memory>
#include <optional>

#include "integrations/RemoteChatServiceSettings.h"

struct ServerMessageTransportResponse {
    std::optional<QJsonDocument> document;
    int httpStatus = 0;
    QString errorMessage;
};

class IServerMessageTransport {
public:
    virtual ~IServerMessageTransport() = default;

    virtual std::optional<QJsonDocument> getJson(
        const QUrl& url,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const = 0;

    virtual std::optional<QByteArray> getBytes(
        const QUrl& url,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const
    {
        Q_UNUSED(url);
        Q_UNUSED(settings);
        if (errorMessage) {
            *errorMessage = QStringLiteral("byte transport is not implemented");
        }
        return std::nullopt;
    }

    virtual std::optional<QJsonDocument> postJson(
        const QUrl& url,
        const QJsonObject& body,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const = 0;

    virtual ServerMessageTransportResponse postJsonWithStatus(
        const QUrl& url,
        const QJsonObject& body,
        const RemoteChatServiceSettings& settings) const
    {
        ServerMessageTransportResponse response;
        response.document = postJson(url, body, settings, &response.errorMessage);
        return response;
    }
};

class NetworkServerMessageTransport final : public IServerMessageTransport {
public:
    std::optional<QJsonDocument> getJson(
        const QUrl& url,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const override;

    std::optional<QByteArray> getBytes(
        const QUrl& url,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const override;

    std::optional<QJsonDocument> postJson(
        const QUrl& url,
        const QJsonObject& body,
        const RemoteChatServiceSettings& settings,
        QString* errorMessage) const override;
    ServerMessageTransportResponse postJsonWithStatus(
        const QUrl& url,
        const QJsonObject& body,
        const RemoteChatServiceSettings& settings) const override;
};

struct ServerMessageDraft {
    QString clientMessageId;
    QString conversationId;
    QString workspaceId;
    QString type;
    QString body;
    QJsonObject payload;
    QString fileId;
    QString contentType;
    QString replyToMessageId;
    QVector<QString> recipientIds;
};

struct ServerMessageAck {
    bool duplicate = false;
    QString serverMessageId;
    QString conversationId;
    qint64 serverSeq = 0;
    qint64 createdAtMs = 0;
};

enum class ServerAckOutcome {
    Acknowledged,
    RetryableFailure,
    MessageNotFound
};

struct ServerAckAttemptResult {
    ServerAckOutcome outcome = ServerAckOutcome::RetryableFailure;
    int httpStatus = 0;
    QString errorMessage;
};

struct ServerMessageRecord {
    QString serverMessageId;
    QString clientMessageId;
    QString conversationId;
    QString workspaceId;
    QString senderId;
    qint64 serverSeq = 0;
    QString type;
    QString body;
    QJsonObject payload;
    QString fileId;
    QString contentType;
    QString replyToMessageId;
    qint64 createdAtMs = 0;
};

struct ServerMessagePage {
    QVector<ServerMessageRecord> messages;
    qint64 nextAfterSeq = 0;
};

struct ServerConversationRecord {
    QString conversationId;
    QString workspaceId;
    qint64 latestServerSeq = 0;
    qint64 updatedAtMs = 0;
};

struct ServerMessageEvent {
    qint64 eventId = 0;
    QString type;
    QString workspaceId;
    QString conversationId;
    QJsonObject data;
};

struct ServerMessageEventPage {
    QVector<ServerMessageEvent> events;
    qint64 nextAfterEventId = 0;
};

struct ServerMessageSessionAck {
    bool ok = false;
    QString sessionId;
    QString clientId;
    QString deviceId;
    QString workspaceId;
    qint64 lastEventId = 0;
};

struct ServerMessageSessionSnapshot {
    QString sessionId;
    QString clientId;
    QString deviceId;
    QString workspaceId;
    qint64 connectedAtMs = 0;
    qint64 lastSeenAtMs = 0;
    qint64 lastEventId = 0;
    QString appVersion;
    QStringList capabilities;
};

struct ServerClientCapabilityProfile {
    QString workspaceId;
    QString clientId;
    QString appVersion;
    QStringList capabilities;
    qint64 updatedAtMs = 0;

    bool supports(const QString& capability) const;
};

struct ServerClientCapabilityQueryResult {
    QString workspaceId;
    QVector<ServerClientCapabilityProfile> profiles;
};

class IServerMessageClient {
public:
    virtual ~IServerMessageClient() = default;

    virtual std::optional<ServerMessageAck> sendMessage(
        const ServerMessageDraft& draft,
        QString* errorMessage = nullptr) const = 0;

    virtual std::optional<ServerMessagePage> listMessages(
        const QString& conversationId,
        qint64 afterSeq,
        int limit,
        QString* errorMessage = nullptr) const = 0;

    virtual std::optional<QVector<ServerConversationRecord>> listConversations(
        const QString& workspaceId,
        int limit,
        QString* errorMessage = nullptr) const
    {
        Q_UNUSED(workspaceId);
        Q_UNUSED(limit);
        if (errorMessage) {
            *errorMessage = QStringLiteral("conversation list is not configured");
        }
        return std::nullopt;
    }

    virtual bool acknowledgeDelivered(const QString& serverMessageId,
                                      qint64 receivedSeq,
                                      QString* errorMessage = nullptr) const = 0;

    virtual bool acknowledgeRead(const QString& serverMessageId,
                                 qint64 readSeq,
                                      QString* errorMessage = nullptr) const = 0;

    virtual ServerAckAttemptResult acknowledgeDeliveredResult(
        const QString& serverMessageId,
        qint64 receivedSeq) const
    {
        QString error;
        const bool acknowledged = acknowledgeDelivered(serverMessageId, receivedSeq, &error);
        ServerAckAttemptResult result;
        result.outcome = acknowledged ? ServerAckOutcome::Acknowledged
                                      : ServerAckOutcome::RetryableFailure;
        result.errorMessage = error;
        return result;
    }

    virtual ServerAckAttemptResult acknowledgeReadResult(
        const QString& serverMessageId,
        qint64 readSeq) const
    {
        QString error;
        const bool acknowledged = acknowledgeRead(serverMessageId, readSeq, &error);
        ServerAckAttemptResult result;
        result.outcome = acknowledged ? ServerAckOutcome::Acknowledged
                                      : ServerAckOutcome::RetryableFailure;
        result.errorMessage = error;
        return result;
    }

    virtual std::optional<ServerMessageEventPage> listEvents(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 afterEventId,
        int limit,
        QString* errorMessage = nullptr) const
    {
        Q_UNUSED(workspaceId);
        Q_UNUSED(deviceId);
        Q_UNUSED(afterEventId);
        Q_UNUSED(limit);
        if (errorMessage) {
            *errorMessage = QStringLiteral("message event stream is not configured");
        }
        return std::nullopt;
    }

    virtual std::optional<ServerMessageSessionAck> sendSessionHeartbeat(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 lastEventId,
        QString* errorMessage = nullptr) const
    {
        Q_UNUSED(workspaceId);
        Q_UNUSED(deviceId);
        Q_UNUSED(lastEventId);
        if (errorMessage) {
            *errorMessage = QStringLiteral("message session heartbeat is not configured");
        }
        return std::nullopt;
    }

    virtual std::optional<ServerMessageSessionAck> sendSessionHeartbeat(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 lastEventId,
        const QString& appVersion,
        const QStringList& capabilities,
        QString* errorMessage = nullptr) const
    {
        Q_UNUSED(workspaceId);
        Q_UNUSED(deviceId);
        Q_UNUSED(lastEventId);
        Q_UNUSED(appVersion);
        Q_UNUSED(capabilities);
        if (errorMessage) {
            *errorMessage = QStringLiteral("message session heartbeat is not configured");
        }
        return std::nullopt;
    }

    virtual std::optional<QVector<ServerMessageSessionSnapshot>>
    listOnlineSessions(const QString& workspaceId,
                       QString* errorMessage = nullptr) const
    {
        Q_UNUSED(workspaceId);
        if (errorMessage) {
            *errorMessage = QStringLiteral("online session list is not configured");
        }
        return std::nullopt;
    }

    virtual std::optional<ServerClientCapabilityQueryResult>
    queryClientCapabilities(const QString& workspaceId,
                            const QStringList& clientIds,
                            const QString& requiredCapability,
                            QString* errorMessage = nullptr) const
    {
        Q_UNUSED(workspaceId);
        Q_UNUSED(clientIds);
        Q_UNUSED(requiredCapability);
        if (errorMessage) {
            *errorMessage = QStringLiteral("client capability query is not configured");
        }
        return std::nullopt;
    }
};

class ServerMessageClient : public IServerMessageClient {
public:
    explicit ServerMessageClient(
        RemoteChatServiceSettings settings = {},
        std::shared_ptr<IServerMessageTransport> transport =
            std::make_shared<NetworkServerMessageTransport>());
    ServerMessageClient(
        RemoteChatServiceSettings settings,
        QString clientId,
        std::shared_ptr<IServerMessageTransport> transport =
            std::make_shared<NetworkServerMessageTransport>());

    std::optional<ServerMessageAck> sendMessage(
        const ServerMessageDraft& draft,
        QString* errorMessage = nullptr) const override;

    std::optional<ServerMessagePage> listMessages(
        const QString& conversationId,
        qint64 afterSeq,
        int limit,
        QString* errorMessage = nullptr) const override;
    std::optional<QVector<ServerConversationRecord>> listConversations(
        const QString& workspaceId,
        int limit,
        QString* errorMessage = nullptr) const override;

    bool acknowledgeDelivered(const QString& serverMessageId,
                              qint64 receivedSeq,
                              QString* errorMessage = nullptr) const override;

    bool acknowledgeRead(const QString& serverMessageId,
                         qint64 readSeq,
                         QString* errorMessage = nullptr) const override;
    ServerAckAttemptResult acknowledgeDeliveredResult(
        const QString& serverMessageId,
        qint64 receivedSeq) const override;
    ServerAckAttemptResult acknowledgeReadResult(
        const QString& serverMessageId,
        qint64 readSeq) const override;

    std::optional<ServerMessageEventPage> listEvents(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 afterEventId,
        int limit,
        QString* errorMessage = nullptr) const override;

    std::optional<ServerMessageSessionAck> sendSessionHeartbeat(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 lastEventId,
        QString* errorMessage = nullptr) const override;
    std::optional<ServerMessageSessionAck> sendSessionHeartbeat(
        const QString& workspaceId,
        const QString& deviceId,
        qint64 lastEventId,
        const QString& appVersion,
        const QStringList& capabilities,
        QString* errorMessage = nullptr) const override;
    std::optional<QVector<ServerMessageSessionSnapshot>> listOnlineSessions(
        const QString& workspaceId,
        QString* errorMessage = nullptr) const override;
    std::optional<ServerClientCapabilityQueryResult> queryClientCapabilities(
        const QString& workspaceId,
        const QStringList& clientIds,
        const QString& requiredCapability,
        QString* errorMessage = nullptr) const override;

    bool checkHealth(QString* errorMessage = nullptr) const;

private:
    QUrl healthUrl() const;
    QUrl messagesUrl() const;
    QUrl conversationsUrl(const QString& workspaceId, int limit) const;
    QUrl conversationMessagesUrl(const QString& conversationId,
                                 qint64 afterSeq,
                                 int limit) const;
    QUrl eventsStreamUrl(const QString& workspaceId,
                         const QString& deviceId,
                         qint64 afterEventId,
                         int limit) const;
    QUrl sessionHeartbeatUrl() const;
    QUrl onlineSessionsUrl(const QString& workspaceId) const;
    QUrl clientCapabilitiesQueryUrl() const;
    QUrl deliveryAckUrl(const QString& serverMessageId) const;
    QUrl readAckUrl(const QString& serverMessageId) const;

    bool validateReady(QString* errorMessage) const;

    RemoteChatServiceSettings m_settings;
    std::shared_ptr<IServerMessageTransport> m_transport;
};
