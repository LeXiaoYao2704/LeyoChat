#include "app/ApplicationInfo.h"

#include "AppBuildInfo.h"

#include <QFile>
#include <QTextStream>

namespace {

QString loadUtf8Text(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    return stream.readAll().trimmed();
}

} // namespace

namespace ApplicationInfo {

QString productName()
{
    return QStringLiteral(LEYOCHAT_PRODUCT_NAME);
}

QString companyName()
{
    return QStringLiteral(LEYOCHAT_COMPANY_NAME);
}

QString currentVersion()
{
    return QStringLiteral(LEYOCHAT_APP_VERSION);
}

QString releaseNotesText()
{
    return loadUtf8Text(QStringLiteral(":/docs/release-notes/current.txt"));
}

bool shouldShowReleaseNotesOnStartup(const QString& previousVersion,
                                     const QString& currentVersion)
{
    const QString previous = previousVersion.trimmed();
    const QString current = currentVersion.trimmed();
    if (current.isEmpty()) {
        return false;
    }
    if (previous.isEmpty()) {
        return false;
    }
    return previous != current;
}

} // namespace ApplicationInfo
