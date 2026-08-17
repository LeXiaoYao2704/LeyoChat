#include "app/AppSettings.h"

#include <QCoreApplication>

namespace {

constexpr auto kDefaultOrganizationName = "LeyoChat";
constexpr auto kDefaultApplicationName = "LeyoChat";
constexpr auto kPropertyWindowTitleSuffix = "leyochat.devTest.windowTitleSuffix";

}

namespace AppSettings {

QString organizationName()
{
    return qApp && !qApp->organizationName().trimmed().isEmpty()
        ? qApp->organizationName().trimmed()
        : QString::fromLatin1(kDefaultOrganizationName);
}

QString applicationName()
{
    return qApp && !qApp->applicationName().trimmed().isEmpty()
        ? qApp->applicationName().trimmed()
        : QString::fromLatin1(kDefaultApplicationName);
}

QSettings createSettings()
{
    return QSettings(organizationName(), applicationName());
}

QString windowTitle(const QString& baseTitle)
{
    const QString suffix = qApp
        ? qApp->property(kPropertyWindowTitleSuffix).toString().trimmed()
        : QString();
    return suffix.isEmpty() ? baseTitle : QStringLiteral("%1 %2").arg(baseTitle, suffix);
}

}  // namespace AppSettings
