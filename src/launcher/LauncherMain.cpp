#include "launcher/LauncherPolicy.h"

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{
constexpr wchar_t kWindowClassName[] = L"LeyoChatLauncherHiddenWindow";
constexpr wchar_t kLauncherMutexName[] = L"Local\\LeyoChatLauncher_SingleInstance";
constexpr wchar_t kChildExecutableName[] = L"LeyoChat.exe";
constexpr std::uintmax_t kMaximumLogBytes = 1024 * 1024;

LauncherShutdownState g_shutdownState;

std::wstring modulePath()
{
    std::vector<wchar_t> buffer(32768);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size())
        return {};
    return std::wstring(buffer.data(), length);
}

std::wstring directoryOf(const std::wstring& path)
{
    const std::size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : path.substr(0, slash);
}

std::wstring localAppDataPath()
{
    PWSTR rawPath = nullptr;
    const HRESULT result = SHGetKnownFolderPath(FOLDERID_LocalAppData,
                                                KF_FLAG_CREATE,
                                                nullptr,
                                                &rawPath);
    if (FAILED(result) || !rawPath)
        return {};
    std::wstring path(rawPath);
    CoTaskMemFree(rawPath);
    return path;
}

std::wstring launcherLogPath()
{
    const std::wstring localRoot = localAppDataPath();
    if (localRoot.empty())
        return {};
    const std::filesystem::path logDirectory =
        std::filesystem::path(localRoot) / L"LeyoChat" / L"LeyoChat" / L"logs";
    std::error_code error;
    std::filesystem::create_directories(logDirectory, error);
    return (logDirectory / L"leyochat-launcher.log").wstring();
}

std::string utf8(const std::wstring& text)
{
    if (text.empty())
        return {};
    const int required = WideCharToMultiByte(CP_UTF8, 0, text.data(),
                                             static_cast<int>(text.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (required <= 0)
        return {};
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                        result.data(), required, nullptr, nullptr);
    return result;
}

void rotateLogIfNeeded(const std::wstring& path)
{
    if (path.empty())
        return;
    std::error_code error;
    const std::filesystem::path active(path);
    if (!std::filesystem::exists(active, error)
        || std::filesystem::file_size(active, error) < kMaximumLogBytes)
        return;

    const std::filesystem::path archive = active.parent_path() / L"leyochat-launcher.1.log";
    std::filesystem::remove(archive, error);
    error.clear();
    std::filesystem::rename(active, archive, error);
}

void appendLog(const std::wstring& message)
{
    const std::wstring path = launcherLogPath();
    if (path.empty())
        return;
    rotateLogIfNeeded(path);

    SYSTEMTIME time{};
    GetLocalTime(&time);
    std::wostringstream line;
    line << L'[' << std::setfill(L'0')
         << std::setw(4) << time.wYear << L'-'
         << std::setw(2) << time.wMonth << L'-'
         << std::setw(2) << time.wDay << L'T'
         << std::setw(2) << time.wHour << L':'
         << std::setw(2) << time.wMinute << L':'
         << std::setw(2) << time.wSecond << L'.'
         << std::setw(3) << time.wMilliseconds << L"] "
         << message << L"\r\n";
    const std::string bytes = utf8(line.str());

    HANDLE file = CreateFileW(path.c_str(), FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    DWORD written = 0;
    WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(file);
}

std::wstring win32ErrorMessage(DWORD code)
{
    wchar_t* rawMessage = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&rawMessage), 0, nullptr);
    std::wstring message = length > 0 && rawMessage
        ? std::wstring(rawMessage, length)
        : L"Unknown error";
    if (rawMessage)
        LocalFree(rawMessage);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n'))
        message.pop_back();
    return message;
}

std::wstring newSessionId()
{
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid)))
    {
        std::wostringstream fallback;
        fallback << L"fallback-" << GetCurrentProcessId() << L'-' << GetTickCount64();
        return fallback.str();
    }
    wchar_t buffer[64]{};
    StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
    return buffer;
}

LRESULT CALLBACK launcherWindowProc(HWND window, UINT message,
                                    WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_QUERYENDSESSION:
        g_shutdownState.onQueryEndSession();
        return TRUE;
    case WM_ENDSESSION:
        g_shutdownState.onEndSession(wParam != FALSE);
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

HWND createHiddenWindow(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = launcherWindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kWindowClassName;
    RegisterClassExW(&windowClass);
    return CreateWindowExW(0, kWindowClassName, L"LeyoChat Launcher",
                           WS_OVERLAPPED, 0, 0, 0, 0,
                           nullptr, nullptr, instance, nullptr);
}

bool pumpMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            g_shutdownState.requestShutdown();
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return true;
}

bool waitForChild(HANDLE process)
{
    for (;;)
    {
        const DWORD result = MsgWaitForMultipleObjects(
            1, &process, FALSE, INFINITE, QS_ALLINPUT);
        if (result == WAIT_OBJECT_0)
            return true;
        if (result == WAIT_OBJECT_0 + 1)
        {
            pumpMessages();
            continue;
        }
        return false;
    }
}

bool waitForRestartDelay(int delayMs)
{
    const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(delayMs);
    while (!g_shutdownState.shutdownRequested())
    {
        const ULONGLONG now = GetTickCount64();
        if (now >= deadline)
            return true;
        const ULONGLONG remaining = deadline - now;
        const DWORD timeout = remaining > MAXDWORD
            ? MAXDWORD
            : static_cast<DWORD>(remaining);
        const DWORD result = MsgWaitForMultipleObjects(
            0, nullptr, FALSE, timeout, QS_ALLINPUT);
        if (result == WAIT_TIMEOUT)
            return true;
        if (result == WAIT_OBJECT_0)
            pumpMessages();
        else
            return false;
    }
    return false;
}

std::wstring quoteArgument(const std::wstring& argument)
{
    std::wstring quoted = L"\"";
    std::size_t slashCount = 0;
    for (const wchar_t ch : argument)
    {
        if (ch == L'\\')
        {
            ++slashCount;
            continue;
        }
        if (ch == L'\"')
        {
            quoted.append(slashCount * 2 + 1, L'\\');
            quoted.push_back(ch);
            slashCount = 0;
            continue;
        }
        quoted.append(slashCount, L'\\');
        slashCount = 0;
        quoted.push_back(ch);
    }
    quoted.append(slashCount * 2, L'\\');
    quoted.push_back(L'\"');
    return quoted;
}

bool startChild(const std::wstring& childPath,
                const std::wstring& workingDirectory,
                const std::wstring& sessionId,
                bool recoveredFromCrash,
                PROCESS_INFORMATION* processInfo,
                DWORD* errorCode)
{
    std::wstring commandLine = quoteArgument(childPath)
        + L" --leyochat-supervised --recovery-session=" + sessionId;
    if (recoveredFromCrash)
        commandLine += L" --recovered-from-crash";
    std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
    mutableCommand.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION child{};
    const BOOL started = CreateProcessW(
        childPath.c_str(), mutableCommand.data(), nullptr, nullptr, FALSE,
        CREATE_UNICODE_ENVIRONMENT, nullptr, workingDirectory.c_str(),
        &startupInfo, &child);
    if (!started)
    {
        if (errorCode)
            *errorCode = GetLastError();
        return false;
    }
    CloseHandle(child.hThread);
    *processInfo = child;
    return true;
}

void showFatalMessage(const std::wstring& message)
{
    MessageBoxW(nullptr, message.c_str(), L"LeyoChat",
                MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HANDLE launcherMutex = CreateMutexW(nullptr, TRUE, kLauncherMutexName);
    if (!launcherMutex)
        return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        CloseHandle(launcherMutex);
        return 0;
    }

    SetProcessShutdownParameters(0x100, SHUTDOWN_NORETRY);
    HWND hiddenWindow = createHiddenWindow(instance);
    if (!hiddenWindow)
    {
        CloseHandle(launcherMutex);
        return 1;
    }

    const std::wstring launcherPath = modulePath();
    const std::wstring installDirectory = directoryOf(launcherPath);
    const std::wstring childPath = installDirectory + L"\\" + kChildExecutableName;
    if (GetFileAttributesW(childPath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        appendLog(L"child-missing path=" + childPath);
        showFatalMessage(L"LeyoChat.exe 不存在，安装可能已损坏。请重新安装 LeyoChat。");
        DestroyWindow(hiddenWindow);
        CloseHandle(launcherMutex);
        return 1;
    }

    const std::wstring sessionId = newSessionId();
    LauncherPolicy policy;
    bool recoveredFromCrash = false;
    appendLog(L"launcher-start pid=" + std::to_wstring(GetCurrentProcessId())
              + L" session=" + sessionId);

    int launcherExitCode = 0;
    for (;;)
    {
        PROCESS_INFORMATION child{};
        DWORD startError = ERROR_SUCCESS;
        if (!startChild(childPath, installDirectory, sessionId,
                        recoveredFromCrash, &child, &startError))
        {
            appendLog(L"child-start-failed error=" + std::to_wstring(startError)
                      + L" message=" + win32ErrorMessage(startError));
            showFatalMessage(L"无法启动 LeyoChat。\n\n错误："
                             + win32ErrorMessage(startError));
            launcherExitCode = 1;
            break;
        }

        const ULONGLONG startedAt = GetTickCount64();
        appendLog(L"child-start pid=" + std::to_wstring(child.dwProcessId)
                  + L" recovered=" + (recoveredFromCrash ? L"true" : L"false"));
        if (!waitForChild(child.hProcess))
            g_shutdownState.requestShutdown();

        DWORD childExitCode = 1;
        GetExitCodeProcess(child.hProcess, &childExitCode);
        CloseHandle(child.hProcess);
        const std::int64_t runtimeMs = static_cast<std::int64_t>(GetTickCount64() - startedAt);
        const std::int64_t nowMs = static_cast<std::int64_t>(GetTickCount64());
        const LauncherDecision decision = policy.afterChildExit(
            childExitCode, runtimeMs, nowMs, g_shutdownState.shutdownRequested());

        std::wostringstream exitLine;
        exitLine << L"child-exit code=" << childExitCode
                 << L" hex=0x" << std::hex << std::uppercase << childExitCode << std::dec
                 << L" runtimeMs=" << runtimeMs
                 << L" crashCount=" << decision.recentCrashCount
                 << L" action=" << static_cast<int>(decision.action)
                 << L" delayMs=" << decision.delayMs
                 << L" shutdown=" << (g_shutdownState.shutdownRequested() ? L"true" : L"false");
        appendLog(exitLine.str());

        if (decision.action == LauncherAction::Exit)
            break;
        if (decision.action == LauncherAction::StopCrashLoop)
        {
            showFatalMessage(
                L"LeyoChat 在 5 分钟内连续异常退出，已暂停自动恢复，避免反复闪退。\n\n"
                L"崩溃日志和 DMP 已保留，请重新启动 LeyoChat；如果仍然异常，请导出诊断包。");
            launcherExitCode = 2;
            break;
        }
        if (!waitForRestartDelay(decision.delayMs))
            break;
        recoveredFromCrash = true;
    }

    appendLog(L"launcher-exit code=" + std::to_wstring(launcherExitCode)
              + L" shutdown=" + (g_shutdownState.shutdownRequested() ? L"true" : L"false"));
    DestroyWindow(hiddenWindow);
    ReleaseMutex(launcherMutex);
    CloseHandle(launcherMutex);
    return launcherExitCode;
}
