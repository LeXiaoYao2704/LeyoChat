#pragma once

#include <QString>

#include "architecture/RuntimeArchitectureSnapshot.h"
#include "integrations/AzureDevOpsSettings.h"
#include "integrations/OutlookSettings.h"

namespace Diagnostics {

struct BundleSourcePaths {
    QString appDataDir;
    QString appLocalDataDir;
    QString databasePath;
    QString logsDir;
    QString crashDir;
    QString screenshotsDir;
    QString runtimeDir;
};

BundleSourcePaths defaultSourcePaths();
QString defaultExportBaseName();
bool writeRuntimeArchitectureSnapshot(const BundleSourcePaths& sourcePaths,
                                      const RuntimeArchitectureSnapshot& snapshot);
bool writeIntegrationSnapshot(const BundleSourcePaths& sourcePaths,
                              const AzureDevOpsConnectionSettings& azureDevOpsSettings,
                              const OutlookConnectionSettings& outlookSettings);
bool exportBundle(const BundleSourcePaths& sourcePaths,
                  const QString& targetRootDir,
                  QString* exportedDir,
                  QString* errorMessage);

}  // namespace Diagnostics
