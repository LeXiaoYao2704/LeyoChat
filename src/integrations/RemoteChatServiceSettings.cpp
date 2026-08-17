#include "integrations/RemoteChatServiceSettings.h"

#include "app/AppSettings.h"

#include <QUrl>

namespace {

constexpr auto kRemoteChatServiceGroup = "integrations/remoteChatService";
constexpr auto kEnabledKey = "enabled";
constexpr auto kBaseUrlKey = "baseUrl";
constexpr auto kBearerTokenKey = "bearerToken";
constexpr auto kWorkspaceIdKey = "workspaceId";
constexpr auto kModeKey = "mode";
constexpr auto kAllowP2PFallbackKey = "allowP2PFallback";
constexpr auto kAllowAutomaticPeerConnectionsKey = "allowAutomaticPeerConnections";
constexpr auto kLastHealthCheckAtMsKey = "lastHealthCheckAtMs";
constexpr auto kLastHealthSuccessAtMsKey = "lastHealthSuccessAtMs";
constexpr auto kLastErrorMessageKey = "lastErrorMessage";
constexpr auto kDefaultRemoteChatServicePort = 8765;
constexpr auto kDefaultRemoteChatServiceWorkspaceId = "default";

QString normalizedModeString(const QString& value)
{
    QString normalized = value.trimmed().toLower();
    normalized.replace(QLatin1Char('-'), QLatin1Char('_'));
    return normalized;
}

RemoteChatServiceSettings loadFromSettings(QSettings& settings)
{
    RemoteChatServiceSettings config;
    settings.beginGroup(QString::fromLatin1(kRemoteChatServiceGroup));
    config.enabled =
        settings.value(QString::fromLatin1(kEnabledKey), false).toBool();
    config.baseUrl = normalizeRemoteChatServiceBaseUrl(
        settings.value(QString::fromLatin1(kBaseUrlKey),
                       defaultRemoteChatServiceBaseUrl()).toString());
    config.bearerToken =
        settings.value(QString::fromLatin1(kBearerTokenKey),
                       defaultRemoteChatServiceToken()).toString().trimmed();
    config.workspaceId =
        settings.value(QString::fromLatin1(kWorkspaceIdKey),
                       defaultRemoteChatServiceWorkspaceId()).toString().trimmed();
    config.mode = remoteChatTransportModeFromString(
        settings.value(QString::fromLatin1(kModeKey),
                       remoteChatTransportModeToString(
                           RemoteChatTransportMode::P2POnly)).toString());
    config.allowP2PFallback =
        settings.value(QString::fromLatin1(kAllowP2PFallbackKey), true).toBool();
    config.allowAutomaticPeerConnections =
        settings.value(QString::fromLatin1(kAllowAutomaticPeerConnectionsKey), false).toBool();
    config.lastHealthCheckAtMs =
        qMax<qint64>(0, settings.value(
                         QString::fromLatin1(kLastHealthCheckAtMsKey), 0).toLongLong());
    config.lastHealthSuccessAtMs =
        qMax<qint64>(0, settings.value(
                         QString::fromLatin1(kLastHealthSuccessAtMsKey), 0).toLongLong());
    config.lastErrorMessage =
        settings.value(QString::fromLatin1(kLastErrorMessageKey)).toString().trimmed();
    settings.endGroup();
    return config;
}

void saveToSettings(const RemoteChatServiceSettings& config, QSettings& settings)
{
    settings.beginGroup(QString::fromLatin1(kRemoteChatServiceGroup));
    settings.setValue(QString::fromLatin1(kEnabledKey), config.enabled);
    settings.setValue(QString::fromLatin1(kBaseUrlKey),
                      normalizeRemoteChatServiceBaseUrl(config.baseUrl));
    settings.setValue(QString::fromLatin1(kBearerTokenKey),
                      config.bearerToken.trimmed());
    settings.setValue(QString::fromLatin1(kWorkspaceIdKey),
                      config.workspaceId.trimmed());
    settings.setValue(QString::fromLatin1(kModeKey),
                      remoteChatTransportModeToString(config.mode));
    settings.setValue(QString::fromLatin1(kAllowP2PFallbackKey),
                      config.allowP2PFallback);
    settings.setValue(QString::fromLatin1(kAllowAutomaticPeerConnectionsKey),
                      config.allowAutomaticPeerConnections);
    settings.setValue(QString::fromLatin1(kLastHealthCheckAtMsKey),
                      qMax<qint64>(0, config.lastHealthCheckAtMs));
    settings.setValue(QString::fromLatin1(kLastHealthSuccessAtMsKey),
                      qMax<qint64>(0, config.lastHealthSuccessAtMs));
    settings.setValue(QString::fromLatin1(kLastErrorMessageKey),
                      config.lastErrorMessage.trimmed());
    settings.endGroup();
    settings.sync();
}

}  // namespace

QString remoteChatTransportModeToString(RemoteChatTransportMode mode)
{
    switch (mode) {
    case RemoteChatTransportMode::ServerPreferred:
        return QStringLiteral("server_preferred");
    case RemoteChatTransportMode::ServerOnly:
        return QStringLiteral("server_only");
    case RemoteChatTransportMode::P2POnly:
        return QStringLiteral("p2p_only");
    }
    return QStringLiteral("p2p_only");
}

QString defaultRemoteChatServiceBaseUrl()
{
    return {};
}

QString defaultRemoteChatServiceToken()
{
    return {};
}

QString defaultRemoteChatServiceWorkspaceId()
{
    return QString::fromLatin1(kDefaultRemoteChatServiceWorkspaceId);
}

QString normalizeRemoteChatServiceBaseUrl(const QString& value)
{
    QString text = value.trimmed();
    if (text.isEmpty()) {
        return {};
    }

    if (!text.contains(QStringLiteral("://"))) {
        text.prepend(QStringLiteral("http://"));
    }

    QUrl url(text);
    if (!url.isValid() || url.host().trimmed().isEmpty()) {
        return {};
    }

    if (url.scheme().trimmed().isEmpty()) {
        url.setScheme(QStringLiteral("http"));
    }
    if (url.port() < 0) {
        url.setPort(kDefaultRemoteChatServicePort);
    }
    url.setUserName(QString());
    url.setPassword(QString());
    url.setQuery(QString());
    url.setFragment(QString());

    QString normalized = url.toString(QUrl::RemoveUserInfo
                                      | QUrl::RemoveQuery
                                      | QUrl::RemoveFragment);
    while (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized;
}

RemoteChatTransportMode remoteChatTransportModeFromString(const QString& value)
{
    const QString normalized = normalizedModeString(value);
    if (normalized == QStringLiteral("server_preferred")
        || normalized == QStringLiteral("serverpreferred")) {
        return RemoteChatTransportMode::ServerPreferred;
    }
    if (normalized == QStringLiteral("server_only")
        || normalized == QStringLiteral("serveronly")) {
        return RemoteChatTransportMode::ServerOnly;
    }
    return RemoteChatTransportMode::P2POnly;
}

namespace RemoteChatServiceSettingsStore {

RemoteChatServiceSettings load(QSettings* settings)
{
    if (settings) {
        return loadFromSettings(*settings);
    }
    QSettings ownedSettings = AppSettings::createSettings();
    return loadFromSettings(ownedSettings);
}

void save(const RemoteChatServiceSettings& config, QSettings* settings)
{
    if (settings) {
        saveToSettings(config, *settings);
        return;
    }
    QSettings ownedSettings = AppSettings::createSettings();
    saveToSettings(config, ownedSettings);
}

}  // namespace RemoteChatServiceSettingsStore
