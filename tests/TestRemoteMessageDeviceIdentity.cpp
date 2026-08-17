#include <QtTest/QTest>

#include <QRegularExpression>
#include <QSettings>
#include <QTemporaryDir>

#include "services/RemoteMessageDeviceIdentity.h"

class TestRemoteMessageDeviceIdentity : public QObject {
    Q_OBJECT

private slots:
    void loadOrCreate_returnsPersistedDeviceId()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QSettings settings(dir.filePath(QStringLiteral("identity.ini")),
                           QSettings::IniFormat);
        settings.setValue(QStringLiteral("remoteChat/deviceId"),
                          QStringLiteral("  pc-existing  "));

        const QString deviceId =
            RemoteMessageDeviceIdentity::loadOrCreate(&settings);

        QCOMPARE(deviceId, QStringLiteral("pc-existing"));
        QCOMPARE(settings.value(QStringLiteral("remoteChat/deviceId")).toString(),
                 QStringLiteral("  pc-existing  "));
    }

    void loadOrCreate_generatesAndStoresStableUuidWhenMissing()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QSettings settings(dir.filePath(QStringLiteral("identity.ini")),
                           QSettings::IniFormat);

        const QString first =
            RemoteMessageDeviceIdentity::loadOrCreate(&settings);
        const QString second =
            RemoteMessageDeviceIdentity::loadOrCreate(&settings);

        QVERIFY(!first.isEmpty());
        QCOMPARE(second, first);
        QCOMPARE(settings.value(QStringLiteral("remoteChat/deviceId")).toString(),
                 first);
        QVERIFY(QRegularExpression(
                    QStringLiteral("^[0-9a-fA-F-]{36}$")).match(first).hasMatch());
    }

    void loadOrCreate_replacesBlankValueInsteadOfUsingClientId()
    {
        QTemporaryDir dir; QVERIFY(dir.isValid());
        QSettings settings(dir.filePath(QStringLiteral("identity.ini")),
                           QSettings::IniFormat);
        settings.setValue(QStringLiteral("remoteChat/deviceId"), QStringLiteral("   "));

        const QString deviceId =
            RemoteMessageDeviceIdentity::loadOrCreate(
                &settings, QStringLiteral("local-client-id"));

        QVERIFY(!deviceId.isEmpty());
        QVERIFY(deviceId != QStringLiteral("local-client-id"));
        QCOMPARE(settings.value(QStringLiteral("remoteChat/deviceId")).toString(),
                 deviceId);
    }

    void loadOrCreate_usesLegacyFallbackOnlyWithoutSettings()
    {
        QCOMPARE(RemoteMessageDeviceIdentity::loadOrCreate(
                     nullptr, QStringLiteral("legacy-device")),
                 QStringLiteral("legacy-device"));
    }
};

QTEST_MAIN(TestRemoteMessageDeviceIdentity)
#include "TestRemoteMessageDeviceIdentity.moc"
