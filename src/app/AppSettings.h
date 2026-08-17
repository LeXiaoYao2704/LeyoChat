#pragma once

#include <QSettings>
#include <QString>

namespace AppSettings {

QString organizationName();
QString applicationName();
QSettings createSettings();
QString windowTitle(const QString& baseTitle);

}  // namespace AppSettings
