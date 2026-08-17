#pragma once

#include <QString>

class QSettings;

namespace RemoteMessageDeviceIdentity {

QString loadOrCreate(QSettings* settings,
                     const QString& legacyFallbackDeviceId = {});

}  // namespace RemoteMessageDeviceIdentity
