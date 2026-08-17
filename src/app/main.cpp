#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QIcon>
#include <QLinearGradient>
#include <QLockFile>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QRandomGenerator>
#include <QSplashScreen>
#include <QStandardPaths>

#include <memory>

#include "app/ApplicationInfo.h"
#include "app/LeyoApplication.h"
#include "app/TestModeContext.h"
#include "diagnostics/RuntimeDiagnostics.h"
#include "recovery/ClientStartupOptions.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // 允许低权限进程向本进程拖拽文件（防止 UIPI 阻断 OLE 拖拽）
    ::ChangeWindowMessageFilter(WM_DROPFILES, MSGFLT_ADD);
    ::ChangeWindowMessageFilter(WM_COPYDATA, MSGFLT_ADD);
    ::ChangeWindowMessageFilter(0x0049 /*WM_COPYGLOBALDATA*/, MSGFLT_ADD);
#endif
    QElapsedTimer mainTimer;
    mainTimer.start();
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);  // 托盘常驻，仅"退出"菜单可终止
    const ClientStartupOptions startupOptions =
        ClientStartupOptions::fromArguments(QCoreApplication::arguments());

    // (#7) 全局 QPixmapCache 限额：防止头像/缩略图缓存无限增长
    QPixmapCache::setCacheLimit(20480); // 20MB

    std::unique_ptr<QSplashScreen> splash;
    if (!startupOptions.suppressSplash()) {
        // ── 精致启动闪屏 ──
        const int splashW = 480, splashH = 320;
        const qreal dpr = app.devicePixelRatio();
        QPixmap splashPixmap(static_cast<int>(splashW * dpr),
                             static_cast<int>(splashH * dpr));
        splashPixmap.setDevicePixelRatio(dpr);
        QPainter p(&splashPixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);

        // 渐变背景：深青色 → 深蓝绿
        QLinearGradient bg(0, 0, splashW, splashH);
        bg.setColorAt(0.0, QColor(20, 90, 80));
        bg.setColorAt(1.0, QColor(15, 55, 65));
        p.fillRect(0, 0, splashW, splashH, bg);

        // 底部装饰弧线
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 8));
        QPainterPath arc;
        arc.moveTo(-40, splashH + 60);
        arc.quadTo(splashW / 2.0, splashH - 80, splashW + 40, splashH + 60);
        arc.lineTo(splashW + 40, splashH);
        arc.lineTo(-40, splashH);
        arc.closeSubpath();
        p.drawPath(arc);

        // 应用图标（64x64）
        const QPixmap icon(QStringLiteral(":/app/leyochat-icon.png"));
        if (!icon.isNull()) {
            const int iconSize = 64;
            const int iconX = (splashW - iconSize) / 2;
            p.drawPixmap(iconX, 40, iconSize, iconSize,
                         icon.scaled(static_cast<int>(iconSize * dpr),
                                     static_cast<int>(iconSize * dpr),
                                     Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
        }

        // 品牌名称
        p.setPen(Qt::white);
        QFont brandFont(QString(), 26, QFont::Bold);
        brandFont.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        p.setFont(brandFont);
        p.drawText(QRect(0, 112, splashW, 40), Qt::AlignCenter,
                   QStringLiteral("LeyoChat"));

        // 副标题
        p.setPen(QColor(200, 220, 215));
        p.setFont(QFont(QString(), 11));
        p.drawText(QRect(0, 155, splashW, 25), Qt::AlignCenter,
                   QStringLiteral("LeyoChat \u5F00\u6E90\u5373\u65F6\u901A\u8BAF\u5E73\u53F0"));

        // 正能量文案（随机）
        const QStringList quotes = {
            QStringLiteral("\u6C9F\u901A\u521B\u9020\u4EF7\u503C\uFF0C\u534F\u4F5C\u6210\u5C31\u672A\u6765"),
            QStringLiteral("\u6BCF\u4E00\u6B21\u5BF9\u8BDD\uFF0C\u90FD\u662F\u4FE1\u4EFB\u7684\u5F00\u59CB"),
            QStringLiteral("\u8FDE\u63A5\u4F60\u6211\uFF0C\u8D4B\u80FD\u56E2\u961F"),
            QStringLiteral("\u7B80\u5355\u9AD8\u6548\uFF0C\u8BA9\u5DE5\u4F5C\u66F4\u6709\u6E29\u5EA6"),
            QStringLiteral("\u4E0E\u4F18\u79C0\u7684\u4EBA\u540C\u884C\uFF0C\u505A\u6709\u4EF7\u503C\u7684\u4E8B"),
            QStringLiteral("\u6280\u672F\u62C9\u8FD1\u8DDD\u79BB\uFF0C\u534F\u4F5C\u7A81\u7834\u8FB9\u754C"),
        };
        const int quoteIndex =
            QRandomGenerator::global()->bounded(static_cast<int>(quotes.size()));
        p.setPen(QColor(180, 210, 200, 200));
        QFont quoteFont(QString(), 10);
        quoteFont.setItalic(true);
        p.setFont(quoteFont);
        p.drawText(QRect(30, 200, splashW - 60, 25), Qt::AlignCenter,
                   QStringLiteral("\u300C%1\u300D").arg(quotes[quoteIndex]));

        // 加载提示
        p.setPen(QColor(150, 180, 170, 180));
        p.setFont(QFont(QString(), 9));
        p.drawText(QRect(0, 255, splashW, 20), Qt::AlignCenter,
                   QStringLiteral("\u6B63\u5728\u521D\u59CB\u5316\u670D\u52A1\u2026"));

        // 版本号
        p.setPen(QColor(120, 150, 140, 150));
        p.setFont(QFont(QString(), 8));
        p.drawText(QRect(0, splashH - 28, splashW - 16, 20),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("v%1").arg(ApplicationInfo::currentVersion()));
        splash = std::make_unique<QSplashScreen>(splashPixmap);
        splash->show();
        app.processEvents();
        qInfo() << "[startup-perf] splash shown:" << mainTimer.elapsed() << "ms";
    } else {
        qInfo() << "[startup-recovery] splash suppressed for crash recovery";
    }

    const TestModeContext testModeContext =
        TestModeContext::fromArguments(QCoreApplication::arguments());
    testModeContext.applyToApplication(app);
    QApplication::setApplicationDisplayName("LeyoChat");
    QApplication::setApplicationVersion(ApplicationInfo::currentVersion());
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/app/leyochat-icon.png")));
    Diagnostics::installRuntimeDiagnostics();

#ifdef Q_OS_WIN
    if (startupOptions.shouldRegisterWindowsArr()) {
        const std::wstring restartCommandLine =
            startupOptions.windowsArrCommandLine().toStdWString();
        const HRESULT restartResult = ::RegisterApplicationRestart(
            restartCommandLine.c_str(), RESTART_NO_PATCH | RESTART_NO_REBOOT);
        if (SUCCEEDED(restartResult)) {
            qInfo() << "[startup-recovery] Windows ARR registered";
        } else {
            qWarning() << "[startup-recovery] Windows ARR registration failed, hr="
                       << Qt::hex << static_cast<qulonglong>(restartResult);
        }
    } else {
        qInfo() << "[startup-recovery] launcher supervision active; Windows ARR disabled";
    }
#endif

    // Measure time from process creation to now using Win32 GetProcessTimes
#ifdef Q_OS_WIN
    {
        FILETIME creationTime, exitTime, kernelTime, userTime;
        if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
            FILETIME nowFt;
            GetSystemTimeAsFileTime(&nowFt);
            ULARGE_INTEGER now, created;
            now.LowPart = nowFt.dwLowDateTime;
            now.HighPart = nowFt.dwHighDateTime;
            created.LowPart = creationTime.dwLowDateTime;
            created.HighPart = creationTime.dwHighDateTime;
            const qint64 processAgeMs = static_cast<qint64>((now.QuadPart - created.QuadPart) / 10000ULL);
            qInfo() << "[startup-perf] process age at diagnostics:" << processAgeMs << "ms (mainTimer:" << mainTimer.elapsed() << "ms)";
        }
    }
#else
    qInfo() << "[startup-perf] main() after QApp+diagnostics:" << mainTimer.elapsed() << "ms";
#endif

#ifdef Q_OS_WIN
    const std::wstring mutexName =
        QStringLiteral("Global\\%1").arg(testModeContext.singleInstanceKey()).toStdWString();
    HANDLE singleInstanceMutex = ::CreateMutexW(nullptr, TRUE, mutexName.c_str());
    if (singleInstanceMutex && ::GetLastError() == ERROR_ALREADY_EXISTS) {
        ::CloseHandle(singleInstanceMutex);
        return 0;
    }
#else
    QString lockRoot = testModeContext.appLocalDataRoot();
    if (lockRoot.isEmpty()) {
        lockRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    if (lockRoot.isEmpty()) {
        lockRoot = QDir::tempPath();
    }
    QDir().mkpath(lockRoot);
    QLockFile singleInstanceLock(QDir(lockRoot).filePath(testModeContext.lockFileName()));
    singleInstanceLock.setStaleLockTime(0);
    if (!singleInstanceLock.tryLock(0)) {
        return 0;
    }
#endif

    LeyoApplication leyoApplication(app, startupOptions);
    if (splash) {
        leyoApplication.setSplashScreen(splash.get());
    }
    qInfo() << "[startup-perf] pre-run():" << mainTimer.elapsed() << "ms";
    const int exitCode = leyoApplication.run();
    qInfo() << "[app-exit] run() returned, exitCode=" << exitCode << "elapsed=" << mainTimer.elapsed() << "ms";
#ifdef Q_OS_WIN
    if (singleInstanceMutex) {
        ::ReleaseMutex(singleInstanceMutex);
        ::CloseHandle(singleInstanceMutex);
    }
#endif
    return exitCode;
}
