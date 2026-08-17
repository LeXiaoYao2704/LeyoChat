#include "chatservice/ServiceHostPolicy.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace {
constexpr wchar_t kServiceName[] = L"LeyoChatService";
constexpr DWORD kChildStopTimeoutMs = 15000;

struct HostOptions {
    QString serviceExePath;
    QString configPath;
    QString logDirectory;
    QString stdoutLogPath;
    QString stderrLogPath;
    bool serviceMode = false;
    bool foregroundMode = false;
};

HostOptions g_options;
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
DWORD g_statusCheckpoint = 1;
HANDLE g_stopEvent = nullptr;
HANDLE g_childProcess = nullptr;
CRITICAL_SECTION g_childProcessLock;

std::wstring nativePath(const QString& path)
{
    return QDir::toNativeSeparators(path).toStdWString();
}

void setChildProcess(HANDLE process)
{
    EnterCriticalSection(&g_childProcessLock);
    g_childProcess = process;
    LeaveCriticalSection(&g_childProcessLock);
}

void clearChildProcess(HANDLE process)
{
    EnterCriticalSection(&g_childProcessLock);
    if (g_childProcess == process)
        g_childProcess = nullptr;
    LeaveCriticalSection(&g_childProcessLock);
}

HANDLE duplicateChildProcessHandle()
{
    HANDLE duplicated = nullptr;
    EnterCriticalSection(&g_childProcessLock);
    if (g_childProcess) {
        DuplicateHandle(GetCurrentProcess(),
                        g_childProcess,
                        GetCurrentProcess(),
                        &duplicated,
                        SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION,
                        FALSE,
                        0);
    }
    LeaveCriticalSection(&g_childProcessLock);
    return duplicated;
}

QString archiveNameForLog(const QFileInfo& activeLog)
{
    const QString timestamp = QDateTime::currentDateTimeUtc().toString(
        QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    return QStringLiteral("%1.%2.archive")
        .arg(activeLog.fileName(), timestamp);
}

void pruneArchivedLogs(const QString& activeLogPath,
                       const LeyoChatService::ServiceLogRetentionPolicy& policy)
{
    const QFileInfo activeLog(activeLogPath);
    QDir dir(activeLog.absolutePath());
    const QStringList filters{
        activeLog.fileName() + QStringLiteral(".*.archive")
    };
    const QFileInfoList archives =
        dir.entryInfoList(filters, QDir::Files, QDir::Time);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    for (int i = 0; i < archives.size(); ++i) {
        const QFileInfo& archive = archives.at(i);
        if (!LeyoChatService::ServiceHostPolicy::shouldPruneArchivedLog(
                archive.lastModified().toMSecsSinceEpoch(),
                nowMs,
                i,
                policy)) {
            continue;
        }
        if (!QFile::remove(archive.absoluteFilePath())) {
            qWarning().noquote()
                << "[service-log-retention] failed to remove archive"
                << archive.absoluteFilePath();
        }
    }
}

void prepareLogFile(const QString& path,
                    const LeyoChatService::ServiceLogRetentionPolicy& policy)
{
    QDir().mkpath(QFileInfo(path).absolutePath());

    const QFileInfo activeLog(path);
    if (activeLog.exists()
        && LeyoChatService::ServiceHostPolicy::shouldRotateActiveLog(
            activeLog.size(),
            policy)) {
        const QString archivePath =
            QDir(activeLog.absolutePath()).filePath(archiveNameForLog(activeLog));
        if (!QFile::rename(path, archivePath)) {
            qWarning().noquote()
                << "[service-log-retention] failed to archive active log"
                << path
                << "archive=" << archivePath;
        }
    }

    pruneArchivedLogs(path, policy);
}

void terminateChildProcess()
{
    HANDLE process = duplicateChildProcessHandle();
    if (!process)
        return;

    DWORD exitCode = 0;
    if (GetExitCodeProcess(process, &exitCode) && exitCode == STILL_ACTIVE) {
        TerminateProcess(process, 1);
        WaitForSingleObject(process, kChildStopTimeoutMs);
    }
    CloseHandle(process);
}

void reportServiceStatus(DWORD state,
                         DWORD win32ExitCode = NO_ERROR,
                         DWORD waitHintMs = 0)
{
    if (!g_statusHandle)
        return;

    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwControlsAccepted =
        (state == SERVICE_START_PENDING || state == SERVICE_STOPPED)
            ? 0
            : (SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN);
    g_status.dwWin32ExitCode = win32ExitCode;
    g_status.dwWaitHint = waitHintMs;
    g_status.dwCheckPoint =
        (state == SERVICE_RUNNING || state == SERVICE_STOPPED)
            ? 0
            : g_statusCheckpoint++;

    SetServiceStatus(g_statusHandle, &g_status);
}

DWORD WINAPI serviceControlHandler(DWORD control,
                                   DWORD,
                                   LPVOID,
                                   LPVOID)
{
    switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        reportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, kChildStopTimeoutMs);
        if (g_stopEvent)
            SetEvent(g_stopEvent);
        terminateChildProcess();
        return NO_ERROR;
    default:
        return NO_ERROR;
    }
}

HANDLE openLogFile(const QString& path)
{
    prepareLogFile(
        path,
        LeyoChatService::ServiceHostPolicy::defaultLogRetentionPolicy());

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    const std::wstring pathW = nativePath(path);
    HANDLE file = CreateFileW(pathW.c_str(),
                              FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                              &securityAttributes,
                              OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    if (file != INVALID_HANDLE_VALUE)
        SetFilePointer(file, 0, nullptr, FILE_END);
    return file;
}

HANDLE openNullInput()
{
    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    return CreateFileW(L"NUL",
                       GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       &securityAttributes,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       nullptr);
}

HANDLE startServiceChild()
{
    HANDLE stdoutLog = openLogFile(g_options.stdoutLogPath);
    if (stdoutLog == INVALID_HANDLE_VALUE) {
        qCritical() << "Failed to open service stdout log"
                    << g_options.stdoutLogPath
                    << "error" << GetLastError();
        return nullptr;
    }

    HANDLE stderrLog = openLogFile(g_options.stderrLogPath);
    if (stderrLog == INVALID_HANDLE_VALUE) {
        qCritical() << "Failed to open service stderr log"
                    << g_options.stderrLogPath
                    << "error" << GetLastError();
        CloseHandle(stdoutLog);
        return nullptr;
    }

    HANDLE stdinHandle = openNullInput();
    if (stdinHandle == INVALID_HANDLE_VALUE)
        stdinHandle = GetStdHandle(STD_INPUT_HANDLE);

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdInput = stdinHandle;
    startupInfo.hStdOutput = stdoutLog;
    startupInfo.hStdError = stderrLog;

    PROCESS_INFORMATION processInfo{};
    const std::wstring applicationPath = nativePath(g_options.serviceExePath);
    std::wstring commandLine = nativePath(
        LeyoChatService::ServiceHostPolicy::childCommandLine(
            g_options.serviceExePath,
            g_options.configPath));
    const std::wstring workingDirectory =
        nativePath(QFileInfo(g_options.serviceExePath).absolutePath());

    const BOOL created = CreateProcessW(applicationPath.c_str(),
                                        commandLine.data(),
                                        nullptr,
                                        nullptr,
                                        TRUE,
                                        CREATE_NO_WINDOW,
                                        nullptr,
                                        workingDirectory.empty()
                                            ? nullptr
                                            : workingDirectory.c_str(),
                                        &startupInfo,
                                        &processInfo);

    if (stdinHandle && stdinHandle != INVALID_HANDLE_VALUE
        && stdinHandle != GetStdHandle(STD_INPUT_HANDLE)) {
        CloseHandle(stdinHandle);
    }
    CloseHandle(stdoutLog);
    CloseHandle(stderrLog);

    if (!created) {
        qCritical() << "Failed to start LeyoChatService child"
                    << g_options.serviceExePath
                    << "error" << GetLastError();
        return nullptr;
    }

    CloseHandle(processInfo.hThread);
    return processInfo.hProcess;
}

int waitForStopOrDelay(DWORD delayMs)
{
    const DWORD waitResult = WaitForSingleObject(g_stopEvent, delayMs);
    return waitResult == WAIT_OBJECT_0 ? 0 : 1;
}

int superviseChild()
{
    QDir().mkpath(g_options.logDirectory);

    QList<qint64> crashTimesMs;
    int restartCount = 0;

    while (WaitForSingleObject(g_stopEvent, 0) != WAIT_OBJECT_0) {
        HANDLE child = startServiceChild();
        if (!child) {
            crashTimesMs.push_back(QDateTime::currentMSecsSinceEpoch());
            if (!LeyoChatService::ServiceHostPolicy::shouldRestartAfterCrash(
                    crashTimesMs,
                    crashTimesMs.back())) {
                qCritical() << "LeyoChatService failed to start too many times";
                return 1;
            }
            if (waitForStopOrDelay(
                    LeyoChatService::ServiceHostPolicy::restartDelayMs(
                        restartCount++)) == 0) {
                return 0;
            }
            continue;
        }

        setChildProcess(child);
        HANDLE waitHandles[] = {g_stopEvent, child};
        const DWORD waitResult =
            WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);

        if (waitResult == WAIT_OBJECT_0) {
            terminateChildProcess();
            clearChildProcess(child);
            CloseHandle(child);
            return 0;
        }

        DWORD childExitCode = 0;
        GetExitCodeProcess(child, &childExitCode);
        clearChildProcess(child);
        CloseHandle(child);

        if (childExitCode == 0) {
            qInfo() << "LeyoChatService child exited normally";
            return 0;
        }

        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        crashTimesMs.push_back(nowMs);
        if (!LeyoChatService::ServiceHostPolicy::shouldRestartAfterCrash(
                crashTimesMs,
                nowMs)) {
            qCritical() << "LeyoChatService crashed too many times in the "
                           "restart window";
            return 1;
        }

        const int delayMs =
            LeyoChatService::ServiceHostPolicy::restartDelayMs(restartCount++);
        qWarning() << "LeyoChatService exited with code" << childExitCode
                   << "- restarting in" << delayMs << "ms";
        if (waitForStopOrDelay(static_cast<DWORD>(delayMs)) == 0)
            return 0;
    }

    return 0;
}

void WINAPI serviceMain(DWORD, LPWSTR*)
{
    g_statusHandle =
        RegisterServiceCtrlHandlerExW(kServiceName, serviceControlHandler, nullptr);
    if (!g_statusHandle)
        return;

    reportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 30000);
    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        reportServiceStatus(SERVICE_STOPPED, GetLastError());
        return;
    }

    reportServiceStatus(SERVICE_RUNNING);
    const int result = superviseChild();

    if (g_stopEvent) {
        CloseHandle(g_stopEvent);
        g_stopEvent = nullptr;
    }
    reportServiceStatus(SERVICE_STOPPED,
                        result == 0 ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR);
}

int runServiceDispatcher()
{
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        {const_cast<LPWSTR>(kServiceName), serviceMain},
        {nullptr, nullptr},
    };

    if (!StartServiceCtrlDispatcherW(serviceTable)) {
        qCritical() << "StartServiceCtrlDispatcherW failed with error"
                    << GetLastError();
        return 1;
    }
    return 0;
}

HostOptions parseOptions(QCoreApplication& app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("LeyoChat Windows service host"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption serviceModeOpt(
        QStringLiteral("service"),
        QStringLiteral("Run under the Windows Service Control Manager."));
    QCommandLineOption foregroundModeOpt(
        QStringLiteral("run-foreground"),
        QStringLiteral("Run the supervisor in the foreground for debugging."));
    QCommandLineOption serviceExeOpt(
        QStringLiteral("service-exe"),
        QStringLiteral("Path to LeyoChatService.exe."),
        QStringLiteral("path"));
    QCommandLineOption configOpt(
        QStringLiteral("config"),
        QStringLiteral("Path to leyochat-service.json."),
        QStringLiteral("path"));

    parser.addOption(serviceModeOpt);
    parser.addOption(foregroundModeOpt);
    parser.addOption(serviceExeOpt);
    parser.addOption(configOpt);
    parser.process(app);

    const QString programData =
        qEnvironmentVariable("ProgramData", "C:/ProgramData");
    auto paths = LeyoChatService::ServiceHostPolicy::defaultPaths(
        QCoreApplication::applicationDirPath(),
        programData);

    HostOptions options;
    options.serviceMode = parser.isSet(serviceModeOpt);
    options.foregroundMode = parser.isSet(foregroundModeOpt);
    options.serviceExePath = parser.isSet(serviceExeOpt)
        ? QDir::cleanPath(parser.value(serviceExeOpt))
        : paths.serviceExePath;
    options.configPath = parser.isSet(configOpt)
        ? QDir::cleanPath(parser.value(configOpt))
        : paths.configPath;
    options.logDirectory = paths.logDirectory;
    options.stdoutLogPath = paths.stdoutLogPath;
    options.stderrLogPath = paths.stderrLogPath;

    if (options.serviceMode && options.foregroundMode) {
        qCritical() << "--service and --run-foreground cannot be used together";
        ::ExitProcess(2);
    }
    if (!options.serviceMode && !options.foregroundMode) {
        qCritical() << "Specify --service or --run-foreground";
        ::ExitProcess(2);
    }

    return options;
}
} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("LeyoChatServiceHost"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    InitializeCriticalSection(&g_childProcessLock);
    g_options = parseOptions(app);

    int result = 0;
    if (g_options.serviceMode) {
        result = runServiceDispatcher();
    } else {
        g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!g_stopEvent) {
            qCritical() << "Failed to create stop event" << GetLastError();
            result = 1;
        } else {
            result = superviseChild();
            CloseHandle(g_stopEvent);
            g_stopEvent = nullptr;
        }
    }

    DeleteCriticalSection(&g_childProcessLock);
    return result;
}
