#include "recovery/ClientStartupOptions.h"

namespace
{
constexpr qsizetype kMaxRecoverySessionIdLength = 128;
const auto kRecoverySessionPrefix = QStringLiteral("--recovery-session=");
}

ClientStartupOptions ClientStartupOptions::fromArguments(const QStringList& arguments)
{
    ClientStartupOptions options;
    for (const QString& argument : arguments)
    {
        if (argument == QStringLiteral("--leyochat-supervised"))
        {
            options.supervised = true;
        }
        else if (argument == QStringLiteral("--recovered-from-crash"))
        {
            options.recoveredFromCrash = true;
        }
        else if (argument.startsWith(kRecoverySessionPrefix))
        {
            options.recoverySessionId = argument.mid(kRecoverySessionPrefix.size()).trimmed();
        }
    }
    return options;
}

bool ClientStartupOptions::validRecoveryRequest() const
{
    return recoveredFromCrash
           && !recoverySessionId.isEmpty()
           && recoverySessionId.size() <= kMaxRecoverySessionIdLength;
}

bool ClientStartupOptions::suppressSplash() const
{
    return validRecoveryRequest();
}

bool ClientStartupOptions::shouldRegisterWindowsArr() const
{
    return !supervised;
}

QString ClientStartupOptions::windowsArrCommandLine() const
{
    return QStringLiteral("--recovered-from-crash --recovery-session=windows-arr");
}

QString ClientStartupOptions::stateSessionId() const
{
    if (!supervised)
    {
        return QStringLiteral("windows-arr");
    }
    if (!recoverySessionId.isEmpty()
        && recoverySessionId.size() <= kMaxRecoverySessionIdLength)
    {
        return recoverySessionId;
    }
    return {};
}
