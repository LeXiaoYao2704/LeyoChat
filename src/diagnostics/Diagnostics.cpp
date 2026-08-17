// @AI-Generated: true
// @AI-Model: GitHub Copilot
// @Summary: 累计AI新增83行/修改1行/删除0行; 总行数472行
// @AI-LastModified: 2026-04-16 09:13:58

#include "diagnostics/Diagnostics.h"
#include "app/TestModeContext.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUuid>

namespace Diagnostics {
namespace {

QString copyFileInto(const QString& sourcePath, const QString& destinationPath)
{
    if (sourcePath.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return QStringLiteral("源文件不存在: %1").arg(sourcePath);
    }

    QDir().mkpath(QFileInfo(destinationPath).absolutePath());
    QFile::remove(destinationPath);
    if (!QFile::copy(sourcePath, destinationPath)) {
        return QStringLiteral("复制文件失败: %1").arg(sourcePath);
    }

    return {};
}

QString copyOptionalFileInto(const QString& sourcePath, const QString& destinationPath)
{
    if (sourcePath.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        QFile::remove(destinationPath);
        return {};
    }

    return copyFileInto(sourcePath, destinationPath);
}

QString sqliteQuotedLiteral(QString value)
{
    value = QDir::fromNativeSeparators(value);
    value.replace(QStringLiteral("'"), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(value);
}

QString snapshotDatabaseInto(const QString& sourcePath, const QString& destinationPath)
{
    if (sourcePath.trimmed().isEmpty()) {
        return {};
    }

    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
        return QStringLiteral("源文件不存在: %1").arg(sourcePath);
    }

    QDir().mkpath(QFileInfo(destinationPath).absolutePath());
    QFile::remove(destinationPath);

    const QString connectionName = QStringLiteral("leyochat-diagnostics-export-%1")
                                       .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QString sqliteError;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        if (!database.isValid()) {
            sqliteError = QStringLiteral("无法创建数据库快照连接");
        } else {
            database.setDatabaseName(sourcePath);
            if (!database.open()) {
                sqliteError = database.lastError().text().trimmed();
            } else {
                QSqlQuery timeoutQuery(database);
                timeoutQuery.exec(QStringLiteral("PRAGMA busy_timeout = 2000"));

                QSqlQuery snapshotQuery(database);
                const QString statement = QStringLiteral("VACUUM INTO %1")
                                              .arg(sqliteQuotedLiteral(destinationPath));
                if (!snapshotQuery.exec(statement)) {
                    sqliteError = snapshotQuery.lastError().text().trimmed();
                }
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);

    if (sqliteError.isEmpty() && QFileInfo::exists(destinationPath)) {
        return {};
    }

    const QString fallbackError = copyFileInto(sourcePath, destinationPath);
    if (fallbackError.isEmpty()) {
        static const QStringList sidecarSuffixes = {
            QStringLiteral("-wal"),
            QStringLiteral("-shm"),
            QStringLiteral("-journal")
        };
        for (const QString& suffix : sidecarSuffixes) {
            const QString sidecarError = copyOptionalFileInto(sourcePath + suffix,
                                                              destinationPath + suffix);
            if (!sidecarError.isEmpty()) {
                return sidecarError;
            }
        }
        return {};
    }
    if (!sqliteError.isEmpty()) {
        return QStringLiteral("复制数据库失败: %1").arg(sqliteError);
    }
    return fallbackError;
}

QString copyDirectoryRecursively(const QString& sourceDirPath, const QString& destinationDirPath)
{
    if (sourceDirPath.trimmed().isEmpty()) {
        return {};
    }

    const QDir sourceDir(sourceDirPath);
    if (!sourceDir.exists()) {
        return {};
    }

    QDir().mkpath(destinationDirPath);
    for (const QFileInfo& entry : sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        const QString destinationPath = QDir(destinationDirPath).filePath(entry.fileName());
        if (entry.isDir()) {
            const QString nestedError = copyDirectoryRecursively(entry.absoluteFilePath(), destinationPath);
            if (!nestedError.isEmpty()) {
                return nestedError;
            }
            continue;
        }

        const QString fileError = copyFileInto(entry.absoluteFilePath(), destinationPath);
        if (!fileError.isEmpty()) {
            return fileError;
        }
    }

    return {};
}

QJsonObject buildEnvironmentObject(const BundleSourcePaths& sourcePaths)
{
    QJsonObject object;
    object.insert(QStringLiteral("appDisplayName"), QStringLiteral("LeyoChat"));
    object.insert(QStringLiteral("organizationName"), QCoreApplication::organizationName());
    object.insert(QStringLiteral("applicationName"), QCoreApplication::applicationName());
    object.insert(QStringLiteral("applicationVersion"), QCoreApplication::applicationVersion());
    object.insert(QStringLiteral("generatedAt"),
                  QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("qtVersion"), QString::fromLatin1(qVersion()));
    object.insert(QStringLiteral("productType"), QSysInfo::productType());
    object.insert(QStringLiteral("productVersion"), QSysInfo::productVersion());
    object.insert(QStringLiteral("prettyProductName"), QSysInfo::prettyProductName());
    object.insert(QStringLiteral("cpuArchitecture"), QSysInfo::currentCpuArchitecture());
    object.insert(QStringLiteral("databasePresent"), QFileInfo::exists(sourcePaths.databasePath));
    object.insert(QStringLiteral("appDataDir"), sourcePaths.appDataDir);
    object.insert(QStringLiteral("appLocalDataDir"), sourcePaths.appLocalDataDir);
    object.insert(QStringLiteral("databasePath"), sourcePaths.databasePath);
    object.insert(QStringLiteral("logsDir"), sourcePaths.logsDir);
    object.insert(QStringLiteral("crashDir"), sourcePaths.crashDir);
    object.insert(QStringLiteral("screenshotsDir"), sourcePaths.screenshotsDir);
    object.insert(QStringLiteral("runtimeDir"), sourcePaths.runtimeDir);
    object.insert(QStringLiteral("runtimeSnapshotPresent"),
                  QFileInfo::exists(QDir(sourcePaths.runtimeDir).filePath(QStringLiteral("stage2-snapshot.json"))));
    return object;
}

QJsonObject capabilityObject(const ServiceCapability& capability)
{
    QJsonObject object;
    object.insert(QStringLiteral("capabilityId"), capability.capabilityId);
    object.insert(QStringLiteral("capabilityName"), capability.capabilityName);
    object.insert(QStringLiteral("version"), capability.version);
    object.insert(QStringLiteral("enabled"), capability.enabled);
    return object;
}

QJsonObject serviceObject(const ServiceDiscoverySnapshot& snapshot)
{
    QJsonArray capabilities;
    for (const ServiceCapability& capability : snapshot.capabilities) {
        capabilities.push_back(capabilityObject(capability));
    }

    QJsonObject object;
    object.insert(QStringLiteral("serviceId"), snapshot.serviceId);
    object.insert(QStringLiteral("serviceName"), snapshot.serviceName);
    object.insert(QStringLiteral("organizationName"), snapshot.organizationName);
    object.insert(QStringLiteral("environmentName"), snapshot.environmentName);
    object.insert(QStringLiteral("observedAtMs"), QString::number(snapshot.observedAtMs));
    object.insert(QStringLiteral("capabilities"), capabilities);
    return object;
}

QJsonObject snapshotObject(const RuntimeArchitectureSnapshot& snapshot)
{
    QJsonArray services;
    for (const ServiceDiscoverySnapshot& service : snapshot.discoveryResult.services) {
        services.push_back(serviceObject(service));
    }

    QJsonObject object;
    object.insert(QStringLiteral("generatedAt"),
                  QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("defaultServiceId"), snapshot.discoveryResult.defaultServiceId);
    object.insert(QStringLiteral("multipleServicesDetected"),
                  snapshot.discoveryResult.multipleServicesDetected);
    object.insert(QStringLiteral("services"), services);
    object.insert(QStringLiteral("registryCount"), snapshot.serviceRegistry.size());
    object.insert(QStringLiteral("workspaceBindingCount"), snapshot.workspaceBindings.size());
    object.insert(QStringLiteral("groupBindingCount"), snapshot.groupBindings.size());
    object.insert(QStringLiteral("visibleResourceCount"), snapshot.visibleResources.size());
    object.insert(QStringLiteral("bound"), snapshot.selection.bound);
    object.insert(QStringLiteral("workspaceId"), snapshot.selection.workspaceId);
    object.insert(QStringLiteral("groupId"), snapshot.selection.groupId);
    object.insert(QStringLiteral("serviceId"), snapshot.selection.serviceId);
    object.insert(QStringLiteral("serviceName"), snapshot.selection.serviceName);
    object.insert(QStringLiteral("selectionSource"), snapshot.selection.selectionSource);
    object.insert(QStringLiteral("selectedResourceId"), snapshot.selection.selectedResource.resourceId);
    return object;
}

QJsonObject azureDevOpsTargetObject(const AzureDevOpsNotificationTarget& target)
{
    QJsonObject object;
    object.insert(QStringLiteral("organization"), target.organization);
    object.insert(QStringLiteral("project"), target.project);
    object.insert(QStringLiteral("enabled"), target.enabled);
    object.insert(QStringLiteral("lastNotifiedBuildId"), target.lastNotifiedBuildId);
    object.insert(QStringLiteral("lastNotifiedPullRequestUpdatedAtMs"),
                  QString::number(target.lastNotifiedPullRequestUpdatedAtMs));
    object.insert(QStringLiteral("lastNotifiedAssignedWorkItemUpdatedAtMs"),
                  QString::number(target.lastNotifiedAssignedWorkItemUpdatedAtMs));
    object.insert(QStringLiteral("lastNotifiedBuildResult"), target.lastNotifiedBuildResult);
    object.insert(QStringLiteral("lastPollAttemptAtMs"), QString::number(target.lastPollAttemptAtMs));
    object.insert(QStringLiteral("lastPollSuccessAtMs"), QString::number(target.lastPollSuccessAtMs));
    object.insert(QStringLiteral("lastPollErrorMessage"), target.lastPollErrorMessage);
    object.insert(QStringLiteral("lastPollErrorCategory"), target.lastPollErrorCategory);
    object.insert(QStringLiteral("consecutivePollFailures"), target.consecutivePollFailures);
    return object;
}

QString azureDevOpsTargetSummary(const AzureDevOpsNotificationTarget& target)
{
    const QString organization = target.organization.trimmed();
    const QString project = target.project.trimmed();
    if (!organization.isEmpty() && !project.isEmpty()) {
        return QStringLiteral("%1 / %2").arg(organization, project);
    }
    return !organization.isEmpty() ? organization : project;
}

QJsonObject azureDevOpsSettingsObject(const AzureDevOpsConnectionSettings& settings)
{
    QJsonArray targets;
    QJsonArray enabledTargetSummaries;
    int enabledTargetCount = 0;
    for (const AzureDevOpsNotificationTarget& target : settings.notificationTargets) {
        targets.push_back(azureDevOpsTargetObject(target));
        if (target.enabled) {
            ++enabledTargetCount;
            enabledTargetSummaries.push_back(azureDevOpsTargetSummary(target));
        }
    }

    QJsonObject object;
    object.insert(QStringLiteral("enabled"), settings.enabled);
    object.insert(QStringLiteral("baseUrl"), settings.baseUrl);
    object.insert(QStringLiteral("organization"), settings.organization);
    object.insert(QStringLiteral("project"), settings.project);
    object.insert(QStringLiteral("notificationsEnabled"), settings.notificationsEnabled);
    object.insert(QStringLiteral("notificationPollIntervalMinutes"), settings.notificationPollIntervalMinutes);
    object.insert(QStringLiteral("notificationTargetCount"), settings.notificationTargets.size());
    object.insert(QStringLiteral("enabledNotificationTargetCount"), enabledTargetCount);
    AzureDevOpsNotificationTarget defaultTarget;
    defaultTarget.organization = settings.organization;
    defaultTarget.project = settings.project;
    defaultTarget.enabled = true;
    object.insert(QStringLiteral("defaultNotificationTarget"),
                  azureDevOpsTargetSummary(defaultTarget));
    object.insert(QStringLiteral("enabledTargetSummaries"), enabledTargetSummaries);
    object.insert(QStringLiteral("lastPollAttemptAtMs"), QString::number(settings.lastPollAttemptAtMs));
    object.insert(QStringLiteral("lastPollSuccessAtMs"), QString::number(settings.lastPollSuccessAtMs));
    object.insert(QStringLiteral("lastPollErrorMessage"), settings.lastPollErrorMessage);
    object.insert(QStringLiteral("lastPollErrorCategory"), settings.lastPollErrorCategory);
    object.insert(QStringLiteral("consecutivePollFailures"), settings.consecutivePollFailures);
    object.insert(QStringLiteral("targets"), targets);
    return object;
}

QJsonObject outlookSettingsObject(const OutlookConnectionSettings& settings)
{
    QJsonObject object;
    object.insert(QStringLiteral("enabled"), settings.enabled);
    object.insert(QStringLiteral("serverUrl"), settings.serverUrl);
    object.insert(QStringLiteral("accountEmail"), settings.accountEmail);
    object.insert(QStringLiteral("displayName"), settings.displayName);
    object.insert(QStringLiteral("notificationsEnabled"), settings.notificationsEnabled);
    object.insert(QStringLiteral("notificationPollIntervalMinutes"), settings.notificationPollIntervalMinutes);
    object.insert(QStringLiteral("hasCredentialConfiguration"), settings.hasCredentialConfiguration());
    object.insert(QStringLiteral("authorizationState"),
                  settings.hasCredentialConfiguration()
                      ? QStringLiteral("configured")
                      : QStringLiteral("unconfigured"));
    object.insert(QStringLiteral("recentMailCount"), settings.recentMailIds.size());
    object.insert(QStringLiteral("recentEventCount"), settings.recentEventIds.size());
    object.insert(QStringLiteral("lastPollAttemptAtMs"), QString::number(settings.lastPollAttemptAtMs));
    object.insert(QStringLiteral("lastPollSuccessAtMs"), QString::number(settings.lastPollSuccessAtMs));
    object.insert(QStringLiteral("lastPollErrorMessage"), settings.lastPollErrorMessage);
    object.insert(QStringLiteral("lastPollErrorCategory"), settings.lastPollErrorCategory);
    object.insert(QStringLiteral("consecutivePollFailures"), settings.consecutivePollFailures);
    return object;
}

}  // namespace

BundleSourcePaths defaultSourcePaths()
{
    const TestModeContext testModeContext = TestModeContext::current();
    BundleSourcePaths source;
    source.appDataDir = testModeContext.appDataRoot();
    if (source.appDataDir.isEmpty()) {
        source.appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    }
    source.appLocalDataDir = testModeContext.appLocalDataRoot();
    if (source.appLocalDataDir.isEmpty()) {
        source.appLocalDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    source.databasePath = !testModeContext.databasePath().isEmpty()
                              ? testModeContext.databasePath()
                              : source.appDataDir.isEmpty()
                              ? QStringLiteral("leyochat.db")
                              : QDir(source.appDataDir).filePath(QStringLiteral("leyochat.db"));
    source.logsDir = !testModeContext.logsDirectoryPath().isEmpty()
                         ? testModeContext.logsDirectoryPath()
                         : source.appLocalDataDir.isEmpty()
                         ? QStringLiteral("logs")
                         : QDir(source.appLocalDataDir).filePath(QStringLiteral("logs"));
    source.crashDir = !testModeContext.crashDirectoryPath().isEmpty()
                          ? testModeContext.crashDirectoryPath()
                          : source.appLocalDataDir.isEmpty()
                          ? QStringLiteral("crash")
                          : QDir(source.appLocalDataDir).filePath(QStringLiteral("crash"));
    source.screenshotsDir = !testModeContext.screenshotsDirectoryPath().isEmpty()
                                ? testModeContext.screenshotsDirectoryPath()
                                : source.appLocalDataDir.isEmpty()
                                ? QStringLiteral("screenshots")
                                : QDir(source.appLocalDataDir).filePath(QStringLiteral("screenshots"));
    source.runtimeDir = !testModeContext.runtimeDirectoryPath().isEmpty()
                            ? testModeContext.runtimeDirectoryPath()
                            : source.appLocalDataDir.isEmpty()
                            ? QStringLiteral("runtime")
                            : QDir(source.appLocalDataDir).filePath(QStringLiteral("runtime"));
    return source;
}

QString defaultExportBaseName()
{
    return QStringLiteral("LeyoChat-diagnostics-%1")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
}

bool writeRuntimeArchitectureSnapshot(const BundleSourcePaths& sourcePaths,
                                      const RuntimeArchitectureSnapshot& snapshot)
{
    const QString runtimeDir = sourcePaths.runtimeDir.trimmed();
    if (runtimeDir.isEmpty()) {
        return false;
    }

    QDir().mkpath(runtimeDir);
    QFile runtimeFile(QDir(runtimeDir).filePath(QStringLiteral("stage2-snapshot.json")));
    if (!runtimeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    runtimeFile.write(QJsonDocument(snapshotObject(snapshot)).toJson(QJsonDocument::Indented));
    runtimeFile.close();
    return true;
}

bool writeIntegrationSnapshot(const BundleSourcePaths& sourcePaths,
                              const AzureDevOpsConnectionSettings& azureDevOpsSettings,
                              const OutlookConnectionSettings& outlookSettings)
{
    const QString runtimeDir = sourcePaths.runtimeDir.trimmed();
    if (runtimeDir.isEmpty()) {
        return false;
    }

    QDir().mkpath(runtimeDir);
    QFile runtimeFile(QDir(runtimeDir).filePath(QStringLiteral("integration-snapshot.json")));
    if (!runtimeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("generatedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    root.insert(QStringLiteral("azureDevOps"), azureDevOpsSettingsObject(azureDevOpsSettings));
    root.insert(QStringLiteral("outlook"), outlookSettingsObject(outlookSettings));
    runtimeFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    runtimeFile.close();
    return true;
}

bool exportBundle(const BundleSourcePaths& sourcePaths,
                  const QString& targetRootDir,
                  QString* exportedDir,
                  QString* errorMessage)
{
    if (exportedDir) {
        exportedDir->clear();
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    const QString trimmedTargetRoot = targetRootDir.trimmed();
    if (trimmedTargetRoot.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("未选择导出目录");
        }
        return false;
    }

    QDir rootDir(trimmedTargetRoot);
    if (!rootDir.exists() && !QDir().mkpath(trimmedTargetRoot)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建导出目录");
        }
        return false;
    }

    const QString bundleDirPath = rootDir.filePath(defaultExportBaseName());
    if (!QDir().mkpath(bundleDirPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建诊断包目录");
        }
        return false;
    }

    const QString databaseError =
        snapshotDatabaseInto(sourcePaths.databasePath,
                             QDir(bundleDirPath).filePath(QStringLiteral("leyochat.db")));
    if (!databaseError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = databaseError;
        }
        return false;
    }

    const QString logsError =
        copyDirectoryRecursively(sourcePaths.logsDir, QDir(bundleDirPath).filePath(QStringLiteral("logs")));
    if (!logsError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = logsError;
        }
        return false;
    }

    const QString crashError =
        copyDirectoryRecursively(sourcePaths.crashDir, QDir(bundleDirPath).filePath(QStringLiteral("crash")));
    if (!crashError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = crashError;
        }
        return false;
    }

    const QString screenshotError = copyDirectoryRecursively(
        sourcePaths.screenshotsDir, QDir(bundleDirPath).filePath(QStringLiteral("screenshots")));
    if (!screenshotError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = screenshotError;
        }
        return false;
    }

    const QString runtimeError =
        copyDirectoryRecursively(sourcePaths.runtimeDir, QDir(bundleDirPath).filePath(QStringLiteral("runtime")));
    if (!runtimeError.isEmpty()) {
        if (errorMessage) {
            *errorMessage = runtimeError;
        }
        return false;
    }

    QFile environmentFile(QDir(bundleDirPath).filePath(QStringLiteral("environment.json")));
    if (!environmentFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法写入环境信息");
        }
        return false;
    }

    environmentFile.write(QJsonDocument(buildEnvironmentObject(sourcePaths)).toJson(QJsonDocument::Indented));
    environmentFile.close();

    if (exportedDir) {
        *exportedDir = bundleDirPath;
    }
    return true;
}

}  // namespace Diagnostics
