#include "app/TestModeContext.h"

#include <QDir>
#include <QRegularExpression>
#include <QVariant>

namespace {

constexpr auto kDefaultOrganizationName = "LeyoChat";
constexpr auto kDefaultApplicationName = "LeyoChat";
constexpr auto kDevOrganizationName = "LeyoChatDevTest";
constexpr auto kPropertyEnabled = "leyochat.devTest.enabled";
constexpr auto kPropertyProfile = "leyochat.devTest.profile";
constexpr auto kPropertyDataRoot = "leyochat.devTest.dataRoot";
constexpr auto kPropertyListenPort = "leyochat.devTest.listenPort";
constexpr auto kPropertyClientId = "leyochat.devTest.clientId";
constexpr auto kPropertyDisplayName = "leyochat.devTest.displayName";
constexpr auto kPropertyWindowTitleSuffix = "leyochat.devTest.windowTitleSuffix";

QString normalizedKeySegment(const QString& value)
{
    QString normalized = value.trimmed();
    normalized.replace(QRegularExpression(QStringLiteral(R"([^A-Za-z0-9._-])")),
                       QStringLiteral("-"));
    normalized.remove(QRegularExpression(QStringLiteral(R"(-{2,})")));
    normalized = normalized.trimmed();
    if (normalized.isEmpty()) {
        normalized = QStringLiteral("default");
    }
    return normalized;
}

QString argumentValue(const QStringList& arguments, const QString& name)
{
    const QString prefix = name + QLatin1Char('=');
    for (int index = 0; index < arguments.size(); ++index) {
        const QString argument = arguments.at(index);
        if (argument == name && index + 1 < arguments.size()) {
            return arguments.at(index + 1).trimmed();
        }
        if (argument.startsWith(prefix)) {
            return argument.mid(prefix.size()).trimmed();
        }
    }
    return {};
}

QString joinPath(const QString& baseRoot, const QString& first, const QString& second = QString())
{
    if (baseRoot.trimmed().isEmpty()) {
        return {};
    }

    QDir directory(QDir::fromNativeSeparators(baseRoot));
    QString path = directory.filePath(first);
    if (!second.isEmpty()) {
        path = QDir(path).filePath(second);
    }
    return QDir::cleanPath(path);
}

}  // namespace

TestModeContext TestModeContext::fromArguments(const QStringList& arguments)
{
    TestModeContext context;
    context.profile = argumentValue(arguments, QStringLiteral("--dev-test-profile"));
    context.dataRoot = argumentValue(arguments, QStringLiteral("--dev-test-data-root"));
    context.clientId = argumentValue(arguments, QStringLiteral("--dev-test-client-id"));
    context.displayName = argumentValue(arguments, QStringLiteral("--dev-test-display-name"));

    bool ok = false;
    const auto parsedPort = argumentValue(arguments, QStringLiteral("--dev-test-port")).toUInt(&ok);
    context.listenPort = ok && parsedPort <= 65535U ? static_cast<quint16>(parsedPort) : 0;

    context.profile = normalizedKeySegment(context.profile);
    context.dataRoot = QDir::cleanPath(QDir::fromNativeSeparators(context.dataRoot.trimmed()));
    context.clientId = context.clientId.trimmed();
    context.displayName = context.displayName.trimmed();

    context.enabled = !context.profile.isEmpty()
        && !context.dataRoot.isEmpty()
        && !context.clientId.isEmpty()
        && !context.displayName.isEmpty()
        && context.listenPort != 0;

    if (!context.enabled) {
        return {};
    }

    return context;
}

TestModeContext TestModeContext::current()
{
    if (!qApp || !qApp->property(kPropertyEnabled).toBool()) {
        return {};
    }

    TestModeContext context;
    context.enabled = true;
    context.profile = qApp->property(kPropertyProfile).toString().trimmed();
    context.dataRoot = QDir::cleanPath(
        QDir::fromNativeSeparators(qApp->property(kPropertyDataRoot).toString().trimmed()));
    context.listenPort = static_cast<quint16>(qApp->property(kPropertyListenPort).toUInt());
    context.clientId = qApp->property(kPropertyClientId).toString().trimmed();
    context.displayName = qApp->property(kPropertyDisplayName).toString().trimmed();
    return context;
}

void TestModeContext::applyToApplication(QCoreApplication& application) const
{
    if (!enabled) {
        application.setOrganizationName(QString::fromLatin1(kDefaultOrganizationName));
        application.setApplicationName(QString::fromLatin1(kDefaultApplicationName));
        application.setProperty(kPropertyEnabled, false);
        application.setProperty(kPropertyProfile, QVariant());
        application.setProperty(kPropertyDataRoot, QVariant());
        application.setProperty(kPropertyListenPort, QVariant());
        application.setProperty(kPropertyClientId, QVariant());
        application.setProperty(kPropertyDisplayName, QVariant());
        application.setProperty(kPropertyWindowTitleSuffix, QVariant());
        return;
    }

    application.setOrganizationName(settingsOrganizationName());
    application.setApplicationName(settingsApplicationName());
    application.setProperty(kPropertyEnabled, true);
    application.setProperty(kPropertyProfile, profile);
    application.setProperty(kPropertyDataRoot, dataRoot);
    application.setProperty(kPropertyListenPort, static_cast<uint>(listenPort));
    application.setProperty(kPropertyClientId, clientId);
    application.setProperty(kPropertyDisplayName, displayName);
    application.setProperty(kPropertyWindowTitleSuffix, windowTitleSuffix());
}

QString TestModeContext::settingsOrganizationName() const
{
    return enabled ? QString::fromLatin1(kDevOrganizationName)
                   : QString::fromLatin1(kDefaultOrganizationName);
}

QString TestModeContext::settingsApplicationName() const
{
    return enabled ? QStringLiteral("LeyoChat-%1").arg(normalizedKeySegment(profile))
                   : QString::fromLatin1(kDefaultApplicationName);
}

QString TestModeContext::singleInstanceKey() const
{
    return enabled ? QStringLiteral("LeyoChat_SingleInstance_dev-%1").arg(normalizedKeySegment(profile))
                   : QStringLiteral("LeyoChat_SingleInstance");
}

QString TestModeContext::lockFileName() const
{
    return enabled ? QStringLiteral("leyochat-%1.lock").arg(normalizedKeySegment(profile))
                   : QStringLiteral("leyochat.lock");
}

QString TestModeContext::windowTitleSuffix() const
{
    return enabled ? QStringLiteral("[dev %1]").arg(normalizedKeySegment(profile))
                   : QString();
}

QString TestModeContext::appDataRoot() const
{
    return enabled ? joinPath(dataRoot, profile, QStringLiteral("appdata")) : QString();
}

QString TestModeContext::appLocalDataRoot() const
{
    return enabled ? joinPath(dataRoot, profile, QStringLiteral("local")) : QString();
}

QString TestModeContext::databasePath() const
{
    return enabled ? QDir(appDataRoot()).filePath(QStringLiteral("leyochat.db")) : QString();
}

QString TestModeContext::avatarDirectoryPath() const
{
    return enabled ? QDir(appDataRoot()).filePath(QStringLiteral("avatars")) : QString();
}

QString TestModeContext::logsDirectoryPath() const
{
    return enabled ? QDir(appLocalDataRoot()).filePath(QStringLiteral("logs")) : QString();
}

QString TestModeContext::crashDirectoryPath() const
{
    return enabled ? QDir(appLocalDataRoot()).filePath(QStringLiteral("crash")) : QString();
}

QString TestModeContext::screenshotsDirectoryPath() const
{
    return enabled ? QDir(appLocalDataRoot()).filePath(QStringLiteral("screenshots")) : QString();
}

QString TestModeContext::runtimeDirectoryPath() const
{
    return enabled ? QDir(appLocalDataRoot()).filePath(QStringLiteral("runtime")) : QString();
}

QString TestModeContext::incomingFilesDirectoryPath() const
{
    return enabled ? QDir(appLocalDataRoot()).filePath(QStringLiteral("received")) : QString();
}
