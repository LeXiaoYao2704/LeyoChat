#pragma once

#include <QString>

#include "integrations/RemoteChatServiceSettings.h"
#include "integrations/RemoteFileServiceSettings.h"

namespace GroupFileServiceConfigResolver {

GroupFileServiceConfig makeDefaultConfig(
    const QString& groupId,
    const RemoteChatServiceSettings& remoteSettings);

RemoteFileServiceConnectionSettings makeConnectionSettings(
    const GroupFileServiceConfig& config);

}  // namespace GroupFileServiceConfigResolver
