#include "app/GroupFileServiceConfigResolver.h"

namespace GroupFileServiceConfigResolver {

GroupFileServiceConfig makeDefaultConfig(
    const QString& groupId,
    const RemoteChatServiceSettings& remoteSettings)
{
    GroupFileServiceConfig config;
    config.groupId = groupId.trimmed();
    config.baseUrl = normalizeRemoteChatServiceBaseUrl(remoteSettings.baseUrl);
    config.bearerToken = remoteSettings.bearerToken.trimmed();
    config.workspaceId = remoteSettings.workspaceId.trimmed();
    config.enabled = !config.groupId.isEmpty()
        && remoteSettings.enabled
        && remoteSettings.mode != RemoteChatTransportMode::P2POnly
        && !config.baseUrl.isEmpty()
        && !config.bearerToken.isEmpty()
        && !config.workspaceId.isEmpty();
    return config;
}

RemoteFileServiceConnectionSettings makeConnectionSettings(
    const GroupFileServiceConfig& config)
{
    RemoteFileServiceConnectionSettings settings;
    settings.enabled = config.enabled;
    settings.baseUrl = config.baseUrl.trimmed();
    settings.bearerToken = config.bearerToken.trimmed();
    settings.defaultWorkspaceId = config.workspaceId.trimmed();
    return settings;
}

}  // namespace GroupFileServiceConfigResolver
