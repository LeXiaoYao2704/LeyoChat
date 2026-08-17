#include "app/AppResourceDiagnostics.h"

#include <QDebug>
#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#endif

AppResourceSnapshot currentAppResourceSnapshot()
{
    AppResourceSnapshot snapshot;
#ifdef Q_OS_WIN
    HANDLE process = ::GetCurrentProcess();
    DWORD handleCount = 0;
    if (::GetProcessHandleCount(process, &handleCount)) {
        snapshot.handleCount = handleCount;
    }

    snapshot.userObjects = ::GetGuiResources(process, GR_USEROBJECTS);
    snapshot.gdiObjects = ::GetGuiResources(process, GR_GDIOBJECTS);

    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (::GetProcessMemoryInfo(process,
                               reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                               sizeof(counters))) {
        snapshot.workingSetBytes = counters.WorkingSetSize;
        snapshot.privateBytes = counters.PrivateUsage;
    }

    const DWORD pid = ::GetCurrentProcessId();
    HANDLE threadSnapshot = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (threadSnapshot != INVALID_HANDLE_VALUE) {
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        if (::Thread32First(threadSnapshot, &entry)) {
            do {
                if (entry.th32OwnerProcessID == pid) {
                    ++snapshot.threadCount;
                }
                entry.dwSize = sizeof(entry);
            } while (::Thread32Next(threadSnapshot, &entry));
        }
        ::CloseHandle(threadSnapshot);
    }

    snapshot.valid = true;
#endif
    return snapshot;
}

void logUserObjects(const char* label)
{
#ifdef Q_OS_WIN
    const DWORD userObjs = ::GetGuiResources(::GetCurrentProcess(), GR_USEROBJECTS);
    const DWORD gdiObjs = ::GetGuiResources(::GetCurrentProcess(), GR_GDIOBJECTS);
    qInfo().noquote() << QStringLiteral("[USER-OBJECTS] %1: USER=%2 GDI=%3")
                             .arg(QLatin1String(label),
                                  QString::number(userObjs),
                                  QString::number(gdiObjs));
#else
    Q_UNUSED(label);
#endif
}

void logProcessResources(const char* label)
{
    const AppResourceSnapshot snapshot = currentAppResourceSnapshot();
    if (!snapshot.valid) {
        Q_UNUSED(label);
        return;
    }

    qInfo().noquote()
        << QStringLiteral("[resource-watch] %1: handles=%2 USER=%3 GDI=%4 threads=%5 workingSetKB=%6 privateKB=%7")
               .arg(QLatin1String(label),
                    QString::number(snapshot.handleCount),
                    QString::number(snapshot.userObjects),
                    QString::number(snapshot.gdiObjects),
                    QString::number(snapshot.threadCount),
                    QString::number(snapshot.workingSetBytes / 1024),
                    QString::number(snapshot.privateBytes / 1024));
}
