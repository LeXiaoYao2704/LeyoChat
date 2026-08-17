#pragma once

#include <QSettings>
#include <QString>
#include <QStringList>

struct OutlookConnectionSettings {
    bool enabled = false;
    QString serverUrl;                          // e.g. https://mail.example.com
    QString username;                           // e.g. testuser
    QString password;                           // stored in QSettings (no token cache needed)
    QString accountEmail;                       // filled after a successful testConnection
    QString displayName;                        // filled after a successful testConnection
    bool notificationsEnabled = false;
    int notificationPollIntervalMinutes = 5;
    QString notificationConversationId;
    QString notificationConversationTitle;
    QStringList recentMailIds;
    QStringList recentEventIds;
    qint64 lastPollAttemptAtMs = 0;
    qint64 lastPollSuccessAtMs = 0;
    QString lastPollErrorMessage;
    QString lastPollErrorCategory;
    int consecutivePollFailures = 0;

    bool hasCredentialConfiguration() const;   // serverUrl + username non-empty
    bool hasRequiredConfiguration() const;     // enabled && hasCredentialConfiguration()
    bool hasNotificationConfiguration() const; // hasRequiredConfiguration() + notif enabled
};

namespace OutlookSettingsStore {

OutlookConnectionSettings load(QSettings* settings = nullptr);
void save(const OutlookConnectionSettings& config, QSettings* settings = nullptr);
OutlookConnectionSettings mergePollState(const OutlookConnectionSettings& latestSettings,
                                         const OutlookConnectionSettings& polledSettings);
QString summarizeErrorMessage(const QString& errorMessage,
                              const QString& errorCategory = QString());
QString formatPollHealthSummary(const OutlookConnectionSettings& config, int nextPollMinutes);

}  // namespace OutlookSettingsStore
