#pragma once

#include <QString>

namespace IntegrationNotificationPresentation {

QString trayUnreadToolTip(const QString& appDisplayName, int unreadCount);
QString trayUnreadStatusActionText(const QString& lastUnreadTitle, int unreadCount);

QString integrationFailureTitle(const QString& sourceLabel, const QString& category);
QString integrationFailureSummary(const QString& sourceLabel,
                                  const QString& category,
                                  int consecutiveFailures,
                                  int nextPollMinutes);
QString integrationRecoveryTitle(const QString& sourceLabel);
QString integrationRecoverySummary(const QString& sourceLabel);

}  // namespace IntegrationNotificationPresentation
