#include "services/RemoteMessageDeviceIdentity.h"

#include <QSettings>
#include <QUuid>

namespace {

constexpr auto kDeviceIdKey = "remoteChat/deviceId";

QString normalized(const QString& value)
{
    return value.trimmed();
}

QString newDeviceId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

}  // namespace

namespace RemoteMessageDeviceIdentity {

QString loadOrCreate(QSettings* settings,
                     const QString& legacyFallbackDeviceId)
{
    if (!settings) {
        return normalized(legacyFallbackDeviceId);
    }

    const QString stored =
        normalized(settings->value(QString::fromLatin1(kDeviceIdKey)).toString());
    if (!stored.isEmpty()) {
        return stored;
    }

    const QString generated = newDeviceId();
    settings->setValue(QString::fromLatin1(kDeviceIdKey), generated);
    settings->sync();
    return generated;
}

}  // namespace RemoteMessageDeviceIdentity
