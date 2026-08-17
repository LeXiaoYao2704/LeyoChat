#include "chatservice/ServiceConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <utility>

namespace {
using LeyoChatService::ServiceConfig;
using LeyoChatService::ServiceConfigResult;

ServiceConfigResult success(ServiceConfig config)
{
    return ServiceConfigResult{true, std::move(config), QString()};
}

ServiceConfigResult failure(ServiceConfig config, const QString& error)
{
    return ServiceConfigResult{false, std::move(config), error};
}

QJsonObject objectValue(const QJsonObject& root, const QString& key)
{
    const QJsonValue value = root.value(key);
    return value.isObject() ? value.toObject() : QJsonObject{};
}

QString stringValue(const QJsonObject& object,
                    const QString& key,
                    const QString& fallback)
{
    const QJsonValue value = object.value(key);
    if (!value.isString())
        return fallback;
    return value.toString().trimmed();
}

int intValue(const QJsonObject& object, const QString& key, int fallback)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInt(fallback) : fallback;
}

bool boolValue(const QJsonObject& object, const QString& key, bool fallback)
{
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool(fallback) : fallback;
}

qint64 int64Value(const QJsonObject& object,
                  const QString& key,
                  qint64 fallback)
{
    const QJsonValue value = object.value(key);
    return value.isDouble() ? value.toInteger(fallback) : fallback;
}

QStringList workspaceListValue(const QJsonValue& value)
{
    QStringList result;
    if (value.isArray()) {
        const QJsonArray array = value.toArray();
        for (const QJsonValue& item : array) {
            const QString workspaceId = item.toString().trimmed();
            if (!workspaceId.isEmpty())
                result.push_back(workspaceId);
        }
        return result;
    }
    if (value.isString()) {
        const QString raw = value.toString().trimmed();
        if (raw == QStringLiteral("*"))
            return {};
        const QStringList parts =
            raw.split(QLatin1Char(','), Qt::SkipEmptyParts);
        for (const QString& part : parts) {
            const QString workspaceId = part.trimmed();
            if (!workspaceId.isEmpty())
                result.push_back(workspaceId);
        }
    }
    return result;
}

bool hasWildcardWorkspace(const QJsonObject& auth)
{
    if (auth.value(QStringLiteral("allowWildcardWorkspaces")).toBool(false))
        return true;
    return auth.value(QStringLiteral("workspaces")).isString()
        && auth.value(QStringLiteral("workspaces")).toString().trimmed()
            == QStringLiteral("*");
}
}

namespace LeyoChatService {

ServiceConfig defaultServiceConfig()
{
    return ServiceConfig{};
}

ServiceConfigResult loadServiceConfigFile(const QString& path)
{
    ServiceConfig config = defaultServiceConfig();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(config,
                       QStringLiteral("failed to open service config: %1")
                           .arg(path));
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(config,
                       QStringLiteral("service config must be a JSON object"));
    }

    const QJsonObject root = document.object();
    const QJsonObject listen = objectValue(root, QStringLiteral("listen"));
    config.listenPort =
        intValue(listen, QStringLiteral("port"), config.listenPort);

    const QJsonObject database = objectValue(root, QStringLiteral("database"));
    config.databasePath =
        stringValue(database, QStringLiteral("path"), config.databasePath);

    const QJsonObject storage = objectValue(root, QStringLiteral("storage"));
    config.storagePath =
        stringValue(storage, QStringLiteral("path"), config.storagePath);

    const QJsonObject auth = objectValue(root, QStringLiteral("auth"));
    config.seedToken =
        stringValue(auth, QStringLiteral("token"), config.seedToken);
    config.legacyFileAccessEnabled =
        boolValue(auth,
                  QStringLiteral("legacyFileAccess"),
                  config.legacyFileAccessEnabled);
    config.allowWildcardWorkspaces = hasWildcardWorkspace(auth);
    config.workspaceIds =
        workspaceListValue(auth.value(QStringLiteral("workspaces")));

    const QJsonObject sessions = objectValue(root, QStringLiteral("sessions"));
    const qint64 ttlSeconds =
        int64Value(sessions, QStringLiteral("ttlSeconds"), config.sessionTtlMs / 1000);
    config.sessionTtlMs = ttlSeconds * 1000;

    const QJsonObject rateLimit = objectValue(root, QStringLiteral("rateLimit"));
    config.rateLimitMaxRequestsPerWindow =
        intValue(rateLimit,
                 QStringLiteral("maxRequestsPerWindow"),
                 config.rateLimitMaxRequestsPerWindow);
    config.rateLimitWindowMs =
        int64Value(rateLimit,
                   QStringLiteral("windowMs"),
                   config.rateLimitWindowMs);

    const QJsonObject onlyOffice =
        objectValue(root, QStringLiteral("onlyOffice"));
    config.onlyOfficeUrl =
        stringValue(onlyOffice, QStringLiteral("url"), config.onlyOfficeUrl);
    config.onlyOfficeJwtSecret =
        stringValue(onlyOffice,
                    QStringLiteral("jwtSecret"),
                    config.onlyOfficeJwtSecret);

    config.externalUrl =
        stringValue(root, QStringLiteral("externalUrl"), config.externalUrl);

    const QJsonObject chatFiles = objectValue(root, QStringLiteral("chatFiles"));
    config.chatFileTtlDays =
        intValue(chatFiles, QStringLiteral("ttlDays"), config.chatFileTtlDays);
    config.chatFileQuotaMb =
        intValue(chatFiles, QStringLiteral("quotaMb"), config.chatFileQuotaMb);

    return validateServiceConfig(config);
}

ServiceConfigResult validateServiceConfig(const ServiceConfig& config)
{
    if (config.listenPort < 1 || config.listenPort > 65535) {
        return failure(config,
                       QStringLiteral("listen port must be between 1 and 65535"));
    }
    if (config.databasePath.trimmed().isEmpty()) {
        return failure(config, QStringLiteral("database path is required"));
    }
    if (config.storagePath.trimmed().isEmpty()) {
        return failure(config, QStringLiteral("storage path is required"));
    }
    if (config.seedToken.trimmed().isEmpty()) {
        return failure(config, QStringLiteral("auth token is required"));
    }
    if (config.allowWildcardWorkspaces && !config.workspaceIds.isEmpty()) {
        return failure(
            config,
            QStringLiteral("workspace allowlist and wildcard are mutually exclusive"));
    }
    if (!config.allowWildcardWorkspaces && config.workspaceIds.isEmpty()) {
        return failure(config, QStringLiteral("workspace scope is required"));
    }
    for (const QString& workspaceId : config.workspaceIds) {
        if (workspaceId.trimmed().isEmpty()) {
            return failure(config,
                           QStringLiteral("workspace IDs must not be empty"));
        }
    }
    if (config.sessionTtlMs <= 0) {
        return failure(config, QStringLiteral("session TTL must be positive"));
    }
    if (config.rateLimitMaxRequestsPerWindow <= 0
        || config.rateLimitWindowMs <= 0) {
        return failure(config,
                       QStringLiteral("rate limit values must be positive"));
    }
    if (config.chatFileTtlDays < 0 || config.chatFileQuotaMb <= 0) {
        return failure(config,
                       QStringLiteral("chat file cleanup values are invalid"));
    }
    return success(config);
}

ServiceConfigResult validateServiceStartupPaths(const ServiceConfig& config)
{
    const ServiceConfigResult validated = validateServiceConfig(config);
    if (!validated.ok)
        return validated;

    const QFileInfo dbFile(config.databasePath);
    const QDir dbDir = dbFile.absoluteDir();
    if (!dbDir.exists()) {
        return failure(config,
                       QStringLiteral("database directory does not exist: %1")
                           .arg(dbDir.absolutePath()));
    }
    const QFileInfo dbDirInfo(dbDir.absolutePath());
    if (!dbDirInfo.isDir() || !dbDirInfo.isWritable()) {
        return failure(config,
                       QStringLiteral("database directory is not writable: %1")
                           .arg(dbDir.absolutePath()));
    }
    if (dbFile.exists() && !dbFile.isWritable()) {
        return failure(config,
                       QStringLiteral("database file is not writable: %1")
                           .arg(dbFile.absoluteFilePath()));
    }
    return success(config);
}

QString workspaceScopeJson(const ServiceConfig& config)
{
    if (config.allowWildcardWorkspaces)
        return QStringLiteral("*");

    QJsonArray array;
    for (const QString& workspaceId : config.workspaceIds) {
        const QString trimmed = workspaceId.trimmed();
        if (!trimmed.isEmpty())
            array.append(trimmed);
    }
    return QString::fromUtf8(
        QJsonDocument(array).toJson(QJsonDocument::Compact));
}

} // namespace LeyoChatService
