#include "integrations/OutlookSettings.h"

#include "app/AppSettings.h"

#include <QDateTime>

namespace {

constexpr auto kOutlookGroup = "integrations/outlook";
constexpr auto kEnabledKey = "enabled";
constexpr auto kServerUrlKey = "serverUrl";
constexpr auto kUsernameKey = "username";
constexpr auto kPasswordKey = "password";
constexpr auto kAccountEmailKey = "accountEmail";
constexpr auto kDisplayNameKey = "displayName";
constexpr auto kNotificationsEnabledKey = "notificationsEnabled";
constexpr auto kNotificationPollIntervalMinutesKey = "notificationPollIntervalMinutes";
constexpr auto kNotificationConversationIdKey = "notificationConversationId";
constexpr auto kNotificationConversationTitleKey = "notificationConversationTitle";
constexpr auto kRecentMailIdsKey = "recentMailIds";
constexpr auto kRecentEventIdsKey = "recentEventIds";
constexpr auto kLastPollAttemptAtMsKey = "lastPollAttemptAtMs";
constexpr auto kLastPollSuccessAtMsKey = "lastPollSuccessAtMs";
constexpr auto kLastPollErrorMessageKey = "lastPollErrorMessage";
constexpr auto kLastPollErrorCategoryKey = "lastPollErrorCategory";
constexpr auto kConsecutivePollFailuresKey = "consecutivePollFailures";

bool sameCredentialContext(const OutlookConnectionSettings& lhs,
                           const OutlookConnectionSettings& rhs)
{
    return lhs.serverUrl.trimmed().compare(rhs.serverUrl.trimmed(), Qt::CaseInsensitive) == 0
        && lhs.username.trimmed().compare(rhs.username.trimmed(), Qt::CaseInsensitive) == 0;
}

OutlookConnectionSettings loadFromSettings(QSettings& settings)
{
    OutlookConnectionSettings config;
    settings.beginGroup(QString::fromLatin1(kOutlookGroup));
    config.enabled = settings.value(QString::fromLatin1(kEnabledKey), false).toBool();
    config.serverUrl = settings.value(QString::fromLatin1(kServerUrlKey)).toString().trimmed();
    config.username = settings.value(QString::fromLatin1(kUsernameKey)).toString().trimmed();
    config.password = settings.value(QString::fromLatin1(kPasswordKey)).toString();
    config.accountEmail = settings.value(QString::fromLatin1(kAccountEmailKey)).toString().trimmed();
    config.displayName = settings.value(QString::fromLatin1(kDisplayNameKey)).toString().trimmed();
    config.notificationsEnabled =
        settings.value(QString::fromLatin1(kNotificationsEnabledKey), false).toBool();
    config.notificationPollIntervalMinutes = qMax(
        1, settings.value(QString::fromLatin1(kNotificationPollIntervalMinutesKey), 5).toInt());
    config.notificationConversationId =
        settings.value(QString::fromLatin1(kNotificationConversationIdKey)).toString().trimmed();
    config.notificationConversationTitle =
        settings.value(QString::fromLatin1(kNotificationConversationTitleKey)).toString().trimmed();
    config.recentMailIds = settings.value(QString::fromLatin1(kRecentMailIdsKey)).toStringList();
    config.recentEventIds = settings.value(QString::fromLatin1(kRecentEventIdsKey)).toStringList();
    config.lastPollAttemptAtMs = qMax<qint64>(
        0, settings.value(QString::fromLatin1(kLastPollAttemptAtMsKey), 0).toLongLong());
    config.lastPollSuccessAtMs = qMax<qint64>(
        0, settings.value(QString::fromLatin1(kLastPollSuccessAtMsKey), 0).toLongLong());
    config.lastPollErrorMessage =
        settings.value(QString::fromLatin1(kLastPollErrorMessageKey)).toString().trimmed();
    config.lastPollErrorCategory =
        settings.value(QString::fromLatin1(kLastPollErrorCategoryKey)).toString().trimmed();
    config.consecutivePollFailures =
        qMax(0, settings.value(QString::fromLatin1(kConsecutivePollFailuresKey), 0).toInt());
    settings.endGroup();
    return config;
}

void saveToSettings(const OutlookConnectionSettings& config, QSettings& settings)
{
    settings.beginGroup(QString::fromLatin1(kOutlookGroup));
    settings.setValue(QString::fromLatin1(kEnabledKey), config.enabled);
    settings.setValue(QString::fromLatin1(kServerUrlKey), config.serverUrl.trimmed());
    settings.setValue(QString::fromLatin1(kUsernameKey), config.username.trimmed());
    settings.setValue(QString::fromLatin1(kPasswordKey), config.password);
    settings.setValue(QString::fromLatin1(kAccountEmailKey), config.accountEmail.trimmed());
    settings.setValue(QString::fromLatin1(kDisplayNameKey), config.displayName.trimmed());
    settings.setValue(QString::fromLatin1(kNotificationsEnabledKey), config.notificationsEnabled);
    settings.setValue(QString::fromLatin1(kNotificationPollIntervalMinutesKey),
                      qMax(1, config.notificationPollIntervalMinutes));
    settings.setValue(QString::fromLatin1(kNotificationConversationIdKey),
                      config.notificationConversationId.trimmed());
    settings.setValue(QString::fromLatin1(kNotificationConversationTitleKey),
                      config.notificationConversationTitle.trimmed());
    settings.setValue(QString::fromLatin1(kRecentMailIdsKey), config.recentMailIds);
    settings.setValue(QString::fromLatin1(kRecentEventIdsKey), config.recentEventIds);
    settings.setValue(QString::fromLatin1(kLastPollAttemptAtMsKey),
                      qMax<qint64>(0, config.lastPollAttemptAtMs));
    settings.setValue(QString::fromLatin1(kLastPollSuccessAtMsKey),
                      qMax<qint64>(0, config.lastPollSuccessAtMs));
    settings.setValue(QString::fromLatin1(kLastPollErrorMessageKey),
                      config.lastPollErrorMessage.trimmed());
    settings.setValue(QString::fromLatin1(kLastPollErrorCategoryKey),
                      config.lastPollErrorCategory.trimmed());
    settings.setValue(QString::fromLatin1(kConsecutivePollFailuresKey),
                      qMax(0, config.consecutivePollFailures));
    settings.endGroup();
    settings.sync();
}

}  // namespace

bool OutlookConnectionSettings::hasCredentialConfiguration() const
{
    return !serverUrl.trimmed().isEmpty() && !username.trimmed().isEmpty();
}

bool OutlookConnectionSettings::hasRequiredConfiguration() const
{
    return enabled && hasCredentialConfiguration();
}

bool OutlookConnectionSettings::hasNotificationConfiguration() const
{
    return hasRequiredConfiguration()
        && notificationsEnabled
        && notificationPollIntervalMinutes > 0;
}

namespace OutlookSettingsStore {

OutlookConnectionSettings load(QSettings* settings)
{
    if (settings) {
        return loadFromSettings(*settings);
    }
    QSettings ownedSettings = AppSettings::createSettings();
    return loadFromSettings(ownedSettings);
}

void save(const OutlookConnectionSettings& config, QSettings* settings)
{
    if (settings) {
        saveToSettings(config, *settings);
        return;
    }
    QSettings ownedSettings = AppSettings::createSettings();
    saveToSettings(config, ownedSettings);
}

OutlookConnectionSettings mergePollState(const OutlookConnectionSettings& latestSettings,
                                         const OutlookConnectionSettings& polledSettings)
{
    OutlookConnectionSettings merged = latestSettings;
    merged.recentMailIds = polledSettings.recentMailIds;
    merged.recentEventIds = polledSettings.recentEventIds;
    merged.lastPollAttemptAtMs = qMax<qint64>(0, polledSettings.lastPollAttemptAtMs);
    merged.lastPollSuccessAtMs = qMax<qint64>(0, polledSettings.lastPollSuccessAtMs);
    merged.lastPollErrorMessage = polledSettings.lastPollErrorMessage.trimmed();
    merged.lastPollErrorCategory = polledSettings.lastPollErrorCategory.trimmed();
    merged.consecutivePollFailures = qMax(0, polledSettings.consecutivePollFailures);

    if (sameCredentialContext(latestSettings, polledSettings)) {
        if (merged.accountEmail.trimmed().isEmpty()) {
            merged.accountEmail = polledSettings.accountEmail.trimmed();
        }
        if (merged.displayName.trimmed().isEmpty()) {
            merged.displayName = polledSettings.displayName.trimmed();
        }
    }

    return merged;
}

QString summarizeErrorMessage(const QString& errorMessage, const QString& errorCategory)
{
    const QString simplified = errorMessage.simplified();
    if (simplified.isEmpty()) {
        if (errorCategory == QStringLiteral("auth")) {
            return QStringLiteral("认证失败");
        }
        if (errorCategory == QStringLiteral("network")) {
            return QStringLiteral("网络连接失败");
        }
        return QStringLiteral("连接失败");
    }

    const QString lowered = simplified.toLower();
    if (lowered.contains(QStringLiteral("timeout"))
        || lowered.contains(QStringLiteral("timed out"))
        || simplified.contains(QStringLiteral("超时"))) {
        return QStringLiteral("请求超时");
    }

    if (errorCategory == QStringLiteral("auth")
        || lowered.contains(QStringLiteral("401"))
        || lowered.contains(QStringLiteral("unauthorized"))
        || simplified.contains(QStringLiteral("认证"))
        || simplified.contains(QStringLiteral("密码"))) {
        return QStringLiteral("认证失败");
    }

    if (lowered.contains(QStringLiteral("soap"))
        || lowered.contains(QStringLiteral("fault"))
        || lowered.contains(QStringLiteral("schema"))
        || lowered.contains(QStringLiteral("responsecode"))
        || lowered.contains(QStringLiteral("exchange"))
        || lowered.contains(QStringLiteral("xml"))) {
        return QStringLiteral("服务返回异常响应");
    }

    if (errorCategory == QStringLiteral("network")) {
        return QStringLiteral("网络连接失败");
    }

    QString summary = simplified;
    if (summary.size() > 72) {
        summary = summary.left(69) + QStringLiteral("...");
    }
    return summary;
}

QString formatPollHealthSummary(const OutlookConnectionSettings& config, int nextPollMinutes)
{
    const auto formatTime = [](qint64 ms) {
        return ms > 0
            ? QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))
            : QStringLiteral("暂无");
    };

    QString categoryText = QStringLiteral("无");
    if (config.lastPollErrorCategory == QStringLiteral("auth")) {
        categoryText = QStringLiteral("认证");
    } else if (config.lastPollErrorCategory == QStringLiteral("network")) {
        categoryText = QStringLiteral("网络");
    } else if (!config.lastPollErrorCategory.trimmed().isEmpty()) {
        categoryText = QStringLiteral("其他");
    }

    return QStringLiteral(
               "最近成功：%1\n最近失败：%2\n错误类型：%3\n连续失败：%4 次\n下次轮询：约 %5 分钟后")
        .arg(formatTime(config.lastPollSuccessAtMs),
             config.lastPollErrorMessage.trimmed().isEmpty()
                 ? QStringLiteral("无")
                 : summarizeErrorMessage(config.lastPollErrorMessage, config.lastPollErrorCategory),
             categoryText,
             QString::number(qMax(0, config.consecutivePollFailures)),
             QString::number(qMax(1, nextPollMinutes)));
}

}  // namespace OutlookSettingsStore
