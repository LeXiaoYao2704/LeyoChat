#include "chatservice/ServiceHostPolicy.h"

#include <QDir>

namespace {
constexpr int kFirstRestartDelayMs = 5000;
constexpr int kSecondRestartDelayMs = 30000;
constexpr int kSteadyRestartDelayMs = 60000;
constexpr qint64 kCrashWindowMs = 5 * 60 * 1000;
constexpr int kMaxCrashesPerWindow = 10;
constexpr qint64 kMaxActiveLogBytes = 10 * 1024 * 1024;
constexpr qint64 kMaxArchiveAgeMs = qint64(14) * 24 * 60 * 60 * 1000;
constexpr int kMaxArchiveFilesPerLog = 8;

QString joinPath(const QString& left, const QString& right)
{
    QDir dir(left);
    return QDir::cleanPath(dir.filePath(right));
}

QString quoteWindowsArgument(QString value)
{
    value.replace(QStringLiteral("\""), QStringLiteral("\\\""));
    return QStringLiteral("\"%1\"").arg(value);
}
} // namespace

namespace LeyoChatService {

ServiceHostPaths ServiceHostPolicy::defaultPaths(const QString& programFilesDir,
                                                 const QString& programDataDir)
{
    ServiceHostPaths paths;
    paths.dataRoot = joinPath(programDataDir, QStringLiteral("LeyoChat/Service"));
    paths.fileStoragePath = joinPath(paths.dataRoot, QStringLiteral("files"));
    paths.logDirectory = joinPath(paths.dataRoot, QStringLiteral("logs"));
    paths.configPath = joinPath(paths.dataRoot,
                                QStringLiteral("leyochat-service.json"));
    paths.serviceExePath = joinPath(programFilesDir,
                                    QStringLiteral("LeyoChatService.exe"));
    paths.stdoutLogPath =
        joinPath(paths.logDirectory, QStringLiteral("LeyoChatService.stdout.log"));
    paths.stderrLogPath =
        joinPath(paths.logDirectory, QStringLiteral("LeyoChatService.stderr.log"));
    return paths;
}

QString ServiceHostPolicy::childCommandLine(const QString& serviceExePath,
                                            const QString& configPath)
{
    return quoteWindowsArgument(serviceExePath)
        + QStringLiteral(" --config ")
        + quoteWindowsArgument(configPath);
}

int ServiceHostPolicy::restartDelayMs(int recentCrashCount)
{
    if (recentCrashCount <= 0)
        return kFirstRestartDelayMs;
    if (recentCrashCount == 1)
        return kSecondRestartDelayMs;
    return kSteadyRestartDelayMs;
}

bool ServiceHostPolicy::shouldRestartAfterCrash(const QList<qint64>& crashTimesMs,
                                                qint64 nowMs)
{
    int recentCrashes = 0;
    const qint64 windowStartMs = nowMs - kCrashWindowMs;
    for (const qint64 crashTimeMs : crashTimesMs) {
        if (crashTimeMs >= windowStartMs && crashTimeMs <= nowMs)
            ++recentCrashes;
    }
    return recentCrashes < kMaxCrashesPerWindow;
}

ServiceLogRetentionPolicy ServiceHostPolicy::defaultLogRetentionPolicy()
{
    ServiceLogRetentionPolicy policy;
    policy.maxActiveLogBytes = kMaxActiveLogBytes;
    policy.maxArchiveAgeMs = kMaxArchiveAgeMs;
    policy.maxArchiveFilesPerLog = kMaxArchiveFilesPerLog;
    return policy;
}

bool ServiceHostPolicy::shouldRotateActiveLog(
    qint64 sizeBytes,
    const ServiceLogRetentionPolicy& policy)
{
    return policy.maxActiveLogBytes > 0 && sizeBytes > policy.maxActiveLogBytes;
}

bool ServiceHostPolicy::shouldPruneArchivedLog(
    qint64 lastModifiedMs,
    qint64 nowMs,
    int sortedArchiveIndex,
    const ServiceLogRetentionPolicy& policy)
{
    if (policy.maxArchiveAgeMs > 0
        && lastModifiedMs < nowMs - policy.maxArchiveAgeMs) {
        return true;
    }
    return policy.maxArchiveFilesPerLog >= 0
        && sortedArchiveIndex >= policy.maxArchiveFilesPerLog;
}

} // namespace LeyoChatService
