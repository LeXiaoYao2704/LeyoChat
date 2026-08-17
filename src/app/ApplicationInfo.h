#pragma once

#include <QString>

namespace ApplicationInfo {

QString productName();
QString companyName();
QString currentVersion();
QString releaseNotesText();
bool shouldShowReleaseNotesOnStartup(const QString& previousVersion,
                                     const QString& currentVersion);

} // namespace ApplicationInfo
