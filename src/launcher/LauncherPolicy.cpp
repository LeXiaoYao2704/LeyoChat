#include "launcher/LauncherPolicy.h"

#include <array>

LauncherDecision LauncherPolicy::afterChildExit(std::uint32_t exitCode,
                                                std::int64_t runtimeMs,
                                                std::int64_t nowMs,
                                                bool shutdownRequested)
{
    if (shutdownRequested || exitCode == 0)
    {
        return {};
    }

    if (runtimeMs >= stableRuntimeMs())
    {
        m_crashTimesMs.clear();
    }

    const std::int64_t windowStartMs = nowMs - crashWindowMs();
    while (!m_crashTimesMs.empty() && m_crashTimesMs.front() < windowStartMs)
    {
        m_crashTimesMs.pop_front();
    }
    m_crashTimesMs.push_back(nowMs);

    const int crashCount = static_cast<int>(m_crashTimesMs.size());
    constexpr std::array<int, 3> kRestartDelaysMs{2000, 10000, 30000};
    if (crashCount > static_cast<int>(kRestartDelaysMs.size()))
    {
        return {LauncherAction::StopCrashLoop, 0, crashCount};
    }

    return {
        LauncherAction::Restart,
        kRestartDelaysMs[static_cast<std::size_t>(crashCount - 1)],
        crashCount,
    };
}
