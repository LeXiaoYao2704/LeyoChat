#pragma once

#include <QSettings>
#include <QString>

enum class RemoteChatTransportMode {
    P2POnly,
    ServerPreferred,
    ServerOnly
};

QString remoteChatTransportModeToString(RemoteChatTransportMode mode);
RemoteChatTransportMode remoteChatTransportModeFromString(const QString& value);
QString defaultRemoteChatServiceBaseUrl();
QString defaultRemoteChatServiceToken();
QString defaultRemoteChatServiceWorkspaceId();
QString normalizeRemoteChatServiceBaseUrl(const QString& value);

struct RemoteChatServiceSettings {
    bool enabled = false;
    QString baseUrl;
    QString bearerToken;
    QString workspaceId = QStringLiteral("default");
    // Runtime request identity. This is deliberately not persisted with the
    // shared service credential; each LeyoChat profile supplies its own id.
    QString clientId;
    RemoteChatTransportMode mode = RemoteChatTransportMode::P2POnly;
    bool allowP2PFallback = true;
    bool allowAutomaticPeerConnections = false;
    qint64 lastHealthCheckAtMs = 0;
    qint64 lastHealthSuccessAtMs = 0;
    QString lastErrorMessage;

    bool hasCredentialConfiguration() const
    {
        return !baseUrl.trimmed().isEmpty() && !bearerToken.trimmed().isEmpty();
    }

    bool canUseMessageService() const
    {
        return enabled
            && mode != RemoteChatTransportMode::P2POnly
            && hasCredentialConfiguration()
            && !workspaceId.trimmed().isEmpty();
    }

    bool hasRecentSuccessfulHealth(qint64 nowMs,
                                   qint64 maxAgeMs = 120000) const
    {
        return lastHealthSuccessAtMs > 0
            && nowMs >= lastHealthSuccessAtMs
            && nowMs - lastHealthSuccessAtMs <= maxAgeMs;
    }

    bool shouldAttemptMessageService(qint64 nowMs,
                                     qint64 maxAgeMs = 120000) const
    {
        return canUseMessageService()
            && hasRecentSuccessfulHealth(nowMs, maxAgeMs);
    }

    bool shouldProbeMessageService(qint64 nowMs,
                                   qint64 maxAgeMs = 120000) const
    {
        return canUseMessageService()
            && (lastHealthCheckAtMs <= 0
                || hasRecentSuccessfulHealth(nowMs, maxAgeMs));
    }
};

namespace RemoteChatServiceSettingsStore {
    RemoteChatServiceSettings load(QSettings* settings = nullptr);
    void save(const RemoteChatServiceSettings& config,
              QSettings* settings = nullptr);
}
