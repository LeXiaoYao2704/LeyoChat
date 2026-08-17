#pragma once

#include <QString>
#include <QStringList>

namespace LeyoChatService {

struct ServiceConfig {
    int listenPort = 8765;
    QString databasePath = QStringLiteral("leyo-chat-service.db");
    QString storagePath = QStringLiteral("./file-storage");
    QString seedToken;
    QStringList workspaceIds;
    bool allowWildcardWorkspaces = false;
    bool legacyFileAccessEnabled = false;
    QString onlyOfficeUrl = QStringLiteral("http://localhost");
    QString externalUrl = QStringLiteral("http://localhost:8765");
    QString onlyOfficeJwtSecret;
    int chatFileTtlDays = 7;
    int chatFileQuotaMb = 2048;
    qint64 sessionTtlMs = 120000;
    int rateLimitMaxRequestsPerWindow = 600;
    qint64 rateLimitWindowMs = 60000;
};

struct ServiceConfigResult {
    bool ok = false;
    ServiceConfig config;
    QString error;
};

ServiceConfig defaultServiceConfig();

ServiceConfigResult loadServiceConfigFile(const QString& path);

ServiceConfigResult validateServiceConfig(const ServiceConfig& config);

ServiceConfigResult validateServiceStartupPaths(const ServiceConfig& config);

QString workspaceScopeJson(const ServiceConfig& config);

} // namespace LeyoChatService
