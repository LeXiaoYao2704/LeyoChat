#pragma once

#include <array>

namespace LeyoChatInstaller
{

enum class ClientProcessKind
{
    Launcher,
    Client,
};

struct ClientLaunchPlan
{
    bool terminateExistingLauncher = false;
    bool terminateExistingClient = false;
    bool launchLauncher = false;
};

inline ClientLaunchPlan clientLaunchPlan(bool serverMode,
                                         bool launcherProcessRunning,
                                         bool clientProcessRunning)
{
    if (serverMode)
        return {};

    return ClientLaunchPlan{
        launcherProcessRunning,
        clientProcessRunning,
        true,
    };
}

inline constexpr std::array<ClientProcessKind, 2> clientProcessStopOrder()
{
    return {ClientProcessKind::Launcher, ClientProcessKind::Client};
}

inline bool shouldStopClientProcessesBeforeInstall(bool serverMode)
{
    return !serverMode;
}

inline bool canContinueAfterStoppingClientProcesses(bool serverMode,
                                                    bool launcherStopped,
                                                    bool clientStopped)
{
    return serverMode || (launcherStopped && clientStopped);
}

} // namespace LeyoChatInstaller
