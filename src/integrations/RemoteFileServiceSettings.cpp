#include "integrations/RemoteFileServiceSettings.h"

#include "app/AppSettings.h"

namespace {

constexpr auto kRemoteFileServiceGroup        = "integrations/remoteFileService";
constexpr auto kEnabledKey                    = "enabled";
constexpr auto kBaseUrlKey                    = "baseUrl";
constexpr auto kBearerTokenKey                = "bearerToken";
constexpr auto kDefaultWorkspaceIdKey         = "defaultWorkspaceId";
constexpr auto kLastPollAttemptAtMsKey        = "lastPollAttemptAtMs";
constexpr auto kLastPollSuccessAtMsKey        = "lastPollSuccessAtMs";
constexpr auto kLastPollErrorMessageKey       = "lastPollErrorMessage";
constexpr auto kConsecutivePollFailuresKey    = "consecutivePollFailures";

RemoteFileServiceConnectionSettings loadFromSettings(QSettings& settings)
{
    RemoteFileServiceConnectionSettings config;
    settings.beginGroup(QString::fromLatin1(kRemoteFileServiceGroup));
    config.enabled =
        settings.value(QString::fromLatin1(kEnabledKey), false).toBool();
    config.baseUrl =
        settings.value(QString::fromLatin1(kBaseUrlKey)).toString().trimmed();
    config.bearerToken =
        settings.value(QString::fromLatin1(kBearerTokenKey)).toString().trimmed();
    config.defaultWorkspaceId =
        settings.value(QString::fromLatin1(kDefaultWorkspaceIdKey)).toString().trimmed();
    config.lastPollAttemptAtMs =
        qMax<qint64>(0, settings.value(QString::fromLatin1(kLastPollAttemptAtMsKey), 0).toLongLong());
    config.lastPollSuccessAtMs =
        qMax<qint64>(0, settings.value(QString::fromLatin1(kLastPollSuccessAtMsKey), 0).toLongLong());
    config.lastPollErrorMessage =
        settings.value(QString::fromLatin1(kLastPollErrorMessageKey)).toString().trimmed();
    config.consecutivePollFailures =
        qMax(0, settings.value(QString::fromLatin1(kConsecutivePollFailuresKey), 0).toInt());
    settings.endGroup();
    return config;
}

void saveToSettings(const RemoteFileServiceConnectionSettings& config, QSettings& settings)
{
    settings.beginGroup(QString::fromLatin1(kRemoteFileServiceGroup));
    settings.setValue(QString::fromLatin1(kEnabledKey), config.enabled);
    settings.setValue(QString::fromLatin1(kBaseUrlKey), config.baseUrl.trimmed());
    settings.setValue(QString::fromLatin1(kBearerTokenKey), config.bearerToken.trimmed());
    settings.setValue(QString::fromLatin1(kDefaultWorkspaceIdKey),
                      config.defaultWorkspaceId.trimmed());
    settings.setValue(QString::fromLatin1(kLastPollAttemptAtMsKey),
                      qMax<qint64>(0, config.lastPollAttemptAtMs));
    settings.setValue(QString::fromLatin1(kLastPollSuccessAtMsKey),
                      qMax<qint64>(0, config.lastPollSuccessAtMs));
    settings.setValue(QString::fromLatin1(kLastPollErrorMessageKey),
                      config.lastPollErrorMessage.trimmed());
    settings.setValue(QString::fromLatin1(kConsecutivePollFailuresKey),
                      qMax(0, config.consecutivePollFailures));
    settings.endGroup();
    settings.sync();
}

}  // namespace

namespace RemoteFileServiceSettingsStore {

RemoteFileServiceConnectionSettings load(QSettings* settings)
{
    if (settings) {
        return loadFromSettings(*settings);
    }
    QSettings ownedSettings = AppSettings::createSettings();
    return loadFromSettings(ownedSettings);
}

void save(const RemoteFileServiceConnectionSettings& config, QSettings* settings)
{
    if (settings) {
        saveToSettings(config, *settings);
        return;
    }
    QSettings ownedSettings = AppSettings::createSettings();
    saveToSettings(config, ownedSettings);
}

}  // namespace RemoteFileServiceSettingsStore

namespace GroupFileServiceSettingsStore {

static QString groupSettingsKey(const QString& groupId) {
    return QStringLiteral("integrations/groupFileService/%1").arg(groupId);
}

static GroupFileServiceConfig loadFromSettings(const QString& groupId, QSettings& s) {
    GroupFileServiceConfig c;
    c.groupId = groupId;
    s.beginGroup(groupSettingsKey(groupId));
    c.enabled     = s.value(QStringLiteral("enabled"), false).toBool();
    c.baseUrl     = s.value(QStringLiteral("baseUrl")).toString();
    c.bearerToken = s.value(QStringLiteral("bearerToken")).toString();
    c.workspaceId = s.value(QStringLiteral("workspaceId")).toString();
    c.chatFileTtlDays = s.value(QStringLiteral("chatFileTtlDays"), 7).toInt();
    c.chatFileQuotaMb = s.value(QStringLiteral("chatFileQuotaMb"), 2048).toInt();
    s.endGroup();
    return c;
}

static void saveToSettings(const GroupFileServiceConfig& config, QSettings& s) {
    s.beginGroup(groupSettingsKey(config.groupId));
    s.setValue(QStringLiteral("enabled"),     config.enabled);
    s.setValue(QStringLiteral("baseUrl"),     config.baseUrl);
    s.setValue(QStringLiteral("bearerToken"), config.bearerToken);
    s.setValue(QStringLiteral("workspaceId"), config.workspaceId);
    s.setValue(QStringLiteral("chatFileTtlDays"), config.chatFileTtlDays);
    s.setValue(QStringLiteral("chatFileQuotaMb"), config.chatFileQuotaMb);
    s.endGroup();
    s.sync();
}

GroupFileServiceConfig load(const QString& groupId, QSettings* settings) {
    if (settings) return loadFromSettings(groupId, *settings);
    QSettings owned = AppSettings::createSettings();
    return loadFromSettings(groupId, owned);
}

void save(const GroupFileServiceConfig& config, QSettings* settings) {
    if (settings) { saveToSettings(config, *settings); return; }
    QSettings owned = AppSettings::createSettings();
    saveToSettings(config, owned);
}

} // namespace GroupFileServiceSettingsStore

// ---------------------------------------------------------------------------
// LocalFileServiceSettingsStore
// ---------------------------------------------------------------------------
namespace {

constexpr auto kLocalFileServiceGroup   = "integrations/localFileService";
constexpr auto kPortKey2                = "port";
constexpr auto kOnlyOfficeUrlKey        = "onlyOfficeUrl";
constexpr auto kOnlyOfficeJwtSecretKey  = "onlyOfficeJwtSecret";
constexpr auto kExternalUrlKey          = "externalUrl";
constexpr auto kChatFileTtlDaysKey2     = "chatFileTtlDays";
constexpr auto kChatFileQuotaMbKey2     = "chatFileQuotaMb";

LocalFileServiceConfig loadLocalFromSettings(QSettings& s)
{
    LocalFileServiceConfig c;
    s.beginGroup(QString::fromLatin1(kLocalFileServiceGroup));
    c.port              = static_cast<quint16>(s.value(QString::fromLatin1(kPortKey2), 8765).toUInt());
    c.onlyOfficeUrl     = s.value(QString::fromLatin1(kOnlyOfficeUrlKey)).toString().trimmed();
    c.onlyOfficeJwtSecret = s.value(QString::fromLatin1(kOnlyOfficeJwtSecretKey)).toString().trimmed();
    c.externalUrl       = s.value(QString::fromLatin1(kExternalUrlKey)).toString().trimmed();
    c.chatFileTtlDays   = s.value(QString::fromLatin1(kChatFileTtlDaysKey2), 7).toInt();
    c.chatFileQuotaMb   = s.value(QString::fromLatin1(kChatFileQuotaMbKey2), 2048).toInt();
    s.endGroup();
    return c;
}

void saveLocalToSettings(const LocalFileServiceConfig& c, QSettings& s)
{
    s.beginGroup(QString::fromLatin1(kLocalFileServiceGroup));
    s.setValue(QString::fromLatin1(kPortKey2), c.port);
    s.setValue(QString::fromLatin1(kOnlyOfficeUrlKey), c.onlyOfficeUrl.trimmed());
    s.setValue(QString::fromLatin1(kOnlyOfficeJwtSecretKey), c.onlyOfficeJwtSecret.trimmed());
    s.setValue(QString::fromLatin1(kExternalUrlKey), c.externalUrl.trimmed());
    s.setValue(QString::fromLatin1(kChatFileTtlDaysKey2), c.chatFileTtlDays);
    s.setValue(QString::fromLatin1(kChatFileQuotaMbKey2), c.chatFileQuotaMb);
    s.endGroup();
    s.sync();
}

} // namespace

namespace LocalFileServiceSettingsStore {

LocalFileServiceConfig load(QSettings* settings)
{
    if (settings) return loadLocalFromSettings(*settings);
    QSettings owned = AppSettings::createSettings();
    return loadLocalFromSettings(owned);
}

void save(const LocalFileServiceConfig& config, QSettings* settings)
{
    if (settings) { saveLocalToSettings(config, *settings); return; }
    QSettings owned = AppSettings::createSettings();
    saveLocalToSettings(config, owned);
}

} // namespace LocalFileServiceSettingsStore
