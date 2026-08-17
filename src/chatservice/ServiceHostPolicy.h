#pragma once

#include <QList>
#include <QString>

namespace LeyoChatService {

struct ServiceLogRetentionPolicy {
    qint64 maxActiveLogBytes = 0;
    qint64 maxArchiveAgeMs = 0;
    int maxArchiveFilesPerLog = 0;
};

struct ServiceHostPaths {
    QString dataRoot;
    QString fileStoragePath;
    QString logDirectory;
    QString configPath;
    QString serviceExePath;
    QString stdoutLogPath;
    QString stderrLogPath;
};

class ServiceHostPolicy {
public:
    static ServiceHostPaths defaultPaths(const QString& programFilesDir,
                                         const QString& programDataDir);

    static QString childCommandLine(const QString& serviceExePath,
                                    const QString& configPath);

    static int restartDelayMs(int recentCrashCount);

    static bool shouldRestartAfterCrash(const QList<qint64>& crashTimesMs,
                                        qint64 nowMs);

    static ServiceLogRetentionPolicy defaultLogRetentionPolicy();

    static bool shouldRotateActiveLog(qint64 sizeBytes,
                                      const ServiceLogRetentionPolicy& policy);

    static bool shouldPruneArchivedLog(qint64 lastModifiedMs,
                                       qint64 nowMs,
                                       int sortedArchiveIndex,
                                       const ServiceLogRetentionPolicy& policy);
};

} // namespace LeyoChatService
