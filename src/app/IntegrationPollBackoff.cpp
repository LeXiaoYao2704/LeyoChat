#include "app/IntegrationPollBackoff.h"

#include <QtGlobal>

QString classifyIntegrationErrorCategory(const QString& errorMessage)
{
    const QString normalized = errorMessage.trimmed().toLower();
    if (normalized.isEmpty()) {
        return QString();
    }
    for (const QString& token : {QStringLiteral("401"),
                                 QStringLiteral("403"),
                                 QStringLiteral("unauthorized"),
                                 QStringLiteral("forbidden"),
                                 QStringLiteral("token"),
                                 QStringLiteral("pat"),
                                 QStringLiteral("oauth"),
                                 QStringLiteral("refresh"),
                                 QStringLiteral("expired"),
                                 QStringLiteral("login")}) {
        if (normalized.contains(token)) {
            return QStringLiteral("auth");
        }
    }
    for (const QString& token : {QStringLiteral("timeout"),
                                 QStringLiteral("timed out"),
                                 QStringLiteral("network"),
                                 QStringLiteral("host"),
                                 QStringLiteral("ssl"),
                                 QStringLiteral("refused"),
                                 QStringLiteral("unreachable"),
                                 QStringLiteral("temporary"),
                                 QStringLiteral("connection")}) {
        if (normalized.contains(token)) {
            return QStringLiteral("network");
        }
    }
    return QStringLiteral("unknown");
}

int computeIntegrationPollIntervalMs(int baseMinutes,
                                     int consecutiveFailures,
                                     const QString& errorCategory)
{
    const qint64 baseMs = static_cast<qint64>(qMax(1, baseMinutes)) * 60 * 1000;
    qint64 nextMs = baseMs;
    if (consecutiveFailures > 0) {
        const int exponent = qMin(consecutiveFailures - 1, 3);
        const qint64 factor = static_cast<qint64>(1) << exponent;
        nextMs *= qMax<qint64>(1, factor);
    }
    if (errorCategory == QStringLiteral("auth")) {
        nextMs = qMax<qint64>(nextMs, 15 * 60 * 1000);
    }
    return static_cast<int>(qMin<qint64>(nextMs, 60 * 60 * 1000));
}
