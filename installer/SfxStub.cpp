// 微型 SFX 启动器 — 不依赖 Qt，编译后 <50KB
//
// 二进制布局（bundle 版）：
//   [SfxStub.exe] | [ui.bundle] | [inner-setup.exe raw] | [12字节尾部]
//   尾部：uiBundleSize(4) + innerSetupSize(4) + magic(4)
//
// ui.bundle 格式（自定义，纯顺序读写，无压缩，无子进程）：
//   [magic: 0x4C42434855 "HCBL"] [num_files: 4]
//   for each file: [pathLen: 2(wchar_t 数量)] [path: pathLen*2 bytes UTF-16LE]
//                  [dataSize: 4] [data: dataSize bytes]
//
// 启动流程：
//   1. 显示 splash（立即）
//   2. 直接从 exe 解包 bundle（纯 Win32 I/O，~0.5s，无子进程无解压）
//   3. 后台线程 raw 拷贝 inner-setup.exe（与 Qt 启动并行）
//   4. 立即启动 LeyoChatSetup.exe
//   5. 等待 Qt UI 退出 → 清理临时目录

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <stdio.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

// ── 调试日志（带毫秒时间戳）──
static HANDLE g_hLog = INVALID_HANDLE_VALUE;
static DWORD  g_startTick = 0;

static void LogInit()
{
    g_startTick = GetTickCount();
    wchar_t logPath[MAX_PATH];
    GetTempPathW(MAX_PATH, logPath);
    wcscat_s(logPath, L"LeyoChatSfx.log");
    g_hLog = CreateFileW(logPath, GENERIC_WRITE, FILE_SHARE_READ,
                         NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void Log(const char* fmt, ...)
{
    if (g_hLog == INVALID_HANDLE_VALUE) return;
    DWORD ms = GetTickCount() - g_startTick;
    char buf[1200];
    int pre = _snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%5u ms] ", ms);
    if (pre < 0) pre = 0;
    va_list ap;
    va_start(ap, fmt);
    int n = _vsnprintf_s(buf + pre, sizeof(buf) - pre, _TRUNCATE, fmt, ap);
    va_end(ap);
    int total = pre + (n > 0 ? n : 0);
    buf[total] = '\n'; buf[total + 1] = 0;
    DWORD w;
    WriteFile(g_hLog, buf, total + 1, &w, NULL);
    FlushFileBuffers(g_hLog);
}

static void LogClose() { if (g_hLog != INVALID_HANDLE_VALUE) CloseHandle(g_hLog); }
// ── 调试日志结束 ──

static const DWORD MAGIC = 0x48435346; // "HCSF"
static const DWORD MAGIC_BUNDLE = 0x4C424348U; // "HCBL"

// 12字节尾部：uiBundleSize(4) + innerSetupSize(4) + magic(4)
struct SfxTrailer {
    DWORD uiZipSize;       // 复用字段名，实际存 ui.bundle 字节数
    DWORD innerSetupSize;  // 原始 Inno Setup 安装器字节数（raw）
    DWORD magic;           // MAGIC
};

// ── 递归创建目录（不依赖 Shell API）──────────────────────────────────────
static void CreateDirRecursive(wchar_t* path)
{
    for (wchar_t* p = path + 1; *p; p++) {
        if (*p == L'\\') {
            *p = L'\0';
            CreateDirectoryW(path, NULL); // 忽略已存在的错误
            *p = L'\\';
        }
    }
    CreateDirectoryW(path, NULL);
}

// ── 从 exe 直接解包 ui.bundle（无中间文件，无子进程，纯 Win32 I/O）────────
// 性能：SSD 上 75MB ~= 0.4-0.8s（比 tar.exe 快 4-6 倍）
static bool ExtractBundle(HANDLE hFile, LONGLONG bundleOffset, const wchar_t* targetDir)
{
    LARGE_INTEGER li; li.QuadPart = bundleOffset;
    if (!SetFilePointerEx(hFile, li, NULL, FILE_BEGIN)) {
        Log("ExtractBundle: seek failed err=%u", GetLastError());
        return false;
    }

    DWORD nRead;
    DWORD magic = 0;
    ReadFile(hFile, &magic, 4, &nRead, NULL);
    if (magic != MAGIC_BUNDLE) {
        Log("ExtractBundle: bad magic 0x%08X (expected 0x%08X)", magic, MAGIC_BUNDLE);
        return false;
    }

    DWORD numFiles = 0;
    ReadFile(hFile, &numFiles, 4, &nRead, NULL);
    Log("ExtractBundle: %u files", numFiles);

    static char dataBuf[1 << 20]; // 1MB 静态读写缓冲

    for (DWORD i = 0; i < numFiles; i++) {
        // 路径长度（wchar_t 个数）
        WORD pathLen = 0;
        ReadFile(hFile, &pathLen, 2, &nRead, NULL);
        if (pathLen == 0 || pathLen >= MAX_PATH) {
            Log("ExtractBundle: bad pathLen=%u at file %u", pathLen, i);
            return false;
        }

        // 路径（UTF-16LE）
        wchar_t relPath[MAX_PATH];
        ReadFile(hFile, relPath, pathLen * sizeof(wchar_t), &nRead, NULL);
        relPath[pathLen] = L'\0';

        // 文件大小
        DWORD dataSize = 0;
        ReadFile(hFile, &dataSize, 4, &nRead, NULL);

        // 完整路径
        wchar_t fullPath[MAX_PATH];
        _snwprintf_s(fullPath, _countof(fullPath), _TRUNCATE, L"%s\\%s", targetDir, relPath);

        // 创建父目录
        {
            wchar_t parentDir[MAX_PATH];
            wcscpy_s(parentDir, fullPath);
            wchar_t* last = wcsrchr(parentDir, L'\\');
            if (last) { *last = L'\0'; CreateDirRecursive(parentDir); }
        }

        if (dataSize == 0) {
            // 空文件
            HANDLE h = CreateFileW(fullPath, GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
            continue;
        }

        HANDLE hOut = CreateFileW(fullPath, GENERIC_WRITE, 0, NULL,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOut == INVALID_HANDLE_VALUE) {
            Log("ExtractBundle: cannot create %ls err=%u -- skip", relPath, GetLastError());
            LARGE_INTEGER skip; skip.QuadPart = dataSize;
            SetFilePointerEx(hFile, skip, NULL, FILE_CURRENT);
            continue;
        }

        DWORD remaining = dataSize;
        while (remaining > 0) {
            DWORD toRead = (remaining < (DWORD)sizeof(dataBuf)) ? remaining : (DWORD)sizeof(dataBuf);
            DWORD nr = 0;
            if (!ReadFile(hFile, dataBuf, toRead, &nr, NULL) || nr == 0) break;
            DWORD nw;
            WriteFile(hOut, dataBuf, nr, &nw, NULL);
            remaining -= nr;
        }
        CloseHandle(hOut);
    }

    Log("ExtractBundle: done");
    return true;
}

// ── 后台线程：raw 拷贝 inner-setup.exe ───────────────────────────────────
struct CopyThreadParams {
    wchar_t srcPath[MAX_PATH];  // 源文件（SFX exe 自身）
    LONGLONG offset;            // inner-setup 在源文件中的字节偏移
    DWORD    size;              // inner-setup 的字节数
    wchar_t dstPath[MAX_PATH];  // 目标路径
    BOOL    success;
};

static DWORD WINAPI InnerSetupCopyThread(LPVOID param)
{
    CopyThreadParams* p = (CopyThreadParams*)param;
    Log("copy-thread: start, size=%u dst=%ls", p->size, p->dstPath);

    HANDLE hSrc = CreateFileW(p->srcPath, GENERIC_READ, FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hSrc == INVALID_HANDLE_VALUE) {
        Log("copy-thread: cannot open src, err=%u", GetLastError());
        p->success = FALSE;
        return 1;
    }

    LARGE_INTEGER li; li.QuadPart = p->offset;
    SetFilePointerEx(hSrc, li, NULL, FILE_BEGIN);

    HANDLE hDst = CreateFileW(p->dstPath, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hDst == INVALID_HANDLE_VALUE) {
        Log("copy-thread: cannot create dst, err=%u", GetLastError());
        CloseHandle(hSrc);
        p->success = FALSE;
        return 1;
    }

    static char buf[1 << 20]; // 1MB 缓冲
    DWORD remaining = p->size;
    while (remaining > 0) {
        DWORD toRead = (remaining < sizeof(buf)) ? remaining : (DWORD)sizeof(buf);
        DWORD nRead = 0;
        if (!ReadFile(hSrc, buf, toRead, &nRead, NULL) || nRead == 0) break;
        DWORD nWritten;
        WriteFile(hDst, buf, nRead, &nWritten, NULL);
        remaining -= nRead;
    }
    CloseHandle(hDst);
    CloseHandle(hSrc);

    p->success = (remaining == 0);
    Log("copy-thread: done, remaining=%u success=%d", remaining, (int)p->success);
    return p->success ? 0 : 1;
}

// ── 后台线程：解包 ui.bundle（与 splash 创建并行）────────────────────────
struct ExtractThreadParams {
    wchar_t  selfPath[MAX_PATH];
    LONGLONG bundleOffset;
    wchar_t  tempDir[MAX_PATH];
    BOOL     success;
};

static DWORD WINAPI ExtractBundleThread(LPVOID param)
{
    ExtractThreadParams* p = (ExtractThreadParams*)param;
    HANDLE hFile = CreateFileW(p->selfPath, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Log("extract-thread: cannot open self, err=%u", GetLastError());
        p->success = FALSE;
        return 1;
    }
    p->success = ExtractBundle(hFile, p->bundleOffset, p->tempDir);
    CloseHandle(hFile);
    return p->success ? 0 : 1;
}

static void DeleteDirectoryRecursive(const wchar_t* dir)
{
    wchar_t pattern[MAX_PATH];
    _snwprintf_s(pattern, _countof(pattern), _TRUNCATE, L"%s\\*", dir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern, &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0)
            continue;
        wchar_t path[MAX_PATH];
        _snwprintf_s(path, _countof(path), _TRUNCATE, L"%s\\%s", dir, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            DeleteDirectoryRecursive(path);
        else
            DeleteFileW(path);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    RemoveDirectoryW(dir);
}

// ── 解压进度提示窗口 ──
static const wchar_t* SPLASH_CLASS = L"LeyoChatSfxSplash";
static HWND g_hSplash = NULL;

static LRESULT CALLBACK SplashProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        // 背景
        HBRUSH bg = CreateSolidBrush(RGB(245, 245, 250));
        FillRect(hdc, &rc, bg);
        DeleteObject(bg);
        // 文字
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(80, 80, 100));
        Log("WM_PAINT: CreateFontW...");
        HFONT hFont = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0,
            DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Microsoft YaHei");
        Log("WM_PAINT: CreateFontW done, SelectObject...");
        HFONT oldFont = (HFONT)SelectObject(hdc, hFont);
        Log("WM_PAINT: DrawTextW...");
        DrawTextW(hdc, L"\x6b63\x5728\x51c6\x5907\x5b89\x88c5\x7a0b\x5e8f\xff0c\x8bf7\x7a0d\x5019\x2026", -1,
                  &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        Log("WM_PAINT: DrawTextW done");
        SelectObject(hdc, oldFont);
        DeleteObject(hFont);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void ShowSplash(HINSTANCE hInst)
{
    Log("ShowSplash: RegisterClassExW...");
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = SplashProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_WAIT);
    wc.lpszClassName = SPLASH_CLASS;
    RegisterClassExW(&wc);
    Log("ShowSplash: RegisterClassExW done");

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    Log("ShowSplash: GetSystemMetrics done sw=%d sh=%d", sw, sh);
    int ww = 340, wh = 80;
    g_hSplash = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        SPLASH_CLASS, L"LeyoChat", WS_POPUP | WS_VISIBLE,
        (sw - ww) / 2, (sh - wh) / 2, ww, wh,
        NULL, NULL, hInst, NULL);
    Log("ShowSplash: CreateWindowExW done hwnd=%p", (void*)g_hSplash);
    UpdateWindow(g_hSplash);
    Log("ShowSplash: UpdateWindow done");

    // 处理消息让窗口立刻绘制
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Log("ShowSplash: PeekMessage drain done");
}

static void HideSplash()
{
    if (g_hSplash) { DestroyWindow(g_hSplash); g_hSplash = NULL; }
}
// ── 解压进度提示窗口结束 ──

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int)
{
    LogInit();
    Log("SfxStub started (requireAdministrator + ImmDisableIME, bundle layout)");

    // 1. 禁用 IME：跳过 CreateWindowExW 首次触发的输入法初始化（~1.3s 延迟）
    {
        HMODULE hImm = LoadLibraryW(L"imm32.dll");
        if (hImm) {
            typedef BOOL (WINAPI* PFN_ImmDisableIME)(DWORD);
            PFN_ImmDisableIME pfn = (PFN_ImmDisableIME)GetProcAddress(hImm, "ImmDisableIME");
            if (pfn) { pfn((DWORD)-1); Log("ImmDisableIME: ok"); }
            FreeLibrary(hImm);
        }
    }

    // 2. 获取自身路径并读取 trailer（几乎瞬间，在 splash 之前完成）
    wchar_t selfPath[MAX_PATH];
    GetModuleFileNameW(NULL, selfPath, MAX_PATH);
    Log("self: %ls", selfPath);

    HANDLE hFile = CreateFileW(selfPath, GENERIC_READ, FILE_SHARE_READ,
                               NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Log("FAIL: cannot open self, err=%u", GetLastError());
        MessageBoxW(NULL, L"Cannot open self.", L"LeyoChat Setup", MB_ICONERROR);
        LogClose(); return 1;
    }

    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    Log("fileSize=%lld", fileSize.QuadPart);

    SfxTrailer trailer = {};
    LARGE_INTEGER trailerPos;
    trailerPos.QuadPart = fileSize.QuadPart - (LONGLONG)sizeof(SfxTrailer);
    SetFilePointerEx(hFile, trailerPos, NULL, FILE_BEGIN);
    DWORD bytesRead;
    ReadFile(hFile, &trailer, sizeof(trailer), &bytesRead, NULL);
    CloseHandle(hFile);
    Log("trailer: magic=0x%08X uiBundleSize=%u innerSetupSize=%u",
        trailer.magic, trailer.uiZipSize, trailer.innerSetupSize);

    if (trailer.magic != MAGIC || trailer.uiZipSize == 0) {
        Log("FAIL: bad trailer");
        MessageBoxW(NULL, L"Invalid or corrupted installer package.",
                    L"LeyoChat Setup", MB_ICONERROR);
        LogClose(); return 1;
    }

    // 3. 计算偏移 & 创建临时目录
    LONGLONG innerOffset  = trailerPos.QuadPart - (LONGLONG)trailer.innerSetupSize;
    LONGLONG bundleOffset = innerOffset - (LONGLONG)trailer.uiZipSize;
    Log("bundle offset=%lld size=%u, inner offset=%lld size=%u",
        bundleOffset, trailer.uiZipSize, innerOffset, trailer.innerSetupSize);

    wchar_t tempBase[MAX_PATH], tempDir[MAX_PATH];
    GetTempPathW(MAX_PATH, tempBase);
    _snwprintf_s(tempDir, _countof(tempDir), _TRUNCATE,
                 L"%sLeyoChatSetup_%u", tempBase, GetCurrentProcessId());
    CreateDirectoryW(tempDir, NULL);
    Log("tempDir: %ls", tempDir);

    // 4. 立即启动后台解压线程（与 splash 创建并行，消除等待空隙）
    static ExtractThreadParams extractParams;
    wcscpy_s(extractParams.selfPath, selfPath);
    extractParams.bundleOffset = bundleOffset;
    wcscpy_s(extractParams.tempDir, tempDir);
    extractParams.success = FALSE;
    HANDLE hExtractThread = CreateThread(NULL, 0, ExtractBundleThread, &extractParams, 0, NULL);
    Log("extract thread started");

    // 5. 显示 splash（与解压并行）
    ShowSplash(hInstance);
    Log("splash shown");

    // 6. 等待解压线程完成
    if (hExtractThread) {
        WaitForSingleObject(hExtractThread, 60000);
        CloseHandle(hExtractThread);
    }
    if (!extractParams.success) {
        MessageBoxW(NULL, L"Failed to extract installer files.",
                    L"LeyoChat Setup", MB_ICONERROR);
        DeleteDirectoryRecursive(tempDir);
        LogClose(); return 1;
    }
    Log("bundle extracted");

    // 8. 后台线程：raw 拷贝 inner-setup.exe（与 Qt 启动并行）
    wchar_t innerSetupDst[MAX_PATH];
    _snwprintf_s(innerSetupDst, _countof(innerSetupDst), _TRUNCATE,
                 L"%s\\LeyoChat-inner-setup.exe", tempDir);

    HANDLE hCopyThread = NULL;
    static CopyThreadParams copyParams;
    if (trailer.innerSetupSize > 0) {
        wcscpy_s(copyParams.srcPath, selfPath);
        wcscpy_s(copyParams.dstPath, innerSetupDst);
        copyParams.offset  = innerOffset;
        copyParams.size    = trailer.innerSetupSize;
        copyParams.success = FALSE;
        hCopyThread = CreateThread(NULL, 0, InnerSetupCopyThread, &copyParams, 0, NULL);
        Log("inner-setup copy thread started");
    }

    // 9. 启动 Qt Quick UI
    wchar_t uiExe[MAX_PATH];
    _snwprintf_s(uiExe, _countof(uiExe), _TRUNCATE,
                 L"%s\\LeyoChatSetup.exe", tempDir);
    if (!PathFileExistsW(uiExe)) {
        Log("FAIL: LeyoChatSetup.exe not found after extraction");
        if (hCopyThread) { WaitForSingleObject(hCopyThread, 30000); CloseHandle(hCopyThread); }
        MessageBoxW(NULL, L"LeyoChatSetup.exe not found after extraction.",
                    L"LeyoChat Setup", MB_ICONERROR);
        DeleteDirectoryRecursive(tempDir);
        LogClose(); return 1;
    }

    const wchar_t *installerMode =
        wcsstr(selfPath, L"LeyoChatServer") ? L"server" : L"client";

    wchar_t uiArgs[2048];
    _snwprintf_s(uiArgs, _countof(uiArgs), _TRUNCATE,
                 L"--inner-setup \"%s\" --installer-mode %s",
                 innerSetupDst,
                 installerMode);

    HideSplash();

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask       = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb      = L"open";  // 已经是管理员权限，无需再次提权
    sei.lpFile      = uiExe;
    sei.lpParameters = uiArgs;
    sei.lpDirectory = tempDir;
    sei.nShow       = SW_SHOWNORMAL;

    Log("ShellExecuteExW LeyoChatSetup.exe...");
    if (ShellExecuteExW(&sei) && sei.hProcess) {
        if (hCopyThread) {
            WaitForSingleObject(hCopyThread, 10000);
            CloseHandle(hCopyThread);
            Log("inner-setup copy done: success=%d", (int)copyParams.success);
        }
        Log("waiting for Qt UI to exit...");
        WaitForSingleObject(sei.hProcess, INFINITE);
        DWORD childExit = 0; GetExitCodeProcess(sei.hProcess, &childExit);
        Log("Qt UI exited: %u", childExit);
        CloseHandle(sei.hProcess);
    } else {
        Log("ShellExecuteExW failed, err=%u", GetLastError());
        if (hCopyThread) { WaitForSingleObject(hCopyThread, 10000); CloseHandle(hCopyThread); }
    }

    // 10. 清理临时目录
    Log("cleanup...");
    DeleteDirectoryRecursive(tempDir);

    Log("done");
    LogClose();
    return 0;
}
