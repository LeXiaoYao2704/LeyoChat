#include "diagnostics/RuntimeDiagnostics.h"

#include "diagnostics/Diagnostics.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <csignal>
#include <exception>

#ifdef Q_OS_WIN
#include <windows.h>
#include <DbgHelp.h>
#include <io.h>
#endif

namespace Diagnostics {
namespace {

QMutex& logMutex()
{
    static QMutex mutex;
    return mutex;
}

QString& logFilePathStorage()
{
    static QString path;
    return path;
}

BundleSourcePaths& sourcePathsStorage()
{
    static BundleSourcePaths paths;
    return paths;
}

QtMessageHandler& previousHandlerStorage()
{
    static QtMessageHandler previous = nullptr;
    return previous;
}

QString messageTypeName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("DEBUG");
    case QtInfoMsg:
        return QStringLiteral("INFO");
    case QtWarningMsg:
        return QStringLiteral("WARN");
    case QtCriticalMsg:
        return QStringLiteral("ERROR");
    case QtFatalMsg:
        return QStringLiteral("FATAL");
    }
    return QStringLiteral("LOG");
}

void appendPlainTextLine(const QString& filePath, const QString& line)
{
    QMutexLocker locker(&logMutex());
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << line << Qt::endl;
}

// 主日志文件使用持久句柄，避免每条日志 open/flush/close 导致主线程 I/O 阻塞
void appendToMainLog(const QString& line, bool flush = false)
{
    QMutexLocker locker(&logMutex());
    static QFile file;
    if (!file.isOpen()) {
        const QString& path = logFilePathStorage();
        if (path.isEmpty()) return;
        QDir().mkpath(QFileInfo(path).absolutePath());
        file.setFileName(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return;
        }
    }
    file.write(line.toUtf8());
    file.write("\n", 1);
    if (flush) {
        file.flush();
    }
}

QString crashTextPath()
{
    const auto& paths = sourcePathsStorage();
    return QDir(paths.crashDir).filePath(QStringLiteral("latest-crash.txt"));
}

QString crashDumpPath()
{
    const auto& paths = sourcePathsStorage();
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    return QDir(paths.crashDir).filePath(QStringLiteral("crash-%1.dmp").arg(timestamp));
}

void purgeOldCrashDumps(int maxKeep = 10)
{
    const auto& paths = sourcePathsStorage();
    QDir crashDir(paths.crashDir);
    if (!crashDir.exists()) return;
    QStringList filters;
    filters << QStringLiteral("crash-*.dmp");
    QFileInfoList dumps = crashDir.entryInfoList(filters, QDir::Files, QDir::Time);
    // QDir::Time 按修改时间降序排列，前 maxKeep 个是最新的
    while (dumps.size() > maxKeep) {
        QFile::remove(dumps.takeLast().absoluteFilePath());
    }
}

void writeCrashSummary(const QString& title, const QString& details)
{
    const QString line = QStringLiteral("[%1] %2\n%3")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  title,
                                  details);
    appendPlainTextLine(crashTextPath(), line);
    appendPlainTextLine(logFilePathStorage(), QStringLiteral("[CRASH] %1 — %2").arg(title, details));
}

#ifdef Q_OS_WIN
void writeMiniDump(EXCEPTION_POINTERS* exceptionPointers)
{
    QDir().mkpath(sourcePathsStorage().crashDir);
    QFile dumpFile(crashDumpPath());
    if (!dumpFile.open(QIODevice::WriteOnly)) {
        return;
    }

    HMODULE dbgHelpModule = ::LoadLibraryW(L"DbgHelp.dll");
    if (!dbgHelpModule) {
        return;
    }

    using MiniDumpWriteDumpFn =
        BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                      PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
    auto* miniDumpWriteDump =
        reinterpret_cast<MiniDumpWriteDumpFn>(::GetProcAddress(dbgHelpModule, "MiniDumpWriteDump"));
    if (!miniDumpWriteDump) {
        ::FreeLibrary(dbgHelpModule);
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
    exceptionInfo.ThreadId = ::GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exceptionPointers;
    exceptionInfo.ClientPointers = FALSE;

    miniDumpWriteDump(::GetCurrentProcess(),
                      ::GetCurrentProcessId(),
                      reinterpret_cast<HANDLE>(_get_osfhandle(dumpFile.handle())),
                      static_cast<MINIDUMP_TYPE>(MiniDumpNormal
                          | MiniDumpWithModuleHeaders
                          | MiniDumpWithDataSegs),
                      exceptionPointers ? &exceptionInfo : nullptr,
                      nullptr,
                      nullptr);
    dumpFile.close();
    ::FreeLibrary(dbgHelpModule);
    purgeOldCrashDumps(10);
}

LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
    const DWORD code = exceptionPointers && exceptionPointers->ExceptionRecord
        ? exceptionPointers->ExceptionRecord->ExceptionCode : 0;
    const auto addr = exceptionPointers && exceptionPointers->ExceptionRecord
        ? exceptionPointers->ExceptionRecord->ExceptionAddress : nullptr;
    writeCrashSummary(QStringLiteral("UnhandledExceptionFilter"),
                      QStringLiteral("exception=0x%1 addr=%2")
                          .arg(QString::number(code, 16),
                               QString::number(reinterpret_cast<quint64>(addr), 16)));
    writeMiniDump(exceptionPointers);
    return EXCEPTION_EXECUTE_HANDLER;
}

// Vectored Exception Handler — 在 CRT signal handler 之前捕获 ACCESS_VIOLATION，
// 这样 minidump 中才有完整的 ExceptionPointers 和调用栈。
static LONG WINAPI vectoredExceptionHandler(EXCEPTION_POINTERS* exceptionPointers)
{
    // 防止重入：第一次崩溃后不再处理级联异常
    static volatile LONG s_entered = 0;
    if (::InterlockedCompareExchange(&s_entered, 1, 0) != 0) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    if (!exceptionPointers || !exceptionPointers->ExceptionRecord) {
        ::InterlockedExchange(&s_entered, 0);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const DWORD code = exceptionPointers->ExceptionRecord->ExceptionCode;
    // 只捕获致命异常
    if (code != EXCEPTION_ACCESS_VIOLATION
        && code != EXCEPTION_STACK_OVERFLOW
        && code != EXCEPTION_ILLEGAL_INSTRUCTION
        && code != EXCEPTION_INT_DIVIDE_BY_ZERO) {
        ::InterlockedExchange(&s_entered, 0);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto addr = exceptionPointers->ExceptionRecord->ExceptionAddress;

    // 解析崩溃所在模块名和偏移
    QString moduleName = QStringLiteral("unknown");
    quint64 moduleOffset = reinterpret_cast<quint64>(addr);
    HMODULE hCrashModule = nullptr;
    if (::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(addr), &hCrashModule) && hCrashModule) {
        wchar_t modPath[MAX_PATH] = {};
        if (::GetModuleFileNameW(hCrashModule, modPath, MAX_PATH) > 0) {
            moduleName = QString::fromWCharArray(modPath);
            // 只保留文件名
            const int lastSlash = moduleName.lastIndexOf(QLatin1Char('\\'));
            if (lastSlash >= 0) {
                moduleName = moduleName.mid(lastSlash + 1);
            }
            moduleOffset = reinterpret_cast<quint64>(addr) - reinterpret_cast<quint64>(hCrashModule);
        }
    }

    writeCrashSummary(QStringLiteral("VectoredExceptionHandler"),
                      QStringLiteral("exception=0x%1 addr=0x%2 module=%3 offset=0x%4")
                          .arg(QString::number(code, 16),
                               QString::number(reinterpret_cast<quint64>(addr), 16),
                               moduleName,
                               QString::number(moduleOffset, 16)));
    writeMiniDump(exceptionPointers);
    // 不阻止后续处理，让进程正常终止
    return EXCEPTION_CONTINUE_SEARCH;
}
#endif

void signalHandler(int signalNumber)
{
    writeCrashSummary(QStringLiteral("Signal"),
                      QStringLiteral("Process terminated by signal %1").arg(signalNumber));
#ifdef Q_OS_WIN
    // 生成 minidump 以便分析调用栈
    writeMiniDump(nullptr);
#endif
    std::_Exit(signalNumber);
}

void terminateHandler()
{
    QString details = QStringLiteral("std::terminate called without an active exception.");
    if (const auto current = std::current_exception()) {
        try {
            std::rethrow_exception(current);
        } catch (const std::exception& ex) {
            details = QString::fromLocal8Bit(ex.what());
        } catch (...) {
            details = QStringLiteral("std::terminate called with a non-standard exception.");
        }
    }

    writeCrashSummary(QStringLiteral("Terminate"), details);
    std::_Exit(EXIT_FAILURE);
}

void runtimeMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    const QString category = context.category ? QString::fromUtf8(context.category) : QString();
    const QString line = QStringLiteral("[%1] [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                  messageTypeName(type),
                                  category.isEmpty() ? QStringLiteral("default") : category,
                                  message);
    // DEBUG/INFO 不 flush，WARN 及以上立即 flush，避免高频日志阻塞主线程
    const bool needsFlush = (type >= QtWarningMsg);
    appendToMainLog(line, needsFlush);

    if (auto previous = previousHandlerStorage()) {
        previous(type, context, message);
    }

    if (type == QtFatalMsg) {
        writeCrashSummary(QStringLiteral("QtFatal"), message);
        abort();
    }
}

}  // namespace

void installRuntimeDiagnostics()
{
    const BundleSourcePaths paths = defaultSourcePaths();
    sourcePathsStorage() = paths;
    QDir().mkpath(paths.logsDir);
    QDir().mkpath(paths.crashDir);
    QDir().mkpath(paths.runtimeDir);

    logFilePathStorage() =
        QDir(paths.logsDir).filePath(QStringLiteral("leyochat-%1.log")
                                         .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd"))));

    previousHandlerStorage() = qInstallMessageHandler(runtimeMessageHandler);
    std::set_terminate(terminateHandler);
    std::signal(SIGABRT, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGSEGV, signalHandler);
#ifdef Q_OS_WIN
    ::SetUnhandledExceptionFilter(unhandledExceptionFilter);
    ::AddVectoredExceptionHandler(1 /* first handler */, vectoredExceptionHandler);
#endif

    appendToMainLog(QStringLiteral("[%1] [INFO] [startup] Runtime diagnostics installed")
                        .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs)),
                    true /* flush */);
}

}  // namespace Diagnostics
