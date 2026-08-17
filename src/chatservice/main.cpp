#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QJsonObject>
#include <QLoggingCategory>

#include "FileServiceAuth.h"
#include "FileServiceDatabase.h"
#include "FileServiceHttpServer.h"
#include "FileStorageManager.h"
#include "MessageEventBus.h"
#include "MessageServiceDatabase.h"
#include "MessageServiceHttpRoutes.h"
#include "MessageServiceOperations.h"
#include "MessageSessionRegistry.h"
#include "chatservice/ServiceConfig.h"

Q_LOGGING_CATEGORY(lcChatServiceAuth, "leyochat.service.auth")
Q_LOGGING_CATEGORY(lcChatServiceConfig, "leyochat.service.config")
Q_LOGGING_CATEGORY(lcChatServiceStore, "leyochat.service.message_store")

namespace {
QStringList parseWorkspaceCsv(const QString& value)
{
    QStringList result;
    const QStringList parts =
        value.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString workspaceId = part.trimmed();
        if (!workspaceId.isEmpty())
            result.push_back(workspaceId);
    }
    return result;
}

bool applyIntOverride(const QCommandLineParser& parser,
                      const QCommandLineOption& option,
                      int* target,
                      const QString& label)
{
    if (!parser.isSet(option))
        return true;
    bool ok = false;
    const int value = parser.value(option).toInt(&ok);
    if (!ok) {
        qCCritical(lcChatServiceConfig)
            << "ERROR: invalid numeric value for" << label << parser.value(option);
        return false;
    }
    *target = value;
    return true;
}

bool applyInt64Override(const QCommandLineParser& parser,
                        const QCommandLineOption& option,
                        qint64* target,
                        const QString& label)
{
    if (!parser.isSet(option))
        return true;
    bool ok = false;
    const qint64 value = parser.value(option).toLongLong(&ok);
    if (!ok) {
        qCCritical(lcChatServiceConfig)
            << "ERROR: invalid numeric value for" << label << parser.value(option);
        return false;
    }
    *target = value;
    return true;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LeyoChatService"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    const QDateTime processStartedAtUtc = QDateTime::currentDateTimeUtc();

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configOpt(QStringLiteral("config"),
                                 QStringLiteral("JSON service config file"),
                                 QStringLiteral("path"));
    parser.addOption(configOpt);
    QCommandLineOption portOpt(QStringLiteral("port"),
                               QStringLiteral("Listen port override"),
                               QStringLiteral("port"));
    parser.addOption(portOpt);
    QCommandLineOption dbOpt(QStringLiteral("db"),
                             QStringLiteral("SQLite database path override"),
                             QStringLiteral("path"));
    parser.addOption(dbOpt);
    QCommandLineOption storageOpt(QStringLiteral("storage"),
                                  QStringLiteral("File storage root directory override"),
                                  QStringLiteral("path"));
    parser.addOption(storageOpt);

    QCommandLineOption tokenOpt(
        QStringLiteral("token"),
        QStringLiteral("Bearer token to seed on first launch (required unless config supplies it)"),
        QStringLiteral("token"));
    parser.addOption(tokenOpt);

    QCommandLineOption workspacesOpt(
        QStringLiteral("workspaces"),
        QStringLiteral("Comma-separated workspace IDs this token is allowed to access (e.g. \"group-a,group-b\")"),
        QStringLiteral("workspaces"));
    parser.addOption(workspacesOpt);

    QCommandLineOption allowWildcardOpt(
        QStringLiteral("allow-wildcard-workspaces"),
        QStringLiteral("Explicitly allow this token to access ALL workspaces (*). "
                       "Use only in dev/test or when all groups share the same workspace."));
    parser.addOption(allowWildcardOpt);

    QCommandLineOption onlyOfficeUrlOpt(
        QStringLiteral("onlyoffice-url"),
        QStringLiteral("ONLYOFFICE Document Server URL via nginx proxy override"),
        QStringLiteral("url"));
    parser.addOption(onlyOfficeUrlOpt);

    QCommandLineOption externalUrlOpt(
        QStringLiteral("external-url"),
        QStringLiteral("External URL for this service override"),
        QStringLiteral("url"));
    parser.addOption(externalUrlOpt);

    QCommandLineOption jwtSecretOpt(
        QStringLiteral("onlyoffice-jwt-secret"),
        QStringLiteral("JWT secret for ONLYOFFICE Document Server token signing"),
        QStringLiteral("secret"));
    parser.addOption(jwtSecretOpt);

    QCommandLineOption chatFileTtlOpt(
        QStringLiteral("chat-file-ttl-days"),
        QStringLiteral("Chat file expiration in days override"),
        QStringLiteral("days"));
    parser.addOption(chatFileTtlOpt);
    QCommandLineOption chatFileQuotaOpt(
        QStringLiteral("chat-file-quota-mb"),
        QStringLiteral("Chat file disk quota in MB override"),
        QStringLiteral("mb"));
    parser.addOption(chatFileQuotaOpt);
    QCommandLineOption sessionTtlOpt(
        QStringLiteral("session-ttl-seconds"),
        QStringLiteral("Online session TTL in seconds override"),
        QStringLiteral("seconds"));
    parser.addOption(sessionTtlOpt);
    QCommandLineOption rateLimitMaxOpt(
        QStringLiteral("rate-limit-max"),
        QStringLiteral("Max requests per rate-limit window override"),
        QStringLiteral("count"));
    parser.addOption(rateLimitMaxOpt);
    QCommandLineOption rateLimitWindowOpt(
        QStringLiteral("rate-limit-window-ms"),
        QStringLiteral("Rate-limit window in milliseconds override"),
        QStringLiteral("ms"));
    parser.addOption(rateLimitWindowOpt);

    parser.process(app);

    LeyoChatService::ServiceConfig config =
        LeyoChatService::defaultServiceConfig();
    if (parser.isSet(configOpt)) {
        const auto loaded =
            LeyoChatService::loadServiceConfigFile(parser.value(configOpt));
        if (!loaded.ok) {
            qCCritical(lcChatServiceConfig) << "ERROR:" << loaded.error;
            return 1;
        }
        config = loaded.config;
    }

    if (!applyIntOverride(parser,
                          portOpt,
                          &config.listenPort,
                          QStringLiteral("--port"))) {
        return 1;
    }
    if (parser.isSet(dbOpt))
        config.databasePath = parser.value(dbOpt).trimmed();
    if (parser.isSet(storageOpt))
        config.storagePath = parser.value(storageOpt).trimmed();
    if (parser.isSet(tokenOpt))
        config.seedToken = parser.value(tokenOpt).trimmed();
    if (parser.isSet(onlyOfficeUrlOpt))
        config.onlyOfficeUrl = parser.value(onlyOfficeUrlOpt).trimmed();
    if (parser.isSet(externalUrlOpt))
        config.externalUrl = parser.value(externalUrlOpt).trimmed();
    if (parser.isSet(jwtSecretOpt))
        config.onlyOfficeJwtSecret = parser.value(jwtSecretOpt).trimmed();

    if (!applyIntOverride(parser,
                          chatFileTtlOpt,
                          &config.chatFileTtlDays,
                          QStringLiteral("--chat-file-ttl-days"))
        || !applyIntOverride(parser,
                             chatFileQuotaOpt,
                             &config.chatFileQuotaMb,
                             QStringLiteral("--chat-file-quota-mb"))
        || !applyIntOverride(parser,
                             rateLimitMaxOpt,
                             &config.rateLimitMaxRequestsPerWindow,
                             QStringLiteral("--rate-limit-max"))
        || !applyInt64Override(parser,
                               rateLimitWindowOpt,
                               &config.rateLimitWindowMs,
                               QStringLiteral("--rate-limit-window-ms"))) {
        return 1;
    }

    if (parser.isSet(sessionTtlOpt)) {
        qint64 sessionTtlSeconds = 0;
        if (!applyInt64Override(parser,
                                sessionTtlOpt,
                                &sessionTtlSeconds,
                                QStringLiteral("--session-ttl-seconds"))) {
            return 1;
        }
        config.sessionTtlMs = sessionTtlSeconds * 1000;
    }

    if (parser.isSet(workspacesOpt) && parser.isSet(allowWildcardOpt)) {
        qCCritical(lcChatServiceConfig)
            << "ERROR: --workspaces and --allow-wildcard-workspaces are mutually exclusive.";
        return 1;
    }
    if (parser.isSet(workspacesOpt)) {
        config.workspaceIds = parseWorkspaceCsv(parser.value(workspacesOpt));
        config.allowWildcardWorkspaces = false;
    }
    if (parser.isSet(allowWildcardOpt)) {
        config.workspaceIds.clear();
        config.allowWildcardWorkspaces = true;
    }

    const auto validation =
        LeyoChatService::validateServiceStartupPaths(config);
    if (!validation.ok) {
        qCCritical(lcChatServiceConfig) << "ERROR:" << validation.error;
        return 1;
    }

    const QString allowedWorkspaces =
        LeyoChatService::workspaceScopeJson(config);

    FileServiceDatabase fileDb(config.databasePath);
    if (!fileDb.open()) {
        qCCritical(lcChatServiceStore)
            << "Failed to open file service database:" << config.databasePath;
        return 1;
    }

    MessageServiceDatabase messageDb(
        config.databasePath, QStringLiteral("leyo-chat-service-message-db"));
    if (!messageDb.open()) {
        qCCritical(lcChatServiceStore)
            << "Failed to open message service database:" << config.databasePath;
        return 1;
    }

    FileStorageManager storage(config.storagePath);
    FileServiceAuth auth(&fileDb);
    MessageEventBus eventBus;
    MessageServiceOperations operations;
    operations.setRateLimit(config.rateLimitMaxRequestsPerWindow,
                            config.rateLimitWindowMs);
    MessageSessionRegistry sessions(config.sessionTtlMs);
    if (!auth.seedOrUpdateTokenSecurity(config.seedToken,
                                        QStringLiteral("admin"),
                                        QStringLiteral("Administrator"),
                                        allowedWorkspaces,
                                        QStringLiteral("admin"),
                                        QStringLiteral("*"))) {
        qCCritical(lcChatServiceAuth)
            << "Failed to seed or update token scope. Server will not start.";
        return 1;
    }

    FileServiceHttpServer server(
        &fileDb,
        &storage,
        &auth,
        config.onlyOfficeUrl,
        config.externalUrl,
        config.onlyOfficeJwtSecret,
        [&](QHttpServer& httpServer) {
            MessageServiceHttpRoutes::registerRoutes(httpServer,
                                                     &auth,
                                                     &messageDb,
                                                     &eventBus,
                                                     &operations,
                                                     &sessions,
                                                     [&, processStartedAtUtc] {
                                                         const QString version =
                                                             QCoreApplication::applicationVersion().isEmpty()
                                                                 ? QStringLiteral("unknown")
                                                                 : QCoreApplication::applicationVersion();
                                                         return QJsonObject{
                                                             {QStringLiteral("service"), QStringLiteral("LeyoChatService")},
                                                             {QStringLiteral("ready"), true},
                                                             {QStringLiteral("status"), QStringLiteral("ready")},
                                                             {QStringLiteral("version"), version},
                                                             {QStringLiteral("serviceTimeUtc"),
                                                              QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
                                                             {QStringLiteral("processStartedAtUtc"),
                                                              processStartedAtUtc.toString(Qt::ISODate)},
                                                             {QStringLiteral("database"), QJsonObject{
                                                                 {QStringLiteral("open"), true},
                                                                 {QStringLiteral("migrationComplete"), true},
                                                                 {QStringLiteral("path"), config.databasePath}
                                                             }}
                                                         };
                                                     });
        });

    server.setChatFileCleanupConfig(config.chatFileTtlDays,
                                    config.chatFileQuotaMb);
    server.setLegacyFileAccessEnabled(config.legacyFileAccessEnabled);

    if (!server.listen(QHostAddress::Any,
                       static_cast<quint16>(config.listenPort))) {
        qCCritical(lcChatServiceConfig)
            << "Failed to start HTTP server on port" << config.listenPort;
        return 1;
    }

    qCInfo(lcChatServiceConfig)
        << "LeyoChatService listening on port" << config.listenPort;
    qCInfo(lcChatServiceConfig) << "Storage root:" << config.storagePath;
    qCInfo(lcChatServiceConfig) << "Database:" << config.databasePath;
    qCInfo(lcChatServiceConfig)
        << "Rate limit:"
        << config.rateLimitMaxRequestsPerWindow
        << "requests per" << config.rateLimitWindowMs << "ms";
    qCInfo(lcChatServiceConfig)
        << "Session TTL ms:" << config.sessionTtlMs
        << "Started at:" << QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    qCInfo(lcChatServiceConfig)
        << "Legacy file access compatibility:"
        << (config.legacyFileAccessEnabled ? "enabled" : "disabled");

    return app.exec();
}
