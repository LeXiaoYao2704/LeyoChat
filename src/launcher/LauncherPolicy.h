#pragma once

#include <cstdint>
#include <deque>

enum class LauncherAction
{
    Exit,
    Restart,
    StopCrashLoop,
};

struct LauncherDecision
{
    LauncherAction action = LauncherAction::Exit;
    int delayMs = 0;
    int recentCrashCount = 0;
};

class LauncherShutdownState
{
public:
    void requestShutdown() { m_shutdownRequested = true; }
    void onQueryEndSession() { requestShutdown(); }
    void onEndSession(bool ending) { m_shutdownRequested = ending; }
    bool shutdownRequested() const { return m_shutdownRequested; }

private:
    bool m_shutdownRequested = false;
};

class LauncherPolicy
{
public:
    LauncherDecision afterChildExit(std::uint32_t exitCode,
                                    std::int64_t runtimeMs,
                                    std::int64_t nowMs,
                                    bool shutdownRequested);

    static constexpr std::int64_t stableRuntimeMs() { return 5 * 60 * 1000; }
    static constexpr std::int64_t crashWindowMs() { return 5 * 60 * 1000; }

private:
    std::deque<std::int64_t> m_crashTimesMs;
};
