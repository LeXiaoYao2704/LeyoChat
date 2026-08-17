#pragma once

#include <QString>

QString classifyIntegrationErrorCategory(const QString& errorMessage);
int computeIntegrationPollIntervalMs(int baseMinutes,
                                     int consecutiveFailures,
                                     const QString& errorCategory);
